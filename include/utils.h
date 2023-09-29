#pragma once

#include <nvs.h>
#include <string>
#include <sstream>
#include <esp_random.h>

using namespace std;

//#define DEBUG_MODE

#ifdef DEBUG_MODE
#define debug(fmt, args...) printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))

inline uint32_t random(uint32_t max){
    if(max == 0){
        return 0;
    }
    uint32_t val = esp_random();
    return val % max;
}

inline uint32_t random(uint32_t min, uint32_t max){
    if(min >= max){
        return min;
    }
    return random(max - min) + min;
}

inline int64_t millis(){
    return esp_timer_get_time() / 1000LL;
}

inline void strReplace(string& origin, string replace, string str){
    auto index = origin.find(replace);
    if(index == string::npos){
        return;
    }
    origin.replace(index, replace.length(), str);
}

inline string urlDecode(const string& encoded){
    ostringstream decoded;
    for(size_t i = 0; i < encoded.length(); ++i){
        if(encoded[i] == '%'){
            if(i + 2 < encoded.length()){
                int decoded_char;
                char hexStr[3] = {encoded[i + 1], encoded[i + 2], '\0'};
                istringstream(hexStr) >> hex >> decoded_char;
                decoded << static_cast<char>(decoded_char);
                i += 2;
            }else{
                return "";
            }
        }else if(encoded[i] == '+'){
            decoded << ' ';
        }else{
            decoded << encoded[i];
        }
    }
    return decoded.str();
}