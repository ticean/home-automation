/*
  Arduino sketch
  Creates a web server with simple HTTP JSON interface for controlling digital
  outputs on an ESP8266.

  The outputs are intended to control an AC relay which in turn will power
  outdoor LED lights.

  Notes:
  The ESP8266 HTTP update server library is included for OTA updates, but this
  is experimental. I haven't been able to get it to work yet. Library
  instructions:

      To upload through terminal you can use:

        curl -F "image=@firmware.bin" thing.local/update
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

const char* host = "thing";
const char* ssid = "<ssid>";
const char* password = "<password>";

const int LED_PIN = 5;
const int SLOT1 = 12;
const int SLOT2 = 13;
const int SLOT3 = 15;
const int SLOT4 = 16;

// Driving an SainSmart 5V relay board that activates on LOW.
// We'll hide this logical flip-flop externally.
const byte RELAY_OFF = HIGH;
const byte RELAY_ON = LOW;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

byte relayRead(int slot) {
  ~digitalRead(slot);
}

// Blinks the LED and finally resets back to original value.
void blinkLED(int interval, int count) {
  byte original = digitalRead(LED_PIN);
  byte current = original;
  for (int i = 0; i < count; i++) {
    current = (current == HIGH) ? LOW : HIGH;
    digitalWrite(LED_PIN, current);
    delay(100);
  }

  digitalWrite(LED_PIN, original);
}

// Enables LED if any slot is in high state.
void enableLEDOnActivity() {
  byte slot1Value = relayRead(SLOT1);
  byte slot2Value = relayRead(SLOT2);
  byte slot3Value = relayRead(SLOT3);
  byte slot4Value = relayRead(SLOT4);

  // TODO: Is the LED reversed?
  if (slot1Value || slot2Value || slot3Value || slot4Value) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

// Build a JSON string representing the status of all slots.
String getStatusJSON() {
  String body = "";
  body += "{";
  body += "\"slot1\": " + String(relayRead(SLOT1)) + ",";
  body += "\"slot2\": " + String(relayRead(SLOT2)) + ",";
  body += "\"slot3\": " + String(relayRead(SLOT3)) + ",";
  body += "\"slot4\": " + String(relayRead(SLOT4));
  body += "}";
}

void handleRoot() {
  String body = "{\"message:\": \"hello from esp8266!\"}";
  server.send(200, "application/javascript", body);
}

void handleNotFound(){
  String body = "{\"message\": \"File Not Found\"}";
  server.send(404, "application/json", body);
}

void handleStatus(){
  String body = getStatusJSON();
  server.send(200, "application/json", body);
}

void handleSetSlotValue(int pin, byte value) {
  Serial.printf("Set slot value. pin=%s, value=%s logical_value=%s", pin, value, ~value);
  digitalWrite(pin, value);
  enableLEDOnActivity();
  server.send(200, "application/json", getStatusJSON());
}


void setupHardware()
{
  Serial.println("Running setupHardware...");
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(SLOT1,   OUTPUT);
  pinMode(SLOT2,   OUTPUT);
  pinMode(SLOT3,   OUTPUT);
  pinMode(SLOT4,   OUTPUT);

  digitalWrite(LED_PIN, HIGH);
  digitalWrite(SLOT1,   RELAY_OFF);
  digitalWrite(SLOT2,   RELAY_OFF);
  digitalWrite(SLOT3,   RELAY_OFF);
  digitalWrite(SLOT4,   RELAY_OFF);
}

void setupWiFi() {
  Serial.println("Setting up WiFi. ssid=" + String(ssid));
  byte ledStatus = LOW;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Blink the LED while waiting for connection.
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, ledStatus); // Write LED high/low
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;
    delay(100);
  }

  Serial.println("WiFi connected. ip=\"" + String(WiFi.localIP()) + "\"");
  digitalWrite(LED_PIN, HIGH);
}

void setupMDNS() {
  if (!MDNS.begin("thing")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
}

// Set up the web routes.
void setupWebserverRoutes() {
  server.on("/", handleRoot);
  server.on("/slot1/0", [](){ handleSetSlotValue(SLOT1, RELAY_OFF);  });
  server.on("/slot1/1", [](){ handleSetSlotValue(SLOT1, RELAY_ON);   });
  server.on("/slot2/0", [](){ handleSetSlotValue(SLOT2, RELAY_OFF);  });
  server.on("/slot2/1", [](){ handleSetSlotValue(SLOT2, RELAY_ON);   });
  server.on("/slot3/0", [](){ handleSetSlotValue(SLOT3, RELAY_OFF);  });
  server.on("/slot3/1", [](){ handleSetSlotValue(SLOT3, RELAY_ON);   });
  server.on("/slot4/0", [](){ handleSetSlotValue(SLOT4, RELAY_OFF);  });
  server.on("/slot4/1", [](){ handleSetSlotValue(SLOT4, RELAY_ON);   });
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
}

void setup(void){
  Serial.println();
  Serial.println("Running setup...");
  setupHardware();
  setupWiFi();
  setupMDNS();

  httpUpdater.setup(&server);
  server.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);

  setupWebserverRoutes();

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void){
  server.handleClient();
  delay(1);
}
