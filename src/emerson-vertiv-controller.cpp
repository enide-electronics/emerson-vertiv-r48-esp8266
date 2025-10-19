// This sketch configures the ESP8266 to connect to a WiFi network
// (or create its own Access Point) and host a simple web server.
//
// The web page allows you to send commands to a Vertiv R48-2000e3
// power supply over CAN bus and display live measurement data.
// This version uses the mcp_can library, which you confirmed works for you.

// Include necessary libraries for ESP8266, WiFi, WebServer, and mcp_can.
#include <SPI.h>
#include <mcp_can.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// --- WiFi Configuration ---
// Set this to true to create an Access Point, false to connect to a network.
const bool WIFI_AP_MODE = false;

// If WIFI_AP_MODE is true, these will be the AP credentials.
const char* ap_ssid = "VerivR48_AP";
const char* ap_password = "1234567890";

// If WIFI_AP_MODE is false, these will be your network credentials.
const char* ssid = "yourNetworkSSID";
const char* password = "password1234";

// --- Pinout Configuration for multiple ESP controller cards ---
//
// MCP2515 Pin | ESP8266 | XIAO ESP32C6 Pin | ESP32-C6 GPIO
// ------------|---------|------------------|----------------
// MOSI        | D7      | D10              | GPIO18
// MISO        | D6      | D9               | GPIO20
// SCK         | D5      | D8               | GPIO19
// CS          | D8      | D3               | GPIO21 (Flexible, can be any available GPIO)
// INT         | D1      | D2               | GPIO2  (Flexible, can be any available GPIO)

// Define the Chip Select pin for the MCP2515 CAN module
const int SPI_CS_PIN = D8; // 15 is D8 on ESP8266 // 21; // 21 is D3 on XIAO ESP32C6

// Create an instance of the MCP_CAN library with the Chip Select pin
MCP_CAN CAN0(SPI_CS_PIN);

// Create an AsyncWebServer instance on port 80
AsyncWebServer server(80);

// --- CAN Bus Definitions ---
// These are the CAN IDs based on the working example you provided.
const long VERTIV_COMMAND_ID = 0x06080783;
const long VERTIV_READ_REQUEST_ID = 0x06000783;
const long VERTIV_RESPONSE_ID = 0x860F8003; // Updated CAN ID based on your logs
const unsigned long CAN_BUS_SPEED = 125000; // 125 Kbps

// Enum for measurement numbers
enum MeasurementType {
  OUTPUT_VOLTAGE = 0x01,
  OUTPUT_CURRENT = 0x02,
  OUTPUT_CURRENT_LIMIT = 0x03,
  TEMPERATURE = 0x04,
  SUPPLY_VOLTAGE = 0x05,
  // Define command types for confirmation
  SET_PERMANENT_VOLTAGE_CMD = 0x24,
  SET_PERMANENT_CURRENT_LIMIT_CMD = 0x19,
  SET_PERMANENT_MAX_INPUT_CURRENT_CMD = 0x1A
};

// --- Global variables to store the latest measurement data ---
// We'll initialize them with default values.
float outputVoltage = 0.0;
float outputCurrent = 0.0;
float outputCurrentLimit = 0.0;
float temperature = 0.0;
float supplyVoltage = 0.0;

// --- Variables for command delay logic ---
bool isCommandPending = false;
unsigned long commandSentTime = 0;
// Set the fixed delay for permanent commands to 45 seconds as requested
const unsigned long PERMANENT_COMMAND_DELAY = 45000; // 45 seconds

// --- HTML and JavaScript for the Web Page ---
// This entire string will be sent to the client when they access the root URL.
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Vertiv CAN Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; margin: 20px; background-color: #f0f0f0; }
    .container { max-width: 600px; margin: auto; padding: 20px; background-color: #fff; border-radius: 8px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    h1, h2 { color: #333; }
    .data-card { background-color: #e9e9e9; padding: 15px; border-radius: 6px; margin-bottom: 10px; }
    .data-card p { margin: 0; font-size: 1.2em; }
    .data-card span { font-weight: bold; color: #007BFF; }
    form { margin-top: 20px; padding: 15px; background-color: #f9f9f9; border-radius: 6px; }
    input[type="number"], button { width: 100%; padding: 10px; margin-bottom: 10px; border-radius: 4px; border: 1px solid #ccc; box-sizing: border-box; }
    button { background-color: #007BFF; color: white; border: none; cursor: pointer; font-size: 1em; }
    button:hover:not(:disabled) { background-color: #0056b3; }
    button:disabled { background-color: #ccc; cursor: not-allowed; }
    .status-message {
      background-color: #ffc107;
      color: #333;
      padding: 10px;
      border-radius: 6px;
      margin-top: 10px;
      text-align: center;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Vertiv R48-2000e3 Control</h1>
   
    <h2>Live Data</h2>
    <div class="data-card">
      <p>Output Voltage: <span id="outputVoltage">--</span> V</p>
    </div>
    <div class="data-card">
      <p>Output Current: <span id="outputCurrent">--</span> A</p>
    </div>
    <div class="data-card">
      <p>Current Limit: <span id="currentLimit">--</span> %</p>
    </div>
    <div class="data-card">
      <p>Temperature: <span id="temperature">--</span> C</p>
    </div>
    <div class="data-card">
      <p>Supply Voltage: <span id="supplyVoltage">--</span> V</p>
    </div>
    <div id="statusMessage" class="status-message" style="display: none;"></div>

    <h2>Set Permanent Voltage</h2>
    <form id="permVoltageForm">
      <input type="number" step="0.1" name="value" placeholder="e.g., 52.5" required>
      <button type="submit" class="command-button">Set Permanent Voltage</button>
    </form>

    <h2>Set Online Voltage</h2>
    <form id="onlineVoltageForm">
      <input type="number" step="0.1" name="value" placeholder="e.g., 50.0" required>
      <button type="submit" class="command-button">Set Online Voltage</button>
    </form>

    <h2>Set Permanent Current Limit</h2>
    <form id="permCurrentForm">
      <input type="number" step="0.01" name="value" placeholder="e.g., 0.5 (for 50%)" required>
      <button type="submit" class="command-button">Set Permanent Current Limit</button>
    </form>

    <h2>Set Online Current Limit</h2>
    <form id="onlineCurrentForm">
      <input type="number" step="0.01" name="value" placeholder="e.g., 0.5 (for 50%)" required>
      <button type="submit" class="command-button">Set Online Current Limit</button>
    </form>

    <h2>Set Diesel Input Current Limit</h2>
    <form id="dieselCurrentForm">
      <input type="number" step="0.01" name="value" placeholder="e.g., 5.21 (for 1200W)" required>
      <button type="submit" class="command-button">Set Diesel Input Current Limit</button>
    </form>

    <h2>Set Fan Speed</h2>
    <form id="fanSpeedForm">
      <button type="submit" name="speed" value="auto" class="command-button">Auto</button>
      <button type="submit" name="speed" value="full" class="command-button">Full Speed</button>
    </form>

    <h2>Walk-in Control</h2>
    <form id="walkInStateForm">
      <button type="submit" name="state" value="on" class="command-button">Walk-in On</button>
      <button type="submit" name="state" value="off" class="command-button">Walk-in Off</button>
    </form>
    <form id="walkInTimeForm">
      <input type="number" step="1" name="value" placeholder="e.g., 10 (seconds)" required>
      <button type="submit" class="command-button">Set Walk-in Time</button>
    </form>

  </div>

  <script>
    // Function to update the data display and button states
    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('outputVoltage').innerText = data.outputVoltage.toFixed(2);
          document.getElementById('outputCurrent').innerText = data.outputCurrent.toFixed(2);
          document.getElementById('currentLimit').innerText = (data.outputCurrentLimit * 100).toFixed(2);
          document.getElementById('temperature').innerText = data.temperature.toFixed(2);
          document.getElementById('supplyVoltage').innerText = data.supplyVoltage.toFixed(2);
         
          const buttons = document.querySelectorAll('.command-button');
          const messageBox = document.getElementById('statusMessage');

          if (data.isCommandPending) {
            buttons.forEach(button => button.disabled = true);
            messageBox.style.display = 'block';
            messageBox.innerText = 'Waiting for command to be processed... ' + data.remainingTime + ' seconds remaining';
          } else {
            buttons.forEach(button => button.disabled = false);
            messageBox.style.display = 'none';
          }
        })
        .catch(error => console.error('Error fetching data:', error));
    }
   
    // Set up form submission handlers
    document.getElementById('permVoltageForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const value = this.elements.value.value;
      fetch('/set_perm_v', { method: 'POST', body: 'value=' + value, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });

    document.getElementById('onlineVoltageForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const value = this.elements.value.value;
      fetch('/set_online_v', { method: 'POST', body: 'value=' + value, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });

    document.getElementById('permCurrentForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const value = this.elements.value.value;
      fetch('/set_perm_c', { method: 'POST', body: 'value=' + value, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });

    document.getElementById('onlineCurrentForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const value = this.elements.value.value;
      fetch('/set_online_c', { method: 'POST', body: 'value=' + value, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });

    document.getElementById('dieselCurrentForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const value = this.elements.value.value;
      fetch('/set_diesel_input_c', { method: 'POST', body: 'value=' + value, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });
   
    // New handler for fan speed form submission
    document.getElementById('fanSpeedForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const speed = event.submitter.value;
      fetch('/set_fan_speed', { method: 'POST', body: 'speed=' + speed, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });

    // New handler for walk-in state form submission (on/off)
    document.getElementById('walkInStateForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const state = event.submitter.value;
      fetch('/set_walk_in', { method: 'POST', body: 'state=' + state, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });

    // New form handler for walk-in time using a POST request
    document.getElementById('walkInTimeForm').addEventListener('submit', function(event) {
      event.preventDefault();
      const value = this.elements.value.value;
      fetch('/set_walk_in_time', { method: 'POST', body: 'value=' + value, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
        .then(response => response.text())
        .then(text => alert(text))
        .catch(error => console.error('Error:', error));
    });

    // Request data every 1 second to keep the countdown live
    setInterval(updateData, 1000);
    // Initial data fetch on page load
    updateData();
  </script>
</body>
</html>
)rawliteral";

// --- Function Prototypes ---
void setVertivVoltagePermanent(float voltage);
void setVertivVoltageOnline(float voltage);
void setVertivCurrentPermanent(float currentPercentage);
void setVertivCurrentOnline(float currentPercentage);
void setVertivMaxInputCurrent(float current);
void readVertivSetting(byte measurementNo);
void processIncomingCanMessages();
void setVertivFanSpeed(bool fullSpeed);
void setVertivWalkIn(bool on);
void setVertivWalkInTime(float seconds);

// Helper function to convert 4 bytes (big-endian) to a float
float bytesToFloat(byte b[4]) {
  union { float f; byte b[4]; } converter;
  // Copy bytes with explicit order, assuming CAN data is big-endian
  converter.b[3] = b[0];
  converter.b[2] = b[1];
  converter.b[1] = b[2];
  converter.b[0] = b[3];
  return converter.f;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D1, INPUT); // MCP2515 INT pin as input (unused in this sketch)
  digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off initially to indicate offline state

  Serial.begin(115200);
  Serial.println("ESP32 Web Server for Vertiv CAN Control");

  // Initialize MCP2515 running at 8MHz with a baudrate of 125kb/s and the masks and filters disabled.
  if(CAN0.begin(MCP_ANY, CAN_125KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else {
    Serial.println("Error Initializing MCP2515...");
    // blink default LED 3 times to indicate error and restart
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(250);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      
      yield();
    }

    delay(1000);

    ESP.restart();
  }
 
  // The Adafruit library's begin() function handles setting the bitrate.
  Serial.println("CAN init OK!");
 
  // Set to normal mode to allow messages to be transmitted.
  CAN0.setMode(MCP_NORMAL);

  // --- WiFi Setup ---
  if (WIFI_AP_MODE) {
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("Access Point created! IP Address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi! IP Address: ");
    Serial.println(WiFi.localIP());
  }

  digitalWrite(LED_BUILTIN, LOW); // Turn the LED on to indicate we are now online
 
  // --- Web Server Routes Setup ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", html_page);
  });
 
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String jsonResponse = "{\"outputVoltage\":";
    jsonResponse += String(outputVoltage, 2);
    jsonResponse += ",\"outputCurrent\":";
    jsonResponse += String(outputCurrent, 2);
    jsonResponse += ",\"outputCurrentLimit\":";
    jsonResponse += String(outputCurrentLimit, 2);
    jsonResponse += ",\"temperature\":";
    jsonResponse += String(temperature, 2);
    jsonResponse += ",\"supplyVoltage\":";
    jsonResponse += String(supplyVoltage, 2);
    jsonResponse += ",\"isCommandPending\":";
    jsonResponse += isCommandPending ? "true" : "false";
    jsonResponse += ",\"remainingTime\":";
   
    // Calculate the remaining time in seconds for the countdown
    if (isCommandPending) {
        unsigned long elapsedTime = millis() - commandSentTime;
        long remaining = (long)PERMANENT_COMMAND_DELAY - (long)elapsedTime;
        jsonResponse += String(remaining > 0 ? remaining / 1000 : 0);
    } else {
        jsonResponse += "0";
    }
    jsonResponse += "}";

    request->send(200, "application/json", jsonResponse);
  });
 
  server.on("/set_perm_v", HTTP_POST, [](AsyncWebServerRequest *request){
    float voltage = request->getParam(0)->value().toFloat();
    if (voltage > 0) {
      setVertivVoltagePermanent(voltage);
      // Dynamic alert message
      String message = "Command sent: set_perm_v " + String(voltage) + ". Please wait " + String(PERMANENT_COMMAND_DELAY / 1000) + " seconds for confirmation.";
      request->send(200, "text/plain", message);
    } else {
      request->send(400, "text/plain", "Invalid voltage value.");
    }
  });

  server.on("/set_online_v", HTTP_POST, [](AsyncWebServerRequest *request){
    float voltage = request->getParam(0)->value().toFloat();
    if (voltage > 0) {
      setVertivVoltageOnline(voltage);
      request->send(200, "text/plain", "Command sent: set_online_v " + String(voltage));
    } else {
      request->send(400, "text/plain", "Invalid voltage value.");
    }
  });

  server.on("/set_perm_c", HTTP_POST, [](AsyncWebServerRequest *request){
    float currentPercentage = request->getParam(0)->value().toFloat();
    if (currentPercentage >= 0.1 && currentPercentage <= 1.21) {
      setVertivCurrentPermanent(currentPercentage);
      // Dynamic alert message
      String message = "Command sent: set_perm_c " + String(currentPercentage) + ". Please wait " + String(PERMANENT_COMMAND_DELAY / 1000) + " seconds for confirmation.";
      request->send(200, "text/plain", message);
    } else {
      request->send(400, "text/plain", "Invalid current percentage.");
    }
  });

  server.on("/set_online_c", HTTP_POST, [](AsyncWebServerRequest *request){
    float currentPercentage = request->getParam(0)->value().toFloat();
    if (currentPercentage >= 0.1 && currentPercentage <= 1.21) {
      setVertivCurrentOnline(currentPercentage);
      request->send(200, "text/plain", "Command sent: set_online_c " + String(currentPercentage));
    } else {
      request->send(400, "text/plain", "Invalid current percentage.");
    }
  });

  server.on("/set_diesel_input_c", HTTP_POST, [](AsyncWebServerRequest *request){
    float current = request->getParam(0)->value().toFloat();
    if (current >= 3 && current <= 13) {
      setVertivMaxInputCurrent(current);
      request->send(200, "text/plain", "Command sent: set_diesel_input_c " + String(current));
    } else {
      request->send(400, "text/plain", "Invalid current, valid values between 3 and 13.");
    }
  });
 
  server.on("/set_fan_speed", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("speed", true)) {
        String speed = request->getParam("speed", true)->value();
        if (speed == "full") {
          setVertivFanSpeed(true);
          String message = "Command sent: set fan to full speed. Please wait " + String(PERMANENT_COMMAND_DELAY / 1000) + " seconds for confirmation.";
          request->send(200, "text/plain", message);
        } else if (speed == "auto") {
          setVertivFanSpeed(false);
          String message = "Command sent: set fan to auto. Please wait " + String(PERMANENT_COMMAND_DELAY / 1000) + " seconds for confirmation.";
          request->send(200, "text/plain", message);
        } else {
          request->send(400, "text/plain", "Invalid fan speed command.");
        }
    } else {
        request->send(400, "text/plain", "Missing fan speed parameter.");
    }
  });
 
  server.on("/set_walk_in", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("state", true)) {
        String state = request->getParam("state", true)->value();
        if (state == "on") {
          setVertivWalkIn(true);
          String message = "Command sent: set walk-in to ON. Please wait " + String(PERMANENT_COMMAND_DELAY / 1000) + " seconds for confirmation.";
          request->send(200, "text/plain", message);
        } else if (state == "off") {
          setVertivWalkIn(false);
          String message = "Command sent: set walk-in to OFF. Please wait " + String(PERMANENT_COMMAND_DELAY / 1000) + " seconds for confirmation.";
          request->send(200, "text/plain", message);
        } else {
          request->send(400, "text/plain", "Invalid walk-in state command.");
        }
    } else {
        request->send(400, "text/plain", "Missing walk-in state parameter.");
    }
  });
 
  server.on("/set_walk_in_time", HTTP_POST, [](AsyncWebServerRequest *request){
    float seconds = request->getParam(0)->value().toFloat();
    if (seconds >= 0) {
      setVertivWalkInTime(seconds);
      String message = "Command sent: set walk-in time to " + String(seconds) + ". Please wait " + String(PERMANENT_COMMAND_DELAY / 1000) + " seconds for confirmation.";
      request->send(200, "text/plain", message);
    } else {
      request->send(400, "text/plain", "Invalid walk-in time value.");
    }
  });
 
  // Start the web server
  server.begin();
 
  // Initial request for data from the power supply
  readVertivSetting(OUTPUT_VOLTAGE);
  readVertivSetting(OUTPUT_CURRENT);
  readVertivSetting(OUTPUT_CURRENT_LIMIT);
  readVertivSetting(TEMPERATURE);
  readVertivSetting(SUPPLY_VOLTAGE);
}

// State machine variables for sequenced reading
unsigned long lastRequestTime = 0;
int readState = 0; // 0: Idle, 1: Request sent, 2: Waiting for 1s delay
const unsigned long POLLING_INTERVAL = 5000;
const unsigned long CURRENT_LIMIT_DELAY = 1000; // 1 second delay

void loop() {
  // Check for incoming CAN messages from the power supply
  processIncomingCanMessages();
 
  // Sequentially request new data if no command is pending
  if (!isCommandPending) {
    switch (readState) {
      case 0: // Check if it's time to start the sequence
        if (millis() - lastRequestTime > POLLING_INTERVAL) {
          readVertivSetting(OUTPUT_VOLTAGE);
          readState = 1;
          lastRequestTime = millis();
        }
        break;
      case 1: // Request OUTPUT_CURRENT, then set a 1-second delay
        if (millis() - lastRequestTime > 100) { // Small delay between requests
          readVertivSetting(OUTPUT_CURRENT);
          readVertivSetting(OUTPUT_CURRENT_LIMIT);
          readState = 2;
          lastRequestTime = millis();
        }
        break;
      case 2: // Wait for 1 second after requesting current limit
        if (millis() - lastRequestTime > CURRENT_LIMIT_DELAY) {
          readVertivSetting(TEMPERATURE);
          readVertivSetting(SUPPLY_VOLTAGE);
          readState = 0; // Reset state for the next cycle
          lastRequestTime = millis(); // Reset main polling timer
        }
        break;
    }
  }

  // Check if the fixed delay for the permanent command has elapsed
  if (isCommandPending && millis() - commandSentTime > PERMANENT_COMMAND_DELAY) {
    Serial.println("45-second command delay complete. Resuming normal operation.");
    isCommandPending = false;
    readState = 0; // Reset the read state machine
  }
}

/**
 * @brief Sends a CAN message to set the output voltage of the Vertiv R48-2000e3 permanently.
 * @param voltage The desired voltage in Volts (float).
 *
 * This function constructs the 8-byte data payload:
 * [0x03, 0xF0, 0x00, 0x24, (4 bytes IEEE 754 float)]
 * The float voltage is converted to its 4-byte IEEE 754 single-precision representation.
 */
void setVertivVoltagePermanent(float voltage) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = SET_PERMANENT_VOLTAGE_CMD;
  union FloatBytes { float f; byte b[4]; } converter;
  converter.f = voltage;
  data[4] = converter.b[3];
  data[5] = converter.b[2];
  data[6] = converter.b[1];
  data[7] = converter.b[0];

  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent permanent voltage command. Value: "); Serial.println(voltage);
   
    // Set command pending flag and start the timer
    isCommandPending = true;
    commandSentTime = millis();

  } else {
    Serial.println("Error sending voltage command.");
  }
}

/**
 * @brief Sends a CAN message to set the output voltage of the Vertiv R48-2000e3 temporarily (online).
 * @param voltage The desired voltage in Volts (float).
 *
 * This function constructs the 8-byte data payload:
 * [0x03, 0xF0, 0x00, 0x21, (4 bytes IEEE 754 float)]
 */
void setVertivVoltageOnline(float voltage) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = 0x21;
  union FloatBytes { float f; byte b[4]; } converter;
  converter.f = voltage;
  data[4] = converter.b[3];
  data[5] = converter.b[2];
  data[6] = converter.b[1];
  data[7] = converter.b[0];
 
  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent online voltage command. Value: "); Serial.println(voltage);
  } else {
    Serial.println("Error sending voltage command.");
  }
}

/**
 * @brief Sends a CAN message to set the output current limit of the Vertiv R48-2000e3 permanently.
 * @param currentPercentage The desired current as a percentage of rated value (e.g., 0.1 for 10%, 1.21 for 121%).
 *
 * This function constructs the 8-byte data payload:
 * [0x03, 0xF0, 0x00, 0x19, (4 bytes IEEE 754 float)]
 */
void setVertivCurrentPermanent(float currentPercentage) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = SET_PERMANENT_CURRENT_LIMIT_CMD;
  union FloatBytes { float f; byte b[4]; } converter;
  converter.f = currentPercentage;
  data[4] = converter.b[3];
  data[5] = converter.b[2];
  data[6] = converter.b[1];
  data[7] = converter.b[0];
 
  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent permanent current limit command. Value: "); Serial.println(currentPercentage);
   
    // Set command pending flag and start the timer
    isCommandPending = true;
    commandSentTime = millis();
   
  } else {
    Serial.println("Error sending current command.");
  }
}

/**
 * @brief Sends a CAN message to set the output current limit of the Vertiv R48-2000e3 online.
 * @param currentPercentage The desired current as a percentage of rated value (e.g., 0.1 for 10%, 1.21 for 121%).
 *
 * This function constructs the 8-byte data payload:
 * [0x03, 0xF0, 0x00, 0x22, (4 bytes IEEE 754 float)]
 */
void setVertivCurrentOnline(float currentPercentage) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = 0x22;
  union FloatBytes { float f; byte b[4]; } converter;
  converter.f = currentPercentage;
  data[4] = converter.b[3];
  data[5] = converter.b[2];
  data[6] = converter.b[1];
  data[7] = converter.b[0];
 
  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent online current limit command. Value: "); Serial.println(currentPercentage);
  } else {
    Serial.println("Error sending current command.");
  }
}

/**
 * @brief Sends a CAN message to set the (Diesel power limit) max input current of the Vertiv R48-2000e3 permanently.
 * @param current The desired current in Amps (float).
 *
 * This function constructs the 8-byte data payload:
 * [0x03, 0xF0, 0x00, 0x1A, (4 bytes IEEE 754 float)]
 * The float current is converted to its 4-byte IEEE 754 single-precision representation.
 */
void setVertivMaxInputCurrent(float current) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = SET_PERMANENT_MAX_INPUT_CURRENT_CMD;
  union FloatBytes { float f; byte b[4]; } converter;
  converter.f = current;
  data[4] = converter.b[3];
  data[5] = converter.b[2];
  data[6] = converter.b[1];
  data[7] = converter.b[0];

  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent (Diesel) AC input current limit command. Value: "); Serial.println(current);
   
    // Set command pending flag and start the timer
    isCommandPending = true;
    commandSentTime = millis();

  } else {
    Serial.println("Error sending (Diesel) AC input current limit command.");
  }
}

/**
 * @brief Sends a CAN message to request a specific measurement from the Vertiv R48-2000e3.
 * @param measurementNo The measurement number to request (e.g., 0x01 for output voltage).
 *
 * Request format: Send to 0x06000783 => [0x01, 0xF0, 0x00, xx, 0x00, 0x00, 0x00, 0x00]
 */
void readVertivSetting(byte measurementNo) {
  byte data[8];
  data[0] = 0x01;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = measurementNo;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;

  byte sndStat = CAN0.sendMsgBuf(VERTIV_READ_REQUEST_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent read request command. Measurement #: "); Serial.println(measurementNo, HEX);
  } else {
    Serial.println("Error requesting measurement.");
  }
}

/**
 * @brief Sends a CAN message to set the fan speed.
 * @param fullSpeed A boolean flag: true for full speed, false for auto.
 */
void setVertivFanSpeed(bool fullSpeed) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = 0x33;
  data[4] = fullSpeed ? 0x01 : 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
 
  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent fan speed command. Value: ");
    Serial.println(fullSpeed ? "Full Speed" : "Auto");
   
    // Set command pending flag and start the timer
    isCommandPending = true;
    commandSentTime = millis();

  } else {
    Serial.println("Error sending fan speed command.");
  }
}

/**
 * @brief Sends a CAN message to enable or disable the walk-in feature.
 * @param on A boolean flag: true to enable walk-in, false to disable.
 */
void setVertivWalkIn(bool on) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = 0x32;
  data[4] = on ? 0x01 : 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
 
  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent walk-in command. Value: ");
    Serial.println(on ? "On" : "Off");
   
    // Set command pending flag and start the timer
    isCommandPending = true;
    commandSentTime = millis();

  } else {
    Serial.println("Error sending walk-in command.");
  }
}

/**
 * @brief Sends a CAN message to set the walk-in ramp-up time.
 * @param seconds The desired ramp-up time in seconds (float).
 */
void setVertivWalkInTime(float seconds) {
  byte data[8];
  data[0] = 0x03;
  data[1] = 0xF0;
  data[2] = 0x00;
  data[3] = 0x29;
  union FloatBytes { float f; byte b[4]; } converter;
  converter.f = seconds;
  data[4] = converter.b[3];
  data[5] = converter.b[2];
  data[6] = converter.b[1];
  data[7] = converter.b[0];
 
  byte sndStat = CAN0.sendMsgBuf(VERTIV_COMMAND_ID, 1, 8, data);
  if (sndStat == CAN_OK) {
    Serial.print("Sent walk-in time command. Value: ");
    Serial.println(seconds);
   
    // Set command pending flag and start the timer
    isCommandPending = true;
    commandSentTime = millis();

  } else {
    Serial.println("Error sending walk-in time command.");
  }
}

/**
 * @brief Checks for and processes incoming CAN messages from the Vertiv R48-2000e3.
 */
void processIncomingCanMessages() {
  long unsigned int rxId;
  unsigned char len;
  unsigned char rxBuf[8];
 
  // Check if a message has been received
  if(CAN0.checkReceive() == CAN_MSGAVAIL) {
    // Read the message
    CAN0.readMsgBuf(&rxId, &len, rxBuf);
   
    // Log every received message to the Serial Monitor
    Serial.print("RX ID: 0x");
    if (rxId < 0x10000000) Serial.print("0"); // Manual padding for 32-bit ID
    Serial.print(rxId, HEX);
    Serial.print(" Length: ");
    Serial.print(len);
    Serial.print(" Data: ");
    for (int i = 0; i < len; i++) {
      Serial.print("0x");
      if (rxBuf[i] < 0x10) Serial.print("0");
      Serial.print(rxBuf[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
   
    // Parse the message if it's a standard Vertiv response
    if (rxId == (unsigned long) VERTIV_RESPONSE_ID && len == 8 && rxBuf[0] == 0x41 && rxBuf[1] == 0xF0 && rxBuf[2] == 0x00) {
      byte receivedMeasurementNo = rxBuf[3];
      // Create a temporary buffer for the float bytes from the CAN message
      byte floatBytes[4] = {rxBuf[4], rxBuf[5], rxBuf[6], rxBuf[7]};
      float receivedValue = bytesToFloat(floatBytes);
     
      // Log the converted value to the serial monitor for debugging
      Serial.printf("Vertiv response ID = value: 0x%02x = %.2f\n", receivedMeasurementNo, receivedValue);

      // Update global variables
      switch (receivedMeasurementNo) {
        case OUTPUT_VOLTAGE: outputVoltage = receivedValue; break;
        case OUTPUT_CURRENT: outputCurrent = receivedValue; break;
        case OUTPUT_CURRENT_LIMIT: outputCurrentLimit = receivedValue; break;
        case TEMPERATURE: temperature = receivedValue; break;
        case SUPPLY_VOLTAGE: supplyVoltage = receivedValue; break;
        default: 
            Serial.printf("Unknown ID 0x%02x = %.2f\n", receivedMeasurementNo, receivedValue);
            break;
      }
    }
  }
}

