#pragma once

#include "utils.h"
#include "safe_queue.h"

struct DoorState{
    uint32_t open:1;
    uint32_t updateTime:31;
};

namespace door{
    SafeQueue<DoorState> queue;
    RTC_DATA_ATTR bool lastState = false;

    inline int state(){
        return gpio_get_level(SWITCH_PIN) != 0;
    }

    inline void init(){
        lastState = state();
    }

    inline bool check(bool open){
        if(lastState == open){
            return false;
        }
        lastState = open;
        queue.push({
            .open = open,
            .updateTime = (uint32_t) millis(),
        });
        debug(open ? "[Door] 문 열림 (queue size: %d)\n" : "[Door] 문 닫힘 (queue size: %d)\n", queue.size());
        return true;
    }

    inline bool check(){
        return check(state());
    }
}