#pragma once

#include <atomic>
#include <driver/gpio.h>
#include <esp_http_client.h>
#include <esp_websocket_client.h>

#include "door.h"
#include "utils.h"
#include "storage.h"

typedef enum{
    CONTINUITY,
    STRING,
    BINARY,
    QUIT = 0x08,
    PING,
    PONG
} websocket_opcode_t;

namespace ws{
    atomic<bool> sendPhase = false;
    atomic<bool> connectServer = false;
    esp_websocket_client_handle_t webSocket = NULL;

    void sendWelcome(){
        auto device = storage::getDeviceId();
        uint8_t buffer[device.length() + 3] = {0x01, 0x01, getBatteryLevel()}; //{device type, data type, battery level}
        for(uint8_t i = 0; i < device.length(); ++i){
            buffer[3 + i] = device[i];
        }
        esp_websocket_client_send_with_opcode(webSocket, WS_TRANSPORT_OPCODES_BINARY, buffer, device.length() + 3, portMAX_DELAY);
        std::cout << "[WS] Send welcome message\n";
    }

    void sendDoorState(DoorState state){
        sendPhase = true;
        uint8_t buffer[7] = {
            0x01, // device type
            0x02, // data type
            (uint8_t) ((state.open << 4) | (getBatteryLevel() & 0b1111))
        };
        buffer[2] |= state.open << 4;
        uint8_t count = 0;
        int64_t delay = 0;
        do{
            if(millis() - delay < 1000){
                continue;
            }

            if(++count > 3){
                return;
            }
            delay = millis();
            int64_t current = millis() - state.updateTime;
            for(uint8_t byte = 0; byte < 4; ++byte){
                buffer[3 + byte] = (current >> 8 * (3 - byte)) & 0b11111111;
            }
            esp_websocket_client_send_with_opcode(webSocket, WS_TRANSPORT_OPCODES_BINARY, buffer, 7, portMAX_DELAY);
        }while(sendPhase);

        if(count <= 3){
            std::cout << "[WS] Send successful. (state: " << (state.open ? "open" : "close") << ")\n";
        }else{
            std::cout << "[WS] Send failed.\n";
        }
    }

    bool isConnected(){
        return esp_websocket_client_is_connected(webSocket);
    }

    static void eventHandler(void* object, esp_event_base_t base, int32_t eventId, void* eventData){
        esp_websocket_event_data_t* data = (esp_websocket_event_data_t*) eventData;
        if(eventId == WEBSOCKET_EVENT_CONNECTED){
            sendWelcome();
        }else if(eventId == WEBSOCKET_EVENT_DISCONNECTED || eventId == WEBSOCKET_EVENT_ERROR){
            if(connectServer){
                std::cout << "[WS] Disconnected WebSocket\n";
            }
            connectServer = false;
        }else if(eventId == WEBSOCKET_EVENT_DATA){
            if(data->op_code == STRING && !connectServer){
                string device(data->data_ptr, data->data_len);
                if(storage::getDeviceId() == device){
                    connectServer = true;
                    std::cout << "[WS] Connect successful.\n";
                }else{
                    std::cout << "[WS] FAILED. device: " << storage::getDeviceId() << ", receive: " << device << ", len: " << data->data_len << "\n";
                }
            }else if(data->op_code == BINARY){
                sendPhase = false;
            }
        }
    }

    void start(esp_event_handler_t handler){
        auto url = storage::getString("websocket_url");
        if(url.find_first_of("ws") == string::npos){
            storage::setString("websocket_url", url = DEFAULT_WEBSOCKET_URL);
        }

        esp_websocket_client_config_t websocket_cfg = {
            .uri = url.c_str(),
            .reconnect_timeout_ms = 250,
        };

        while(webSocket == NULL){
            webSocket = esp_websocket_client_init(&websocket_cfg);
        }
        esp_websocket_register_events(webSocket, WEBSOCKET_EVENT_ANY, handler, NULL);
        esp_websocket_register_events(webSocket, WEBSOCKET_EVENT_ANY, eventHandler, NULL);

        esp_err_t err = ESP_FAIL;
        while(err != ESP_OK){
            err = esp_websocket_client_start(webSocket);
        }
    }
}