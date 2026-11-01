#ifndef WIFI_H
#define WIFI_H
// --- WiFi Configuration ---
// Set this to true to create an Access Point, false to connect to a network.
const bool WIFI_AP_MODE = false;

// If WIFI_AP_MODE is true, these will be the AP credentials.
const char* ap_ssid = "VerivR48_AP";
const char* ap_password = "1234567890";

// If WIFI_AP_MODE is false, these will be your network credentials.
const char* ssid = "VertivR48";
const char* password = "1234567890";
#endif // WIFI_H