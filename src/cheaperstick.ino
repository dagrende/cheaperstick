#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <ArduinoJson.h>

// Connect to the WiFi
typedef struct {
  long magicNumber = 77824591l;
  char ssid[50] = "";
  char password[50] = "";
  char mqtt_server[50] = "192.168.0.33";
} Prefs;

Prefs prefs;
Prefs defaultPrefs;

int mqttTriesLeft = 5;
int wifiTriesLeft = 6;

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer webServer(80);
RCSwitch mySwitch = RCSwitch();

const byte ledPin = LED_BUILTIN; // LED pin on Wemos d1 mini

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  if (strcmp(topic, "esp/2/proove/send") == 0) {
    char code[100];
    if (length < sizeof code) {
      strncpy(code, (char *)payload, length);
      code[length] = 0;
      Serial.print("esp/2/proove/send: ");
      Serial.println(code);
      mySwitch.send(code);
    }
  } else if (strcmp(topic, "esp/2/setProtocol") == 0) {
    int protocolNo = ((char)payload[0]) - '0';
    Serial.println(protocolNo);
    if (1 <= protocolNo && protocolNo <= 6) {
      Serial.print("changed to protocol no "); Serial.println(protocolNo);
      mySwitch.setProtocol(protocolNo);
    }
  }
  Serial.println();
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting ("); Serial.print(mqttTriesLeft); Serial.print(") MQTT connection to "); Serial.print(prefs.mqtt_server); Serial.print("...");
    // Attempt to connect
    if (client.connect("ESP8266 Client")) {
      Serial.println("connected");
      // ... and subscribe to topic
      client.subscribe("esp/2/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

  // void handleNotFound() {
  //   String query = server.uri();
  //   if (query.startsWith("/prefs/")) {
  //   }
  // }

  void setStringFromArg(char *dest, size_t destSize, char *argName, boolean &anySet) {
    if (webServer.hasArg(argName)) {
      strncpy(dest, webServer.arg(argName).c_str(), destSize);
      dest[destSize - 1] = 0;
      anySet = true;
    }
  }

  void setup() {
    Serial.begin(115200);

    EEPROM.get(0, prefs);
    if (prefs.magicNumber != defaultPrefs.magicNumber) {
      // EEPROM was empty - init with default prefs
      Serial.println("empty prefs - setting defaults");
      memcpy(&prefs, &defaultPrefs, sizeof prefs);
    }

    delay(10);

    Serial.print("Connecting to ");
    Serial.println(prefs.ssid);
    WiFi.begin(prefs.ssid, prefs.password);
    while (WiFi.status() != WL_CONNECTED && wifiTriesLeft > 0) {
      delay(500);
      Serial.print(".");
      wifiTriesLeft--;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.print("WiFi connected as "); Serial.println(WiFi.localIP());

      client.setServer(prefs.mqtt_server, 1883);
      client.setCallback(callback);
    } else {
      Serial.println("changing to Access Point mode ");
      WiFi.mode(WIFI_AP);
      WiFi.softAP("cheaperstick", "bettertoo");
    }

    // get all prefs except password as json
    webServer.on("/prefs", HTTP_GET, [](){
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& jsonObject = jsonBuffer.createObject();
      jsonObject["ssid"] = prefs.ssid;
      jsonObject["mqtt_server"] = prefs.mqtt_server;
      char printBuf[200];
      jsonObject.printTo(printBuf, sizeof printBuf);
      webServer.send(200, "text/plain", printBuf);
    });

    // set POSTed prefs and save if EEPROM
    webServer.on("/prefs", HTTP_POST, [](){
      boolean anySet = false;
      setStringFromArg(prefs.ssid, sizeof prefs.ssid, "ssid", anySet);
      setStringFromArg(prefs.password, sizeof prefs.password, "password", anySet);
      setStringFromArg(prefs.mqtt_server, sizeof prefs.mqtt_server, "mqtt_server", anySet);
      if (anySet) {
        Serial.println("saving prefs");
        EEPROM.put(0, prefs);
      }
      webServer.send(200);
    });

    webServer.begin();

    // webServer.onNotFound(handleNotFound);

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    mySwitch.enableReceive(digitalPinToInterrupt(4));
    mySwitch.enableTransmit(14);
  }

  void loop() {
    if (WiFi.status() == WL_CONNECTED && !client.connected() && mqttTriesLeft > 0) {
      reconnect();
      mqttTriesLeft--;
    }
    client.loop();
    webServer.handleClient();

    if (mySwitch.available()) {
      char buf[100];
      ltoa(mySwitch.getReceivedValue(), buf, 2);
      Serial.print("esp/2/proove/receive: ");
      Serial.println(buf);
      client.publish("esp/2/proove/receive", buf);
      mySwitch.resetAvailable();
    }
  }
