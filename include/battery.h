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

    static uint8_t voltToLevel(uint16_t mVolt){
        if(mVolt < 1000){ // 배터리 연결이 안되어있는 장치라고 판단
            return 15;
        }
        int16_t calculate = (mVolt - 3550) * 10 / (4150 - 3550);
        cout << "[battery] raw volt: " << mVolt << ", value: " << (calculate * 10) << "%\n";
        return (uint8_t) MIN(10, MAX(0, calculate));
    }

    void calculate(void* args){
        level = voltToLevel(analogReadMilliVolts(GPIO_NUM_1) * 2.038);

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
                sum += analogReadMilliVolts(GPIO_NUM_1) * 2.038;
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