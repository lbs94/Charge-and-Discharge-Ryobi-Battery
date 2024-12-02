#include <AsyncTCP.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// Pin configurations
#define VOLTAGE_PIN 34
#define CURRENT_PIN 36
#define TEMP_PIN 32
#define BUTTON_PIN 27
#define RELAY1_PIN 25 // for charge
#define RELAY2_PIN 26 // for discharge

// Global variables
float voltage1 = 0;
float current1 = 0;
float temperature1 = 0;
unsigned long relayTimer = 0;
int retryCount = 0;
const int maxRetries = 5;
bool relayConditionMet = false;
int Cycle = 0;

// Current sensor constants
const float ACS712_SENSITIVITY = 0.1;
const float NOISE_THRESHOLD = 0.1;
static float zeroPoint = -1;
const int NUM_SAMPLES = 10;

// Temperature sensor constants
const float BETA_COEFFICIENT = 3435;
const float R_SERIES = 9950;
const float T0_25 = 298.15;
const float R0 = 10000;

float VOLTAGE_DIVIDER_RATIO = (4633.0 + 2190.0) / 2190.0;
const float VREF = 3.3;
const int ADC_RESOLUTION = 4095;

// System state
bool systemEnabled = false;
bool relayState1 = false;
bool relayState2 = false;
static bool lastButtonState = HIGH;

// Logging
const int MAX_LOG_MESSAGES = 10;
String logMessages[MAX_LOG_MESSAGES];
int logIndex = 0;

// WiFi credentials
const char* defaultSSID = "TTi Guest";
const char* defaultPassword = "ttiSHTP@2023";

// Server instances
AsyncWebServer server(80);
WebSocketsServer webSocket(81);  // WebSocket server on port 81

void setupPins() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    digitalWrite(RELAY1_PIN, LOW);
    digitalWrite(RELAY2_PIN, LOW);
}

void addLogMessage(String message) {
    if (logIndex >= MAX_LOG_MESSAGES) {
        logIndex = 0;
    }
    logMessages[logIndex] = message;
    logIndex++;
    Serial.println(message);
    
    // Send log update via WebSocket
    String json = "{\"type\":\"log\",\"message\":\"" + message + "\"}";
    webSocket.broadcastTXT(json);
}

float readFilteredADC(int pin) {
    const int NUM_READINGS = 20;
    float readings[NUM_READINGS];
    float sum = 0;
    float avg = 0;
    float stdDev = 0;
    
    for(int i = 0; i < NUM_READINGS; i++) {
        readings[i] = analogRead(pin);
        sum += readings[i];
        delay(5);
    }
    
    avg = sum / NUM_READINGS;
    
    for(int i = 0; i < NUM_READINGS; i++) {
        stdDev += pow(readings[i] - avg, 2);
    }
    stdDev = sqrt(stdDev / NUM_READINGS);
    
    sum = 0;
    int count = 0;
    for(int i = 0; i < NUM_READINGS; i++) {
        if(abs(readings[i] - avg) <= 2 * stdDev) {
            sum += readings[i];
            count++;
        }
    }
    
    return (count > 0) ? (sum / count) : avg;
}

float readVoltage() {
    float adcValue = readFilteredADC(VOLTAGE_PIN);
    float voltage = (adcValue / ADC_RESOLUTION) * VREF * VOLTAGE_DIVIDER_RATIO;
    if(abs(voltage) < NOISE_THRESHOLD) {
        voltage = 0.0;
    }
    return voltage;
}

float calibrateACS712() {
    delay(100);
    float sum1 = 0;
    const int numReadings = 50;
    
    for(int i = 0; i < numReadings; i++) {
        sum1 += analogRead(CURRENT_PIN);
        delay(10);
    }
    
    float avgADC = sum1 / numReadings;
    avgADC = (avgADC / ADC_RESOLUTION) * VREF;
    return avgADC * 2;
}

float readCurrent() {
    float adcValue = readFilteredADC(CURRENT_PIN);
    float voltage = (adcValue / ADC_RESOLUTION) * VREF;
    float current = (voltage - zeroPoint) / ACS712_SENSITIVITY;
    
    if(abs(current) < NOISE_THRESHOLD) {
        current = 0.0;
    }
    
    return current;
}

float readTemperature() {
    float adcValue = readFilteredADC(TEMP_PIN);
    float voltage = (adcValue / ADC_RESOLUTION) * VREF;
    float resistance = (R_SERIES * (VREF - voltage)) / voltage;
    float tempKelvin = 1 / ((log(resistance / R0) / BETA_COEFFICIENT) + (1 / T0_25));
    return tempKelvin - 273.15;
}

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                Serial.printf("[%u] Connected!\n", num);
                // Send initial state
                sendSensorData();
            }
            break;
        case WStype_TEXT:
            {
                String message = String((char*)payload);
                if(message == "toggleSystem") {
                    systemEnabled = !systemEnabled;
                    digitalWrite(RELAY1_PIN, LOW);
                    digitalWrite(RELAY2_PIN, LOW);
                    addLogMessage(systemEnabled ? "System Enabled" : "System Disabled");
                }
            }
            break;
    }
}

void sendSensorData() {
    String json = "{";
    json += "\"type\":\"sensorData\",";
    json += "\"voltage\":" + String(readVoltage(), 2) + ",";
    json += "\"current\":" + String(readCurrent(), 2) + ",";
    json += "\"temperature\":" + String(readTemperature(), 2) + ",";
    json += "\"cycle\":" + String(Cycle) + ",";
    json += "\"voltCurrentSensorCalib\":" + String(zeroPoint, 2) + ",";
    json += "\"relay1\":" + String(digitalRead(RELAY1_PIN)) + ",";
    json += "\"relay2\":" + String(digitalRead(RELAY2_PIN)) + ",";
    json += "\"button\":" + String(!digitalRead(BUTTON_PIN)) + ",";
    json += "\"systemEnabled\":" + String(systemEnabled);
    json += "}";
    
    webSocket.broadcastTXT(json);
}

void connectToWiFi() {
    WiFi.begin(defaultSSID, defaultPassword);
    Serial.print("Connecting to WiFi");
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected!");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect. Starting AP mode:192.168.4.1");
        WiFi.softAP("ESP32-AP");
        Serial.println(WiFi.softAPIP());
    }
}
// Define HTML content as a separate const char array
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>ESP32 Monitoring</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #4ee625; padding: 10px; margin-bottom: 20px; }
        .data-container { margin-bottom: 20px; }
        .log-container { background-color: #f0f0f0; padding: 10px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Data Monitoring</h1>
    </div>
    <div class="data-container">
        <p>Voltage (GPIO34): <span id="voltage">--</span> V</p>
        <p>Current (GPIO36): <span id="current">--</span> A</p>
        <p>Temperature (GPIO32): <span id="temperature">--</span> ℃</p>
        <p>Cycle: <span id="cycle">--</span></p>
        <p>Volt Current sensor calib: <span id="voltCurrentSensorCalib">--</span> V</p>
        <p>Relay1 (GPIO25) State: <span id="relay1">--</span></p>
        <p>Relay2 (GPIO26) State: <span id="relay2">--</span></p>
        <p>Button (GPIO27) State: <span id="button">--</span></p>
        <button onclick="toggleSystem()">Toggle System</button>
    </div>
    <div class="log-container">
        <h3>System Messages</h3>
        <div id="logMessages"></div>
    </div>
    <script>
        let ws;
        function connect() {
            ws = new WebSocket('ws://' + window.location.hostname + ':81');
            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                if(data.type === 'sensorData') {
                    document.getElementById('voltage').textContent = data.voltage;
                    document.getElementById('current').textContent = data.current;
                    document.getElementById('temperature').textContent = data.temperature;
                    document.getElementById('cycle').textContent = data.cycle;
                    document.getElementById('voltCurrentSensorCalib').textContent = data.voltCurrentSensorCalib;
                    document.getElementById('relay1').textContent = data.relay1 ? 'ON' : 'OFF';
                    document.getElementById('relay2').textContent = data.relay2 ? 'ON' : 'OFF';
                    document.getElementById('button').textContent = data.button ? 'Pressed' : 'Released';
                } else if(data.type === 'log') {
                    const logDiv = document.getElementById('logMessages');
                    logDiv.innerHTML = data.message + '<br>' + logDiv.innerHTML;
                }
            };
            ws.onclose = function() {
                setTimeout(connect, 1000);
            };
        }
        function toggleSystem() {
            ws.send('toggleSystem');
        }
        connect();
    </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    setupPins();
    connectToWiFi();
    
    // Setup WebSocket server
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });
    server.begin();
    zeroPoint = calibrateACS712();
}

void loop() {
    webSocket.loop();
    
    // Handle button
    bool currentButtonState = digitalRead(BUTTON_PIN);
    if (lastButtonState == HIGH && currentButtonState == LOW) {
        systemEnabled = !systemEnabled;
        digitalWrite(RELAY1_PIN, LOW);
        digitalWrite(RELAY2_PIN, LOW);
        addLogMessage(systemEnabled ? "System Enabled" : "System Disabled");
    }
    lastButtonState = currentButtonState;

    // Control logic
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
                        addLogMessage("Error: Failed to maintain current. Check system.");
                        digitalWrite(RELAY1_PIN, LOW);
                    } else {
                        relayTimer = millis();
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
                    addLogMessage("New cycle started. Cycle count: " + String(Cycle));
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
    
    // Send sensor data every second
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 1000) {
        sendSensorData();
        lastUpdate = millis();
    }
}