#pragma once

#include "utils.h"
#include "safe_queue.h"

struct DoorState{
    uint32_t open:1;
    uint32_t updateTime:31;
};

SafeQueue<DoorState> doorStateQueue;
RTC_DATA_ATTR bool lastOpenDoor = false;

namespace door{
    DoorState state(){
        return {
            .open = gpio_get_level(SWITCH_PIN) != 0,
            .updateTime = (uint32_t) millis(),
        };
    }

    bool check(){
        auto door = state();
        if(lastOpenDoor != door.open){
            lastOpenDoor = door.open;
            doorStateQueue.push(door);
            debug(door.open ? "[Door] 문 열림 (%d개 대기중)\n" : "[Door] 문 닫힘 (%d개 대기중)\n", doorStateQueue.size() - 1);
            return true;
        }
        return false;
    }
}