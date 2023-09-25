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
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

    #if SOC_PM_SUPPORT_RTC_PERIPH_PD
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    #endif

    #if SOC_PM_SUPPORT_RTC_SLOW_MEM_PD
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    #endif

    #if SOC_PM_SUPPORT_RTC_FAST_MEM_PD
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    #endif
    esp_deep_sleep_start();
}