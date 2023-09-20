#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <driver/rtc_io.h>

#include "web.h"
#include "sleep.h"
#include "queue.h"
#include "storage.h"

#define SWITCH GPIO_NUM_25

bool lastOpenDoor = true;
int64_t lastUpdateTime = 0;
int64_t wifiTryConnectTime = 0;
wl_status_t beginWiFi = WL_NO_SHIELD;
EventGroupHandle_t s_wifi_event_group;

WebServer httpServer(80);

TaskHandle_t networkTask;

struct DoorState{
    bool open;
    int64_t updateTime;
};
SafeQueue<DoorState> doorStateQueue;

void sendNetworkLoop(void* args);

void changeWiFiMode(bool isApSta){
    //Serial.printf("Change WiFi Mode: %s\n", isApSta ? "AP" : "Station");

    if(isApSta){
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("Checker");
    }else{
        auto ssid = Storage.getWiFiSSID();
        if(ssid.length() < 1){
            changeWiFiMode(true);
            return;
        }

        auto password = Storage.getWiFiPassword();
        if(password.length() < 1){
            changeWiFiMode(true);
            return;
        }

        wifiTryConnectTime = -1;
        WiFi.mode(WIFI_STA);
        if(beginWiFi == WL_NO_SHIELD){
            beginWiFi = WiFi.begin(ssid, password);
        }
    }
}

void indexPage(){
    char* html = (char*) String(indexHtml).c_str();
    bool listMode = false;
    if(listMode){ //
        //String ssidList = "<select name='ssid' style='width: 100%'><option>fold2</option><option>2F_WiFi_2GHz</option><option>RM404</option>";
        String ssidList = "<select name='ssid' style='width: 100%'>";
        auto length = WiFi.scanNetworks();
        for(uint8_t i = 0; i < length; ++i){
            ssidList += "<option>" + WiFi.SSID(i) + "</option>";
        }
        httpServer.send(200, "text/html", getIndexPageListSSID(ssidList + "</select>", length == 0));
    }else{
        httpServer.send(200, "text/html", getIndexPageInputSSID());
    }
}

void savePage(){
    httpServer.send(200, "text/html", getSavePage());
    Storage.setWiFiData(httpServer.arg("ssid"), httpServer.arg("password"));
    beginWiFi = WL_NO_SHIELD;
    changeWiFiMode(false);
}

void setup(){
    Storage.begin();
    xEventGroupClearBits(s_wifi_event_group, BIT0);

    /*Serial.begin(115200);
    while(!Serial){
        delay(10);
    }*/

    httpServer.on("/", indexPage);
    httpServer.on("/save", savePage);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(SWITCH, INPUT_PULLUP);
    digitalWrite(LED_BUILTIN, LOW);
    lastOpenDoor = digitalRead(SWITCH) == LOW;
    if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0){
        DoorState state = {
            .open = lastOpenDoor,
            .updateTime = 0,
        };
        doorStateQueue.push(state);
    }

    changeWiFiMode(false);
    httpServer.begin();

    xTaskCreatePinnedToCore(
        sendNetworkLoop,
        "network",
        10000,
        NULL,
        1,
        &networkTask,
        0
    );
}

esp_err_t wifi_handler(void *ctx, system_event_t *event){
    switch(event->event_id){
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(s_wifi_event_group, BIT0);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits(s_wifi_event_group, BIT0);
            break;		
    }
    return ESP_OK;
}

void sendNetworkLoop(void* args){
    esp_event_loop_init(wifi_handler, NULL);
    for(;;){
        DynamicJsonDocument json(2048);
        json["id"] = Storage.getDeviceId();
        JsonArray array = json.createNestedArray("data");

        do{
            auto data = doorStateQueue.pop();
            JsonObject object = array.createNestedObject();
            object["open"] = data.open;
            object["time"] = data.updateTime;
        }while(!doorStateQueue.empty());
        
        HTTPClient http;
        http.begin("http://leinne.net:33877/req_door");
        http.addHeader("Content-Type", "application/json");

        json["current"] = esp_timer_get_time() / 1000LL;
        
        String body;
        serializeJson(json, body);
        while(http.POST(body) != HTTP_CODE_OK){
            delay(100);

            body = "";
            json["current"] = esp_timer_get_time() / 1000LL;
            serializeJson(json, body);
        }
    }
}

void loop(){
    bool openDoor = digitalRead(SWITCH) == LOW;
    if(lastOpenDoor != openDoor){
        DoorState state = {
            .open = openDoor,
            .updateTime = esp_timer_get_time() / 1000LL,
        };
        doorStateQueue.push(state);
        //Serial.printf(openDoor ? "[%d] 문 열림\n" : "[%d] 문 닫힘\n", state.updateTime);

        lastOpenDoor = state.open;
        lastUpdateTime = esp_timer_get_time();
    }

    if(WiFi.status() != WL_CONNECTED){
        if(WiFi.getMode() == WIFI_AP_STA){
            httpServer.handleClient();
        }else{
            auto now = millis();
            if(wifiTryConnectTime == 0){
                wifiTryConnectTime = now;
            }
            if(now - wifiTryConnectTime >= 10 * 1000){
                changeWiFiMode(true);
            }
        }
        return;
    }

    if(WiFi.getMode() == WIFI_AP_STA){
        changeWiFiMode(false);
    }

    if(esp_timer_get_time() - lastUpdateTime > 15 * 1000000){ // 업데이트가 발생한지 15초가 지난 경우
        //Serial.println("deep sleep start");

        rtc_gpio_pullup_en(SWITCH);
        esp_sleep_enable_ext0_wakeup(SWITCH, !digitalRead(SWITCH));
        esp_deep_sleep_start();
    }
}