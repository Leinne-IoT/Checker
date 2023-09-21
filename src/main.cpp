/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <nvs.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <ArduinoJson.h>
#include <string>
#include <esp_http_client.h>

#include "web.h"
#include "queue.h"
#include "storage.h"

using namespace std;

bool connectWifi = false;
bool lastOpenDoor = false;
int64_t lastUpdateTime = 0;
int64_t wifiTryConnectTime = 0;

TaskHandle_t networkTask;
TaskHandle_t doorTask;

struct DoorState{
    bool open;
    int64_t updateTime;
};
SafeQueue<DoorState> doorStateQueue;

static void wifiHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    printf("[WiFi] Code: %ld\n", event_id);
    if(event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED){
        esp_wifi_connect();
        connectWifi = false;
    }/*else if(event_id == WIFI_EVENT_STA_DISCONNECTED){
        if(s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY){
            ++s_retry_num;
            esp_wifi_connect();
            ESP_LOGI(TAG, "와이파이에 다시 연결합니다.");
        }else{
            ESP_LOGI(TAG, "와이파이 연결에 실패했습니다.");
            xEventGroupSetBits(wifi_status, WIFI_FAIL_BIT);
        }
    }*/
}

static void ipHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    connectWifi = true;
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    printf("[WiFi] 아이피: " IPSTR "\n", IP2STR(&event->ip_info.ip));
}

void initWiFi(){
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiHandler, NULL));

    /*wifi_config_t staConfig = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };
    strcpy((char*) staConfig.sta.ssid, ssid.c_str());
    strcpy((char*) staConfig.sta.password, password.c_str());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &staConfig));*/

    wifi_config_t apConfig = {
        .ap = {
            .ssid = "Checker",
            .password = "",
            .ssid_len = strlen("Checker"),
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 1,
        }
    };
    esp_wifi_set_config(WIFI_IF_AP, &apConfig);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void networkLoop(void* args){
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    initWiFi();
    httpd_handle_t server = NULL;
    for(;;){
        if(!connectWifi){
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            if(mode == WIFI_MODE_APSTA){
                if(!server){
                    ESP_ERROR_CHECK(startWebServer(&server));
                }
            }else{
                auto now = esp_timer_get_time();
                if(wifiTryConnectTime == 0){
                    wifiTryConnectTime = now;
                }
                if(now - wifiTryConnectTime >= 10 * 1000000){
                    esp_wifi_set_mode(WIFI_MODE_APSTA);
                }
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        DynamicJsonDocument json(2048);
        json["id"] = Storage.getDeviceId();
        JsonArray dataArray = json.createNestedArray("data");

        do{
            auto data = doorStateQueue.pop();
            auto array = dataArray.createNestedArray();
            array.add(data.open ? 1 : 0);
            array.add(data.updateTime);
        }while(!doorStateQueue.empty());
        
        esp_http_client_config_t config = {.url = "http://leinne.net:33877/req_door"};
        auto client = esp_http_client_init(&config);
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");

        string body;
        json["current"] = esp_timer_get_time() / 1000LL;
        serializeJson(json, body);
        //printf("%s\n", body.c_str());
        esp_http_client_set_post_field(client, body.c_str(), body.length());
        esp_http_client_perform(client);
        esp_http_client_cleanup(client);

        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if(mode == WIFI_MODE_APSTA){
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            stopWebServer(&server);
        }
    }
}

void checkDoor(void* args){
    for(;;){
        bool openDoor = !gpio_get_level(GPIO_NUM_25);
        if(lastOpenDoor != openDoor){
            DoorState state = {
                .open = openDoor,
                .updateTime = esp_timer_get_time() / 1000LL,
            };
            doorStateQueue.push(state);
            //printf(openDoor ? "[%lld] 문 열림\n" : "[%lld] 문 닫힘\n", state.updateTime);

            lastOpenDoor = openDoor;
            lastUpdateTime = esp_timer_get_time();
        }

        if(esp_timer_get_time() - lastUpdateTime > 8 * 1000000){ // 업데이트가 발생한지 8초가 지난 경우
            printf("[SLEEP] Start Deep Sleep\n");

            rtc_gpio_pullup_en(GPIO_NUM_25);
            esp_sleep_enable_ext0_wakeup(GPIO_NUM_25, openDoor);
            esp_deep_sleep_start();
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main(){
    gpio_set_direction(GPIO_NUM_25, GPIO_MODE_INPUT);
    gpio_pullup_en(GPIO_NUM_25);

    esp_log_level_set("*", ESP_LOG_NONE);
    xTaskCreatePinnedToCore(checkDoor, "door", 10000, NULL, 1, &doorTask, 0);
    xTaskCreatePinnedToCore(networkLoop, "network", 10000, NULL, 1, &networkTask, 1);
}