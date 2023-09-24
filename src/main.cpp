#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>
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
#include <atomic>

#ifdef CONFIG_IDF_TARGET_ESP32
#define SWITCH_PIN GPIO_NUM_25
#else
#define SWITCH_PIN GPIO_NUM_7
#endif

//#define DEBUG_MODE

#include "log.h"
#include "web.h"
#include "utils.h"
#include "storage.h"
#include "safe_queue.h"

using namespace std;

string devicdId = "";

atomic<bool> startAP = false;
atomic<bool> sendPhase = false;
atomic<bool> connectWifi = false;

httpd_handle_t server = NULL;

int64_t lastUpdateTime = 0;
int64_t wifiTryConnectTime = 0;
RTC_DATA_ATTR bool lastOpenDoor = false;

TaskHandle_t doorTask;
TaskHandle_t networkTask;

struct DoorState{
    bool open;
    int64_t updateTime;
};
SafeQueue<DoorState> doorStateQueue;

string getDeviceId(StorageClass storage){
    if(devicdId.length() == 6){
        return devicdId;
    }

    string id = storage.getString("DEVICE_ID", 7);
    if(id.length() == 6){
        return devicdId = id;
    }else{
        stringstream stream;
        for(uint8_t i = 0; i < 6; ++i){
            stream << (char) random('a', 'z');
        }
        storage.setString("DEVICE_ID", devicdId = stream.str(), false);
    }
    return devicdId;
}

static void wifiHandler(void* arg, esp_event_base_t base, int32_t id, void* data){
    debugPrint("[WiFi] Code: %ld\n", id);
    switch(id){
        case WIFI_EVENT_STA_START:
        case WIFI_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            connectWifi = false;
            break;
        case WIFI_EVENT_AP_START:
            startAP = true;
            break;
        case WIFI_EVENT_AP_STOP:
            startAP = false;
            break;
    }
}

static void ipHandler(void* arg, esp_event_base_t basename, int32_t id, void* data){
    connectWifi = true;
    ip_event_got_ip_t* ipData = (ip_event_got_ip_t*) data;
    debugPrint("[WiFi] 아이피: " IPSTR, IP2STR(&ipData->ip_info.ip));
}

void initWiFi(){
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiHandler, NULL));

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
    initWiFi();
    
    StorageClass storage;
    storage.begin();
    for(;;){
        if(!connectWifi){
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            if(mode == WIFI_MODE_APSTA){
                if(startAP && server == NULL){
                    startWebServer(&server);
                }
            }else{
                auto now = esp_timer_get_time();
                if(wifiTryConnectTime == 0){
                    wifiTryConnectTime = now;
                }
                if(now - wifiTryConnectTime >= 8 * 1000000){
                    debugPrint("[WiFi] Start AP Mode\n");
                    esp_wifi_set_mode(WIFI_MODE_APSTA);
                }
            }
            continue;
        }

        DynamicJsonDocument json(2048);
        json["id"] = getDeviceId(storage);
        JsonArray dataArray = json.createNestedArray("data");

        do{
            auto data = doorStateQueue.pop();
            auto array = dataArray.createNestedArray();
            array.add(data.open ? 1 : 0);
            array.add(data.updateTime);
        }while(!doorStateQueue.empty());
        
        sendPhase = true;
        esp_http_client_config_t config = {.url = "http://leinne.net:33877/req_door"};
        auto client = esp_http_client_init(&config);
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");

        esp_err_t err = ESP_FAIL;
        while(err != ESP_OK){
            string body;
            json["current"] = esp_timer_get_time() / 1000LL + 100; // 전송시의 약간의 오차 100ms 추가
            serializeJson(json, body);
            esp_http_client_set_post_field(client, body.c_str(), body.length());
            err = esp_http_client_perform(client);
        }
        esp_http_client_cleanup(client);
        sendPhase = false;
    }
}

void checkDoor(void* args){
    for(;;){
        bool openDoor = !gpio_get_level(SWITCH_PIN);
        if(lastOpenDoor != openDoor){
            doorStateQueue.push({
                .open = openDoor,
                .updateTime = esp_timer_get_time() / 1000LL,
            });
            debugPrint(openDoor ? "문 열림\n" : "문 닫힘\n");

            lastOpenDoor = openDoor;
            lastUpdateTime = esp_timer_get_time();
        }

        if(!connectWifi){
            continue;
        }

        if(server != NULL){
            debugPrint("[WiFi] Stop AP Mode\n");
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            stopWebServer(&server);
            lastUpdateTime = esp_timer_get_time();
        }
        
        if(!sendPhase && esp_timer_get_time() - lastUpdateTime > 8 * 1000000){
            debugPrint("[SLEEP] Start Deep Sleep\n");
            rtc_gpio_pullup_en(SWITCH_PIN);
            esp_sleep_enable_ext0_wakeup(SWITCH_PIN, openDoor);

            #if defined(DEBUG_MODE) && defined(CONFIG_IDF_TARGET_ESP32S3)
            lastUpdateTime = esp_timer_get_time();
            #else
            esp_deep_sleep_start();
            #endif
        }
        //vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main(){
    gpio_pullup_en(SWITCH_PIN);
    gpio_set_direction(SWITCH_PIN, GPIO_MODE_INPUT);

    auto cause = esp_sleep_get_wakeup_cause();
    if(cause == ESP_SLEEP_WAKEUP_EXT0){
        lastOpenDoor = !lastOpenDoor;
        doorStateQueue.push({
            .open = lastOpenDoor,
            .updateTime = esp_timer_get_time() / 1000LL,
        });
        debugPrint(lastOpenDoor ? "문 열림\n" : "문 닫힘\n");
    }else{
        lastOpenDoor = !gpio_get_level(SWITCH_PIN);
        debugPrint("[MAIN] Wake Up Cause: %d\n", cause);
    }

    xTaskCreatePinnedToCore(checkDoor, "door", 10000, NULL, 1, &doorTask, 0);
    xTaskCreatePinnedToCore(networkLoop, "network", 10000, NULL, 1, &networkTask, 1);
}