#include <WiFi.h>
#include <HTTPClient.h>
#include "mbedtls/gcm.h"
#include "config.h"   // WIFI_SSID, WIFI_PASSWORD, PSK_HEX (64 hex chars)

const int RX_PIN = 16;            // from Uno via divider
const int TX_PIN = 17;            // unused, leave unconnected
const int SERVER_PORT = 5000;

uint8_t key[32];
uint32_t seq;
unsigned long lastWifiTry = 0;

bool hexToBytes(const char* hex, uint8_t* out, size_t n) {
  if (strlen(hex) != n * 2) return false;
  for (size_t i = 0; i < n; i++) {
    char b[3] = {hex[2 * i], hex[2 * i + 1], 0};
    out[i] = (uint8_t)strtol(b, NULL, 16);
  }
  return true;
}

void appendHex(String& s, const uint8_t* d, size_t n) {
  const char* h = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    s += h[d[i] >> 4];
    s += h[d[i] & 15];
  }
}

// returns hex of: nonce(12) | ciphertext | tag(16), or "" on error
String encryptToHex(const String& plain) {
  uint8_t nonce[12], tag[16], ct[128];
  size_t len = plain.length();
  if (len == 0 || len > sizeof(ct)) return "";
  esp_fill_random(nonce, sizeof(nonce));   // fresh nonce every message

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);
  int rc = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (rc == 0) {
    rc = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, len,
           nonce, sizeof(nonce), NULL, 0,
           (const uint8_t*)plain.c_str(), ct, sizeof(tag), tag);
  }
  mbedtls_gcm_free(&ctx);
  if (rc != 0) return "";

  String out;
  out.reserve(2 * (12 + len + 16));
  appendHex(out, nonce, 12);
  appendHex(out, ct, len);
  appendHex(out, tag, 16);
  return out;
}

bool connectWifi(unsigned long timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < timeoutMs) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool sendSOS(const String& line) {
  // seq inside the encrypted data lets the server reject replays
  String hex = encryptToHex(line + "," + String(seq++));
  if (hex.length() == 0) { Serial.println("[-] Encrypt failed"); return false; }
  String body = "{\"d\":\"" + hex + "\"}";

  for (int attempt = 1; attempt <= 3; attempt++) {
    if (WiFi.status() != WL_CONNECTED && !connectWifi(8000)) {
      Serial.println("[-] No Wi-Fi, retrying");
      continue;
    }
    // phone is the hotspot AND the server, so server = gateway
    String url = "http://" + WiFi.gatewayIP().toString() + ":" +
                 String(SERVER_PORT) + "/sos";
    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);
    http.end();
    if (code == 200) { Serial.println("[+] SOS sent"); return true; }
    Serial.println("[-] Attempt " + String(attempt) + " failed: " + String(code));
    delay(1000);
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial2.setTimeout(200);

  if (!hexToBytes(PSK_HEX, key, 32)) {
    Serial.println("PSK_HEX must be 64 hex characters");
    while (true) delay(1000);
  }
  seq = esp_random();               // random start so a reboot doesn't repeat numbers
  connectWifi(15000);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiTry > 5000) {
    lastWifiTry = millis();
    WiFi.reconnect();
  }

  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();
    if (line.startsWith("SOS,")) {          // ignore noise
      Serial.println("[!] From Uno: " + line);
      sendSOS(line);
    }
  }
}