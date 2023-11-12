#pragma once

#include <time.h>
#include <string>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_random.h>
#include <driver/rtc_io.h>

using namespace std;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

inline uint32_t random_int(uint32_t max){
    if(max == 0){
        return 0;
    }
    uint32_t val = esp_random();
    return val % max;
}

inline uint32_t random_int(uint32_t min, uint32_t max){
    if(min >= max){
        return min;
    }
    return random_int(max - min) + min;
}

inline int64_t getCurrentMillis(){
    timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000LL + now.tv_usec / 1000LL;
}

inline int64_t getCurrentMicros(){
    timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000LL + now.tv_usec;
}

inline void strReplace(string& origin, string replace, string str){
    auto index = origin.find(replace);
    if(index == string::npos){
        return;
    }
    origin.replace(index, replace.length(), str);
}

inline void lightSleep(gpio_num_t pin, int level, uint64_t time = 0){
    rtc_gpio_pullup_en(pin);
    esp_sleep_enable_ext0_wakeup(pin, level);
    if(time > 0){
        esp_sleep_enable_timer_wakeup(time);
    }
    esp_light_sleep_start();
}

inline void deepSleep(gpio_num_t pin, int level, uint64_t time = 0){
    rtc_gpio_pullup_en(pin);
    esp_sleep_enable_ext0_wakeup(pin, level);
    if(time > 0){
        esp_sleep_enable_timer_wakeup(time);
    }
    esp_deep_sleep_start();
}