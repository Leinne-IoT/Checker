#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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

#define DEFAULT_WEBSOCKET_URL "ws://leinne.net:33877/"

#ifdef CONFIG_IDF_TARGET_ESP32
#define RESET_PIN GPIO_NUM_26
#define SWITCH_PIN GPIO_NUM_25
#define BUZZER_PIN GPIO_NUM_27
#define BATTERY_PIN GPIO_NUM_0
#define LED_BUILTIN GPIO_NUM_2
#else
#define RESET_PIN GPIO_NUM_8
#define SWITCH_PIN GPIO_NUM_7
#define BUZZER_PIN GPIO_NUM_9
#define BATTERY_PIN GPIO_NUM_0
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
            }else if(millis() - lastReset > 8 * 1000){
                wifi::clear();
                esp_restart();
            }
        }else if(millis() - lastReset < 300){
            lastReset = -1;
        }else if(lastReset != -1){
            esp_restart();
        }

        if(ws::connectServer && door::queue.empty() && millis() - lastUpdateTime > DEEP_SLEEP_DELAY){
            deepSleep(SWITCH_PIN, !door::lastState);
        }
    }
}

static void webSocketHandler(void* object, esp_event_base_t base, int32_t eventId, void* eventData){
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*) eventData;
    if(eventId == WEBSOCKET_EVENT_CONNECTED){
        lastUpdateTime = millis();
    }else if(eventId == WEBSOCKET_EVENT_DATA && data->op_code == BINARY){
        switch(data->data_ptr[0]){
            case 0x10: // LED LIGHT
                lastUpdateTime = millis();
                debug("[WS] LED 점등\n");
                gpio_set_level(LED_BUILTIN, data->data_ptr[1]);
                break;
            case 0x20: // 피에조 부저
                lastUpdateTime = millis();
                // TODO: 부저 작동시키기
                break;
        }
    }
}

static void wifiHandler(void* arg, esp_event_base_t base, int32_t id, void* data){
    if(id == WIFI_EVENT_AP_START){
        web::start();
    }else if(web::stop()){
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
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

    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiHandler, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &wifiHandler, NULL);

    for(;;){
        int64_t time = millis();
        while(!wifi::connect){
            if(
                wifi::getMode() != WIFI_MODE_APSTA &&
                millis() - time >= 6 * 1000
            ){
                wifi::setApMode();
            }
            continue;
        }
        
        time = millis();
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
    gpio_pullup_en(SWITCH_PIN);
    gpio_set_direction(SWITCH_PIN, GPIO_MODE_INPUT);

    auto cause = esp_sleep_get_wakeup_cause();
    if(cause == ESP_SLEEP_WAKEUP_EXT0){
        door::check(!door::lastState);
    }else{
        door::init();
        debug("[Main] Wake Up Cause: %d\n", cause);
    }

    gpio_pullup_en(RESET_PIN);
    gpio_set_direction(RESET_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_BUILTIN, GPIO_MODE_OUTPUT);

    TaskHandle_t gpioTask;
    TaskHandle_t networkTask;
    xTaskCreatePinnedToCore(checkGPIO, "gpio", 10000, NULL, 1, &gpioTask, 1);
    xTaskCreatePinnedToCore(networkLoop, "network", 10000, NULL, 1, &networkTask, 0);

    for(;;){
        door::queue.waitPush();
        while(!ws::isConnected() || !ws::connectServer);
        do{
            ws::sendDoorState(door::queue.pop());
        }while(!door::queue.empty());
    }
}