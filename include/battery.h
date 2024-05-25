#pragma once

#include <atomic>
#include <iostream>

#include "utils.h"

#define CHECK_TICK 10 // 확인할 간격
#define CHECK_COUNT 100 // 측정할 횟수
#define CHECK_INTERVAL 5000 // 확인 후 간격

using namespace std;

namespace battery{
    atomic<uint8_t> level = 0;

    static uint8_t voltToLevel(uint16_t voltage){
        if(voltage < 3000){ // 배터리 측정 기능이 없거나 DC 전원으로 동작하는 장치
            return 15;
        }
        int16_t calculate = (voltage - 3300) * 10 / (4200 - 3300);
        cout << "[battery] raw volt: " << voltage << ", value: " << (calculate * 10) << "%\n";
        return (uint8_t) MIN(10, MAX(0, calculate));
    }

    void calculate(void* args){
        level = voltToLevel(analogReadMilliVolts(BATTERY_PIN) * 2.042);

        uint64_t sum = 0;
        uint16_t count = 0;
        uint64_t time = millis();
        for(;;){
            if(millis() - time < CHECK_TICK){
                continue;
            }
            if(count < CHECK_COUNT){
                ++count;
                time = millis();
                sum += analogReadMilliVolts(BATTERY_PIN) * 2.038;
                if(count >= CHECK_COUNT){
                    level = voltToLevel(sum / count);
                }
            }else if(millis() - time > CHECK_INTERVAL){
                time = millis();
                sum = count = 0;
            }
        }
    }
}
