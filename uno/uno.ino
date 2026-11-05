#include <SoftwareSerial.h>
#include <TinyGPS++.h>

SoftwareSerial gpsSerial(4, 3);   // RX=D4 (GPS TX), TX=D3 unused
SoftwareSerial espSerial(5, 6);   // RX=D5 unused, TX=D6 -> divider -> ESP32 GPIO16
TinyGPSPlus gps;

const int sosButton = 7;
const unsigned long DEBOUNCE_MS = 50;

bool lastReading = HIGH;
bool stableState = HIGH;
unsigned long lastChange = 0;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  gpsSerial.begin(9600);
  gpsSerial.listen();             // GPS must be the active listener
  pinMode(sosButton, INPUT_PULLUP);
  Serial.println(F("SOS node started"));
}

void sendSOS() {
  bool fresh = gps.location.isValid() && gps.location.age() < 60000;
  String msg;
  if (fresh) {
    msg = "SOS," + String(gps.location.lat(), 6) + "," +
          String(gps.location.lng(), 6) + "," +
          String(gps.location.age() / 1000);
  } else {
    msg = "SOS,NOFIX";
  }
  espSerial.println(msg);
  Serial.println(msg);            // debug on USB
}

void loop() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  bool reading = digitalRead(sosButton);
  if (reading != lastReading) lastChange = millis();
  lastReading = reading;

  if (millis() - lastChange > DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) sendSOS();   // fires once per press
  }

  if (millis() > 10000 && gps.charsProcessed() < 10) {
    Serial.println(F("No GPS data - check wiring"));
    delay(2000);
  }
}