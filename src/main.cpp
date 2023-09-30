#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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
#include "door.h"
#include "wifi.h"
#include "utils.h"
#include "storage.h"
#include "safe_queue.h"
#include "esp_websocket_client.h"

using namespace std;

atomic<bool> sendPhase = false;

int64_t lastUpdateTime = 0;
int64_t wifiTryConnectTime = 0;

void networkLoop(void* args){
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    initWiFi();
    storage::begin();

    auto url = storage::getString("websocket_url");
    if(url.find_first_of("http") == string::npos){
        url = "ws://leinne.net:33877/";
        storage::setString("websocket_url", url);
    }

    esp_websocket_client_handle_t client = NULL;
    esp_websocket_client_config_t websocket_cfg = {
        .uri = url.c_str(),
        .reconnect_timeout_ms = 500,
    };

    while(client == NULL){
        client = esp_websocket_client_init(&websocket_cfg);
        if(esp_websocket_client_start(client) != ESP_OK){
            client = NULL;
        }
        //esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    }

    esp_http_client_config_t config = {.url = url.c_str()};
    for(;;){
        if(!connectWifi){
            if(getWiFiStatus() != WIFI_MODE_APSTA){
                auto now = esp_timer_get_time();
                if(wifiTryConnectTime == 0){
                    wifiTryConnectTime = now;
                }
                if(now - wifiTryConnectTime >= 8 * 1000000){
                    debug("[WiFi] Start AP Mode\n");
                    esp_wifi_set_mode(WIFI_MODE_APSTA);
                }
            }else if(startAP){
                startWebServer();
            }
            continue;
        }

        auto state = doorStateQueue.pop();

        sendPhase = true;
        while(!esp_websocket_client_is_connected(client));

        auto device = storage::getDeviceId();
        uint8_t buffer[64] = {
            state.open,
        };
        
        uint8_t i = 1;
        for(uint8_t max = i + 4; i < max; ++i){
            buffer[i] = (state.updateTime >> (8 * (max - i - 1))) & 255;
        }
        auto current = millis();
        for(uint8_t max = i + 4; i < max; ++i){
            buffer[i] = (current >> (8 * (max - i - 1))) & 255;
        }
        for(uint8_t j = 0; j < device.length(); ++j){
            buffer[i++] = device[j];
        }
        esp_websocket_client_send_with_opcode(client, WS_TRANSPORT_OPCODES_BINARY, buffer, i - 1, portMAX_DELAY);
        sendPhase = false;
    }
}

void checkDoor(void* args){
    int64_t lastReset = -1;
    for(;;){
        checkDoorState(&lastUpdateTime);
        if(gpio_get_level(RESET_PIN) == 0){
            lastUpdateTime = millis();
            if(lastReset == -1){
                lastReset = millis();
            }else if(millis() - lastReset > 5 * 1000){
                clearWiFiData();
            }
        }else{
            lastReset = -1;
        }

        if(!connectWifi){
            continue;
        }

        if(server != NULL){
            stopWebServer();
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            lastUpdateTime = millis();
        }
        
        #if !defined(DEBUG_MODE)
        if(!sendPhase && millis() - lastUpdateTime > 8 * 1000){
            debug("[SLEEP] Start Deep Sleep\n");
            rtc_gpio_pullup_en(SWITCH_PIN);
            esp_sleep_enable_ext0_wakeup(SWITCH_PIN, !lastOpenDoor);

            esp_deep_sleep_start();
        }
        #endif
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
        debug("[Main] Wake Up Cause: %d\n", cause);
    }

    TaskHandle_t doorTask;
    TaskHandle_t networkTask;
    auto core = esp_cpu_get_core_id();
    xTaskCreatePinnedToCore(checkDoor, "door", 10000, NULL, 1, &doorTask, !core);
    xTaskCreatePinnedToCore(networkLoop, "network", 10000, NULL, 1, &networkTask, core);
}