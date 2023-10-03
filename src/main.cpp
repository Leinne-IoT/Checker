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

//#define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEEP_SLEEP_DELAY 0xFFFFFFFF
#else
#define DEEP_SLEEP_DELAY 15000
#endif


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
#include "websocket.h"
#include "safe_queue.h"

using namespace std;

atomic<int64_t> lastUpdateTime = 0;
atomic<uint32_t> sleepDelay = DEEP_SLEEP_DELAY;

static void checkGPIO(void* args){
    int64_t lastReset = -1;
    for(;;){
        if(door::check()){
            lastUpdateTime = millis();
        }

        if(gpio_get_level(RESET_PIN) == 0){
            lastUpdateTime = millis();
            if(lastReset == -1){
                lastReset = millis();
            }else if(millis() - lastReset > 5 * 1000){
                wifi::clear();
                esp_restart();
            }
        }else{
            lastReset = -1;
        }

        if(!wifi::connect){
            continue;
        }

        if(server != NULL){
            web::stop();
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        }

        if(ws::connectServer && doorStateQueue.empty() && millis() - lastUpdateTime > sleepDelay){
            deepSleep(SWITCH_PIN, !lastOpenDoor);
        }
    }
}

static void webSocketHandler(void* object, esp_event_base_t base, int32_t eventId, void* eventData){
    if(eventId == WEBSOCKET_EVENT_CONNECTED){
        lastUpdateTime = millis();
    }
}

static void networkLoop(void* args){
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    storage::begin();
    wifi::begin();
    ws::start(webSocketHandler);

    for(;;){
        int64_t wifiTryConnectTime = millis();
        while(!wifi::connect){
            if(wifi::getMode() != WIFI_MODE_APSTA){
                auto now = millis();
                if(now - wifiTryConnectTime >= 5 * 1000){
                    esp_wifi_set_mode(WIFI_MODE_APSTA);
                }
            }else if(wifi::beginAP){
                web::start();
            }
        }
        
        int64_t time = millis();
        while(!ws::connectServer){
            if(!ws::isConnected() || millis() - time < 500){
                continue;
            }
            time = millis();
            ws::sendWelcome();
        }
    }
}

extern "C" void app_main(){
    gpio_pullup_en(RESET_PIN);
    gpio_pullup_en(SWITCH_PIN);
    gpio_set_direction(RESET_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(SWITCH_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);

    auto cause = esp_sleep_get_wakeup_cause();
    if(cause == ESP_SLEEP_WAKEUP_EXT0){
        DoorState state = {
            .open = lastOpenDoor = !lastOpenDoor,
            .updateTime = millis(),
        };
        doorStateQueue.push(state);
        debug(state.open ? "[Door] 문 열림 (%d개 대기중)\n" : "[Door] 문 닫힘 (%d개 대기중)\n", doorStateQueue.size() - 1);
    }else{
        lastOpenDoor = door::state().open;
        debug("[Main] Wake Up Cause: %d\n", cause);
    }

    TaskHandle_t gpioTask;
    TaskHandle_t networkTask;
    auto core = esp_cpu_get_core_id();
    xTaskCreatePinnedToCore(checkGPIO, "gpio", 10000, NULL, 1, &gpioTask, !core);
    xTaskCreatePinnedToCore(networkLoop, "network", 10000, NULL, 1, &networkTask, core);

    for(;;){
        doorStateQueue.waitPush();
        while(!ws::isConnected() || !ws::connectServer);
        do{
            ws::sendDoorState(doorStateQueue.pop());
        }while(!doorStateQueue.empty());
    }
}