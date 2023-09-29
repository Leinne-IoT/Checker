#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <ArduinoJson.h>
#include <driver/rtc_io.h>
#include <esp_http_client.h>
#include <atomic>
#include <string>

#ifdef CONFIG_IDF_TARGET_ESP32
#define RESET_PIN GPIO_NUM_26
#define SWITCH_PIN GPIO_NUM_25
#define LED_BUILTIN GPIO_NUM_2
#else
#define RESET_PIN GPIO_NUM_8
#define SWITCH_PIN GPIO_NUM_7
#define LED_BUILTIN GPIO_NUM_21
#endif

#include "web.h"
#include "wifi.h"
#include "utils.h"
#include "storage.h"
#include "safe_queue.h"

using namespace std;

string devicdId = "";
StorageClass storage;

atomic<bool> sendPhase = false;

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

string getDeviceId(){
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

void networkLoop(void* args){
    initWiFi();
    storage.begin();

    auto url = storage.getString("send_url");
    if(url.find_first_of("http") == string::npos){
        url = "http://leinne.net:33877/req_door";
        storage.setString("send_url", url);
    }
    esp_http_client_config_t config = {.url = url.c_str()};
    for(;;){
        if(!connectWifi){
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            if(mode == WIFI_MODE_APSTA){
                if(startAP && server == NULL){
                    startWebServer(server);
                }
            }else{
                auto now = esp_timer_get_time();
                if(wifiTryConnectTime == 0){
                    wifiTryConnectTime = now;
                }
                if(now - wifiTryConnectTime >= 8 * 1000000){
                    debug("[WiFi] Start AP Mode\n");
                    esp_wifi_set_mode(WIFI_MODE_APSTA);
                }
            }
            continue;
        }

        DynamicJsonDocument json(1024);
        json["id"] = getDeviceId();
        JsonArray dataArray = json.createNestedArray("data");

        uint8_t length = 0;
        do{
            auto data = doorStateQueue.pop();
            auto array = dataArray.createNestedArray();
            array.add(data.open ? 1 : 0);
            array.add(data.updateTime);
        }while(++length < 10 && !doorStateQueue.empty());
        
        sendPhase = true;
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

DoorState getDoorState(){
    return {
        .open = gpio_get_level(SWITCH_PIN) != 0,
        .updateTime = millis(),
    };
}

void checkDoorState(){
    auto state = getDoorState();
    if(lastOpenDoor != state.open){
        doorStateQueue.push(state);
        debug(state.open ? "[%lld] 문 열림\n" : "[%lld] 문 닫힘\n", state.updateTime);

        lastOpenDoor = state.open;
        lastUpdateTime = state.updateTime;
    }
}

void checkDoor(void* args){
    for(;;){
        checkDoorState();

        if(!connectWifi){
            continue;
        }

        if(server != NULL){
            stopWebServer(&server);
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            lastUpdateTime = millis();
        }
        
        if(!sendPhase && millis() - lastUpdateTime > 8 * 1000){
            debug("[SLEEP] Start Deep Sleep\n");
            rtc_gpio_pullup_en(SWITCH_PIN);
            esp_sleep_enable_ext0_wakeup(SWITCH_PIN, !lastOpenDoor);

            #if defined(DEBUG_MODE) && defined(CONFIG_IDF_TARGET_ESP32S3)
            lastUpdateTime = millis();
            #else
            esp_deep_sleep_start();
            #endif
        }
        //vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main(){
    gpio_pullup_en(RESET_PIN);
    gpio_pullup_en(SWITCH_PIN);
    gpio_set_direction(RESET_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(SWITCH_PIN, GPIO_MODE_INPUT);

    auto cause = esp_sleep_get_wakeup_cause();
    if(cause == ESP_SLEEP_WAKEUP_EXT0){
        DoorState state = {
            .open = lastOpenDoor = !lastOpenDoor,
            .updateTime = millis(),
        };
        doorStateQueue.push(state);
        debug(state.open ? "[%lld] 문 열림\n" : "[%lld] 문 닫힘\n", state.updateTime);
    }else{
        lastOpenDoor = getDoorState().open;
        debug("[MAIN] Wake Up Cause: %d\n", cause);
    }

    xTaskCreatePinnedToCore(checkDoor, "door", 10000, NULL, 1, &doorTask, 0);
    xTaskCreatePinnedToCore(networkLoop, "network", 10000, NULL, 1, &networkTask, 1);
}