#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "secrets.h"  // copie secrets.example.h para secrets.h e preencha

// Pinos do sensor HC-SR04
const int trigPin = 17;  // TX2
const int echoPin = 16;  // RX2

// Distância (cm) abaixo da qual a vaga é considerada ocupada
const float OCCUPIED_DISTANCE_CM = 20.0;
// Tempo máximo esperando o eco (~5 m de alcance)
const unsigned long ECHO_TIMEOUT_US = 30000;
// Janela no boot para entrar no modo de configuração pela serial
const unsigned long CONFIG_WINDOW_MS = 5000;

Preferences prefs;  // Wi-Fi e ID da vaga ficam salvos na memória flash (NVS)

String wifiSsid = "";
String wifiPassword = "";
String spotId = "";
String lastStatus = "";  // último status enviado

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

String readSerialLine() {
  while (!Serial.available()) {
    delay(100);
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  return line;
}

void scanNetworks() {
  Serial.println("Escaneando redes Wi-Fi...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("Nenhuma rede encontrada");
    return;
  }
  Serial.print(n);
  Serial.println(" redes encontradas");
  for (int i = 0; i < n; ++i) {
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (");
    Serial.print(WiFi.RSSI(i));
    Serial.print(")");
    Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
    delay(10);
  }
  Serial.println();
}

void loadConfig() {
  prefs.begin("parking", true);
  wifiSsid = prefs.getString("ssid", "");
  wifiPassword = prefs.getString("password", "");
  spotId = prefs.getString("spot_id", "");
  prefs.end();
}

void saveConfig() {
  prefs.begin("parking", false);
  prefs.putString("ssid", wifiSsid);
  prefs.putString("password", wifiPassword);
  prefs.putString("spot_id", spotId);
  prefs.end();
}

bool isConfigured() {
  return wifiSsid.length() > 0 && spotId.length() > 0;
}

// Pede Wi-Fi e ID da vaga pela serial e salva na memória
void configureViaSerial() {
  scanNetworks();

  Serial.println("Digite o nome da rede Wi-Fi:");
  wifiSsid = readSerialLine();
  Serial.print("Rede: ");
  Serial.println(wifiSsid);

  Serial.println("Digite a senha da rede Wi-Fi:");
  wifiPassword = readSerialLine();
  Serial.print("Senha: ");
  for (unsigned int i = 0; i < wifiPassword.length(); i++) {
    Serial.print("*");
  }
  Serial.println();

  Serial.println("Digite o ID da vaga de estacionamento:");
  spotId = readSerialLine();
  Serial.print("ID da vaga: ");
  Serial.println(spotId);

  saveConfig();
  Serial.println("Configuração salva.");
}

// Se já está configurado, dá alguns segundos para o usuário pedir reconfiguração
bool userWantsToReconfigure() {
  Serial.print("Pressione 'c' + Enter em ");
  Serial.print(CONFIG_WINDOW_MS / 1000);
  Serial.println("s para reconfigurar Wi-Fi/ID da vaga...");
  unsigned long start = millis();
  while (millis() - start < CONFIG_WINDOW_MS) {
    if (Serial.available()) {
      String answer = Serial.readStringUntil('\n');
      answer.trim();
      return answer == "c" || answer == "C";
    }
    delay(50);
  }
  return false;
}

bool connectWiFi() {
  Serial.print("Conectando ao Wi-Fi ");
  Serial.println(wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Falha ao conectar ao Wi-Fi.");
    return false;
  }
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

void connectAWS() {
  // ID único por sensor: dois clientes com o mesmo ID derrubam um ao outro
  String clientId = "tech4parking-" + spotId;

  Serial.println("Conectando ao AWS IoT...");
  int retries = 0;
  while (!mqttClient.connect(clientId.c_str()) && retries < 5) {
    Serial.print("Falha na conexão, rc=");
    Serial.print(mqttClient.state());
    Serial.println(". Tentando novamente em 5 segundos...");
    delay(5000);
    retries++;
  }

  if (mqttClient.connected()) {
    Serial.println("Conectado ao AWS IoT");
  } else {
    Serial.println("Não foi possível conectar ao AWS IoT após 5 tentativas");
  }
}

// Retorna a distância em cm, ou -1 se não houve eco (leitura inválida)
float readDistanceCm() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) {
    return -1;
  }
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(9600);
  delay(500);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  loadConfig();
  if (!isConfigured() || userWantsToReconfigure()) {
    configureViaSerial();
  } else {
    Serial.print("ID da vaga: ");
    Serial.println(spotId);
  }

  if (!connectWiFi()) {
    Serial.println("Verifique as credenciais (reinicie e pressione 'c' para reconfigurar).");
    delay(10000);
    ESP.restart();
  }

  wifiClient.setCACert(AWS_CERT_CA);
  wifiClient.setCertificate(AWS_CERT_CRT);
  wifiClient.setPrivateKey(AWS_CERT_PRIVATE);
  mqttClient.setServer(AWS_IOT_ENDPOINT, 8883);
  connectAWS();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexão Wi-Fi perdida. Reconectando...");
    if (!connectWiFi()) {
      delay(5000);
      return;
    }
  }

  if (!mqttClient.connected()) {
    connectAWS();
  }
  mqttClient.loop();

  float distance = readDistanceCm();
  if (distance < 0) {
    Serial.println("Leitura inválida do sensor (sem eco), ignorando.");
    delay(5000);
    return;
  }

  String currentStatus = (distance < OCCUPIED_DISTANCE_CM) ? "ocupada" : "disponível";

  Serial.print("Distância: ");
  Serial.print(distance);
  Serial.print(" cm, Status: ");
  Serial.println(currentStatus);

  if (currentStatus != lastStatus && mqttClient.connected()) {
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
}
