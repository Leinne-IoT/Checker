#include <string>
#include <esp_http_server.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

using namespace std;

const char* indexHtml =
R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, minimum-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Checker WiFi Settings</title>
    <style>
        tr > td > input{
            padding: 0;
            width: 100%;
            line-height: 24px;
            //font-size: 16px;
            border: 1px solid black;
        }
        table{
            width: 92%;
            margin: auto;
            border: 1px solid black;
            border-collapse: collapse;
        }
        td{
            padding: 10px;
            border: 1px solid black;
        }
    </style>
</head>
<body>
    <h1><center>Checker WiFi Settings</center></h1>
    <form method="POST" action="save">
        <table>
            <tr>
                <td width="24%">SSID</td>
                <td>${ssid}</td>
            </tr>
            <tr>
                <td>Password</td>
                <td><input type="password" required minlength="8" maxlength="100" name="password"></td>
            </tr>
            <tr>
                <td colspan='2'><center><input style="width: 50%; font-weight: bold" type="submit" value="Save Data" ${disabled}></center></td>
            </tr>
        </table>
    </form>
</body>
</html>
)rawliteral";

const char* saveHtml =
R"(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, minimum-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Checker WiFi Settings</title>
</head>
<body>
    <center>
        <h1>Checker WiFi Settings</h1>
        <div>Checker WiFi AP settings have been successful.</div>
        <div>You will be able to play the game soon.</div>
    </center>
</body>
</html>
)";

/*unordered_map<string, string> parseParameter(char* data){
    unordered_map<string, string> result;
    istringstream iss(data);
    string token;
    while(getline(iss, token, '&')){
        size_t equalPos = token.find('=');
        if(equalPos != string::npos){
            result[token.substr(0, equalPos)] = token.substr(equalPos + 1);
        }
    }
    return result;
}*/

string getIndexPageInputSSID(){
    string index = indexHtml;
    index.replace(index.find("${ssid}"), 7, "<input type='text' required maxlength='100' name='ssid'>");
    index.replace(index.find("${disabled}"), 11, "");
    return index;
}
   
string getIndexPageListSSID(string ssidList, bool disabled){
    string index = indexHtml;
    index.replace(index.find("${ssid}"), 7, ssidList);
    index.replace(index.find("${disabled}"), 11, disabled ? "disabled" : "");
    return index;
}

string getSavePage(){
    string save = saveHtml;
    return save;
}

esp_err_t indexPage(httpd_req_t* req){
    httpd_resp_send(req, getIndexPageInputSSID().c_str(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t savePage(httpd_req_t* req){
    httpd_resp_send(req, saveHtml, HTTPD_RESP_USE_STRLEN);

    char content[req->content_len + 1] = {0};
    int ret = httpd_req_recv(req, content, req->content_len);
    
    if(req->content_len > 0){
        //auto data = parseParameter(content);
        printf("[Web]: %s\n", content);
    }

    if(ret <= 0){
        if(ret == HTTPD_SOCK_ERR_TIMEOUT){
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t startWebServer(httpd_handle_t* server){
    printf("[Web] Start Server\n");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = httpd_start(server, &config);
    if(err == ESP_OK){
        httpd_uri_t index = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = indexPage,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index);

        httpd_uri_t saveUri = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = savePage,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &saveUri);
    }
    return err;
}

void stopWebServer(httpd_handle_t* server){
    printf("[Web] Stop Server\n");
    if(*server != NULL){
        httpd_stop(*server);
        *server = NULL;
    }
}