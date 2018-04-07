
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
  char mqtt_station[40];
  char mqtt_server[40];
  char mqtt_port[6] = "1883";
//char mqtt_username[10];
//char mqtt_password[10];

String clientName;

char mqtt_topic[34];
char mqtt_city[30];
char mqtt_state[30];
char mqtt_country[30];
//char mqtt_latitude[10];
//char mqtt_longitude[10];

// Parametros do Sensor DHT
#define DHTPIN 2            // Define o pino que está conectado o DHT
#define DHTTYPE DHT22      // Modelo do DHT 11 ou 22
#define REPORT_INTERVAL 30 // Intervalo em Segundos para envio dos dados
DHT dht(DHTPIN, DHTTYPE);

// Servidor de Localizacao
#define GOOGLE_KEY "AIzaSyDSCHdFoGy9XlchColcjtdF3t-e-k2v314"      // Chave da API do Google Maps
WifiLocation location(GOOGLE_KEY);

//Variáveis e objetos globais
WiFiClient espClient; // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient

// Definicao de nome do dispositivo
//  clientName += "climaduino-";
  
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
  void sendDataMQTT(String payload);

// Fim das Declaração Globais e Inicio do Codigo
void setup() 
{
//inicializações:
    initSerial();
    initDHT();
    initWiFiManager();
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
  WiFiManagerParameter custom_mqtt_station("station", "Nome da estacao", mqtt_station, 10);
  wifiManager.addParameter(&custom_mqtt_station);
  WiFiManagerParameter custom_mqtt_server("server", "Servidor MQTT", mqtt_server, 40);
  wifiManager.addParameter(&custom_mqtt_server);
  WiFiManagerParameter custom_mqtt_port("port", "Porta do Servidor", mqtt_port, 6);
  wifiManager.addParameter(&custom_mqtt_port);
  WiFiManagerParameter custom_mqtt_topic("topic", "climaduino topic", mqtt_topic, 32);
  wifiManager.addParameter(&custom_mqtt_topic);
//  WiFiManagerParameter custom_mqtt_port("username", "Usuario do Broker MQTT", mqtt_username, 15);
//  wifiManager.addParameter(&custom_mqtt_username);
//  WiFiManagerParameter custom_mqtt_port("password", "Senha do Broker MQTT", mqtt_password, 6);
//  wifiManager.addParameter(&custom_mqtt_password);
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
  WiFiManagerParameter custom_mqtt_text3("<br/>Disponibilizado sob licenca BSD. <br/>");
  wifiManager.addParameter(&custom_mqtt_text3);

// Se quiser poderá setar um IP estático
//  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
   // Apagar informações para testes
 wifiManager.resetSettings();

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


  // Salvar os parametros de configuração
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_station"] = mqtt_station;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_topic"] = mqtt_topic;
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
  String lon = String(loc.lon, 7);
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

	      if (MQTT.connect(clientName)) {
           Serial.println("Conectado com sucesso ao broker MQTT!");
            MQTT.subscribe(mqtt_topic);
        }
        else 
        {
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

  location_t loc = location.getGeoFromWiFi();
  float h = dht.readHumidity();
  float t = dht.readTemperature(true);
  float f = dht.readTemperature();
  
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
  payload += String(loc.lat, 7);
  payload += ",\"longitude\":";
  payload += String(loc.lon, 7);
  payload += "}";
    
  if (t != oldT || h != oldH ) {
    sendDataMQTT(payload);
    oldT = t;
    oldH = h;
  }

  int cnt = REPORT_INTERVAL;

  while (cnt--)
    delay(1000);
  }

void sendDataMQTT(String payload) {
  if (!MQTT.connected()) {
    if (MQTT.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again");
      Serial.print("Topic is: ");
      Serial.println(mqtt_topic);
//      Serial.println(mqtt_latitude);
//      Serial.println(mqtt_longitude);
    }
    else {
      Serial.println("MQTT connect failed");
//      Serial.println("Will reset and try again...");
//      abort();
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
//    checkwifiemqtt();
    //localizacao Ip Geolocation
    ipgeolocation();
    // Exibe dados temperaty
    initDHT();
    // Envia dados temperatura
    dataDHT();
    //keep-alive da comunicação com broker MQTT
    MQTT.loop();
}

//String macToStr(const uint8_t* mac) {
//  String result;
//  for (int i = 0; i < 6; ++i) {
//    result += String(mac[i], 16);
//    if (i < 5)
//      result += ':';
//  }
//  return result;
//}
