/*
 * ESP32 LAM Device Firmware - Ultra Minimal Version
 * Fixed GPIO pins, no hardware detection loops
 * Designed for ESP32-S3 with built-in LED and button
 * 
 * LED Status Indicators:
 * - Startup: 3 quick flashes (hardware test)
 * - Setup Mode: Fast blinking (500ms intervals)  
 * - WiFi Connecting: Slow blinking (1500ms intervals)
 * - Connected: Solid ON
 * - Reset Success: 5 rapid flashes
 * 
 * API Endpoints:
 * POST /configure - Save WiFi credentials (JSON body: {"ssid":"name", "password":"pass"})
 * 
 * Button Functions:
 * - 5 second hold: Clear WiFi config and enter setup mode
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ----------------------------------------------------------------------------
// Configuration Constants
// ----------------------------------------------------------------------------
const char* SETUP_AP_PASSWORD = "thrombus123";
const char* SETUP_AP_PREFIX = "Thrombus-Setup-";
const unsigned long RESET_BUTTON_HOLD_TIME = 5000; // 5 seconds
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;  // 30 seconds
const unsigned long HEARTBEAT_TIMEOUT = 10000;     // 10 seconds

// Fixed GPIO Pins for ESP32-S3 built-in components
const int ACTIVE_RESET_BUTTON_PIN = 0;  // Built-in boot button
const int ACTIVE_STATUS_LED_PIN = 48;   // Built-in RGB LED (try 48 first, fallback to 2)

// LED Blink Patterns (milliseconds) - Slower for stability
const int LED_FAST_BLINK = 500;        // Setup mode
const int LED_SLOW_BLINK = 1500;       // WiFi connecting
const int LED_SOLID_ON = 0;            // Connected
const int LED_OFF = -1;                // LED off

// ----------------------------------------------------------------------------
// Global Variables
// ----------------------------------------------------------------------------
enum DeviceMode {
  MODE_SETUP,     // SoftAP mode for configuration
  MODE_NORMAL     // Normal operation mode
};

const String DEVICE_TYPE = "LAM";

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

// mDNS
String mDNS_hostname;
const char* mDNS_service = "_imu";

// Streaming & Status
bool streaming = false;
int samplingRate = 30;

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
void initializeHardware();
void startupLEDSequence();
void loadConfiguration();
void saveConfiguration(const String& ssid, const String& password);
void clearConfiguration();
String getDeviceMAC();
unsigned int calculatePortFromMAC();
void enterSetupMode();
void setupSoftAP();
void setupWebServer();
void setupMDNS();
void handleSave();
void handleDeviceInfo();
void handleNotFound();
void connectToWiFi();
void processCommand(const String& cmd);
void sendDeviceData();
void sendStatus();
void handleHeartbeat();
void resumeAdvertising();
void updateLED();
void checkResetButton();

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32-S3 LAM Device Starting...");
  Serial.flush();
  
  // Initialize hardware with fixed pins - NO TESTING LOOPS
  Serial.println("Initializing hardware...");
  Serial.flush();
  initializeHardware();
  
  Serial.println("Hardware initialized");
  Serial.flush();
  
  // Simple LED test
  Serial.println("LED test...");
  Serial.flush();
  startupLEDSequence();
  
  Serial.println("LED test complete");
  Serial.flush();
  
  // Initialize preferences
  Serial.println("Initializing preferences...");
  Serial.flush();
  preferences.begin("lam-config", false);
  
  Serial.println("Getting device identifiers...");
  Serial.flush();
  
  // Get device identifiers with proper ESP32-S3 initialization
  deviceMAC = getDeviceMAC();
  
  deviceMAC.replace(":", "");
  setupSSID = SETUP_AP_PREFIX + deviceMAC.substring(8);
  String macSuffix = deviceMAC.substring(8);
  macSuffix.toLowerCase();
  mDNS_hostname = "thrombus-lam";
  // assignedPort = calculatePortFromMAC();
  
  Serial.println("Device MAC: " + deviceMAC);
  Serial.println("Setup SSID: " + setupSSID);
  Serial.println("Port: " + String(assignedPort));
  Serial.flush();
  
  // Load configuration
  Serial.println("Loading configuration...");
  Serial.flush();
  loadConfiguration();
  
  // Start in appropriate mode
  Serial.println("Starting mode selection...");
  Serial.flush();
  
  if (isConfigured) {
    Serial.println("Connecting to saved WiFi...");
    Serial.flush();
    connectToWiFi();
  } else {
    Serial.println("Starting setup mode...");
    Serial.flush();
    enterSetupMode();
  }
  
  Serial.println("Setup complete - entering main loop");
  Serial.flush();
}

// ----------------------------------------------------------------------------
// Main Loop
// ----------------------------------------------------------------------------
void loop() {
  checkResetButton();
  updateLED();
  
  if (currentMode == MODE_SETUP) {
    webServer.handleClient();
  } else if (currentMode == MODE_NORMAL) {
    // Handle UDP packets
    int packetSize = udp.parsePacket();
    if (packetSize) {
      remoteIP = udp.remoteIP();
      remotePort = udp.remotePort();
      
      char packetBuffer[128];
      int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
      packetBuffer[len] = 0;
      String cmd = String(packetBuffer);
      
      if (cmd == "ping") {
        handleHeartbeat();
      } else {
        processCommand(cmd);
      }
    }
    
    // Send device data if streaming
    unsigned long currentTime = millis();
    unsigned long interval = 1000UL / samplingRate;
    if (streaming && (currentTime - lastSampleTime >= interval)) {
      lastSampleTime = currentTime;
      sendDeviceData();
    }
    
    // Check heartbeat timeout
    if (connected && (millis() - lastHeartbeatTime > HEARTBEAT_TIMEOUT)) {
      Serial.println("Heartbeat timeout");
      connected = false;
      currentLEDPattern = LED_SLOW_BLINK;
      resumeAdvertising();
    }
  }
}

// ----------------------------------------------------------------------------
// Hardware Functions - NO TESTING LOOPS
// ----------------------------------------------------------------------------
void initializeHardware() {
  // Fixed GPIO pins - no detection needed
  pinMode(ACTIVE_RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ACTIVE_STATUS_LED_PIN, OUTPUT);
  digitalWrite(ACTIVE_STATUS_LED_PIN, LOW);
  
  Serial.println("Using fixed GPIO pins:");
  Serial.println("  LED Pin: " + String(ACTIVE_STATUS_LED_PIN));
  Serial.println("  Button Pin: " + String(ACTIVE_RESET_BUTTON_PIN));
}

void startupLEDSequence() {
  // Simple 3-flash sequence - no loops that could hang
  for (int i = 0; i < 3; i++) {
    digitalWrite(ACTIVE_STATUS_LED_PIN, HIGH);
    delay(200);
    digitalWrite(ACTIVE_STATUS_LED_PIN, LOW);
    delay(200);
  }
}

// ----------------------------------------------------------------------------
// Configuration Functions
// ----------------------------------------------------------------------------
void loadConfiguration() {
  String savedSSID = preferences.getString("wifi_ssid", "");
  String savedPassword = preferences.getString("wifi_password", "");
  
  if (savedSSID.length() > 0 && savedPassword.length() > 0) {
    isConfigured = true;
    Serial.println("Found WiFi config: " + savedSSID);
  } else {
    isConfigured = false;
    Serial.println("No WiFi config found");
  }
}

void saveConfiguration(const String& ssid, const String& password) {
  preferences.putString("wifi_ssid", ssid);
  preferences.putString("wifi_password", password);
  isConfigured = true;
  Serial.println("WiFi config saved: " + ssid);
}

void clearConfiguration() {
  preferences.clear();
  isConfigured = false;
  Serial.println("Configuration cleared");
}

String getDeviceMAC() {
  Serial.println("Getting MAC address with ESP32-S3 initialization...");
  
  // Try WiFi MAC first with proper initialization
  WiFi.mode(WIFI_STA);
  delay(1000);  // ESP32-S3 needs more time
  
  String mac = WiFi.macAddress();
  Serial.println("WiFi MAC attempt: " + mac);
  
  // Check if MAC is valid (not all zeros)
  if (mac != "00:00:00:00:00:00" && mac.length() > 0) {
    WiFi.mode(WIFI_OFF);
    delay(100);
    Serial.println("✓ Valid WiFi MAC obtained: " + mac);
    return mac;
  }
  
  // Fallback to chip ID if WiFi MAC fails
  Serial.println("WiFi MAC invalid, using chip ID fallback...");
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  uint64_t chipid = ESP.getEfuseMac();
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           (uint8_t)(chipid >> 40), (uint8_t)(chipid >> 32), (uint8_t)(chipid >> 24),
           (uint8_t)(chipid >> 16), (uint8_t)(chipid >> 8), (uint8_t)chipid);
  
  String fallbackMAC = String(macStr);
  Serial.println("✓ Chip ID MAC generated: " + fallbackMAC);
  return fallbackMAC;
}

unsigned int calculatePortFromMAC() {
  String lastBytes = deviceMAC.substring(14, 16);
  uint8_t suffix = strtol(lastBytes.c_str(), NULL, 16);
  return 5000 + (suffix % 5);
}

// ----------------------------------------------------------------------------
// Mode Management
// ----------------------------------------------------------------------------
void enterSetupMode() {
  currentMode = MODE_SETUP;
  currentLEDPattern = LED_FAST_BLINK;
  
  Serial.println("Entering setup mode");
  
  // Stop any existing connections
  udp.stop();
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(500);  // Give more time for WiFi to fully shut down
  
  // Set to AP mode and keep it active
  WiFi.mode(WIFI_AP);
  delay(100);
  
  setupSoftAP();
  setupWebServer();
  
  Serial.println("Setup mode active - SoftAP should stay up");
  Serial.println("Connect to: " + setupSSID);
  Serial.println("Password: " + String(SETUP_AP_PASSWORD));
  Serial.println("Configure by posting {ssid: '...', password: '...'} at: http://192.168.4.1/configure");
}

void setupSoftAP() {
  WiFi.softAP(setupSSID.c_str(), SETUP_AP_PASSWORD);
  IPAddress IP = WiFi.softAPIP();
  Serial.println("AP IP: " + IP.toString());
}

void setupWebServer() {
  webServer.on("/configure", HTTP_POST, handleSave);
  webServer.on("/deviceInfo", HTTP_GET, handleDeviceInfo);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  Serial.println("Web server started");
}

void connectToWiFi() {
  String ssid = preferences.getString("wifi_ssid", "");
  String password = preferences.getString("wifi_password", "");
  
  if (ssid.length() == 0) {
    Serial.println("No saved SSID found");
    enterSetupMode();
    return;
  }
  
  Serial.println("Initializing WiFi for ESP32-S3...");
  
  // Proper ESP32-S3 WiFi initialization
  WiFi.persistent(false);  // Don't save credentials to flash
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(1000);  // ESP32-S3 needs more time
  
  Serial.println("Connecting to: " + ssid);
  Serial.println("Password length: " + String(password.length()));
  currentLEDPattern = LED_SLOW_BLINK;
  
  // Clear any previous connection state
  WiFi.disconnect();
  delay(100);
  
  // Start connection
  WiFi.begin(ssid.c_str(), password.c_str());
  
  unsigned long startTime = millis();
  int lastStatus = -1;
  
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    
    // Report status changes
    int currentStatus = WiFi.status();
    if (currentStatus != lastStatus) {
      Serial.println();
      Serial.print("WiFi Status: ");
      switch (currentStatus) {
        case WL_IDLE_STATUS: Serial.println("IDLE"); break;
        case WL_NO_SSID_AVAIL: Serial.println("NO_SSID_AVAILABLE"); break;
        case WL_SCAN_COMPLETED: Serial.println("SCAN_COMPLETED"); break;
        case WL_CONNECTED: Serial.println("CONNECTED"); break;
        case WL_CONNECT_FAILED: Serial.println("CONNECT_FAILED"); break;
        case WL_CONNECTION_LOST: Serial.println("CONNECTION_LOST"); break;
        case WL_DISCONNECTED: Serial.println("DISCONNECTED"); break;
        default: Serial.println("UNKNOWN (" + String(currentStatus) + ")"); break;
      }
      lastStatus = currentStatus;
    }
    
    Serial.print(".");
    
    // Check for specific failure conditions
    if (currentStatus == WL_CONNECT_FAILED) {
      Serial.println("\nConnection failed with error: " + String(currentStatus));
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected successfully!");
    Serial.println("IP address: " + WiFi.localIP().toString());
    Serial.println("RSSI: " + String(WiFi.RSSI()) + " dBm");
    Serial.println("Gateway: " + WiFi.gatewayIP().toString());
    Serial.println("DNS: " + WiFi.dnsIP().toString());
    
    currentLEDPattern = LED_SOLID_ON;
    setupMDNS();
    udp.begin(assignedPort);
    Serial.println("UDP listening on port " + String(assignedPort));
    
    currentMode = MODE_NORMAL;
  } else {
    Serial.println("\n✗ WiFi connection failed!");
    Serial.println("Final status: " + String(WiFi.status()));
    Serial.println("Saved SSID: '" + ssid + "'");
    Serial.println("Entering setup mode for reconfiguration...");
    
    // Clear potentially corrupted credentials
    clearConfiguration();
    enterSetupMode();
  }
}

void setupMDNS() {
  if (MDNS.begin(mDNS_hostname.c_str())) {
    Serial.println("mDNS started: " + mDNS_hostname + ".local");
    MDNS.addService(mDNS_service, "_udp", assignedPort);
  }
}

// ----------------------------------------------------------------------------
// Web Server Handlers
// ----------------------------------------------------------------------------
void handleDeviceInfo(){
  String infoMsg = "{";
  infoMsg += "\"deviceType\":\"" + DEVICE_TYPE + "\",";
  infoMsg += "\"macAddress\":\"" + deviceMAC + "\",";
  infoMsg += "\"mode\":\"" + String(currentMode == MODE_SETUP ? "setup" : "normal") + "\",";
  infoMsg += "\"configured\":" + String(isConfigured ? "true" : "false");
  infoMsg += "}";
  webServer.send(200, "application/json", infoMsg);
}
void handleSave() {
  String body = webServer.arg("plain");
  Serial.println("Received: " + body);
  
  // Simple JSON parsing
  int ssidStart = body.indexOf("\"ssid\":");
  int passwordStart = body.indexOf("\"password\":");
  
  if (ssidStart == -1 || passwordStart == -1) {
    webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON format\"}");
    return;
  }
  
  // Extract SSID
  ssidStart = body.indexOf("\"", ssidStart + 7) + 1;
  int ssidEnd = body.indexOf("\"", ssidStart);
  String ssid = body.substring(ssidStart, ssidEnd);
  
  // Extract Password
  passwordStart = body.indexOf("\"", passwordStart + 11) + 1;
  int passwordEnd = body.indexOf("\"", passwordStart);
  String password = body.substring(passwordStart, passwordEnd);
  
  if (ssid.length() > 0) {
    saveConfiguration(ssid, password);
    webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
    
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  } else {
    webServer.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid SSID\"}");
  }
}

void handleNotFound() {
  webServer.send(404, "application/json", "{\"success\":false,\"error\":\"Endpoint not found\"}");
}

// ----------------------------------------------------------------------------
// Hardware Control
// ----------------------------------------------------------------------------
void updateLED() {
  unsigned long currentTime = millis();
  
  if (currentLEDPattern == LED_SOLID_ON) {
    digitalWrite(ACTIVE_STATUS_LED_PIN, HIGH);
  } else if (currentLEDPattern == LED_OFF) {
    digitalWrite(ACTIVE_STATUS_LED_PIN, LOW);
  } else if (currentTime - lastLEDBlink >= currentLEDPattern) {
    ledState = !ledState;
    digitalWrite(ACTIVE_STATUS_LED_PIN, ledState);
    lastLEDBlink = currentTime;
  }
}

void checkResetButton() {
  bool currentButtonState = digitalRead(ACTIVE_RESET_BUTTON_PIN);
  
  // Button pressed (LOW due to pullup)
  if (currentButtonState == LOW && lastResetButtonState == HIGH) {
    resetButtonPressStart = millis();
    resetButtonPressed = true;
    Serial.println("Reset button pressed");
  }
  
  // Button released
  if (currentButtonState == HIGH && lastResetButtonState == LOW) {
    resetButtonPressed = false;
    unsigned long pressDuration = millis() - resetButtonPressStart;
    Serial.println("Button released after " + String(pressDuration) + "ms");
  }
  
  // Check for long press
  if (resetButtonPressed) {
    unsigned long pressTime = millis() - resetButtonPressStart;
    
    if (pressTime >= RESET_BUTTON_HOLD_TIME) {
      Serial.println("Reset button held - clearing config!");
      
      // Simple LED feedback for reset
      for (int i = 0; i < 5; i++) {
        digitalWrite(ACTIVE_STATUS_LED_PIN, HIGH);
        delay(100);
        digitalWrite(ACTIVE_STATUS_LED_PIN, LOW);
        delay(100);
      }
      
      clearConfiguration();
      resetButtonPressed = false;
      enterSetupMode();
    }
  }
  
  lastResetButtonState = currentButtonState;
}

// ----------------------------------------------------------------------------
// Communication Functions
// ----------------------------------------------------------------------------
void resumeAdvertising() {
  MDNS.addService(mDNS_service, "_udp", assignedPort);  // Add the underscore
}

void handleHeartbeat() {
  lastHeartbeatTime = millis();
  if (!connected) {
    Serial.println("Connected to app");
    connected = true;
    currentLEDPattern = LED_SOLID_ON;
  }
}

void processCommand(const String& cmd) {
  if (cmd == "initialize") {
    Serial.println("Device initialized");
  } else if (cmd.startsWith("setSamplingRate:")) {
    String rateStr = cmd.substring(strlen("setSamplingRate:"));
    samplingRate = rateStr.toInt();
    Serial.println("Sampling rate: " + String(samplingRate) + " Hz");
  } else if (cmd == "startStreaming") {
    streaming = true;
    Serial.println("Streaming started");
  } else if (cmd == "stopStreaming") {
    streaming = false;
    Serial.println("Streaming stopped");
  } else if (cmd == "getStatus") {
    sendStatus();
  }
}

void sendDeviceData() {
  String dataMsg = "{";
  dataMsg += "\"timestamp\":" + String(millis()) + ",";
  dataMsg += "\"deviceType\":\"" + DEVICE_TYPE + "\",";
  dataMsg += "\"streaming\":" + String(streaming ? "true" : "false") + ",";
  dataMsg += "\"samplingRate\":" + String(samplingRate) + ",";
  dataMsg += "\"uptime\":" + String(millis()) + ",";
  dataMsg += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  dataMsg += "\"wifiRSSI\":" + String(WiFi.RSSI());
  dataMsg += "}";
  
  udp.beginPacket(remoteIP, remotePort);
  udp.print(dataMsg);
  udp.endPacket();
}

void sendStatus() {
  unsigned long timeSinceHeartbeat = millis() - lastHeartbeatTime;
  String ipStr = WiFi.localIP().toString();
  String remoteIPStr = connected ? remoteIP.toString() : "";
  
  String statusMsg = "{";
  statusMsg += "\"ip\":\"" + ipStr + "\",";
  statusMsg += "\"remoteIP\":\"" + remoteIPStr + "\",";
  statusMsg += "\"port\":" + String(assignedPort) + ",";
  statusMsg += "\"streaming\":" + String(streaming ? "true" : "false") + ",";
  statusMsg += "\"samplingRate\":" + String(samplingRate) + ",";
  statusMsg += "\"lastConnection\":" + String(timeSinceHeartbeat) + ",";
  statusMsg += "\"connected\":" + String(connected ? "true" : "false") + ",";
  statusMsg += "\"mode\":\"" + String(currentMode == MODE_SETUP ? "setup" : "normal") + "\",";
  statusMsg += "\"mac\":\"" + deviceMAC + "\",";
  statusMsg += "\"deviceType\":\"" + DEVICE_TYPE + "\"";
  statusMsg += "}";
  
  udp.beginPacket(remoteIP, remotePort);
  udp.print(statusMsg);
  udp.endPacket();
  
  Serial.println("Status sent");
}
