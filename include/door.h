#pragma once

#include "utils.h"
#include "safe_queue.h"

struct DoorState{
    bool open;
    int64_t updateTime;
};

SafeQueue<DoorState> doorStateQueue;
RTC_DATA_ATTR bool lastOpenDoor = false;

namespace door{
    DoorState get(){
        return {
            .open = gpio_get_level(SWITCH_PIN) != 0,
            .updateTime = millis(),
        };
    }

    void check(int64_t* lastUpdateTime){
        auto state = get();
        if(lastOpenDoor != state.open){
            doorStateQueue.push(state);
            debug(state.open ? "[Door] 문 열림 (%d개 대기중)\n" : "[Door] 문 닫힘 (%d개 대기중)\n", doorStateQueue.size() - 1);

            lastOpenDoor = state.open;
            *lastUpdateTime = state.updateTime;
        }
    }
}