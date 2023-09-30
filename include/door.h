#pragma once

#include "utils.h"
#include "safe_queue.h"

struct DoorState{
    bool open;
    int64_t updateTime;
};

RTC_DATA_ATTR bool lastOpenDoor = false;
static SafeQueue<DoorState> doorStateQueue;

DoorState getDoorState(){
    return {
        .open = gpio_get_level(SWITCH_PIN) != 0,
        .updateTime = millis(),
    };
}

void checkDoorState(int64_t* lastUpdateTime){
    auto state = getDoorState();
    if(lastOpenDoor != state.open){
        doorStateQueue.push(state);
        debug(state.open ? "[%lld] 문 열림\n" : "[%lld] 문 닫힘\n", state.updateTime);

        lastOpenDoor = state.open;
        *lastUpdateTime = state.updateTime;
    }
}