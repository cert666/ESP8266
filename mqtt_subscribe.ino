/**
 * IBM IoT Foundation managed Device
 * 
 * Author: Ant Elder
 * License: Apache License v2
 */
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7
#include <OneWire.h>
#include <DallasTemperature.h>


//-------- Customise these values -----------
const char* ssid = "Peklox";
const char* password = "brouk666";

#define ORG "wv58s8"
#define DEVICE_TYPE "ESP8266_kotel"
#define DEVICE_ID "ESP8266_3"
#define TOKEN "k78A@7yJM7?ScGwk2c"
//-------- Customise the above values --------

//-------- temperature sensor ds18b20 --------
#define ONE_WIRE_BUS 2  // DS18B20 pin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
float temperatureIn = 0;
unsigned int safetyTemp = 19;
//--------------------------------------------


char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

const char publishTopic[] = "iot-2/evt/status/fmt/json";
const char subscribeTopic[] = "iot-2/cmd/switch/fmt/json";
const char responseTopic[] = "iotdm-1/response";
const char manageTopic[] = "iotdevice-1/mgmt/manage";
const char updateTopic[] = "iotdm-1/device/update";
const char rebootTopic[] = "iotdm-1/mgmt/initiate/device/reboot";

void callback(char* topic, byte* payload, unsigned int payloadLength) ;

WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

int publishInterval = 30000; // 30 seconds
long lastPublishMillis;
//LED on ESP8266 GPIO2
const int outPin = 0;


void setup() {
   Serial.begin(115200); Serial.println();

   pinMode(outPin, OUTPUT);
   digitalWrite(outPin, LOW);

   wifiConnect();
   mqttConnect();
   initManagedDevice();
}

void loop() {

   float temperatureInTmp=0 ,temperatureOutTmp=0;
  
   if (millis() - lastPublishMillis > publishInterval) {

      // temperature sensor
      DS18B20.requestTemperatures();

      // get value for first sensor
      temperatureInTmp = DS18B20.getTempCByIndex(0);
      Serial.print("Temperature In: ");
      Serial.println(temperatureIn);

      if(temperatureInTmp != 85)// failure state of ds18b20 sends 85
      {
       temperatureIn=temperatureInTmp; 
      }

      // if the connection got lost, take over 
      // control with a default temperature
      if (!client.connected()) {
           Serial.print("Connection lost.\n Setting safety temperature: ");
           Serial.print(safetyTemp);
           if( temperatureIn > safetyTemp)
           {
              digitalWrite(outPin, LOW);
           }
           else           
           {
              digitalWrite(outPin, HIGH);
           }
      }
    
      publishData(); 
      lastPublishMillis = millis();
   }

   if (!client.loop()) {
      mqttConnect();
   }
}

void wifiConnect() {
   Serial.print("Connecting to "); Serial.print(ssid);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
   } 
   Serial.print("nWiFi connected, IP address: "); Serial.println(WiFi.localIP());
}

void mqttConnect() {
  unsigned int counter = 10;
   if (!!!client.connected()) {
      Serial.print("Reconnecting MQTT client to "); Serial.println(server);
      while (!!!client.connect(clientId, authMethod, token)&& counter--) {
         Serial.print(".");
         Serial.print(counter);
         delay(500);
      }
      Serial.println();
   }
}

void initManagedDevice() {
  
   if (client.subscribe(subscribeTopic)) {
      Serial.println("subscribe to responses OK");
   } else {
      Serial.println("subscribe to responses FAILED");
   }

  
   if (client.subscribe(responseTopic)) {
      Serial.println("subscribe to responses OK");
   } else {
      Serial.println("subscribe to responses FAILED");
   }

   if (client.subscribe(rebootTopic)) {
      Serial.println("subscribe to reboot OK");
   } else {
      Serial.println("subscribe to reboot FAILED");
   }

   if (client.subscribe(updateTopic)) {
      Serial.println("subscribe to update OK");
   } else {
      Serial.println("subscribe to update FAILED");
   }

   StaticJsonBuffer<300> jsonBuffer;
   JsonObject& root = jsonBuffer.createObject();
   JsonObject& d = root.createNestedObject("d");
   JsonObject& metadata = d.createNestedObject("metadata");
   metadata["publishInterval"] = publishInterval;
   JsonObject& supports = d.createNestedObject("supports");
   supports["deviceActions"] = true;

   char buff[300];
   root.printTo(buff, sizeof(buff));
   Serial.println("publishing device metadata:"); Serial.println(buff);
   if (client.publish(manageTopic, buff)) {
      Serial.println("device Publish ok");
   } else {
      Serial.print("device Publish failed:");
   }
}

void publishData() {


   // build payload message 
   String payload =  "{\"d\":{\"temperIn\":";  
   payload += temperatureIn;
   
 //  payload += ",\"temperOut\":";
 //  payload += temperatureOut;
   
   payload += "}}";
 
   Serial.print("Sending payload: "); Serial.println(payload);
 
   if (client.publish(publishTopic, (char*) payload.c_str())) {
      Serial.println("Publish OK");
   } else {
      Serial.println("Publish FAILED");
   }
}

void callback(char* topic, byte* payload, unsigned int payloadLength) {
   Serial.print("callback invoked for topic: "); Serial.println(topic);
   char message_buff[20];
   int i=0;

   if (strcmp (subscribeTopic, topic) == 0) {
      Serial.println("Received cert command...");

      CertHandleUpdate(payload);
     
   }

   if (strcmp (responseTopic, topic) == 0) {
      return; // just print of response for now 
   }

   if (strcmp (rebootTopic, topic) == 0) {
      Serial.println("Rebooting...");
      ESP.restart();
   }

   if (strcmp (updateTopic, topic) == 0) {
      handleUpdate(payload); 
   } 
}

void handleUpdate(byte* payload) {
   StaticJsonBuffer<300> jsonBuffer;
   JsonObject& root = jsonBuffer.parseObject((char*)payload);
   if (!root.success()) {
      Serial.println("handleUpdate: payload parse FAILED");
      return;
   }
   Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();

   JsonObject& d = root["d"];
   JsonArray& fields = d["fields"];
   for(JsonArray::iterator it=fields.begin(); it!=fields.end(); ++it) {
      JsonObject& field = *it;
      const char* fieldName = field["field"];
      if (strcmp (fieldName, "metadata") == 0) {
         JsonObject& fieldValue = field["value"];
         if (fieldValue.containsKey("publishInterval")) {
            publishInterval = fieldValue["publishInterval"];
            Serial.print("publishInterval:"); Serial.println(publishInterval);
         }
      }
   }
}



void CertHandleUpdate(byte* payload) {
   StaticJsonBuffer<100> jsonBuffer;
   int value;
   JsonObject& root = jsonBuffer.parseObject((char*)payload);
   
   if (!root.success()) {
      Serial.println("handleUpdate: payload parse FAILED");
      return;
   }
   Serial.println("handleUpdate payload:"); root.prettyPrintTo(Serial); Serial.println();

   value = root["switch"];
   Serial.println("Incomming value is: ");
   Serial.println(value);

   if(value == 0)
   {
      digitalWrite(outPin, LOW);
   }
   else
   if(value == 1)
   {
      digitalWrite(outPin, HIGH);
   }
  
}


