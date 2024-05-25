#pragma once

#include "utils.h"
#include "safe_queue.h"

struct DoorState{
    uint32_t open:1;
    uint32_t updateTime:31;
};

namespace door{
    int64_t checkDelay = -1;
    SafeQueue<DoorState> queue;
    RTC_DATA_ATTR bool current = false;

    inline int state(){
        return gpio_get_level(SWITCH_PIN) != 0;
    }

    inline bool update(bool newValue){
        if(current == newValue){
            checkDelay = -1;
            return false;
        }

        auto time = millis();
        if(checkDelay == -1){
            checkDelay = time;
        }
        if(time - checkDelay < 800){
            return false;
        }
        current = newValue;
        queue.push({
            .open = newValue,
            .updateTime = (uint32_t) time,
        });
        std::cout << "[Door] 문 " << (newValue ? "열림" : "닫힘") << " (size: " << queue.size() << ")\n";
        return true;
    }

    inline bool update(){
        return update(state());
    }

    inline void init(esp_sleep_wakeup_cause_t cause){
        if(cause > ESP_SLEEP_WAKEUP_ALL){
            update(!current);
        }else{
            current = state();
        }
    }
}