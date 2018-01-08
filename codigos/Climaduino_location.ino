// PROGRAM_VERSION   "2.00"

// Declaração de Bibliotecas
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <WifiLocation.h>

// Aqui devem ser declarados os valores do servidor MQTT
char mqtt_station[40];
char mqtt_server[40];
char mqtt_port[6] = "1883";
//char mqtt_username[10];
//char mqtt_password[10];
String mytopic = String(mqtt_station);
String tmpPath = String("/climaduino/" + mytopic);
char mqtt_topic[34] = "/climaduino/cruzeiro";
char mqtt_city[30];
char mqtt_state[30];
char mqtt_country[30];
//char mqtt_latitude[10];
//char mqtt_longitude[10];

// Parametros do Sensor DHT
#define DHTPIN 2     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
#define REPORT_INTERVAL 30 // in sec

// Servidor de Localizacao
#define GOOGLE_KEY "AIzaSyDSCHdFoGy9XlchColcjtdF3t-e-k2v314"
WifiLocation location(GOOGLE_KEY);

// Parametros da Funcao Callback
void callback(const char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

WiFiClient wifiClient;

// Parametro da biblioteca mqtt 
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Salve a configuracao");
  shouldSaveConfig = true;
}

String clientName;
String topicPreFix = "/climaduino/";
String mymac;
String topicPostFix = "/from_node_red";
DHT dht(DHTPIN, DHTTYPE, 11);


// Declaracao do tipo de valor de Umidade e Temperatura
float oldH ;
float oldT ;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
//  makeGETlocation();
  Serial.println("Teste de Comunicacao DHT");
  delay(20);

  //clean FS, for testing
// SPIFFS.format();

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
//          strcpy(mqtt_latitude, json["mqtt_latitude"]);
//          strcpy(mqtt_longitude, json["mqtt_longitude"]);

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
  WiFiManagerParameter custom_mqtt_city("Cidade", "city", mqtt_city, 32);
  wifiManager.addParameter(&custom_mqtt_city);
  WiFiManagerParameter custom_mqtt_state("Estado", "state", mqtt_state, 32);
  wifiManager.addParameter(&custom_mqtt_state);
  WiFiManagerParameter custom_mqtt_country("Pais", "country", mqtt_country, 32);
  wifiManager.addParameter(&custom_mqtt_country);
//  WiFiManagerParameter custom_mqtt_latitude("latitude", "latitude", mqtt_latitude, 32);
//  wifiManager.addParameter(&custom_mqtt_latitude);
//  WiFiManagerParameter custom_mqtt_longitude("longitude", "longitude", mqtt_longitude, 32);
//  wifiManager.addParameter(&custom_mqtt_longitude);
  WiFiManagerParameter custom_mqtt_text2("<br/> <br/>Desenvolvido pelo Grupo Monitora Cerrado. <br/>");
  wifiManager.addParameter(&custom_mqtt_text2);
//WiFiManagerParameter custom_mqtt_text3("<br/>Disponibilizado sob licenca BSD. <br/>");
//wifiManager.addParameter(&custom_mqtt_text3);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
//  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //reset settings - for testing
// wifiManager.resetSettings();

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
//  strcpy(mqtt_latitude, custom_mqtt_latitude.getValue());
//  strcpy(mqtt_longitude, custom_mqtt_longitude.getValue());

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
    json["mqtt_topic"] = mqtt_topic;
    json["mqtt_city"] = mqtt_city;
    json["mqtt_state"] = mqtt_state;
    json["mqtt_country"] = mqtt_country;
//    json["mqtt_latitude"] = mqtt_latitude;
//    json["mqtt_longitude"] = mqtt_longitude;

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

// Definicao de nome do dispositivo
  clientName += "climaduino-";
//  uint8_t mac[6];
//  WiFi.macAddress(mac);
//  clientName += macToStr(mac);
//  clientName += "-";
//  clientName += String(micros() & 0xff, 16);

//String mytopic = topicPreFix + mymac + topicPostFix;
String mytopic = topicPreFix + mymac + topicPostFix;
//  topic += "mqtt_cidade"
    uint8_t mac[6];
    WiFi.macAddress(mac);
    mymac += macToStr(mac);
//  topic += "-";
//  topic += String(micros() & 0xff, 16);


// Exibe os parametros de geolocalizacao
    location_t loc = location.getGeoFromWiFi();

    Serial.println("Location request data");
    Serial.println(location.getSurroundingWiFiJson());
    Serial.println("Latitude: " + String(loc.lat, 7));
    Serial.println("Longitude: " + String(loc.lon, 7));
    Serial.println("Accuracy: " + String(loc.accuracy));

}

// Inicio do ciclo de repeticao
void loop() {
   
  location_t loc = location.getGeoFromWiFi(); // Parametro da variável loc para exibir latitude e longitude

String lat = String(loc.lat, 7);
String lon = String(loc.lon, 7);

float h = dht.readHumidity();
float t = dht.readTemperature();
float f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
//    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  float hi = dht.computeHeatIndex(f, h);

  Serial.print("Umidade: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hi);
  Serial.println(" *F\t");
  Serial.println("lat");
  Serial.println("lon");
 
  String payload = "{\"umidade\":";
  payload += h;
  payload += ",\"temperatura\":";
  payload += t;
//  payload += ",\"latitude\":";
//  payload += mqtt_latitude;
//  payload += ",\"longitude\":";
//  payload += mqtt_longitude;
  payload += ",\"latitude\":";
  payload += lat;
  payload += ",\"longitude\":";
  payload += lon;
  payload += "}";
 
  if (t != oldT || h != oldH )
  {
    sendTemperature(payload);
    oldT = t;
    oldH = h;
  }

  int cnt = REPORT_INTERVAL;

  while (cnt--)
    delay(1000);

}


void sendTemperature(String payload) {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again");
      Serial.print("Topic is: ");
      Serial.println(mqtt_topic);
//      Serial.println(mqtt_latitude);
//      Serial.println(mqtt_longitude);

    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (client.publish(mqtt_topic, (const char*) payload.c_str())) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }

}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

