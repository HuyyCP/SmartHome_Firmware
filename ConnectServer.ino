#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>


ESP8266WebServer server(80);
const char* ssid = "ESP8266-Config";
const char* password = "12345678";

struct Device {
  String pin;
  bool isConnected;
  int status;
  String id;
};
Device devices[4];

bool isConnectedWifi = true;
bool isSetup = false;
const String SERVER_BACKEND = "http://4dea-2402-800-629c-e161-c970-d2e3-1024-797d.ngrok-free.app";
const String ESP_ID = "56d5c7f7-6b23-4f2d-9e98-ebea3ff643fa";
bool hasWifi = true;
const int TIME_OUT = 1000 * 15;

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  WiFi.mode(WIFI_AP_STA);
  String storedSSID, storedPass;
  if (!readWiFiConfig(storedSSID, storedPass)) {
    hasWifi = false;
  }

  if (hasWifi)
  {
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
    Serial.println("Connecting to WiFi...");
    int timeBreak = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (timeBreak >= TIME_OUT){
        isConnectedWifi = false;
        Serial.println("Not found Wifi....");
        break;
      }
      // Serial.println(storedSSID.length());
      Serial.print(".");
      // Serial.println(storedPass.length());
      delay(500);
      timeBreak += 500;
    }

    if (isConnectedWifi){
      Serial.println("Connected wifi");
    }
  }

  if (!hasWifi && !isConnectedWifi) {
    // Nếu chưa có, phát AP để cấu hình
    WiFi.softAP(ssid, password);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    setupWebServer();
  }
}

void loop() {
  if (!isConnectedWifi) {
    server.handleClient();
  }
  
  if (isConnectedWifi && !isSetup) {
    Serial.println("SetupID");
    if (setupID())
    {
      isSetup = true;
    }
  }
  checkDevices();
}

void checkDevices() {
  for (int i = 0; i < 4; i++)
  {
    Serial.println(devices[i].pin);
    Serial.println(devices[i].id);
    Serial.println(devices[i].status);
  }
  delay(30000);
}

void setupWebServer() {
  // Trang cấu hình WiFi
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", "<h1>Setup WiFi</h1><form action='/setup' method='post'>SSID: <input type='text' name='ssid'><br>Password: <input type='password' name='password'><br><input type='submit' value='Connect'></form>");
  });

  // Nhận thông tin WiFi và lưu vào EEPROM
  server.on("/setup", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    saveWiFiConfig(ssid, password);
    server.send(200, "text/plain", "WiFi credentials saved. Rebooting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

void saveWiFiConfig(String ssid, String password) {
  // Lưu SSID
  for (int i = 0; i < ssid.length(); ++i) {
    EEPROM.write(i, ssid[i]);
  }
  EEPROM.write(ssid.length(), '\0'); // Kết thúc chuỗi

  // Lưu Password
  for (int i = 0; i < password.length(); ++i) {
    EEPROM.write(32 + i, password[i]); // Bắt đầu từ vị trí 32
  }
  EEPROM.write(32 + password.length(), '\0'); // Kết thúc chuỗi

  EEPROM.commit();
}

bool readWiFiConfig(String &ssid, String &password) {
  char ch;
  bool validData = false; // Biến kiểm tra liệu có dữ liệu hợp lệ

  // Đọc SSID
  for (int i = 0; i < 32; ++i) {
    ch = EEPROM.read(i);
    if (ch == '\0') break; // Kết thúc chuỗi khi gặp ký tự null
    if (ch != 0xFF) validData = true; // Kiểm tra liệu có ký tự hợp lệ
    ssid += ch;
  }

  // Chỉ tiếp tục đọc Password nếu đã tìm thấy SSID hợp lệ
  if (validData) {
    validData = false; // Reset lại cho việc kiểm tra mật khẩu
    // Đọc Password
    for (int i = 32; i < 64; ++i) {
      ch = EEPROM.read(i);
      if (ch == '\0') break; // Kết thúc chuỗi khi gặp ký tự null
      if (ch != 0xFF) validData = true; // Kiểm tra liệu có ký tự hợp lệ
      password += ch;
    }
  }

  // Chỉ trả về true nếu cả SSID và Password đều hợp lệ
  return validData && ssid.length() > 0 && password.length() > 0;
}

bool setupID() {
  if (WiFi.status() == WL_CONNECTED) { // Kiểm tra kết nối WiFi
    WiFiClient client;
    HTTPClient http;
    
    // Tạo URL cho yêu cầu GET
    String url = SERVER_BACKEND + "/esp/connect/" + ESP_ID + "?numDevices=4";
    Serial.println(url);

    // Khởi tạo yêu cầu HTTP GET
    http.begin(client, url.c_str()); // Sử dụng URL đã tạo
    
    // Gửi yêu cầu GET
    int httpCode = http.GET();

    // Kiểm tra phản hồi
    if (httpCode > 0) {
      String payload = http.getString(); // Lấy nội dung phản hồi
      Serial.println("Received response:");
      Serial.println(payload);
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      JsonArray devicesArray = doc["devices"];
      for (int i = 0; i < devicesArray.size(); i++) {
        JsonObject obj = devicesArray[i];
        devices[i].pin = obj["pin"].as<String>();
        devices[i].isConnected = obj["isConnected"].as<bool>();
        devices[i].status = obj["status"].as<int>();
        devices[i].id = obj["_id"].as<String>();
      }

    } else {
      Serial.print("Error on sending GET: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }

    http.end(); // Đóng kết nối
    return true; // Trả về true nếu yêu cầu thành công
  } else {
    Serial.println("Not connected to WiFi");
    return false; // Trả về false nếu không kết nối được WiFi
  }
}