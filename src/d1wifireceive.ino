#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <RCSwitch.h>


// Connect to the WiFi
const char* ssid = "xxx";
const char* password = "xxx";
const char* mqtt_server = "192.168.0.33";


WiFiClient espClient;
PubSubClient client(espClient);
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
      Serial.print("Attempting MQTT connection...");
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

  void setup()
  {
    Serial.begin(115200);
    delay(10);

    Serial.print("Connecting to ");
    Serial.println(ssid);
    // WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected to ");
    Serial.println(WiFi.localIP());

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);

    mySwitch.enableReceive(digitalPinToInterrupt(4));
    mySwitch.enableTransmit(14);
  }

  void loop()
  {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    if (mySwitch.available()) {
      char buf[100];
      ltoa(mySwitch.getReceivedValue(), buf, 2);
      Serial.print("esp/2/proove/receive: ");
      Serial.println(buf);
      client.publish("esp/2/proove/receive", buf);
      mySwitch.resetAvailable();
    }
  }
