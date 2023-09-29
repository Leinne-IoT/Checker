#pragma once

#include <nvs.h>
#include <atomic>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include "utils.h"

using namespace std;

static atomic<bool> startAP = false;
static atomic<bool> connectWifi = false;

void wifiHandler(void* arg, esp_event_base_t base, int32_t id, void* data){
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

void ipHandler(void* arg, esp_event_base_t basename, int32_t id, void* data){
    connectWifi = true;
    debug("[WiFi] 아이피: " IPSTR "\n", IP2STR(&((ip_event_got_ip_t*) data)->ip_info.ip));
}

void clearWiFiData(){
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

wifi_mode_t getWiFiStatus(){
    wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode) == ESP_OK){
        return mode;
    }
    return WIFI_MODE_NULL;
}