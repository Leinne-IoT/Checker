#include <nvs.h>
#include <WString.h>

class StorageClass{
public:
    bool begin(String name = "storage");

    bool commit();
    String getDeviceId();
    String getWiFiSSID();
    String getWiFiPassword();

    void setWiFiData(String ssid, String pw);

protected:
    String getString(String key);
    bool setString(String key, String value);

    nvs_handle_t handle;
};

extern StorageClass Storage;