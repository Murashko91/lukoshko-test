/**
   BasicHTTPClient.ino

    Created on: 24.05.2015

*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>


#include <ESP8266HTTPClient.h>

#include <WiFiClient.h>

ESP8266WiFiMulti WiFiMulti;
HTTPClient http;
WiFiClientSecure g_client;




// Шина данных подключена к выводу D2 на ESP8266
#define ONE_WIRE_BUS D2

// Настройка объекта oneWire для связи с любыми устройствами OneWire
OneWire oneWire(ONE_WIRE_BUS);

// Передаем ссылку на объект oneWire объекту Dallas Temperature.
DallasTemperature sensors(&oneWire);

float tempSensor1, tempSensor2, tempSensor3;

uint8_t sensor1[8] = { 0x28, 0x4F, 0xA4, 0x9A, 0x5F, 0x20, 0x01, 0x22  };
//uint8_t sensor2[8] = { 0x28, 0x61, 0x64, 0x12, 0x3C, 0x7C, 0x2F, 0x27  };
//uint8_t sensor3[8] = { 0x28, 0x61, 0x64, 0x12, 0x3F, 0xFD, 0x80, 0xC6  };

// Start Relay State:
int RL1 = D1;


// Start Relay State:
String relays[3] = {"false", "false", "false"};

//Sync Host
const char* oauthHost = "https://login.salesforce.com/services/oauth2/token";
const char* t800Host = "https://smarthouset800-dev-ed.my.salesforce.com/services/apexrest/t800api/process-data";

/////////////////////////////CREDANTIALS!!!!!!!!
String client_id = "3MVG9kBt168mda_.YdgPWY_M_MHgOc8Cx4bKnIYXvM11QLFuDgXP3ZVKddeQYnoXGIZGk7MKoGJF9lFXzScXP";
String client_secret = "5826BB499F89F29FDE0458008855D4D5038B1CE06CA520723E5C15F1B77FC5EB";
String username = "t800@t1000connect.com";
String password = "Arduino1";
String token = "underfined";
////////////////////////////////////

int rq_delay = 10000;

void setup() {
  g_client.setInsecure(); //the magic line, use with caution

  pinMode(RL1, OUTPUT);
  digitalWrite(RL1, HIGH);


  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  WiFi.mode(WIFI_STA);

  WiFiMulti.addAP("TP-LINK_74E6", "99487461");
  //WiFiMulti.addAP("Galaxy1", "96353821");
  //WiFiMulti.addAP("Arduino1", "96353821");
  int i = 0;
  while (WiFiMulti.run() != WL_CONNECTED & i < 30) {
    Serial.println("Attempt to connect №" + String(i));
    i++;
    delay(1000);
  }
  if (i == 30) {
    ESP.restart();
  }
}

void loop() {

  Serial.println("START LOOP");
  T800sync();
  Serial.println("END LOOP");
}


void T800sync() {


  sensors.requestTemperatures();
  tempSensor1 = sensors.getTempC(sensor1);


  Serial.print("[HTTP] begin...\n");

  if (http.begin(g_client, t800Host)) {  // HTTP

    String authToken = "Bearer " + token;
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", authToken.c_str());

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    String postData = "{\"si\" : \"smart_home\",\"is\" : {\"t1\":\"" + String(tempSensor1) + "\",\"r1\":\"" + relays[1] + "\"}}";
    int httpCode = http.POST(postData.c_str());
    delay(100);
    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code 1: %d\n", httpCode);
      Serial.print(http.getString());
      Serial.print("//////");

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {

        parseResponse(http.getString());
        http.end();

      } else {
        Serial.printf("[HTTP] Post... failed, error: %s\n", http.errorToString(httpCode).c_str());
        http.end();
        refreshToken();
      }

      http.end();
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
      Serial.print(http.getString());
      Serial.print(httpCode);
      ESP.restart();

    }
  }

  delay(rq_delay);

}


void parseResponse(String payload) {

  StaticJsonDocument<192> doc;
  deserializeJson(doc, payload);
  JsonObject relays2update = doc["r2u"];
  const char* status = doc["s"]; // "home"
  const int delayRS = doc["d"]; // "home"

  if (delayRS > 0) {
    rq_delay = delayRS;
    Serial.println("Delay");
    Serial.println(rq_delay);

  }
  if (!relays2update.isNull()) {
    //String rel1 = relays2update["r1"];
    processRelay(relays2update["r1"], 1);
  }

}

void processRelay(String relayValue, int i) {

  if (relayValue == String("t")) {
    relays[i] = "t";
    digitalWrite(RL1, LOW);
  } else if (relayValue == String("f")) {
    relays[i] = "f";
    digitalWrite(RL1, HIGH);
  }

}
void refreshToken() {

  http.begin(g_client, oauthHost);

  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Data to send with HTTP POST
  String httpRequestData = "grant_type=password&client_id=" + client_id + "&client_secret=" + client_secret + "&username=" + username + "&password=" + password;

  // Send HTTP POST request
  int httpCode = http.POST(httpRequestData);
  delay(100);
  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code2: %d\n", httpCode);
    Serial.print(httpRequestData);
    Serial.print(http.getString());
    Serial.print("//////1");

    // file found at server
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {

      parseTokenResponse(http.getString());

    } else {
      Serial.printf("[HTTP] Post... failed Token request, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.printf("[HTTP} Unable to connect to get Token\n");
    Serial.print(http.getString());
    Serial.print(httpCode);
    ESP.restart();

  }

}

void parseTokenResponse(String payload) {
  StaticJsonDocument<600> doc;
  deserializeJson(doc, payload);
  const char* tokenRS = doc["access_token"];
  Serial.println("NEW TOKEN:" + String(tokenRS));
  token = String(tokenRS);


}
