// Climaduino Versão "3.00"

//defines - mapeamento de pinos do NodeMCU
#define D0    16
#define D1    5
#define D2    4
#define D3    0
#define D4    2
#define D5    14
#define D6    12
#define D7    13
#define D8    15
#define D9    3
#define D10   1

// Declaração de Bibliotecas
#include <FS.h>                   //https://github.com/esp8266/Arduino/tree/master/cores/esp8266
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>            //https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer
#include <ESP8266WebServer.h>     //https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <DHT.h>                  //https://github.com/adafruit/DHT-sensor-library
#include <PubSubClient.h>         //https://github.com/knolleary/pubsubclient
#include <WifiLocation.h>         //https://github.com/gmag11/ESPWifiLocation

// Variaveis Servidor MQTT
char mqtt_station[30] = "nome_da_estacao";
char mqtt_server[40] = "climaduino.monitoracerrado.org";
char mqtt_port[6] = "1883";
char mqtt_topic[40] = "/climaduino/localizacao";
//char GOOGLE_KEY[40] = "insira a chave da api";
//char mqtt_username[10];
//char mqtt_password[10];

char mqtt_city[30];
char mqtt_state[30];
char mqtt_country[30];
//char mqtt_latitude[10];
//char mqtt_longitude[10];

String clientName;
String lati = "0.00000";
String longi = "0.00000";

// Parametros do Sensor DHT
#define DHTPIN 2            // Conectar o DHT no pino D4
#define DHTTYPE DHT22      // Modelo do DHT 11 ou 22
#define REPORT_INTERVAL 100 // Intervalo em Segundos para envio dos dados
#define REPORT_INTERVAL1 3600 // Intervalo em Segundos para envio dos dados
DHT dht(DHTPIN, DHTTYPE);

// Servidor de Localizacao
#define GOOGLE_KEY "AIzaSyDZtRivPTD3KA_O3OFMHY4P6v1JCPkno6M"      // Chave da API do Google Maps

WifiLocation location(GOOGLE_KEY);

//Variáveis e objetos globais
WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient

//flag for saving data
bool shouldSaveConfig = false;

// Notificação para salvar as informações
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Declaracao do tipo de valor de Umidade e Temperatura
float oldH ;
float oldT ;

double oldLat;
double oldLong;

//Prototypes
void initSerial();
void initDHT();
void initWiFiManager();
void reconnectWiFi();
void serialipgeolocation();
void ipgeolocation();
void initMQTT();
void reconnectMQTT();
void checkwifiemqtt(void);
void dataDHT();
void datalocal();
void sendDataMQTT(String payload);

// Fim das Declaração Globais e Inicio do Codigo
void setup() {
  //inicializações:
  initSerial();
  initDHT();
  initWiFiManager();
  ipgeolocation();
  initMQTT();
}

// Inicializa a comunicacao Serial com o ESP8266
void initSerial() {
  Serial.begin(115200);
}

void initDHT() {
  dht.begin();
  Serial.println("Teste de Comunicacao DHT");
}

// Ler informações coletadas na WEB
void initWiFiManager () {
  //read configuration from FS json
  Serial.println("Montando FS...");

  if (SPIFFS.begin()) {
    Serial.println("Sistema de arquivos montado");
    if (SPIFFS.exists("/config.json")) {
      // Se arquivo existir, deverá ler e carregar as informações
      Serial.println("Lendo arquivo de configurações");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Arquivo de configurações aberto");
        size_t size = configFile.size();
        // Alocar e armazenar as configurações do arquivo
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_topic, json["mqtt_topic"]);

        } else {
          Serial.println("Falha ao carregar o arquivo de configurações");
        }
      }
    }
  } else {
    Serial.println("Falha ao montar sistema de arquivos");
  }

  //WiFiManager
  WiFiManager wifiManager;

  // Lê as informações anteriormente armazenadas
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Dados Exibidos no Servidor WEB
  WiFiManagerParameter custom_mqtt_text("<br/>Dados da Estacao Climaduino: <br/>");
  wifiManager.addParameter(&custom_mqtt_text);

  WiFiManagerParameter custom_mqtt_station("station", "Nome da estacao", mqtt_station, 30);
  wifiManager.addParameter(&custom_mqtt_station);
  WiFiManagerParameter custom_mqtt_server("server", "Servidor MQTT", mqtt_server, 40);
  wifiManager.addParameter(&custom_mqtt_server);
  WiFiManagerParameter custom_mqtt_topic("topic", "Topico Climaduino", mqtt_topic, 40);
  wifiManager.addParameter(&custom_mqtt_topic);
  WiFiManagerParameter custom_mqtt_port("port", "Porta do Servidor", mqtt_port, 6);
  wifiManager.addParameter(&custom_mqtt_port);
  //  WiFiManagerParameter custom_mqtt_username("username", "Usuario do Broker MQTT", mqtt_username, 15);
  //  wifiManager.addParameter(&custom_mqtt_username);
  //  WiFiManagerParameter custom_mqtt_password("password", "Senha do Broker MQTT", mqtt_password, 6);
  //  wifiManager.addParameter(&custom_mqtt_password);

  WiFiManagerParameter custom_mqtt_text1("<br/> <br/>Localizacao da Estacao Climaduino: <br/>");
  wifiManager.addParameter(&custom_mqtt_text1);
//  WiFiManagerParameter custom_GOOGLE_KEY("GOOGLE_KEY", "API Geolocation Google", GOOGLE_KEY, 40);
//  wifiManager.addParameter(&custom_GOOGLE_KEY);
  WiFiManagerParameter custom_mqtt_city("city", "Cidade", mqtt_city, 32);
  wifiManager.addParameter(&custom_mqtt_city);
  WiFiManagerParameter custom_mqtt_state("state", "Estado", mqtt_state, 32);
  wifiManager.addParameter(&custom_mqtt_state);
  WiFiManagerParameter custom_mqtt_country("country", "Pais", mqtt_country, 32);
  wifiManager.addParameter(&custom_mqtt_country);
  //  WiFiManagerParameter custom_mqtt_latitude("latitude", "latitude", mqtt_latitude, 32);
  //  wifiManager.addParameter(&custom_mqtt_latitude);
  //  WiFiManagerParameter custom_mqtt_longitude("longitude", "longitude", mqtt_longitude, 32);
  //  wifiManager.addParameter(&custom_mqtt_longitude);

  WiFiManagerParameter custom_mqtt_text2("<br/> <br/>Desenvolvido pelo Grupo Monitora Cerrado. <br/>");
  wifiManager.addParameter(&custom_mqtt_text2);

  WiFiManagerParameter custom_mqtt_text3("<br/>Disponibilizado sob licenca BSD. <br/>");
  wifiManager.addParameter(&custom_mqtt_text3);

  // Se quiser poderá setar um IP estático
  //  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // Apagar informações para testes
  //   wifiManager.resetSettings();

  // Define o nível mínimo da potência do sinal WiFi o menor valor é 8%
  //wifiManager.setMinimumSignalQuality();

  // Define o tempo máximo para nova tentativa de conexão
  //wifiManager.setTimeout(120);

  // Caso não consiga ler as configurações e conectar irá subir o SSID definido abaixo
  if (!wifiManager.autoConnect("Climaduino", "climaduino")) {
    Serial.println("Falha ao conectar a rede WIFI");
    delay(3000);
    // Reseta as informações armazenadas
    ESP.reset();
    delay(5000);
  }
  // Se conseguir conectar a rede WiFi
  Serial.println("Conectado a Rede Wifi:)");

  // Le os parametros de configuração
  strcpy(mqtt_station, custom_mqtt_station.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
//  strcpy(GOOGLE_KEY, custom_GOOGLE_KEY.getValue());
  strcpy(mqtt_city, custom_mqtt_city.getValue());
  strcpy(mqtt_state, custom_mqtt_state.getValue());
  strcpy(mqtt_country, custom_mqtt_country.getValue());

  // Salvar os parametros de configuração
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_station"] = mqtt_station;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_topic"] = mqtt_topic;
//    json["GOOGLE_KEY"] = GOOGLE_KEY;
    json["mqtt_city"] = mqtt_city;
    json["mqtt_state"] = mqtt_state;
    json["mqtt_country"] = mqtt_country;

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

// Reconecta na Wifi em caso de perda de comunicacao com WiFi
void reconnectWiFi() {
  WiFiManager wifiManager;

  if (WiFi.status() == WL_CONNECTED)   // Se Wifi estive conectado nao faca nada
    return;

  wifiManager.autoConnect();

  while (WiFi.status() != WL_CONNECTED)  // Caso nao esteja conectado a Wifi
  {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado com sucesso na rede WiFi");
  Serial.print("local ip");
  Serial.println("Endereco IP: ");
  Serial.println(WiFi.localIP());
}

void serialipgeolocation() {
  // Exibe os parametros de geolocalizacao
  location_t loc = location.getGeoFromWiFi();

  Serial.println("Location request data");
  Serial.println(location.getSurroundingWiFiJson());
  Serial.println("Latitude: " + String(loc.lat, 7));
  Serial.println("Longitude: " + String(loc.lon, 7));
  Serial.println("Accuracy: " + String(loc.accuracy));
}

void ipgeolocation() {
  // Parametro da variável loc para exibir latitude e longitude
  location_t loc = location.getGeoFromWiFi();
  String lat = String(loc.lat, 7);
  lati = lat;
  String lon = String(loc.lon, 7);
  longi = lon;
}

// Inicializa parâmetros de conexão MQTT(endereço do broker, porta e seta função de callback)
void initMQTT() {
  //  const uint16_t mqtt_port_x = 12025;
  MQTT.setServer(mqtt_server, 1883);  // Atribui os parametros do servidor broker mqtt e porta
  MQTT.setCallback(MQTT_callback);    // Atribui a funcao de callback, utilizada para passar informacao dos topicos subescritos
}

//Função que recebe as mensagens publicadas
void MQTT_callback(char* topic, byte* payload, unsigned int length) {
}

// Reconecta-se ao broker MQTT em caso de queda da conexao
void reconnectMQTT() {
  while (!MQTT.connected())
  {
    Serial.print("* Tentando se conectar ao Broker MQTT: ");

    if (MQTT.connect("espClient")) {
      Serial.println("Conectado com sucesso ao broker MQTT!");
      MQTT.subscribe(mqtt_topic);
    }
    else {
      Serial.println("Falha ao reconectar no broker.");
      Serial.println("Havera nova tentativa de conexao em 2s");
      delay(2000);
    }
  }
}

// Verifica o estado das conexões Wifi e ao broker MQTT.
void checkwifiemqtt(void) {
  if (!MQTT.connected())
    reconnectMQTT(); // Se cliente nao estiver conectado ao Broker MQTT reconecte
  reconnectWiFi();  // Se cliente nao estiver conectado a Rede Wifi reconecte
}

void dataDHT() {

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);

  location_t loc = location.getGeoFromWiFi();

  if (loc.lat == 0.0000000 ) {
    ipgeolocation();
    Serial.println("latitude esta vazia");
    Serial.println("longitude esta vazia");
    Serial.println("Sem dados de latitude e longitude");
  }

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Falha ao obter dados do sensor DHT");
    return;
  }

  // Temperatura em Fahrenheit
  //float hif = dht.computeHeatIndex(f, h, false);
  // Temperatura em Celsius
  float hic = dht.computeHeatIndex(t, h);

  Serial.print("Umidade: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.print(" *C ");
  Serial.print(f);
  Serial.print(" *F\t");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.println(" *F\t");

  if (t != oldT || h != oldH ) {
    Serial.println("Novos dados a serem publicados");

    String payload = "{\"umidade\":";
    payload += h;
    payload += ",\"temperatura\":";
    payload += t;
    payload += ",\"latitude\":";
//    payload += String(loc.lat, 7);
    payload += lati;
    payload += ",\"longitude\":";
//    payload += String(loc.lat, 7);
    payload += longi;

    payload += "}";

    sendDataMQTT(payload);
    oldT = t;
    oldH = h;
  }

  int cnt = REPORT_INTERVAL;

  while (cnt--)
    delay(1000);
}

void datalocal() {

    Serial.println();
    Serial.print("Latitude: ");
    Serial.print(lati);
    Serial.print(" ## Longitude: ");
    Serial.println(longi);
    
    if (lati == "0.00000" || lati == "" || longi == "0.00000" || longi == ""){
      Serial.println("Georeferenciando.");
      ipgeolocation();
    }
    
    Serial.println("Sem Georeferenciamento.");
    
  //  int cnt = REPORT_INTERVAL;

  //  while (cnt--)
  //    delay(1000);
}

void sendDataMQTT(String payload) {
  if (!MQTT.connected()) {
    if (MQTT.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again");
      Serial.print("Topic is: ");
      Serial.println(mqtt_topic);
      Serial.print("API Key is: ");
      Serial.println(GOOGLE_KEY);
    }
    else {
      Serial.println("MQTT connect failed");
      //    Serial.println("Will reset and try again...");
      //    abort();
    }
  }

  if (MQTT.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (MQTT.publish(mqtt_topic, (const char*) payload.c_str())) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }
}

// Funcao Principal do Programa
void loop() {
  //garante funcionamento das conexões WiFi e ao broker MQTT
  //checkwifiemqtt();
  //localizacao Ip Geolocation
  //ipgeolocation();
  // Envia dados temperatura
  dataDHT();
  datalocal();
  //keep-alive da comunicação com broker MQTT
  MQTT.loop();
}
