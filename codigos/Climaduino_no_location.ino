#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <DHT.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

// PROGRAM_VERSION   "1.00"

// Aqui devem ser declarados os valores
char mqtt_station[40];
char mqtt_server[40];
char mqtt_port[6] = "1883";
//char mqtt_username[10];
//char mqtt_password[10];
//char mqtt_topic[34] = "/climaduino/";
char mqtt_latitude[10];
char mqtt_longitude[10];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Salve a configuracao");
  shouldSaveConfig = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
//          strcpy(mqtt_topic, json["mqtt_topic"]);
//          strcpy(username, json["username"]);
//          strcpy(password, json["password"]);
          strcpy(mqtt_latitude, json["mqtt_latitude"]);
          strcpy(mqtt_longitude, json["mqtt_longitude"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_text("<br/>Dados da Estacao Climaduino: <br/>");
  wifiManager.addParameter(&custom_mqtt_text);
  WiFiManagerParameter custom_mqtt_station("station", "Nome da estacao", mqtt_station, 10);
  wifiManager.addParameter(&custom_mqtt_station);
  WiFiManagerParameter custom_mqtt_server("server", "Servidor MQTT", mqtt_server, 40);
  wifiManager.addParameter(&custom_mqtt_server);
  WiFiManagerParameter custom_mqtt_port("port", "Porta do Servidor", mqtt_port, 6);
  wifiManager.addParameter(&custom_mqtt_port);
//  WiFiManagerParameter custom_mqtt_port("username", "Usuario do Broker MQTT", mqtt_username, 15);
//  wifiManager.addParameter(&custom_mqtt_username);
//  WiFiManagerParameter custom_mqtt_port("password", "Senha do Broker MQTT", mqtt_password, 6);
//  wifiManager.addParameter(&custom_mqtt_password);
//  WiFiManagerParameter custom_mqtt_topic("topic", "climaduino topic", mqtt_topic, 32);
//  wifiManager.addParameter(&custom_mqtt_topic);
  WiFiManagerParameter custom_mqtt_text1("<br/> <br/>Localizacao da Estacao Climaduino: <br/>");
  wifiManager.addParameter(&custom_mqtt_text1);
  WiFiManagerParameter custom_mqtt_latitude("latitude", "latitude", mqtt_latitude, 32);
  wifiManager.addParameter(&custom_mqtt_latitude);
  WiFiManagerParameter custom_mqtt_longitude("longitude", "longitude", mqtt_longitude, 32);
  wifiManager.addParameter(&custom_mqtt_longitude);
  WiFiManagerParameter custom_mqtt_text2("<br/> <br/>Desenvolvido pelo Grupo Monitora Cerrado. <br/>");
  wifiManager.addParameter(&custom_mqtt_text2);
//WiFiManagerParameter custom_mqtt_text3("<br/>Disponibilizado sob licenca BSD. <br/>");
//wifiManager.addParameter(&custom_mqtt_text3);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
//  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Climaduino", "climaduino")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("Agora voce esta conectado :)");

//read updated parameters
  strcpy(mqtt_station, custom_mqtt_station.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
//  strcpy(mqtt_username, custom_mqtt_username.getValue());
//  strcpy(mqtt_password, custom_mqtt_password.getValue());
//  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(mqtt_latitude, custom_mqtt_latitude.getValue());
  strcpy(mqtt_longitude, custom_mqtt_longitude.getValue());

//save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_station"] = mqtt_station;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
//    json["mqtt_username"] = mqtt_username;
//    json["mqtt_password"] = mqtt_password;
//    json["mqtt_topic"] = mqtt_topic;
    json["mqtt_latitude"] = mqtt_latitude;
    json["mqtt_longitude"] = mqtt_longitude;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

}

void loop() {
  // put your main code here, to run repeatedly:


}
