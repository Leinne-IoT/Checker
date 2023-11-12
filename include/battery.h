#pragma once

#include <Arduino.h>
#include <atomic>
#include <iostream>

#include "utils.h"

#define CHECK_COUNT 40
#define CHECK_INTERVAL 5000

using namespace std;

namespace battery{
    atomic<uint8_t> level = 0;

    static uint8_t voltToLevel(uint16_t mVolt){
        uint8_t calculate = (mVolt - 3550) * 10 / (4150 - 3550);
        cout << "[battery] " << (calculate * 10) << "%\n";
        return MIN(10, MAX(0, calculate));
    }

    void calculate(void* args){
        level = voltToLevel(analogReadMilliVolts(GPIO_NUM_1) * 2.038);

        uint64_t sum = 0;
        uint16_t count = 0;
        uint64_t time = millis();
        for(;;){
            if(millis() - time < 5){
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