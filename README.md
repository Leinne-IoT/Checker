# Door/Window Open Checker
문 열림 감지 IoT 기기입니다.  
ESP32를 활용하여 여러분의 집을 지켜보세요!

## 준비물
1. WiFi 연결이 가능한 ESP32
2. 마그네틱 도어 센서(NO 스위치 사용)
3. 붉은색 LED
4. 부저
3. 리튬 배터리

## TODO List
* LED 점등
* WebSocket 사용
* 부저를 활용한 경고음

## FAQ
1. ESP8266으로는 불가능할까요?
불가능할것으로 보입니다.  
체커는 깊은 잠(deep sleep)을 통해 전력 소모를 최소화하는데  
ESP8266은 깊은 잠 상태에서 깨우는 방법이 매우 제한적입니다.  