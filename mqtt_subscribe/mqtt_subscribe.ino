/**
 * IBM IoT Foundation managed Device
 * 
 * Author: cij
 * License: Apache License v2
 */
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient/releases/tag/v2.3
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7
#include <OneWire.h>


#include <DallasTemperature.h>
#include <DHT.h>


//-------- Customise these values -----------
const char* ssid = "xxxxxx";
const char* password = "xxxxxx";

#define ORG "xxxxxx"
#define DEVICE_TYPE "xxxxxx"
#define DEVICE_ID "xxxxxx"
#define TOKEN "xxxxxx"
//-------- Customise the above values --------  

//-------- temperature and humidity sensor DHT22 --------
#define pinDHT 12 // set pin for DHT sensor
// odkomentování správného typu čidla
//#define typDHT DHT11     // DHT 11
#define typDHT DHT22   // DHT 22 (AM2302)
float DHThumid = 0, DHTtemp = 0;
//--------------------------------------------

//-------- temperature sensor ds18b20 --------
#define ONE_WIRE_BUS 13  // DS18B20 pin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
float temperatureIn = 0,temperatureOut = 0;
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

// inicializace DHT senzoru s nastaveným pinem a typem senzoru
DHT DHTsens(pinDHT, typDHT);

void setup() {
   Serial.begin(115200); Serial.println();

   pinMode(outPin, OUTPUT);
   digitalWrite(outPin, LOW);

#ifdef DHT22
   // zapnutí komunikace s teploměrem DHT
   DHTsens.begin();
#endif
   
   wifiConnect();
   mqttConnect();
   initManagedDevice();
}

void loop() {

   float temperatureInTmp = 0 ,temperatureOutTmp = 0;
   

  
   if (millis() - lastPublishMillis > publishInterval) {

      // temperature sensor
      DS18B20.requestTemperatures();


      // pomocí funkcí readTemperature a readHumidity načteme
      // do proměnných tep a vlh informace o teplotě a vlhkosti,
      // čtení trvá cca 250 ms
      float DHThumidTmp = 0, DHTtempTmp = 0;
      
      DHTtempTmp = DHTsens.readTemperature();
      DHThumidTmp = DHTsens.readHumidity();
      // kontrola, jestli jsou načtené hodnoty čísla pomocí funkce isnan
      if (!isnan(DHTtempTmp)) {
        DHTtemp = DHTtempTmp;
      }
      if (!isnan(DHThumidTmp))
      {
        DHThumid = DHThumidTmp;
      }

      Serial.print("DHTtemp: ");
      Serial.println(DHTtemp);
      Serial.print("DHThumid: ");
      Serial.println(DHThumid);
      


      // get value for second sensor
      temperatureOutTmp = DS18B20.getTempCByIndex(0);
      Serial.print("Temperature Out: ");
      Serial.println(temperatureOut);

      // filter failure values
      if(temperatureOutTmp != 85) // failure state of ds18b20 sends 85
      {
          temperatureOut = temperatureOutTmp; 
      }


      // check if connection is not estabilished 
      if(WiFi.status() != WL_CONNECTED || !client.connected())
      {
           // keep safety temperature
           Serial.print("Connection lost.\n Setting safety temperature: ");
           safetyTemperature();     
      
      }else
      {
          // send data on server
          publishData();  
      }
      lastPublishMillis = millis();
   }

   // if wifi is connected but mqtt server is not =>
   // request new connection on mqtt
   if (!client.loop() && WiFi.status()== WL_CONNECTED) {
      mqttConnect();
      initManagedDevice();
   }else
   // if wifi is disconnected =>
   // request new wifi connection
   if (WiFi.status()!= WL_CONNECTED) {
      wifiConnect();
   }
}

void wifiConnect() {
  unsigned int counter = 100;
   Serial.print("Connecting to "); Serial.print(ssid);
   WiFi.begin(ssid, password);
   while (WiFi.status() != WL_CONNECTED && counter--) {
      delay(500);
      Serial.print(counter);
      Serial.print(".");
   } 
   if(WiFi.status()== WL_CONNECTED){
      Serial.print("nWiFi connected, IP address: "); Serial.println(WiFi.localIP());
   }
}

void mqttConnect() {
  unsigned int counter = 10;
   if (!!!client.connected()) {
      Serial.print("Reconnecting MQTT client to "); Serial.println(server);
      while (!!!client.connect(clientId, authMethod, token) && counter--) {
         Serial.print(".");
         Serial.print(counter);
         delay(500);
      }
      Serial.println();
   }
}

// in case of none connection,
// temparature is kept on safety level
// for ESP8266-01 board is used inverted logic
void safetyTemperature() {  
    
    Serial.print(safetyTemp);
    if( DHTtemp > safetyTemp)
    {
        digitalWrite(outPin, HIGH);
    }
    else           
    {
        digitalWrite(outPin, LOW);
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

// build message to be and send it on server
void publishData() {

   // build payload message 
   String payload =  "{\"d\":{\"temperIn\":";
   
   payload += DHTtemp;
   
   payload += ",\"humidIn\":";
   payload += DHThumid;
   
   payload += ",\"temperOut\":";
   payload += temperatureOut;

   
   payload += "}}";
 
   Serial.print("Sending payload: "); Serial.println(payload);
 
   if (client.publish(publishTopic, (char*) payload.c_str())) {
      Serial.println("Publish OK");
   } else {
      Serial.println("Publish FAILED");
   }
}

// function is called in case of incomming message
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

// JSON parser for incomming default message
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


// JSON parser for incomming custom message
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


