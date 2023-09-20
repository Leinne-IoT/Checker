#include <WiFi.h>
#include <WString.h>

const char* indexHtml =
R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, minimum-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Checker WiFi Settings</title>
    <script type="text/javascript">
        function sendData(event){
            const ip = document.getElementsByName("ip")[0].value.split(".");
            for(let i = 0; i < 4; ++i){
                if(ip[i] > 255 || ip[i] < 0){
                    alert("Invalid IP address.\nIP address must consist only of numbers between 0 and 255.");
                    event.preventDefault();
                    break;
                }
            }
        }
    </script>
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
    <form method="post" action="save" onsubmit="sendData(event)">
        <table>
            <tr>
                <td width="25%">SSID</td>
                <td>${ssid}<!--input type="text" required maxlength="100" name="ssid"--></td>
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

String getIndexPageInputSSID(){
    String index = String(indexHtml);
    index.replace("${ssid}", "<input type='text' required maxlength='100' name='ssid'>");
    index.replace("${disabled}", "");
    return index;
}
   
String getIndexPageListSSID(String ssidList, bool disabled){
    String index = String(indexHtml);
    index.replace("${ssid}", ssidList);
    index.replace("${disabled}", disabled ? "disabled" : "");
    return index;
}

String getSavePage(){
    String save = String(saveHtml);
    return save;
}