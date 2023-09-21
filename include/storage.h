#include <nvs.h>
#include <string>

using namespace std;

class StorageClass{
private:
    nvs_handle_t handle;

public:
    bool begin(string name = "storage");
    string getDeviceId();
    string getString(string key);
    bool setString(string key, string value);
};

extern StorageClass Storage;