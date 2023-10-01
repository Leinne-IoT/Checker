#pragma once

#include <nvs.h>
#include <atomic>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "utils.h"

using namespace std;

namespace wifi{
    atomic<bool> beginAP = false;
    atomic<bool> connect = false;

    static void wifiHandler(void* arg, esp_event_base_t base, int32_t id, void* data){
        switch(id){
            case WIFI_EVENT_STA_START:
            case WIFI_EVENT_STA_DISCONNECTED:
                esp_wifi_connect();
                connect = false;
                break;
            case WIFI_EVENT_AP_START:
                beginAP = true;
                break;
            case WIFI_EVENT_AP_STOP:
                beginAP = false;
                break;
        }
    }

    static void ipHandler(void* arg, esp_event_base_t basename, int32_t id, void* data){
        connect = true;
        debug("[WiFi] 아이피: " IPSTR "\n", IP2STR(&((ip_event_got_ip_t*) data)->ip_info.ip));
    }

    void begin(){
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        esp_netif_create_default_wifi_ap();
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipHandler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiHandler, NULL));

        wifi_config_t config = {
            .ap = {
                .ssid = "Checker",
                .password = "",
                .ssid_len = 7,
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = 1,
            }
        };
        esp_wifi_set_config(WIFI_IF_AP, &config);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
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

        wifi_config_t apConfig = {
            .ap = {
                .ssid = "Checker",
                .password = "",
                .ssid_len = 7,
                .authmode = WIFI_AUTH_OPEN,
                .max_connection = 1,
            }
        };
        esp_wifi_set_config(WIFI_IF_AP, &apConfig);
        esp_restart();
    }

    wifi_mode_t getMode(){
        wifi_mode_t mode;
        if(esp_wifi_get_mode(&mode) == ESP_OK){
            return mode;
        }
        return WIFI_MODE_NULL;
    }
}