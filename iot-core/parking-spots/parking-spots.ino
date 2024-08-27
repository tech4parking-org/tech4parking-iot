#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Pinos do sensor HC-SR04
const int trigPin = 17;  // TX2
const int echoPin = 16;  // RX2

// Configurações AWS IoT
const char* AWS_IOT_ENDPOINT = "REMOVED";
const char* AWS_IOT_TOPIC = "parking_sensor";

// Certificados
const char* ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

const char* CERTIFICATE = "REMOVED";

const char* PRIVATE_KEY = "REMOVED";

const int EEPROM_SIZE = 512;
const int SPOT_ID_ADDRESS = 0;
const int SPOT_ID_MAX_LENGTH = 50;

String spotId = "";
String lastStatus = "";  // Variável para armazenar o último status enviado

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

void scanNetworks() {
  Serial.println("Escaneando redes Wi-Fi...");
  int n = WiFi.scanNetworks();
  Serial.println("Escaneamento concluído");
  if (n == 0) {
    Serial.println("Nenhuma rede encontrada");
  } else {
    Serial.print(n);
    Serial.println(" redes encontradas");
    for (int i = 0; i < n; ++i) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10);
    }
  }
  Serial.println("");
}

void getWiFiCredentials(String &ssid, String &password) {
  Serial.println("Por favor, digite o nome da rede Wi-Fi que deseja conectar:");
  while (!Serial.available()) {
    delay(100);
  }
  ssid = Serial.readStringUntil('\n');
  ssid.trim();
  
  Serial.print("Você digitou: ");
  Serial.println(ssid);
  
  Serial.println("Agora, digite a senha da rede Wi-Fi:");
  while (!Serial.available()) {
    delay(100);
  }
  password = Serial.readStringUntil('\n');
  password.trim();
  
  // Imprimir asteriscos para cada caractere da senha
  Serial.print("Senha inserida: ");
  for (int i = 0; i < password.length(); i++) {
    Serial.print("*");
  }
  Serial.println();
}

void connectWiFi() {
  scanNetworks();
  String ssid, password;
  getWiFiCredentials(ssid, password);
  
  Serial.println("Conectando ao WiFi...");
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado ao WiFi");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar ao WiFi. Por favor, verifique as credenciais e tente novamente.");
    ESP.restart(); // Reinicia o ESP32
  }
}

void setupSpotId() {
  loadSpotId();
  if (spotId.length() == 0 || askToChangeSpotId()) {
    getSpotId();
  }
}

void loadSpotId() {
  spotId = "";
  for (int i = 0; i < SPOT_ID_MAX_LENGTH; ++i) {
    char c = EEPROM.read(SPOT_ID_ADDRESS + i);
    if (c == 0 || c == 255) break;
    if (isPrintable(c)) {
      spotId += c;
    } else {
      break;
    }
  }
  
  if (spotId.length() > 0) {
    Serial.print("ID da vaga atual: ");
    Serial.println(spotId);
  } else {
    Serial.println("Nenhum ID de vaga configurado.");
  }
}

void getSpotId() {
  Serial.println("Por favor, digite o ID da vaga de estacionamento:");
  while (!Serial.available()) {
    delay(100);
  }
  spotId = Serial.readStringUntil('\n');
  spotId.trim();
  
  for (int i = 0; i < SPOT_ID_MAX_LENGTH; ++i) {
    EEPROM.write(SPOT_ID_ADDRESS + i, 0);
  }
  
  for (int i = 0; i < spotId.length(); ++i) {
    EEPROM.write(SPOT_ID_ADDRESS + i, spotId[i]);
  }
  EEPROM.commit();
  
  Serial.print("ID da vaga configurado: ");
  Serial.println(spotId);
}

bool askToChangeSpotId() {
  Serial.println("Deseja alterar o ID da vaga? (S/N)");
  while (!Serial.available()) {
    delay(100);
  }
  String response = Serial.readStringUntil('\n');
  response.trim();
  return (response == "S" || response == "s");
}

void connectAWS() {
  wifiClient.setCACert(ROOT_CA);
  wifiClient.setCertificate(CERTIFICATE);
  wifiClient.setPrivateKey(PRIVATE_KEY);

  mqttClient.setServer(AWS_IOT_ENDPOINT, 8883);

  Serial.println("Tentando conectar ao AWS IoT...");
  int retries = 0;
  while (!mqttClient.connect("ESP32Client") && retries < 5) {
    Serial.println("Falha na conexão. Tentando novamente em 5 segundos...");
    delay(5000);
    retries++;
  }

  if (mqttClient.connected()) {
    Serial.println("Conectado ao AWS IoT");
  } else {
    Serial.print("Falha na conexão, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" Não foi possível conectar após 5 tentativas");
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // Espera a porta serial conectar. Necessário apenas para placas com USB nativa
  }
  
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Falha ao iniciar EEPROM");
    return;
  }
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  connectWiFi();
  setupSpotId();
  connectAWS();
}

void loop() {
  if (!mqttClient.connected()) {
    connectAWS();
  }
  mqttClient.loop();

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * 0.034 / 2;
  
  String currentStatus = (distance < 20) ? "ocupada" : "disponível";
  
  Serial.print("Distância: ");
  Serial.print(distance);
  Serial.print(" cm, Status: ");
  Serial.println(currentStatus);
  
  if (currentStatus != lastStatus) {
    char payload[256];
    snprintf(payload, sizeof(payload), 
             "{\"spot_id\":\"%s\",\"status\":\"%s\",\"distance\":%.2f}", 
             spotId.c_str(), currentStatus.c_str(), distance);
    
    if (mqttClient.publish(AWS_IOT_TOPIC, payload)) {
      Serial.println("Mensagem publicada no AWS IoT");
      lastStatus = currentStatus;
    } else {
      Serial.println("Falha ao publicar mensagem");
    }
  }
  
  delay(5000);
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexão Wi-Fi perdida. Reconectando...");
    connectWiFi();
  }
}