#pragma once

#include <nvs.h>
#include <atomic>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "utils.h"

using namespace std;

namespace wifi{
    atomic<bool> connect = false;

    static void eventHandler(void* arg, esp_event_base_t base, int32_t id, void* data){
        static int64_t start = -1;
        if(id == IP_EVENT_STA_GOT_IP){
            connect = true;
            printf("[WiFi] 아이피: " IPSTR ", time: %lldms\n", IP2STR(&((ip_event_got_ip_t*) data)->ip_info.ip), millis() - start);
        }else{
            switch(id){
                case WIFI_EVENT_STA_START:
                    std::cout << "[WiFi] Start WiFi\n";
                    start = millis();
                    esp_wifi_connect();
                    break;
                case WIFI_EVENT_STA_DISCONNECTED:
                    if(connect){
                        std::cout << "[WiFi] Disconnected WiFi\n";
                    }
                    connect = false;
                    esp_wifi_connect();
                    break;
                case WIFI_EVENT_AP_START:
                    std::cout << "[WiFi] Start AP\n";
                    break;
            }
        }
    }

    void begin(){
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, NULL));

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    void setApMode(){
        esp_netif_create_default_wifi_ap();
        wifi_config_t config = {
            .ap = {
                .ssid = "Checker",
                .password = "",
                .ssid_len = 7,
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = 2,
            }
        };
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    }

    void setData(string ssid, string password){
        wifi_config_t config = {
            .sta = {
                .ssid = "",
                .password = ""
            }
        };
        strcpy((char*) config.sta.ssid, ssid.c_str());
        strcpy((char*) config.sta.password, password.c_str());
        esp_wifi_set_config(WIFI_IF_STA, &config);
    }
    
    void clear(){
        wifi_config_t staConfig = {
            .sta = {
                .ssid = "",
                .password = "",
            }
        };
        esp_wifi_set_config(WIFI_IF_STA, &staConfig);
    }

    wifi_mode_t getMode(){
        wifi_mode_t mode;
        if(esp_wifi_get_mode(&mode) == ESP_OK){
            return mode;
        }
        return WIFI_MODE_NULL;
    }
}