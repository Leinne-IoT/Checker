#include <esp_sleep.h>
#include <esp32-hal-log.h>

void lightSleep(uint64_t us){
    if(esp_sleep_enable_timer_wakeup(us) != ESP_OK){
        log_e("Invalid sleep time. [value: %d]", us);
        return;
    }
    esp_light_sleep_start();
}

void deepSleep(){
    esp_deep_sleep_start();
}

void deepSleep(uint64_t us){
    if(esp_sleep_enable_timer_wakeup(us) != ESP_OK){
        log_e("Invalid sleep time. [value: %d]", us);
        return;
    }
    esp_deep_sleep_start();
}

void hibernate(){
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    
    deepSleep();
}