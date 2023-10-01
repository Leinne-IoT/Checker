#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <ArduinoJson.h>
#include <driver/rtc_io.h>
#include <esp_http_client.h>
//#include <esp_sntp.h>
//#include <esp_netif_sntp.h>
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

static void networkLoop(void* args){
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    initWiFi();
    storage::begin();

    /** 
     * 내부 타이머 대신 실제 시간을 활용하여 열림/닫힘에 대한
     * 정확한 시간을 확보하여 전달하고자 하였으나
     * sntp 서버에 연결하는 시간이 너무 오래걸려 비활성화했습니다.
     * 
     * 데이터가 신속하게 전달되는것보다 ms 단위의 정밀한 기록이 중요하다면
     * 해당 기능을 활성화해도 됩니다. (기능 활성화시 약 15~20초[WiFi 연결시간 + sntp 연결시간]가 걸림)
     */

    /*esp_sntp_config_t config = {
        .smooth_sync = false,
        .server_from_dhcp = true,
        .wait_for_sync = true,
        .start = false,
        .sync_cb = NULL,
        .renew_servers_after_new_IP = true,
        .ip_event_to_renew = IP_EVENT_STA_GOT_IP,
        .index_of_first_server = 0,
        .num_of_servers = 1,
        .servers = {"time.google.com"}
    };
    esp_netif_sntp_init(&config);*/

    auto url = storage::getString("websocket_url");
    if(url.find_first_of("http") == string::npos){
        storage::setString("websocket_url", url = "ws://leinne.net:33877/");
    }

    esp_websocket_client_handle_t client = NULL;
    esp_websocket_client_config_t websocket_cfg = {
        .uri = url.c_str(),
        .reconnect_timeout_ms = 500,
    };

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
                web::start();
            }
            continue;
        }

        auto state = doorStateQueue.pop();

        sendPhase = true;
        while(!esp_websocket_client_is_connected(client)){
            if(client != NULL){
                continue;
            }
            client = esp_websocket_client_init(&websocket_cfg);
            if(esp_websocket_client_start(client) != ESP_OK){
                client = NULL;
            }
        }

        uint8_t i = 1;
        uint8_t buffer[64] = {state.open};
        
        for(uint8_t max = i + 8; i < max; ++i){
            buffer[i] = (state.updateTime >> (8 * (max - i - 1))) & 0xFF;
        }

        auto current = millis();
        for(uint8_t max = i + 8; i < max; ++i){
            buffer[i] = (current >> (8 * (max - i - 1))) & 0xFF;
        }

        auto device = storage::getDeviceId();
        for(uint8_t j = 0; j < device.length(); ++j){
            buffer[i++] = device[j];
        }
        esp_websocket_client_send_with_opcode(client, WS_TRANSPORT_OPCODES_BINARY, buffer, i, portMAX_DELAY);
        sendPhase = false;
    }
}

static void checkDoor(void* args){
    int64_t lastReset = -1;
    for(;;){
        door::check(&lastUpdateTime);
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
            web::stop();
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
        lastOpenDoor = door::get().open;
        debug("[Main] Wake Up Cause: %d\n", cause);
    }

    TaskHandle_t doorTask;
    TaskHandle_t networkTask;
    auto core = esp_cpu_get_core_id();
    xTaskCreatePinnedToCore(checkDoor, "door", 10000, NULL, 1, &doorTask, !core);
    xTaskCreatePinnedToCore(networkLoop, "network", 10000, NULL, 1, &networkTask, core);
}