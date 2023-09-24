#include <esp_sleep.h>

void lightSleep(int64_t us = -1){
    if(us > 0 && esp_sleep_enable_timer_wakeup(us) != ESP_OK){
        printf("Invalid sleep time. [value: %d]\n", us);
    }
    esp_light_sleep_start();
}

void deepSleep(int64_t us = -1){
    if(us > 0 && esp_sleep_enable_timer_wakeup(us) != ESP_OK){
        printf("Invalid sleep time. [value: %d]\n", us);
    }
    esp_deep_sleep_start();
}

void hibernate(){
    #ifdef ESP_PD_DOMAIN_XTAL
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
    #endif
    
    #ifdef ESP_PD_DOMAIN_RTC_PERIPH
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    #endif

    #ifdef ESP_PD_DOMAIN_RTC_SLOW_MEM
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    #endif

    #ifdef ESP_PD_DOMAIN_RTC_FAST_MEM
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    #endif
    deepSleep();
}