#pragma once

#include "utils.h"
#include "safe_queue.h"

struct DoorState{
    uint32_t open:1;
    uint32_t updateTime:31;
};

namespace door{
    SafeQueue<DoorState> queue;

    int64_t lastUpdate = 0;
    RTC_DATA_ATTR bool lastState = false;

    inline int state(){
        return gpio_get_level(SWITCH_PIN) != 0;
    }

    inline bool update(bool open){
        auto current = millis();
        if(lastState == open || current - lastUpdate < 1000){
            return false;
        }
        lastState = open;
        lastUpdate = current;
        queue.push({
            .open = open,
            .updateTime = (uint32_t) current,
        });
        std::cout << "[Door] 문 " << (open ? "열림" : "닫힘") << " (size: " << queue.size() << ")\n";
        return true;
    }

    inline bool update(){
        return update(state());
    }

    inline void init(esp_sleep_wakeup_cause_t cause){
        if(cause > ESP_SLEEP_WAKEUP_ALL){
            update(!lastState);
        }else{
            lastState = state();
        }
    }
}