#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "DHT.h"  // DHT11使用
#include <Wire.h>
#include <BH1750.h>
#include <soc/rtc.h>
#define DHTPIN 15 // DHT11設定為GPIO 15
#define DHTTYPE DHT11
BH1750 lightMeter; // 建立GY30光線感測實體

#define MoisturePIN 39 // Moisture設定為GPIO 39
int sensorValue;       // 土壤濕度感測使用

char SSID[] = "Your SSID";                                        // 網路名稱
char PASSWORD[] = "YourPassword";                              // 網路密碼


String url = "http://api.thingspeak.com/update?api_key=YourAPIKey"; // 改成自己的Write API key

WiFiClientSecure clientSecure; // 網路連線物件
DHT dht(DHTPIN, DHTTYPE);

WebServer server(80);
char Num;
int relay = 13;

IPAddress staticIP(192, 168, 0, 113);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress DNS(1, 1, 1, 1);

Preferences prefs;

String HTML;

int watermode, water_moisture_condition, water_time_h1, water_time_m1, water_time_h2, water_time_m2, water_time_h3, water_time_m3;
bool watertime1, watertime2, watertime3;

struct tm timeinfo;

void updateHTML()
{
  HTML = R"(<!doctype html>
<html>
  <head>
    <meta charset="utf-8" />
    <title>智慧盆栽</title>

  </head>
  <body>
    <h1>智慧盆栽</h1>
    <form action="/applySettings" method="get">
    <label> 澆水模式</label>
    <input type="radio" name="watermode" id="watermode0" value="0" />關閉
    <input type="radio" name="watermode" id="watermode1" value="1" />定時
    <input type="radio" name="watermode" id="watermode2" value="2" />溼度
    <br />
    <label for="water_moisture_condition"> 自動澆水溼度：</label>
    <input
      type="text"
      id="water_moisture_condition"
      name="water_moisture_condition"
      value="0"
    />
    <br />
    <label for="water_moisture_condition"> 自動澆水時間：</label>
    <input type="checkbox" id="watertime1" name="watertime1" value=""/>
    <select name="water1_hours" id = "water1_hours">

    </select>
    <select name="water1_minutes" id = "water1_minutes">

    </select>
	<input type="checkbox" name="watertime2" value="1"/>
    <select name="water2_hours" id = "water2_hours">

    </select>
    <select name="water2_minutes" id = "water2_minutes">
    </select>
	<input type="checkbox" id="watertime3" name="watertime3" value="1"/>
    <select name="water3_hours" id = "water3_hours">

    </select>
    <select name="water3_minutes" id = "water3_minutes">
    </select>
    <br />
    <input type="submit" value="儲存" />
    </form>
    <script>
    document.addEventListener("DOMContentLoaded", function() {
            for(let i= 1;i<=3;i++){

                const hoursSelect = document.getElementById('water'+i.toString()+'_hours');
                const minutesSelect = document.getElementById('water'+i.toString()+'_minutes');
                for (let ii = 0; ii <= 23; ii++) {
                    const option = document.createElement('option');
                    option.value = ii;
                    option.textContent = ii.toString().padStart(2, '0');
                    hoursSelect.appendChild(option);
                }
                for (let ii = 0; ii <= 59; ii += 5) {
                    const option = document.createElement('option');
                    option.value = ii;
                    option.textContent = ii.toString().padStart(2, '0');
                    minutesSelect.appendChild(option);
                }
            })";

  HTML += '\n';
  HTML += R"(document.getElementById("watermode)" + String(watermode) + R"(").checked = true;)" + "\n";
  HTML += R"(document.getElementById("water_moisture_condition").value = ")" + String(water_moisture_condition) + R"(";)";
  HTML += (watertime1) ? R"(document.getElementById("watertime1").checked = true;)" : "";
  HTML += (watertime2) ? R"(document.getElementById("watertime2").checked = true;)" : "";
  HTML += (watertime3) ? R"(document.getElementById("watertime3").checked = true;)" : "";
  HTML += R"(document.getElementById("water1_hours").value = ")" + String(water_time_h1) + R"(";)";
  HTML += R"(document.getElementById("water1_minutes").value = ")" + String(water_time_m1) + R"(";)";
  HTML += R"(document.getElementById("water2_hours").value = ")" + String(water_time_h2) + R"(";)";
  HTML += R"(document.getElementById("water2_minutes").value = ")" + String(water_time_m2) + R"(";)";
  HTML += R"(document.getElementById("water3_hours").value = ")" + String(water_time_h3) + R"(";)";
  HTML += R"(document.getElementById("water3_minutes").value = ")" + String(water_time_m3) + R"(";)";
  HTML += R"(})
    </script>
  </body>
</html>
)";
}

void update_variables()
{
  watermode = prefs.getInt("watermode", 0);
  water_moisture_condition = prefs.getInt("water_moisture_condition", 100);
  watertime1 = prefs.getBool("watertime1", 0);
  watertime2 = prefs.getBool("watertime2", 0);
  watertime3 = prefs.getBool("watertime3", 0);
  water_time_h1 = prefs.getInt("water_time_h1", 0);
  water_time_m1 = prefs.getInt("water_time_m1", 0);
  water_time_h2 = prefs.getInt("water_time_h2", 0);
  water_time_m2 = prefs.getInt("water_time_m2", 0);
  water_time_h3 = prefs.getInt("water_time_h3", 0);
  water_time_m3 = prefs.getInt("water_time_m3", 0);
}

void setup()
{
  Serial.begin(115200); // 設定鮑率
  prefs.begin("settings");
  updateHTML();
  Serial.print("Connecting Wifi: "); // 連線到指定的WiFi SSID
  Serial.println(SSID);

  if (WiFi.config(staticIP, gateway, subnet,DNS) == false)
  {
    Serial.println("Configuration failed.");
  }


  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println(""); // 連線成功，顯示取得的IP
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
  clientSecure.setInsecure();

  server.on("/", []()
            { server.send(200, "text/html", HTML); });

  server.on("/applySettings", HTTP_GET, []()
            {
    prefs.putInt("watermode",server.arg("watermode").toInt());
    prefs.putInt("water_moisture_condition",server.arg("water_moisture_condition").toInt());    
    if(server.hasArg("watertime1")){
        watertime1 = server.arg("watertime1").toInt();
    }
    if(server.hasArg("watertime2")){
        watertime2 = server.arg("watertime2").toInt();
    }
    if(server.hasArg("watertime3")){
        watertime3 = server.arg("watertime3").toInt();
    }
    for(short i = 1;i<=3;i++){
      prefs.putInt(("water_time_h"+String(i)).c_str(),server.arg("water"+String(i)+"_hours").toInt());
      prefs.putInt(("water_time_m"+String(i)).c_str(),server.arg("water"+String(i)+"_minutes").toInt());
    }
    update_variables(); 
    updateHTML(); 
    server.send(200,"text/html",R"(<html><body><h1>Finish</h1><a href="/">Go Back</a></body></html>)"); });

    
  server.begin();
  Serial.println("HTTP server started");

  configTime(8 * 3600, 0, "time.stdtime.gov.tw");

  dht.begin();                 // 初始化DHT
  pinMode(DHTPIN, INPUT);      // 設定DHTPIN為輸入
  pinMode(MoisturePIN, INPUT); // 設定MoisturePIN為輸入
  pinMode(relay, OUTPUT);      // 設定relay為繼電器輸出
  Wire.begin();                // 初始化 I2C
  lightMeter.begin();
  Serial.println(F("BH1750 Test")); // 存進flash memory 不占用記憶體空間
}

void loop()
{
  server.handleClient();

  getLocalTime(&timeinfo);

  float t = dht.readTemperature(); // 讀取溫度
  float h = dht.readHumidity();    // 讀取濕度

  Serial.print("t=");
  Serial.print(t);
  Serial.print(" h=");
  Serial.println(h);

  float lux = lightMeter.readLightLevel(); // 讀取光照度
  Serial.print("Light: ");
  Serial.print(lux); // Lux是用於照度的SI單位，等於每平方米一個流明
  Serial.println(" lx");

  sensorValue = analogRead(MoisturePIN); // 讀取土壤濕度
  Serial.print("Moisture value:");
  Serial.println(sensorValue);

  if(watermode==2){
    if(watertime1&&water_time_h1==timeinfo.tm_hour&&water_time_m1==timeinfo.tm_min){
      digitalWrite(13, HIGH);
      delay(1000);
      digitalWrite(13, LOW);
      delay(60000);
    }
    if(watertime2&&water_time_h2==timeinfo.tm_hour&&water_time_m2==timeinfo.tm_min){
      digitalWrite(13, HIGH);
      delay(1000);
      digitalWrite(13, LOW);
      delay(60000);
    }
    if(watertime3&&water_time_h3==timeinfo.tm_hour&&water_time_m3==timeinfo.tm_min){
      digitalWrite(13, HIGH);
      delay(1000);
      digitalWrite(13, LOW);
      delay(60000);
    }
  }
  if(watermode==3&&sensorValue<water_moisture_condition){
    digitalWrite(13, HIGH);
    delay(1000);
    digitalWrite(13, LOW);
  }


  delay(1500);
  // thingSpeak
  HTTPClient http;
  String url1 = url + "&field1=" + t + "&field2=" + h + "&field3=" + lux + "&field4=" + sensorValue;
  http.begin(url1);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    String pload = http.getString();
  }
  else
  {
    Serial.println("傳送失敗"); // 讀取失敗
  }
  http.end();
}
