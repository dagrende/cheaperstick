#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <FS.h>

// prefs - add new prefs at end to preserve saved ones
typedef struct {
  long magicNumber = 77824591l;
  char ssid[50] = "";
  char password[50] = "";
  char mqtt_server[50] = "192.168.0.33";
  char mqtt_prefix[50] = "esp/";
  bool enableWifi = true;
  bool enableMqtt = false;
  bool enableRcReceive = true;
  bool enableRcTransmit = true;
} Prefs;

Prefs prefs;
Prefs defaultPrefs;

int mqttTriesLeft = 5;
int wifiTriesLeft = 6;

WiFiClient espClient;
PubSubClient client(espClient);
AsyncWebServer webServer(80);
RCSwitch mySwitch = RCSwitch();

const byte ledPin = LED_BUILTIN; // LED pin on Wemos d1 mini

void httpRespond(WiFiClient client, int status) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.println(" OK");
  client.println("Access-Control-Allow-Origin: *");
  client.println(""); // mark end of headers
}

void callback(char* topic, byte* payload, unsigned int length) {
  if ((String(prefs.mqtt_prefix) + "send") == topic) {
    // got a send message - send on 433MHz
    char code[100];
    if (length < sizeof code) {
      strncpy(code, (char *)payload, length);
      code[length] = 0;
      mySwitch.send(code);
    }
  } else if ((String(prefs.mqtt_prefix) + "setProtocol") == topic) {
    int protocolNo = ((char)payload[0]) - '0';
    if (1 <= protocolNo && protocolNo <= 6) {
      mySwitch.setProtocol(protocolNo);
    }
  }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected() && mqttTriesLeft > 0) {
    Serial.print("mqtt connecting "); Serial.println(prefs.mqtt_server);
    // Attempt to connect
    if (client.connect("cheapstick")) {
      Serial.println("mqtt connected");
      // ... and subscribe to topic
      client.subscribe((String(prefs.mqtt_prefix) + "#").c_str());
    } else {
      Serial.print("mqtt error "); Serial.println(client.state());
      // Wait 5 seconds before retrying
      delay(5000);
      mqttTriesLeft--;
    }
  }
}

// if webServer has the argument argName, then copy it as a string to dest (max destSize bytes), and set anySet to true.
void setStringFromArg(AsyncWebServerRequest *request, char *dest, size_t destSize, const char *argName, bool ignoreEmpty, bool &anySet) {
  if (request->hasArg(argName)) {
    String value = request->arg(argName);
    if (!ignoreEmpty || value.length() > 0) {
      strncpy(dest, value.c_str(), destSize);
      dest[destSize - 1] = 0;
      anySet = true;
    }
  }
}

// if webServer has the argument argName, then copy it as a string to dest (max destSize bytes), and set anySet to true.
void setBoolFromArg(AsyncWebServerRequest *request, bool &dest, const char *argName, bool ignoreEmpty, bool &anySet) {
  if (request->hasArg(argName)) {
    dest = request->arg(argName).equalsIgnoreCase("true");
    anySet = true;
  }
}

void handleNotFound(AsyncWebServerRequest *request){
  request->send(404);
}

void setup() {
  Serial.begin(115200);

  // read saved prefs
  memcpy(&prefs, &defaultPrefs, sizeof prefs);
  EEPROM.begin(512);
  EEPROM.get(0, prefs);
  if (prefs.magicNumber != defaultPrefs.magicNumber) {
    // EEPROM was empty - init with default prefs
    memcpy(&prefs, &defaultPrefs, sizeof prefs);
  }

  delay(10);

  // try connecting to wifi
  Serial.print("\nwifi connecting "); Serial.println(prefs.ssid);
  WiFi.begin(prefs.ssid, prefs.password);
  while (WiFi.status() != WL_CONNECTED && wifiTriesLeft > 0) {
    delay(500);
    wifiTriesLeft--;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    // log success and connect to mqtt setServer
    Serial.print("wifi connected "); Serial.println(WiFi.localIP());

    client.setServer(prefs.mqtt_server, 1883);
    client.setCallback(callback);
  } else {
    // log failure and chage to AP mode
    Serial.println("wifi error - changing to Access Point mode ssid cheaperstick pw bettertoo, open: http://192.168.4.1");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("cheaperstick", "bettertoo");
  }

  SPIFFS.begin();

  // web service to get all prefs except password as json
  webServer.on("/prefs", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *response = request->beginResponseStream("text/json");
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& jsonObject = jsonBuffer.createObject();
    jsonObject.set("enableWifi", prefs.enableWifi);
    jsonObject.set("enableMqtt", prefs.enableMqtt);
    jsonObject.set("enableRcReceive", prefs.enableRcReceive);
    jsonObject.set("enableRcTransmit", prefs.enableRcTransmit);
    jsonObject["ssid"] = prefs.ssid;
    jsonObject["mqtt_server"] = prefs.mqtt_server;
    jsonObject["mqtt_prefix"] = prefs.mqtt_prefix;
    jsonObject.printTo(*response);
    request->send(response);
  });

  // web service to set POSTed prefs and save if EEPROM
  webServer.on("/prefs", HTTP_POST, [](AsyncWebServerRequest *request){
    bool anySet = false;
    setStringFromArg(request, prefs.ssid, sizeof prefs.ssid, "ssid", false, anySet);
    setStringFromArg(request, prefs.password, sizeof prefs.password, "password", true, anySet);
    setStringFromArg(request, prefs.mqtt_server, sizeof prefs.mqtt_server, "mqtt_server", false, anySet);
    setStringFromArg(request, prefs.mqtt_prefix, sizeof prefs.mqtt_prefix, "mqtt_prefix", false, anySet);
    setBoolFromArg(request, prefs.enableWifi, "enableWifi", true, anySet);
    setBoolFromArg(request, prefs.enableMqtt, "enableMqtt", true, anySet);
    setBoolFromArg(request, prefs.enableRcReceive, "enableRcReceive", true, anySet);
    setBoolFromArg(request, prefs.enableRcTransmit, "enableRcTransmit", true, anySet);
    if (anySet) {
      EEPROM.put(0, prefs);
      EEPROM.commit();
    }
    if (request->hasArg("form_submit")) {
      request->redirect("/");
    } else {
      request->send(200);
    }
  });

  webServer.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  webServer.onNotFound(handleNotFound);

  webServer.begin();

  // init LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  // init 433MHz radio driver
  if (prefs.enableRcReceive) {
    mySwitch.enableReceive(digitalPinToInterrupt(4));
  }
  if (prefs.enableRcTransmit) {
    mySwitch.enableTransmit(14);
  }
}

void loop() {
  // if not connected to mqtt broker - connect
  if (prefs.enableMqtt && WiFi.status() == WL_CONNECTED && !client.connected()) {
    reconnect();
  }
  // poll mqtt
  if (prefs.enableMqtt) {
    client.loop();
  }

  // check received 433MHz remote control signal
  if (prefs.enableRcReceive) {
    if (mySwitch.available()) {
      char buf[100];
      ltoa(mySwitch.getReceivedValue(), buf, 2);
      if (prefs.enableMqtt) {
        client.publish((String(prefs.mqtt_prefix) + "receive").c_str(), buf);
      }
      mySwitch.resetAvailable();
    }
  }
}
