#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <ArduinoJson.h>
#include <pgmspace.h>

#include "webapp.h"

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

void httpRespond(WiFiClient client, int status) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.println(" OK");
  client.println("Access-Control-Allow-Origin: *");
  client.println(""); // mark end of headers
}

bool loadFromFlash(WiFiClient client, String path) {
  if (path.endsWith("/")) path += "index.html";
  int NumFiles = sizeof(files)/sizeof(struct t_websitefiles);
  for (int i=0; i<NumFiles; i++) {
    if (path.endsWith(String(files[i].filename))) {
      client.println("HTTP/1.1 200 OK");
      client.print("Content-Type: "); client.println(files[i].mime);
      client.print("Content-Length: "); client.println(String(files[i].len));
      client.println("Access-Control-Allow-Origin: *");
      client.println(""); //  do not forget this one
      _FLASH_ARRAY<uint8_t>* filecontent = (_FLASH_ARRAY<uint8_t>*)files[i].content;
      filecontent->open();
      client.write(*filecontent, 100);
      return true;
    }
  }
  httpRespond(client, 201);
  return false;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  if (strcmp(topic, "esp/2/send") == 0) {
    // got a send message - send on 433MHz
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

// if webServer has the argument argName, then copy it as a string to dest (max destSize bytes), and set anySet to true.
void setStringFromArg(char *dest, size_t destSize, char *argName, boolean ignoreEmpty, boolean &anySet) {
  if (webServer.hasArg(argName)) {
    String value = webServer.arg(argName);
    if (!ignoreEmpty || value.length() > 0) {
      strncpy(dest, value.c_str(), destSize);
      dest[destSize - 1] = 0;
      anySet = true;
    }
  }
}

void handleNotFound(){
  String query = webServer.uri();
  String path = query.substring(1);
  if (query.length() < 2) {
    path = "index.html";
  }
  Serial.println(path);
  loadFromFlash(webServer.client(), path);
  Serial.println("responded");
}

void setup() {
  Serial.begin(115200);

  EEPROM.begin(512);
  EEPROM.get(0, prefs);
  if (prefs.magicNumber != defaultPrefs.magicNumber) {
    // EEPROM was empty - init with default prefs
    Serial.println("empty prefs - setting defaults");
    memcpy(&prefs, &defaultPrefs, sizeof prefs);
  }

  delay(10);

  // try connecting to wifi
  Serial.print("\nConnecting to ");
  Serial.println(prefs.ssid);
  WiFi.begin(prefs.ssid, prefs.password);
  while (WiFi.status() != WL_CONNECTED && wifiTriesLeft > 0) {
    delay(500);
    Serial.print(".");
    wifiTriesLeft--;
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    // log success and connect to mqtt setServer
    Serial.print("WiFi connected as "); Serial.println(WiFi.localIP());

    client.setServer(prefs.mqtt_server, 1883);
    client.setCallback(callback);
  } else {
    // log failure and chage to AP mode
    Serial.println("failed connecting to wifi AP - changing to Access Point mode ssid cheaperstick pw bettertoo");
    Serial.println("browse: http://192.168.4.1");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("cheaperstick", "bettertoo");
  }

  // web service to get all prefs except password as json
  webServer.on("/prefs", HTTP_GET, [](){
    Serial.println("GET /prefs");
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& jsonObject = jsonBuffer.createObject();
    jsonObject["ssid"] = prefs.ssid;
    jsonObject["mqtt_server"] = prefs.mqtt_server;
    char printBuf[200];
    jsonObject.printTo(printBuf, sizeof printBuf);
    webServer.send(200, "text/plain", printBuf);
    Serial.println("GET /prefs responded");
  });

  // web service to set POSTed prefs and save if EEPROM
  webServer.on("/prefs", HTTP_POST, [](){
    Serial.println("POST prefs");
    boolean anySet = false;
    setStringFromArg(prefs.ssid, sizeof prefs.ssid, "ssid", false, anySet);
    setStringFromArg(prefs.password, sizeof prefs.password, "password", true, anySet);
    setStringFromArg(prefs.mqtt_server, sizeof prefs.mqtt_server, "mqtt_server", false, anySet);
    if (anySet) {
      Serial.println("saving prefs");
      // EEPROM.put(0, prefs);
      // EEPROM.commit();
    }
    if (webServer.hasArg("form_submit")) {
      Serial.println("was form submit");
      loadFromFlash(webServer.client(), "index.html");
    } else {
      webServer.send(200);
    }
  });

  webServer.onNotFound(handleNotFound);

  // start web server
  webServer.begin();

  // init LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  // init 433MHz radio driver
  mySwitch.enableReceive(digitalPinToInterrupt(4));
  mySwitch.enableTransmit(14);
}

void loop() {
  // if not connected to mqtt broker - connect
  if (WiFi.status() == WL_CONNECTED && !client.connected() && mqttTriesLeft > 0) {
    reconnect();
    mqttTriesLeft--;
  }
  // poll mqtt
  client.loop();

  // poll web server
  webServer.handleClient();

  // check received 433MHz remote control signal
  if (mySwitch.available()) {
    char buf[100];
    ltoa(mySwitch.getReceivedValue(), buf, 2);
    Serial.print("esp/2/receive: "); Serial.println(buf);
    client.publish("esp/2/receive", buf);
    mySwitch.resetAvailable();
  }
}
