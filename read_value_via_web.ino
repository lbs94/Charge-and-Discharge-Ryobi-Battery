#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Cấu hình cửa ESP32
#define VOLTAGE_PIN 34
#define CURRENT_PIN 36
#define TEMP_PIN 32
#define BUTTON_PIN 27
#define RELAY1_PIN 25
#define RELAY2_PIN 26
float voltage1 = 0;
float current1 = 0;
float temperature1 = 0;
unsigned long relayTimer = 0;
int retryCount = 0;
const int maxRetries = 5;
bool relayConditionMet = false;
int Cycle = 0;
// Dòng điện (ACS712)
const float ACS712_SENSITIVITY = 0.1; // 100mV/A cho ACS712 20A
const float NOISE_THRESHOLD = 0.1;   // Ngưỡng nhiễu
static float zeroPoint = -1;
// Số lần đọc trung bình
const int NUM_SAMPLES = 10;
// Nhiệt độ (NTC 10k)
const float BETA_COEFFICIENT = 3435;  // Beta coefficient của NTC
const float R_SERIES = 9950;         // Điện trở mắc nối tiếp
const float T0_25 = 298.15;              // Nhiệt độ 25°C tính bằng Kelvin (25 + 273.15)
const float R0 = 10000;               // Điện trở tại 25°C
float VOLTAGE_DIVIDER_RATIO = (4633.0 + 2190.0) / 2190.0; //(R1+R2)/R1, Tỷ lệ mạch chia điện áp (4k7k:2k2)
const float VREF = 3.3;         // Điện áp tham chiếu ADC (ESP32)
const int ADC_RESOLUTION = 4095; // Độ phân giải ADC
WebServer server(80); // WebServer chạy trên cổng 80

bool systemEnabled = false;
bool relayState1 = false;
bool relayState2 = false;
// Thông tin WiFi mặc định
const char* defaultSSID = "TTi Guest";
const char* defaultPassword = "ttiSHTP@2023";
void setupPins() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);
}

// Hàm đọc ADC và lọc trung bình
float readFilteredADC(int pin) {
  const int NUM_READINGS = 20;  // Tăng số lần đọc
    float readings[NUM_READINGS];
    float sum = 0;
    float avg = 0;
    float stdDev = 0;
    
    // Đọc các giá trị
    for(int i = 0; i < NUM_READINGS; i++) {
        readings[i] = analogRead(pin);
        sum += readings[i];
        delay(5);  // Delay lớn hơn để ổn định
    }
    
    // Tính trung bình
    avg = sum / NUM_READINGS;
    
    // Tính độ lệch chuẩn
    for(int i = 0; i < NUM_READINGS; i++) {
        stdDev += pow(readings[i] - avg, 2);
    }
    stdDev = sqrt(stdDev / NUM_READINGS);
    
    // Loại bỏ các giá trị nằm ngoài 2 độ lệch chuẩn
    sum = 0;
    int count = 0;
    for(int i = 0; i < NUM_READINGS; i++) {
        if(abs(readings[i] - avg) <= 2 * stdDev) {
            sum += readings[i];
            count++;
        }
    }
    
    // Trả về trung bình của các giá trị hợp lệ
    return (count > 0) ? (sum / count) : avg;
}

// Hàm đọc điện áp
float readVoltage() {
  float adcValue = readFilteredADC(VOLTAGE_PIN);
  float voltage = (adcValue / ADC_RESOLUTION) * VREF * VOLTAGE_DIVIDER_RATIO;
  // Lọc nhiễu cho giá trị nhỏ
    if(abs(voltage) < NOISE_THRESHOLD) {
        voltage = 0.0;
    }
  return voltage; // Đơn vị: V
}
// Hàm calibrate điểm 0 cho ACS712
float calibrateACS712() {
  delay(100);
    float sum1 = 0;
    const int numReadings = 50;
    
    // Đọc nhiều lần để lấy giá trị trung bình
    for(int i = 0; i < numReadings; i++) {
        sum1 += analogRead(CURRENT_PIN);
        delay(10);
    }
    
    float avgADC = sum1 / numReadings;
          avgADC  =(avgADC / ADC_RESOLUTION) * VREF;
    return avgADC*2 ; // mạch chia áp,nhân lại để đúng giá trị
}
// Hàm đọc dòng điện
float readCurrent() {
             
     // Đọc và lọc nhiễu ADC
    float adcValue = readFilteredADC(CURRENT_PIN);
    float voltage = (adcValue / ADC_RESOLUTION) * VREF;
    
    // Tính toán dòng điện với zero point đã calibrate
    float current = (voltage - zeroPoint) / ACS712_SENSITIVITY;
    
    // Lọc nhiễu cho dòng điện nhỏ
    if(abs(current) < NOISE_THRESHOLD) {
        current = 0.0;
    }
    
    return current;
}

// Hàm đọc nhiệt độ
float readTemperature() {
  float adcValue = readFilteredADC(TEMP_PIN);
  float voltage = (adcValue / ADC_RESOLUTION) * VREF;

  // Tính điện trở của NTC
  float resistance = (R_SERIES * (VREF - voltage)) / voltage;

  // Áp dụng phương trình Steinhart-Hart
  float tempKelvin = 1 / ((log(resistance / R0) / BETA_COEFFICIENT) + (1 / T0_25));
  float tempCelsius = tempKelvin - 273.15; // Chuyển sang °C
  return tempCelsius; // Đơn vị: °C
}
void handleRoot() {
    String html = "<html><meta charset='UTF-8'><title>Read Values Via Web</title><body>";
    html += "<h1>Data Monitoring</h1>";
    html += "<p>Voltage: <span id='voltage'>--</span> V</p>";
    html += "<p>Current: <span id='current'>--</span> A</p>";
    html += "<p>Temperature: <span id='temperature'>--</span> &#8451;</p>";
    html += "<p>Cycle: <span id='cycle'>--</span></p>";
    html += "<p>Volt Current sensor calib: <span id='voltCurrentSensorCalib'>--</span> V</p>";
    html += "<p>Relay1 State: <span id='relay1'>--</span></p>";
    html += "<p>Relay2 State: <span id='relay2'>--</span></p>";
    html += "<p>Button State: <span id='button'>--</span></p>";
    html += "<button onclick='toggleButton()'>Toggle Button</button>";
    html += "<script>";
    html += "function toggleButton() {";
    html += "  fetch('/toggleButton', { method: 'POST' })";
    html += "    .then(() => updateData());";  // Add callback to update data after toggle
    html += "}";
    html += "function updateData() {";
    html += "  fetch('/data')";
    html += "    .then(response => response.json())";
    html += "    .then(data => {";
    html += "      document.getElementById('voltage').innerText = data.voltage;";
    html += "      document.getElementById('current').innerText = data.current;";
    html += "      document.getElementById('temperature').innerText = data.temperature;";
    html += "      document.getElementById('cycle').innerText = data.cycle;";
    html += "      document.getElementById('voltCurrentSensorCalib').innerText = data.voltCurrentSensorCalib;";
    html += "      document.getElementById('relay1').innerText = data.relay1 ? 'ON' : 'OFF';";
    html += "      document.getElementById('relay2').innerText = data.relay2 ? 'ON' : 'OFF';";
    html += "      document.getElementById('button').innerText = data.button ? 'Pressed' : 'Released';";
    html += "    });";
    html += "}";
    html += "updateData();"; // Initial update
    html += "setInterval(updateData, 1000);";  // Regular updates
    html += "</script>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    
}
void handleData() {
    String json = "{";
    json += "\"voltage\":" + String(readVoltage(), 2) + ",";
    json += "\"current\":" + String(readCurrent(), 2) + ",";
    json += "\"temperature\":" + String(readTemperature(), 2) + ",";  
    json += "\"cycle\":" + String(Cycle, 2) + ",";  
    json += "\"voltCurrentSensorCalib\":" + String(zeroPoint, 2) + ",";  
    json += "\"relay1\":" + String(digitalRead(RELAY1_PIN)) + ",";
    json += "\"relay2\":" + String(digitalRead(RELAY2_PIN)) + ",";
    json += "\"button\":" + String(!digitalRead(BUTTON_PIN));
    json += "}";
    server.send(200, "application/json", json);
}

void handleToggleSystem() {
    systemEnabled = !systemEnabled;
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);
    server.sendHeader("Location", "/");
    server.send(303);
}
void handleToggleButton() {
    systemEnabled = !systemEnabled;  // Toggle the system state instead of the button pin
    if (!systemEnabled) {
        digitalWrite(RELAY1_PIN, LOW);
        digitalWrite(RELAY2_PIN, LOW);
    }
    server.send(200, "text/plain", "System " + String(systemEnabled ? "Enabled" : "Disabled"));
}
void connectToWiFi() {
    WiFi.begin(defaultSSID,defaultPassword); // wifi mac dinh
    Serial.print("Connecting to WiFi");
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected!");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect. Starting AP mode.");
        WiFi.softAP("ESP32-AP");
        Serial.println(WiFi.softAPIP());
    }
}

void setup() {
    Serial.begin(115200);

    // Thiết lập chân
    setupPins();

    // Kết nối WiFi
    connectToWiFi();

    // Thiết lập WebServer
    server.on("/", handleRoot);
    server.on("/toggleSystem", HTTP_POST, handleToggleSystem);
    server.on("/data", handleData);
    server.on("/toggleButton", HTTP_POST, handleToggleButton);
    server.begin();
    zeroPoint = calibrateACS712();
}

void loop() {
    server.handleClient();

    // Xử lý nút nhấn
    static bool lastButtonState = HIGH;
    bool currentButtonState = digitalRead(BUTTON_PIN);

    if (lastButtonState == HIGH && currentButtonState == LOW) {
        systemEnabled = !systemEnabled;
        digitalWrite(RELAY1_PIN, LOW);
        digitalWrite(RELAY2_PIN, LOW);
        Serial.println(systemEnabled ? "System Enabled" : "System Disabled");
    }
    lastButtonState = currentButtonState;

    // Điều khiển relay theo điều kiện
    if (systemEnabled) {
        voltage1 = readVoltage();
        current1 = readCurrent();
        temperature1 = readTemperature();
        if (temperature1 < 45 && voltage1 < 41.5) {
            digitalWrite(RELAY1_PIN, HIGH);
            if (current1 > 0) {
                relayConditionMet = true;
                relayTimer = millis();
                retryCount = 0;
            } else {
                if (relayConditionMet && millis() - relayTimer >= 20000) {
                    relayConditionMet = false;
                    retryCount++;
                    if (retryCount >= maxRetries) {
                        Serial.println("Error: Failed to maintain current. Check system.");
                        digitalWrite(RELAY1_PIN, LOW);
                    } else {
                        relayTimer = millis(); // Reset timer to retry
                    }
                }
            }
      } else if (temperature1 < 33 && voltage1 >= 41.5) {
            if (current1 <= 0.1) {
                if (!relayConditionMet) {
                    relayTimer = millis();
                    relayConditionMet = true;
                }
                if (millis() - relayTimer >= 20000) {
                    digitalWrite(RELAY2_PIN, HIGH);
                    Cycle++;
                }
            } else {
                relayConditionMet = false;
                digitalWrite(RELAY2_PIN, LOW);
            }
        } else {
            digitalWrite(RELAY1_PIN, LOW);
            digitalWrite(RELAY2_PIN, LOW);
            relayConditionMet = false;
            retryCount = 0;
        }
    }
}
