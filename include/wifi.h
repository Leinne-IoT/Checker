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
        if(id == IP_EVENT_STA_GOT_IP){
            connect = true;
            debug("[WiFi] 아이피: " IPSTR "\n", IP2STR(&((ip_event_got_ip_t*) data)->ip_info.ip));
        }else{
            switch(id){
                case WIFI_EVENT_STA_START:
                    esp_wifi_connect();
                    break;
                case WIFI_EVENT_STA_STOP:
                case WIFI_EVENT_STA_DISCONNECTED:
                    connect = false;
                    esp_wifi_connect();
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
            }
        };
        esp_wifi_set_config(WIFI_IF_AP, &config);
        esp_wifi_set_mode(WIFI_MODE_APSTA);
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