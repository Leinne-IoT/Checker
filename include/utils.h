#include <nvs.h>
#include <string>
#include <sstream>
#include <esp_random.h>

using namespace std;

uint32_t random(uint32_t howbig){
    if(howbig == 0){
        return 0;
    }
    uint32_t val = esp_random();
    return val % howbig;
}

uint32_t random(uint32_t howsmall, uint32_t howbig){
    if(howsmall >= howbig){
        return howsmall;
    }
    return random(howbig - howsmall) + howsmall;
}