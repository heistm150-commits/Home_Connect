#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// WiFi Credentials
const char* ssid = "Gaming_God";
const char* password = "GamingGod";

// mDNS Configuration
const char* mdnsHostname = "homeserver";
const char* mdnsServiceType = "_homeconnect._tcp";
#define WEBSOCKET_PORT 81

// WebSocket server
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT);

// Preferences for state persistence
Preferences preferences;

// IR Pins
#define IR_LED_PIN 25
#define IR_RECV_PIN 26

// ================= PIN DEFINITIONS =================
// Adjust these to match your actual wiring!

// Living Room Pins
#define LIVING_LIGHT_1_PIN 12
#define LIVING_LIGHT_2_PIN 13
#define LIVING_LIGHT_3_PIN 14
#define LIVING_LIGHT_4_PIN 27
#define LIVING_LIGHT_5_PIN 32
#define LIVING_LIGHT_6_PIN 33
#define LIVING_SHADE_1_PIN 15
#define LIVING_SHADE_2_PIN 2
#define LIVING_SHADE_3_PIN 4
#define LIVING_SHADE_4_PIN 16

// Kitchen Pins
#define KITCHEN_LIGHT_1_PIN 17
#define KITCHEN_LIGHT_2_PIN 5
#define KITCHEN_LIGHT_3_PIN 18
#define KITCHEN_LIGHT_4_PIN 19
#define KITCHEN_LIGHT_5_PIN 21
#define KITCHEN_LIGHT_6_PIN 22
#define KITCHEN_SHADE_1_PIN 23
#define KITCHEN_SHADE_2_PIN 26
#define KITCHEN_SHADE_3_PIN 25
#define KITCHEN_SHADE_4_PIN 35

// Dining Room Pins
#define DINING_LIGHT_1_PIN 34
#define DINING_LIGHT_2_PIN 36
#define DINING_LIGHT_3_PIN 39
#define DINING_LIGHT_4_PIN 12  // Note: Might conflict with Living Room - adjust!
#define DINING_LIGHT_5_PIN 13  // Note: Might conflict with Living Room - adjust!
#define DINING_LIGHT_6_PIN 14  // Note: Might conflict with Living Room - adjust!
#define DINING_SHADE_1_PIN 27
#define DINING_SHADE_2_PIN 32
#define DINING_SHADE_3_PIN 33
#define DINING_SHADE_4_PIN 15

// Master Bedroom Pins
#define MBR_LIGHT_1_PIN 2
#define MBR_LIGHT_2_PIN 4
#define MBR_LIGHT_3_PIN 16
#define MBR_LIGHT_4_PIN 17
#define MBR_LIGHT_5_PIN 5
#define MBR_LIGHT_6_PIN 18
#define MBR_SHADE_1_PIN 19
#define MBR_SHADE_2_PIN 21
#define MBR_SHADE_3_PIN 22
#define MBR_SHADE_4_PIN 23

// Guest Room Pins
#define GUEST_LIGHT_1_PIN 26
#define GUEST_LIGHT_2_PIN 27
#define GUEST_LIGHT_3_PIN 14
#define GUEST_LIGHT_4_PIN 12
#define GUEST_LIGHT_5_PIN 13
#define GUEST_LIGHT_6_PIN 15
#define GUEST_SHADE_1_PIN 2
#define GUEST_SHADE_2_PIN 4
#define GUEST_SHADE_3_PIN 16
#define GUEST_SHADE_4_PIN 17

// PWM Configuration
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// AC IR Codes (NEC format - UPDATE WITH YOUR ACTUAL CODES!)
// Separate IR codes for each room's AC if needed
#define AC_POWER_ON 0x20DF10EF
#define AC_POWER_OFF 0x20DF10EF  // Often same as power on for toggle

// Temperature codes (example - replace with your actual codes)
uint32_t acTempCodes[] = {
    0x20DF08F7, // 18°C
    0x20DF8877, // 19°C
    0x20DF48B7, // 20°C
    0x20DFC837, // 21°C
    0x20DF28D7, // 22°C
    0x20DFA857, // 23°C
    0x20DF6897, // 24°C
    0x20DFE817, // 25°C
    0x20DF18E7, // 26°C
    0x20DF9867, // 27°C
    0x20DF58A7, // 28°C
    0x20DFD827, // 29°C
    0x20DF38C7  // 30°C
};

IRsend irsend(IR_LED_PIN);
IRrecv irrecv(IR_RECV_PIN);
decode_results results;

// ================= DEVICE MANAGEMENT =================
struct DeviceState {
    String id;
    int value;
    bool boolean;
    int pin;
    bool isPWM;
    bool isRelay;
    bool isAC;
    bool isShade;
    String room;
    bool enabled; // Some pins might not be physically connected
};

// Room configuration - ADDED GUEST ROOM
const int ROOM_COUNT = 5; // Updated from 4 to 5
const char* ROOM_NAMES[ROOM_COUNT] = {"LIVING", "KITCHEN", "DINING", "MBR", "GUEST"};
const int LIGHTS_PER_ROOM = 6;
const int SHADES_PER_ROOM = 4;

// Calculate total devices: 5 rooms × (2 AC + 6 lights + 4 shades + 1 room_off)
const int TOTAL_DEVICES = ROOM_COUNT * (2 + LIGHTS_PER_ROOM + SHADES_PER_ROOM + 1);

DeviceState devices[TOTAL_DEVICES];
int deviceCount = 0;

int connectedClients = 0;

// Function prototypes
void initializeDevices();
void initializePins();
void connectToWiFi();
void setupmDNS();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void processWebSocketMessage(uint8_t clientNum, char* message);
void processCommand(String id, int value, bool boolean);
void sendDeviceState(String id, int value);
void broadcastDeviceState(String id, int value);
void sendAllDeviceStatesToClient(uint8_t clientNum);
void controlAC(String room, bool powerOn, int temperature = -1);
void controlLight(String deviceId, int brightness);
void controlShade(String deviceId, int position);
void turnOffRoom(String room);
void saveDeviceState(String id, int value, bool boolean);
void loadDeviceStates();
void handleScene(String room, String sceneName);
void applyScene(String room, String sceneName);
void sendCustomNamesToClient(uint8_t clientNum);
void setupGuestRoomDevices(int& deviceIndex, int roomIdx);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n🏠 Home Connect Server (With Guest Room)");
    Serial.println("==========================================");
    
    // Initialize preferences
    preferences.begin("home-connect", false);
    
    // Initialize all devices
    initializeDevices();
    
    // Load saved states
    loadDeviceStates();
    
    // Initialize hardware pins
    initializePins();
    
    // Connect to WiFi
    connectToWiFi();
    
    // Setup mDNS
    setupmDNS();
    
    // Initialize IR
    irsend.begin();
    irrecv.enableIRIn();
    Serial.println("✅ IR initialized");
    
    // Start WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println("✅ WebSocket server started on port 81");
    
    Serial.println("\n📡 Server Information:");
    Serial.print("Hostname: ");
    Serial.print(mdnsHostname);
    Serial.println(".local");
    Serial.print("WebSocket: ws://");
    Serial.print(WiFi.localIP());
    Serial.println(":81");
    Serial.print("Total rooms: ");
    Serial.println(ROOM_COUNT);
    Serial.print("Total devices: ");
    Serial.println(deviceCount);
    Serial.println("\n🚀 Server ready!");
}

void initializeDevices() {
    deviceCount = 0;
    
    // Room pin arrays for easier assignment
    int lightPins[ROOM_COUNT][LIGHTS_PER_ROOM] = {
        // Living Room
        {LIVING_LIGHT_1_PIN, LIVING_LIGHT_2_PIN, LIVING_LIGHT_3_PIN, 
         LIVING_LIGHT_4_PIN, LIVING_LIGHT_5_PIN, LIVING_LIGHT_6_PIN},
        // Kitchen
        {KITCHEN_LIGHT_1_PIN, KITCHEN_LIGHT_2_PIN, KITCHEN_LIGHT_3_PIN,
         KITCHEN_LIGHT_4_PIN, KITCHEN_LIGHT_5_PIN, KITCHEN_LIGHT_6_PIN},
        // Dining Room
        {DINING_LIGHT_1_PIN, DINING_LIGHT_2_PIN, DINING_LIGHT_3_PIN,
         DINING_LIGHT_4_PIN, DINING_LIGHT_5_PIN, DINING_LIGHT_6_PIN},
        // Master Bedroom
        {MBR_LIGHT_1_PIN, MBR_LIGHT_2_PIN, MBR_LIGHT_3_PIN,
         MBR_LIGHT_4_PIN, MBR_LIGHT_5_PIN, MBR_LIGHT_6_PIN},
        // Guest Room - NEW
        {GUEST_LIGHT_1_PIN, GUEST_LIGHT_2_PIN, GUEST_LIGHT_3_PIN,
         GUEST_LIGHT_4_PIN, GUEST_LIGHT_5_PIN, GUEST_LIGHT_6_PIN}
    };
    
    int shadePins[ROOM_COUNT][SHADES_PER_ROOM] = {
        // Living Room
        {LIVING_SHADE_1_PIN, LIVING_SHADE_2_PIN, LIVING_SHADE_3_PIN, LIVING_SHADE_4_PIN},
        // Kitchen
        {KITCHEN_SHADE_1_PIN, KITCHEN_SHADE_2_PIN, KITCHEN_SHADE_3_PIN, KITCHEN_SHADE_4_PIN},
        // Dining Room
        {DINING_SHADE_1_PIN, DINING_SHADE_2_PIN, DINING_SHADE_3_PIN, DINING_SHADE_4_PIN},
        // Master Bedroom
        {MBR_SHADE_1_PIN, MBR_SHADE_2_PIN, MBR_SHADE_3_PIN, MBR_SHADE_4_PIN},
        // Guest Room - NEW
        {GUEST_SHADE_1_PIN, GUEST_SHADE_2_PIN, GUEST_SHADE_3_PIN, GUEST_SHADE_4_PIN}
    };
    
    // Create all devices
    for (int roomIdx = 0; roomIdx < ROOM_COUNT; roomIdx++) {
        String room = ROOM_NAMES[roomIdx];
        
        // AC Devices
        devices[deviceCount++] = {room + "_AC", 24, false, 0, false, false, true, false, room, true};
        devices[deviceCount++] = {room + "_AC_POWER", 0, false, 0, false, false, true, false, room, true};
        
        // Lights (6 per room)
        for (int i = 1; i <= LIGHTS_PER_ROOM; i++) {
            int pin = lightPins[roomIdx][i-1];
            bool enabled = (pin > 0); // Pin 0 means not connected
            
            devices[deviceCount++] = {
                room + "_LIGHT_" + String(i),
                0, false, pin, true, false, false, false, room, enabled
            };
        }
        
        // Shades (4 per room)
        for (int i = 1; i <= SHADES_PER_ROOM; i++) {
            int pin = shadePins[roomIdx][i-1];
            bool enabled = (pin > 0);
            
            devices[deviceCount++] = {
                room + "_SHADE_" + String(i),
                0, false, pin, false, true, false, true, room, enabled
            };
        }
        
        // Room Off command
        devices[deviceCount++] = {
            room + "_ROOM_OFF",
            0, false, 0, false, false, false, false, room, true
        };
    }
    
    Serial.print("✅ Initialized ");
    Serial.print(deviceCount);
    Serial.println(" devices (including Guest Room)");
}

void initializePins() {
    Serial.println("Initializing hardware pins...");
    
    for (int i = 0; i < deviceCount; i++) {
        if (devices[i].enabled && devices[i].pin > 0) {
            pinMode(devices[i].pin, OUTPUT);
            digitalWrite(devices[i].pin, LOW);
            
            if (devices[i].isPWM) {
                ledcAttach(devices[i].pin, PWM_FREQ, PWM_RESOLUTION);
                // Apply saved brightness
                int pwmValue = map(devices[i].value, 0, 100, 0, 255);
                ledcWrite(devices[i].pin, pwmValue);
            }
        }
    }
    
    Serial.println("✅ All pins initialized and restored");
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(1000);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ WiFi connection failed!");
        while(1) delay(1000);
    }
}

void setupmDNS() {
    Serial.print("Setting up mDNS...");
    
    if (!MDNS.begin(mdnsHostname)) {
        Serial.println("❌ Error starting mDNS!");
        return;
    }
    
    MDNS.addService("_homeconnect", "_tcp", WEBSOCKET_PORT);
    MDNS.addServiceTxt("_homeconnect", "_tcp", "name", "HomeServer");
    MDNS.addServiceTxt("_homeconnect", "_tcp", "version", "2.1");
    MDNS.addServiceTxt("_homeconnect", "_tcp", "format", "6L4S"); // 6 Lights, 4 Shades format
    MDNS.addServiceTxt("_homeconnect", "_tcp", "rooms", "5"); // 5 rooms total
    
    Serial.println("✅ mDNS responder started");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("Client [%u] disconnected\n", num);
            connectedClients--;
            break;
            
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("Client [%u] connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                connectedClients++;
                
                // Send welcome message
                StaticJsonDocument<256> welcome;
                welcome["type"] = "welcome";
                welcome["server"] = mdnsHostname;
                welcome["rooms"] = ROOM_COUNT;
                welcome["format"] = "6L4S";
                welcome["guest_room"] = true; // Indicate guest room support
                
                String welcomeMsg;
                serializeJson(welcome, welcomeMsg);
                webSocket.sendTXT(num, welcomeMsg);
                
                // Send all device states
                sendAllDeviceStatesToClient(num);
                
                // Send custom names if any
                sendCustomNamesToClient(num);
            }
            break;
            
        case WStype_TEXT:
            {
                Serial.printf("Client [%u]: %s\n", num, payload);
                processWebSocketMessage(num, (char*)payload);
            }
            break;
            
        default:
            break;
    }
}

void processWebSocketMessage(uint8_t clientNum, char* message) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }
    
    // Check for scene command
    if (doc.containsKey("scene")) {
        String scene = doc["scene"].as<String>();
        String room = doc.containsKey("room") ? doc["room"].as<String>() : "";
        
        if (room != "") {
            handleScene(room, scene);
        } else {
            // Multi-room scene
            if (scene == "ALL_OFF") {
                for (int i = 0; i < ROOM_COUNT; i++) {
                    turnOffRoom(ROOM_NAMES[i]);
                }
            }
        }
        return;
    }
    
    // Standard device command
    if (doc.containsKey("ID")) {
        String id = doc["ID"].as<String>();
        int value = doc.containsKey("value") ? doc["value"].as<int>() : 0;
        bool boolean = doc.containsKey("boolean") ? doc["boolean"].as<bool>() : false;
        
        // Handle custom name updates
        if (id.endsWith("_NAME")) {
            String nameValue = doc.containsKey("value") ? doc["value"].as<String>() : "";
            preferences.putString(id.c_str(), nameValue);
            broadcastDeviceState(id, 0); // Notify other clients
            return;
        }
        
        processCommand(id, value, boolean);
    }
}

void processCommand(String id, int value, bool boolean) {
    Serial.printf("Processing: %s = %d (%s)\n", 
                  id.c_str(), value, boolean ? "true" : "false");
    
    // Special handling for GUEST_ROOM_OFF
    if (id == "GUEST_ROOM_OFF") {
        Serial.println("Turning off Guest Room");
        turnOffRoom("GUEST");
        return;
    }
    
    // Find and update device
    for (int i = 0; i < deviceCount; i++) {
        if (devices[i].id == id) {
            // Update state
            devices[i].value = value;
            devices[i].boolean = boolean;
            
            // Save to flash
            saveDeviceState(id, value, boolean);
            
            // Execute command based on device type
            if (id.endsWith("_AC_POWER")) {
                String room = id.substring(0, id.indexOf("_AC_POWER"));
                controlAC(room, boolean);
                
                // For GUEST_AC_POWER, also update the AC device state
                if (room == "GUEST") {
                    // Find the corresponding GUEST_AC device
                    for (int j = 0; j < deviceCount; j++) {
                        if (devices[j].id == "GUEST_AC") {
                            devices[j].boolean = boolean;
                            broadcastDeviceState("GUEST_AC", devices[j].value);
                            break;
                        }
                    }
                }
            }
            else if (id.endsWith("_AC")) {
                String room = id.substring(0, id.indexOf("_AC"));
                bool acPower = false;
                // Find AC power state
                for (int j = 0; j < deviceCount; j++) {
                    if (devices[j].id == room + "_AC_POWER") {
                        acPower = devices[j].boolean;
                        break;
                    }
                }
                if (acPower) {
                    controlAC(room, true, value);
                }
            }
            else if (id.indexOf("_LIGHT_") != -1 && devices[i].enabled) {
                controlLight(id, value);
            }
            else if (id.indexOf("_SHADE_") != -1 && devices[i].enabled) {
                controlShade(id, value);
            }
            else if (id.endsWith("_ROOM_OFF") && id != "GUEST_ROOM_OFF") {
                String room = id.substring(0, id.indexOf("_ROOM_OFF"));
                turnOffRoom(room);
                return; // Don't broadcast single state - room off broadcasts multiple
            }
            
            // Broadcast update to all clients
            broadcastDeviceState(id, value);
            break;
        }
    }
}

void sendDeviceState(String id, int value) {
    StaticJsonDocument<128> doc;
    doc["ID"] = id;
    doc["value"] = value;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    webSocket.broadcastTXT(jsonString);
}

void broadcastDeviceState(String id, int value) {
    sendDeviceState(id, value);
}

void sendAllDeviceStatesToClient(uint8_t clientNum) {
    Serial.printf("Sending all states to client [%u]\n", clientNum);
    
    for (int i = 0; i < deviceCount; i++) {
        StaticJsonDocument<128> doc;
        doc["ID"] = devices[i].id;
        doc["value"] = devices[i].value;
        
        String jsonString;
        serializeJson(doc, jsonString);
        webSocket.sendTXT(clientNum, jsonString);
        
        // Small delay to prevent overwhelming the client
        delay(5);
    }
}

void sendCustomNamesToClient(uint8_t clientNum) {
    // Send stored custom names
    for (int i = 0; i < deviceCount; i++) {
        // Check for light/shade names
        if (devices[i].id.indexOf("_LIGHT_") != -1 || 
            devices[i].id.indexOf("_SHADE_") != -1) {
            
            String nameKey = devices[i].id + "_NAME";
            String customName = preferences.getString(nameKey.c_str(), "");
            
            if (customName != "") {
                StaticJsonDocument<256> doc;
                doc["ID"] = nameKey;
                doc["value"] = customName;
                
                String jsonString;
                serializeJson(doc, jsonString);
                webSocket.sendTXT(clientNum, jsonString);
                delay(5);
            }
        }
    }
}

void controlAC(String room, bool powerOn, int temperature) {
    Serial.printf("%s AC: %s", room.c_str(), powerOn ? "ON" : "OFF");
    if (temperature > 0) Serial.printf(" at %d°C", temperature);
    Serial.println();
    
    // You might want different IR codes for different rooms
    // For now using same codes for all rooms
    if (powerOn) {
        irsend.sendNEC(AC_POWER_ON, 32);
        delay(100);
        
        if (temperature >= 18 && temperature <= 30) {
            irsend.sendNEC(acTempCodes[temperature - 18], 32);
            delay(100);
        }
    } else {
        irsend.sendNEC(AC_POWER_OFF, 32);
        delay(100);
    }
}

void controlLight(String deviceId, int brightness) {
    // Find the device
    for (int i = 0; i < deviceCount; i++) {
        if (devices[i].id == deviceId && devices[i].enabled && devices[i].isPWM) {
            int pwmValue = map(brightness, 0, 100, 0, 255);
            ledcWrite(devices[i].pin, pwmValue);
            
            Serial.printf("%s → %d%% (PWM: %d)\n", 
                         deviceId.c_str(), brightness, pwmValue);
            break;
        }
    }
}

void controlShade(String deviceId, int position) {
    for (int i = 0; i < deviceCount; i++) {
        if (devices[i].id == deviceId && devices[i].enabled && devices[i].isShade) {
            // Simple relay control - adjust for your motor
            int pulseTime = map(position, 0, 100, 500, 2000); // 0.5-2 seconds
            
            digitalWrite(devices[i].pin, HIGH);
            delay(pulseTime);
            digitalWrite(devices[i].pin, LOW);
            
            Serial.printf("%s → %d%% (%d ms)\n", 
                         deviceId.c_str(), position, pulseTime);
            break;
        }
    }
}

void turnOffRoom(String room) {
    Serial.printf("Turning off all devices in %s\n", room.c_str());
    
    // Track which devices we're turning off for broadcasting
    String devicesToUpdate[50];
    int updateCount = 0;
    
    for (int i = 0; i < deviceCount; i++) {
        if (devices[i].room == room && devices[i].enabled) {
            if (devices[i].id.indexOf("_LIGHT_") != -1 && devices[i].isPWM) {
                // Turn off light
                ledcWrite(devices[i].pin, 0);
                devices[i].value = 0;
                devices[i].boolean = false;
                devicesToUpdate[updateCount++] = devices[i].id;
                saveDeviceState(devices[i].id, 0, false);
            }
            else if (devices[i].id == room + "_AC_POWER" && devices[i].boolean) {
                // Turn off AC
                controlAC(room, false);
                devices[i].value = 0;
                devices[i].boolean = false;
                devicesToUpdate[updateCount++] = devices[i].id;
                saveDeviceState(devices[i].id, 0, false);
                
                // Also update the AC temperature device state
                for (int j = 0; j < deviceCount; j++) {
                    if (devices[j].id == room + "_AC") {
                        devices[j].boolean = false;
                        devicesToUpdate[updateCount++] = devices[j].id;
                        saveDeviceState(devices[j].id, devices[j].value, false);
                        break;
                    }
                }
            }
        }
    }
    
    // Broadcast all updates at once
    for (int i = 0; i < updateCount; i++) {
        broadcastDeviceState(devicesToUpdate[i], 0);
    }
}

void saveDeviceState(String id, int value, bool boolean) {
    String keyValue = id + "_v";
    String keyBool = id + "_b";
    
    preferences.putInt(keyValue.c_str(), value);
    preferences.putBool(keyBool.c_str(), boolean);
}

void loadDeviceStates() {
    Serial.println("Loading saved device states...");
    
    for (int i = 0; i < deviceCount; i++) {
        String keyValue = devices[i].id + "_v";
        String keyBool = devices[i].id + "_b";
        
        devices[i].value = preferences.getInt(keyValue.c_str(), devices[i].value);
        devices[i].boolean = preferences.getBool(keyBool.c_str(), devices[i].boolean);
    }
    
    Serial.println("✅ Device states loaded");
}

void handleScene(String room, String sceneName) {
    Serial.printf("Activating %s scene in %s\n", sceneName.c_str(), room.c_str());
    applyScene(room, sceneName);
}

void applyScene(String room, String sceneName) {
    if (sceneName == "MOVIE") {
        // Dim lights to 20%, close shades
        for (int i = 0; i < deviceCount; i++) {
            if (devices[i].room == room && devices[i].enabled) {
                if (devices[i].id.indexOf("_LIGHT_") != -1) {
                    controlLight(devices[i].id, 20);
                    broadcastDeviceState(devices[i].id, 20);
                }
                else if (devices[i].isShade) {
                    controlShade(devices[i].id, 0);
                    broadcastDeviceState(devices[i].id, 0);
                }
            }
        }
    }
    else if (sceneName == "SLEEP") {
        // Dim lights to 10%, close shades, turn off AC
        for (int i = 0; i < deviceCount; i++) {
            if (devices[i].room == room && devices[i].enabled) {
                if (devices[i].id.indexOf("_LIGHT_") != -1) {
                    controlLight(devices[i].id, 10);
                    broadcastDeviceState(devices[i].id, 10);
                }
                else if (devices[i].isShade) {
                    controlShade(devices[i].id, 0);
                    broadcastDeviceState(devices[i].id, 0);
                }
                else if (devices[i].id == room + "_AC_POWER") {
                    controlAC(room, false);
                    broadcastDeviceState(devices[i].id, 0);
                }
            }
        }
    }
    else if (sceneName == "COOK") {
        // Bright lights (80%), open shades halfway
        for (int i = 0; i < deviceCount; i++) {
            if (devices[i].room == room && devices[i].enabled) {
                if (devices[i].id.indexOf("_LIGHT_") != -1) {
                    controlLight(devices[i].id, 80);
                    broadcastDeviceState(devices[i].id, 80);
                }
                else if (devices[i].isShade) {
                    controlShade(devices[i].id, 50);
                    broadcastDeviceState(devices[i].id, 50);
                }
            }
        }
    }
    else if (sceneName == "READ") {
        // Medium lights (60%), open shades 75%
        for (int i = 0; i < deviceCount; i++) {
            if (devices[i].room == room && devices[i].enabled) {
                if (devices[i].id.indexOf("_LIGHT_") != -1) {
                    controlLight(devices[i].id, 60);
                    broadcastDeviceState(devices[i].id, 60);
                }
                else if (devices[i].isShade) {
                    controlShade(devices[i].id, 75);
                    broadcastDeviceState(devices[i].id, 75);
                }
            }
        }
    }
    else if (sceneName == "GUEST_WELCOME") {
        // Special guest room scene
        if (room == "GUEST") {
            // Turn on lights to 75%, open shades 100%, set AC to comfortable temp
            for (int i = 0; i < deviceCount; i++) {
                if (devices[i].room == "GUEST" && devices[i].enabled) {
                    if (devices[i].id.indexOf("_LIGHT_") != -1) {
                        controlLight(devices[i].id, 75);
                        broadcastDeviceState(devices[i].id, 75);
                    }
                    else if (devices[i].isShade) {
                        controlShade(devices[i].id, 100);
                        broadcastDeviceState(devices[i].id, 100);
                    }
                    else if (devices[i].id == "GUEST_AC") {
                        controlAC("GUEST", true, 22); // 22°C
                        broadcastDeviceState("GUEST_AC", 22);
                        broadcastDeviceState("GUEST_AC_POWER", 1);
                    }
                }
            }
        }
    }
}

void loop() {
    webSocket.loop();
    
    // IR learning mode
    if (irrecv.decode(&results)) {
        Serial.print("📡 IR Code: 0x");
        Serial.println(results.value, HEX);
        irrecv.resume();
    }
    
    // WiFi maintenance
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Reconnecting WiFi...");
            connectToWiFi();
        }
    }
    
    // Periodic state save
    static unsigned long lastSave = 0;
    if (millis() - lastSave > 60000) {
        lastSave = millis();
        for (int i = 0; i < deviceCount; i++) {
            saveDeviceState(devices[i].id, devices[i].value, devices[i].boolean);
        }
        Serial.println("💾 States saved to flash");
    }
}