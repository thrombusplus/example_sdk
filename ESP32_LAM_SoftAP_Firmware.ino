/*
 * ESP32 LAM Device Firmware with SoftAP Setup
 * Enhanced version with WiFi provisioning via SoftAP
 * Compatible with LAM SDK
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "MPU6050.h"

// ----------------------------------------------------------------------------
// Configuration Constants
// ----------------------------------------------------------------------------
const char* SETUP_AP_PASSWORD = "thrombus123";
const char* SETUP_AP_PREFIX = "Thrombus-Setup-";
const unsigned long RESET_BUTTON_HOLD_TIME = 5000; // 5 seconds
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;  // 30 seconds
const unsigned long HEARTBEAT_TIMEOUT = 10000;     // 10 seconds

// GPIO Pins
const int RESET_BUTTON_PIN = 0;  // Boot button on most ESP32 boards
const int STATUS_LED_PIN = 2;    // Built-in LED on most ESP32 boards
const int I2C_SDA = 14;
const int I2C_SCL = 15;

// LED Blink Patterns (milliseconds)
const int LED_FAST_BLINK = 200;   // Setup mode
const int LED_SLOW_BLINK = 1000;  // Connecting
const int LED_SOLID_ON = 0;       // Connected

// ----------------------------------------------------------------------------
// Global Variables
// ----------------------------------------------------------------------------
// Device State
enum DeviceMode {
  MODE_SETUP,     // SoftAP mode for configuration
  MODE_NORMAL     // Normal operation mode
};

// Device Types - Set by manufacturer during firmware compilation
const String DEVICE_TYPE = "LAM"; // Manufacturer sets this: "LAM", "LRM", or "EIM"

DeviceMode currentMode = MODE_SETUP;
bool isConfigured = false;
String deviceMAC;
String setupSSID;
unsigned int assignedPort = 5000;

// WiFi & Network
WiFiUDP udp;
WebServer webServer(80);
IPAddress remoteIP;
unsigned int remotePort = 0;

// IMU
MPU6050 imu;

// mDNS
String mDNS_hostname;
const char* mDNS_service = "_imu";

// Streaming & Status
bool streaming = false;
int samplingRate = 30;
bool enableAccel[3] = { true, true, true };
bool enableGyro[3] = { true, true, true };
bool enableMag[3] = { false, false, false };

// Timing
unsigned long lastSampleTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long lastLEDBlink = 0;
unsigned long resetButtonPressStart = 0;
bool connected = false;
bool ledState = false;
int currentLEDPattern = LED_FAST_BLINK;

// Storage
Preferences preferences;

// Button state
bool resetButtonPressed = false;
bool lastResetButtonState = HIGH;

// ----------------------------------------------------------------------------
// Function Prototypes
// ----------------------------------------------------------------------------
void initializeDevice();
void loadConfiguration();
void saveConfiguration(const String& ssid, const String& password);
void clearConfiguration();
unsigned int calculatePortFromMAC();
void enterSetupMode();
void enterNormalMode();
void setupSoftAP();
void setupWebServer();
void setupMDNS();
void handleRoot();
void handleScan();
void handleSave();
void handleNotFound();
void connectToWiFi();
void processCommand(const String& cmd);
void sendIMUData();
void sendStatus();
void handleHeartbeat();
void resumeAdvertising();
void updateLED();
void checkResetButton();
String getWiFiScanJSON();

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== ESP32 LAM Device with SoftAP Setup ===");
  
  // Initialize hardware
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  // Initialize device
  initializeDevice();
  
  // Load saved configuration
  loadConfiguration();
  
  // Calculate device-specific port
  assignedPort = calculatePortFromMAC();
  
  // Generate device identifiers
  deviceMAC = WiFi.macAddress();
  deviceMAC.replace(":", "");
  setupSSID = SETUP_AP_PREFIX + deviceMAC.substring(8); // Last 4 chars of MAC
  mDNS_hostname = "thrombus-lam-" + deviceMAC.substring(8).toLowerCase();
  
  Serial.println("Device MAC: " + deviceMAC);
  Serial.println("Setup SSID: " + setupSSID);
  Serial.println("Assigned Port: " + String(assignedPort));
  Serial.println("mDNS Hostname: " + mDNS_hostname);
  
  // Initialize IMU
  Wire.begin(I2C_SDA, I2C_SCL);
  imu.initialize();
  if (!imu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
  } else {
    Serial.println("MPU6050 connected!");
  }
  
  // Start in appropriate mode
  if (isConfigured) {
    connectToWiFi();
  } else {
    enterSetupMode();
  }
}

// ----------------------------------------------------------------------------
// Main Loop
// ----------------------------------------------------------------------------
void loop() {
  // Handle reset button
  checkResetButton();
  
  // Update LED status
  updateLED();
  
  if (currentMode == MODE_SETUP) {
    // Handle web server requests in setup mode
    webServer.handleClient();
    
  } else if (currentMode == MODE_NORMAL) {
    // Normal operation mode
    
    // Check for incoming UDP packets
    int packetSize = udp.parsePacket();
    if (packetSize) {
      remoteIP = udp.remoteIP();
      remotePort = udp.remotePort();
      
      char packetBuffer[128];
      int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
      packetBuffer[len] = 0;
      String cmd = String(packetBuffer);
      
      Serial.println("Received Command: " + cmd);
      
      if (cmd == "ping") {
        handleHeartbeat();
      } else {
        processCommand(cmd);
      }
    }
    
    // Send IMU data if streaming
    unsigned long currentTime = millis();
    unsigned long interval = 1000UL / samplingRate;
    if (streaming && (currentTime - lastSampleTime >= interval)) {
      lastSampleTime = currentTime;
      sendIMUData();
    }
    
    // Check heartbeat timeout
    if (connected && (millis() - lastHeartbeatTime > HEARTBEAT_TIMEOUT)) {
      Serial.println("Heartbeat timeout. Disconnected.");
      connected = false;
      currentLEDPattern = LED_SLOW_BLINK;
      resumeAdvertising();
    }
  }
}

// ----------------------------------------------------------------------------
// Initialization Functions
// ----------------------------------------------------------------------------
void initializeDevice() {
  preferences.begin("lam-config", false);
  
  // Set WiFi mode
  WiFi.mode(WIFI_AP_STA);
  
  Serial.println("Device initialized");
}

void loadConfiguration() {
  String savedSSID = preferences.getString("wifi_ssid", "");
  String savedPassword = preferences.getString("wifi_password", "");
  
  if (savedSSID.length() > 0 && savedPassword.length() > 0) {
    isConfigured = true;
    Serial.println("Found saved WiFi configuration for: " + savedSSID);
    Serial.println("Device type: " + DEVICE_TYPE);
  } else {
    isConfigured = false;
    Serial.println("No WiFi configuration found");
  }
}

void saveConfiguration(const String& ssid, const String& password) {
  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_password", password);
  isConfigured = true;
  Serial.println("WiFi configuration saved: " + ssid);
}

void clearConfiguration() {
  preferences.clear();
  isConfigured = false;
  Serial.println("Configuration cleared");
}

unsigned int calculatePortFromMAC() {
  String mac = WiFi.macAddress();
  // Get last 2 hex digits of MAC address
  String lastBytes = mac.substring(15, 17);
  uint8_t suffix = strtol(lastBytes.c_str(), NULL, 16);
  // Distribute across ports 5000-5004 for up to 5 devices
  return 5000 + (suffix % 5);
}

// ----------------------------------------------------------------------------
// Mode Management
// ----------------------------------------------------------------------------
void enterSetupMode() {
  currentMode = MODE_SETUP;
  currentLEDPattern = LED_FAST_BLINK;
  
  Serial.println("Entering Setup Mode");
  
  // Stop any existing connections
  udp.stop();
  WiFi.disconnect();
  
  // Setup SoftAP
  setupSoftAP();
  
  // Setup web server
  setupWebServer();
  
  Serial.println("Setup mode active. Connect to: " + setupSSID);
  Serial.println("Password: " + String(SETUP_AP_PASSWORD));
  Serial.println("Configure at: http://192.168.4.1");
}

void enterNormalMode() {
  currentMode = MODE_NORMAL;
  currentLEDPattern = LED_SLOW_BLINK;
  
  Serial.println("Entering Normal Mode");
  
  // Stop web server and SoftAP
  webServer.stop();
  WiFi.softAPdisconnect(true);
  
  // Connect to WiFi
  connectToWiFi();
}

void setupSoftAP() {
  WiFi.softAP(setupSSID.c_str(), SETUP_AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("SoftAP IP address: " + IP.toString());
}

void setupWebServer() {
  webServer.on("/", handleRoot);
  webServer.on("/scan", handleScan);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.onNotFound(handleNotFound);
  
  webServer.begin();
  Serial.println("Web server started");
}

void connectToWiFi() {
  String ssid = preferences.getString("wifi_ssid", "");
  String password = preferences.getString("wifi_password", "");
  
  if (ssid.length() == 0) {
    Serial.println("No WiFi credentials found");
    enterSetupMode();
    return;
  }
  
  Serial.println("Connecting to WiFi: " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.println("IP address: " + WiFi.localIP().toString());
    
    currentLEDPattern = LED_SOLID_ON;
    
    // Setup mDNS
    setupMDNS();
    
    // Start UDP listener
    udp.begin(assignedPort);
    Serial.println("UDP listening on port " + String(assignedPort));
    
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("Entering setup mode...");
    enterSetupMode();
  }
}

void setupMDNS() {
  if (!MDNS.begin(mDNS_hostname.c_str())) {
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started. Hostname: " + mDNS_hostname + ".local");
    MDNS.addService(mDNS_service, "udp", assignedPort);
  }
}

// ----------------------------------------------------------------------------
// Web Server Handlers
// ----------------------------------------------------------------------------
void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Thrombus LAM Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #333; text-align: center; }
        .device-info { background: #e8f4f8; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input, select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
        button { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 16px; }
        button:hover { background: #0056b3; }
        .scan-btn { background: #28a745; margin-bottom: 10px; }
        .scan-btn:hover { background: #1e7e34; }
        #networks { margin-top: 10px; }
        .network-item { padding: 8px; border: 1px solid #ddd; margin-bottom: 5px; cursor: pointer; border-radius: 4px; }
        .network-item:hover { background: #f8f9fa; }
        .loading { text-align: center; color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔧 Thrombus LAM Setup</h1>
        
        <div class="device-info">
            <strong>Device Information:</strong><br>
            MAC Address: )" + deviceMAC + R"(<br>
            Device Type: )" + DEVICE_TYPE + R"(<br>
            Assigned Port: )" + String(assignedPort) + R"(<br>
            Hostname: )" + mDNS_hostname + R"(.local
        </div>
        
        <form action="/save" method="post">
            <div class="form-group">
                <label for="ssid">WiFi Network:</label>
                <input type="text" id="ssid" name="ssid" required placeholder="Enter WiFi network name">
                <button type="button" class="scan-btn" onclick="scanNetworks()">🔍 Scan Networks</button>
                <div id="networks"></div>
            </div>
            
            <div class="form-group">
                <label for="password">WiFi Password:</label>
                <input type="password" id="password" name="password" required placeholder="Enter WiFi password">
            </div>
            
            <button type="submit">💾 Save & Connect</button>
        </form>
    </div>

    <script>
        function scanNetworks() {
            document.getElementById('networks').innerHTML = '<div class="loading">Scanning for networks...</div>';
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    let html = '';
                    data.networks.forEach(network => {
                        html += `<div class="network-item" onclick="selectNetwork('${network.ssid}')">${network.ssid} (${network.rssi} dBm) ${network.encryption ? '🔒' : '🔓'}</div>`;
                    });
                    document.getElementById('networks').innerHTML = html;
                })
                .catch(error => {
                    document.getElementById('networks').innerHTML = '<div style="color: red;">Error scanning networks</div>';
                });
        }
        
        function selectNetwork(ssid) {
            document.getElementById('ssid').value = ssid;
            document.getElementById('networks').innerHTML = '';
        }
        
        // Auto-scan on page load
        window.onload = function() {
            scanNetworks();
        };
    </script>
</body>
</html>
)";
  
  webServer.send(200, "text/html", html);
}

void handleScan() {
  String json = getWiFiScanJSON();
  webServer.send(200, "application/json", json);
}

void handleSave() {
  String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  
  if (ssid.length() > 0) {
    saveConfiguration(ssid, password);
    
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Configuration Saved</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .success { color: #28a745; font-size: 24px; margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="success">✅ Configuration Saved!</div>
        <p>The device will now restart and connect to your WiFi network.</p>
        <p>You can close this page and use the LAM SDK to discover the device.</p>
    </div>
    <script>
        setTimeout(function() {
            window.close();
        }, 5000);
    </script>
</body>
</html>
)";
    
    webServer.send(200, "text/html", html);
    
    // Restart device after a short delay
    delay(2000);
    ESP.restart();
  } else {
    webServer.send(400, "text/plain", "Invalid SSID");
  }
}

void handleNotFound() {
  webServer.send(404, "text/plain", "Not found");
}

String getWiFiScanJSON() {
  int n = WiFi.scanNetworks();
  String json = "{\"networks\":[";
  
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  
  json += "]}";
  return json;
}

// ----------------------------------------------------------------------------
// Hardware Control
// ----------------------------------------------------------------------------
void updateLED() {
  unsigned long currentTime = millis();
  
  if (currentLEDPattern == LED_SOLID_ON) {
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else if (currentTime - lastLEDBlink >= currentLEDPattern) {
    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState);
    lastLEDBlink = currentTime;
  }
}

void checkResetButton() {
  bool currentButtonState = digitalRead(RESET_BUTTON_PIN);
  
  // Button pressed (LOW due to pullup)
  if (currentButtonState == LOW && lastResetButtonState == HIGH) {
    resetButtonPressStart = millis();
    resetButtonPressed = true;
  }
  
  // Button released
  if (currentButtonState == HIGH && lastResetButtonState == LOW) {
    resetButtonPressed = false;
  }
  
  // Check for long press
  if (resetButtonPressed && (millis() - resetButtonPressStart >= RESET_BUTTON_HOLD_TIME)) {
    Serial.println("Reset button held for 5+ seconds - clearing configuration");
    clearConfiguration();
    resetButtonPressed = false;
    
    // Enter setup mode
    enterSetupMode();
  }
  
  lastResetButtonState = currentButtonState;
}

// ----------------------------------------------------------------------------
// Communication Functions
// ----------------------------------------------------------------------------
void resumeAdvertising() {
  Serial.println("Resuming mDNS advertising...");
  MDNS.addService(mDNS_service, "udp", assignedPort);
}

void handleHeartbeat() {
  lastHeartbeatTime = millis();
  if (!connected) {
    Serial.println("Connected to C# app via heartbeat.");
    connected = true;
    currentLEDPattern = LED_SOLID_ON;
    // Stop advertising to reduce network traffic
    mdns_service_remove(mDNS_service, "_udp");
  }
}

void processCommand(const String& cmd) {
  if (cmd == "initialize") {
    Serial.println("Initialized IMU module");
    
  } else if (cmd.startsWith("setSamplingRate:")) {
    String rateStr = cmd.substring(strlen("setSamplingRate:"));
    samplingRate = rateStr.toInt();
    Serial.println("Set sampling rate to " + String(samplingRate) + " Hz");
    
  } else if (cmd == "startStreaming") {
    streaming = true;
    Serial.println("IMU streaming started");
    
  } else if (cmd == "stopStreaming") {
    streaming = false;
    Serial.println("IMU streaming stopped");
    
  } else if (cmd == "getStatus") {
    sendStatus();
    
  } else {
    Serial.println("Unknown command: " + cmd);
  }
}

void sendIMUData() {
  int16_t ax, ay, az, gx, gy, gz;
  imu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // Convert raw to floats
  float accelX = float(ax) / 16384.0f;
  float accelY = float(ay) / 16384.0f;
  float accelZ = float(az) / 16384.0f;
  float gyroX = float(gx) / 131.0f;
  float gyroY = float(gy) / 131.0f;
  float gyroZ = float(gz) / 131.0f;
  
  // Magnetometer placeholders
  float magX = 0.0f;
  float magY = 0.0f;
  float magZ = 0.0f;
  
  // Timestamp in ms (converted to float)
  float timestampMs = (float)millis();
  
  // Build the binary payload: 10 floats => 40 bytes
  float data[10];
  data[0] = enableAccel[0] ? accelX : 0.0f;
  data[1] = enableAccel[1] ? accelY : 0.0f;
  data[2] = enableAccel[2] ? accelZ : 0.0f;
  data[3] = enableGyro[0] ? gyroX : 0.0f;
  data[4] = enableGyro[1] ? gyroY : 0.0f;
  data[5] = enableGyro[2] ? gyroZ : 0.0f;
  data[6] = enableMag[0] ? magX : 0.0f;
  data[7] = enableMag[1] ? magY : 0.0f;
  data[8] = enableMag[2] ? magZ : 0.0f;
  data[9] = timestampMs;
  
  // Send over UDP
  udp.beginPacket(remoteIP, remotePort);
  udp.write((uint8_t*)data, sizeof(data));
  udp.endPacket();
}

void sendStatus() {
  // Time since last heartbeat
  unsigned long timeSinceHeartbeat = millis() - lastHeartbeatTime;
  
  String ipStr = WiFi.localIP().toString();
  String remoteIPStr = connected ? remoteIP.toString() : "";
  
  String statusMsg = "{";
  statusMsg += "\"ip\":\"" + ipStr + "\",";
  statusMsg += "\"remoteIP\":\"" + remoteIPStr + "\",";
  statusMsg += "\"port\":" + String(assignedPort) + ",";
  statusMsg += "\"streaming\":" + String((streaming ? "true" : "false")) + ",";
  statusMsg += "\"samplingRate\":" + String(samplingRate) + ",";
  statusMsg += "\"lastConnection\":" + String(timeSinceHeartbeat) + ",";
  statusMsg += "\"connected\":" + String((connected ? "true" : "false")) + ",";
  statusMsg += "\"mode\":\"" + String((currentMode == MODE_SETUP ? "setup" : "normal")) + "\",";
  statusMsg += "\"mac\":\"" + deviceMAC + "\",";
  statusMsg += "\"deviceType\":\"" + DEVICE_TYPE + "\"";
  statusMsg += "}";
  
  // Send over UDP
  udp.beginPacket(remoteIP, remotePort);
  udp.print(statusMsg);
  udp.endPacket();
  
  Serial.println("Sent status: " + statusMsg);
}
