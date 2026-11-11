#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

// GPS module: GPS TX -> Uno pin 4, GPS RX -> Uno pin 3
SoftwareSerial gpsSerial(4, 3);
// Link to ESP32: Uno pin 11 (TX) -> [divider] -> ESP32 GPIO16 (RX2)
// Pin 10 (RX) is unused, only needed by the library
SoftwareSerial espSerial(10, 11);

TinyGPSPlus gps;

const int BUTTON_PIN = 7;          // other leg of button to GND
unsigned long lastPress = 0;

void setup() {
  Serial.begin(9600);              // USB (Serial Monitor)
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  espSerial.begin(9600);
  gpsSerial.begin(9600);
  gpsSerial.listen();              // only one SoftwareSerial can listen: keep it on GPS

  Serial.println("Uno ready");
}

void sendSOS() {
  String msg;
  if (gps.location.isValid()) {
    msg = "SOS," + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  } else {
    msg = "SOS,NOFIX";             // still send the alert even without a GPS fix
  }
  espSerial.println(msg);          // goes to ESP32
  Serial.println("Sent: " + msg);  // also show on laptop
}

void loop() {
  // keep feeding GPS data
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // button pressed (LOW) with 1 second gap to avoid repeats/bounce
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPress > 1000) {
    lastPress = millis();
    sendSOS();
  }
}
