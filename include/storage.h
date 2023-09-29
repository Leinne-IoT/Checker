#pragma once

#include <nvs.h>
#include <string>

using namespace std;

class StorageClass{
private:
    nvs_handle_t handle;

public:
    bool begin(string name = "storage");

    string getString(string key);
    string getString(string key, size_t length);

    bool setString(string key, string value, bool saveLength = true);
};