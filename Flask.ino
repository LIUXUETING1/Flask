#include <M5Unified.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <VL53L0X.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define SERIAL_BAUD 115200

Adafruit_BMP280 bmp;  // BMP280 传感器对象
VL53L0X vl53;         // VL53 距离传感器对象

// WiFi 配置
const char* ssid = "elecom2g-789ae3";         // 替换为您的 WiFi 名称
const char* password = "4jnuckircyem";        // 替换为您的 WiFi 密码

// Flask API 配置
const char* flaskUrl = "http://192.168.2.185:5000/predict";  // 替换为你的 Flask 服务器地址

// 数据上传相关变量
unsigned long lastUploadTime = 0; // 上次数据上传时间

void setup() {
  Serial.begin(SERIAL_BAUD);
  auto cfg = M5.config();
  cfg.internal_imu = false;
  M5.begin(cfg);

  // 设置屏幕为横屏模式
  M5.Lcd.setRotation(3);  // 横屏显示

  // I2C 引脚设置
  Wire.begin(0, 26);

  // WiFi 初始化
  WiFi.begin(ssid, password);
  M5.Lcd.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("WiFi connected");

  // 初始化 BMP280
  if (!bmp.begin(0x76)) {
    M5.Lcd.println("BMP280 not found!");
    while (1);
  }
  M5.Lcd.println("BMP280 initialized.");

  // 初始化 VL53L0X
  if (!vl53.init()) {
    M5.Lcd.println("VL53L0X init failed!");
    while (1);
  }
  vl53.startContinuous();
}

void loop() {
  M5.update();
  unsigned long currentTime = millis();

  // 获取传感器数据
  float temperature = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F;  // 获取气压（单位：hPa）
  uint16_t distance = vl53.readRangeContinuousMillimeters();  // 获取距离

  // 显示传感器数据
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(50, 50);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.printf("Temp: %.2f C\n", temperature);
  M5.Lcd.printf("Pressure: %.2f hPa\n", pressure);
  M5.Lcd.printf("Distance: %d mm\n", distance);

  // 发送数据到 Flask 进行预测
  if (currentTime - lastUploadTime >= 5000) {  // 每5秒上传一次数据
    uploadToFlask(temperature, pressure, distance);
    lastUploadTime = currentTime;
  }

  delay(1000);  // 延迟 1 秒更新数据
}

// 向 Flask 发送数据并获取预测结果
void uploadToFlask(float temperature, float pressure, uint16_t distance) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return;
  }

  HTTPClient http;
  http.begin(flaskUrl);  // Flask 服务器地址
  http.addHeader("Content-Type", "application/json");

  // 创建 JSON 数据
  String jsonData = "{\"temperature\": " + String(temperature) + 
                    ", \"pressure\": " + String(pressure) + 
                    ", \"distance\": " + String(distance) + "}";

  // 发送 POST 请求
  int httpResponseCode = http.POST(jsonData);
  String response = "";

  if (httpResponseCode > 0) {
    response = http.getString();  // 获取响应内容
    Serial.println("Flask Response: " + response);

    // 解析预测结果
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      const char* prediction = doc["prediction"];
      M5.Lcd.setCursor(50, 120);
      M5.Lcd.setTextColor(TFT_GREEN);
      M5.Lcd.printf("Prediction: %s\n", prediction);  // 显示预测结果
    } else {
      M5.Lcd.setCursor(50, 120);
      M5.Lcd.setTextColor(TFT_RED);
      M5.Lcd.println("Error parsing response");
    }
  } else {
    Serial.printf("Error in HTTP request: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}