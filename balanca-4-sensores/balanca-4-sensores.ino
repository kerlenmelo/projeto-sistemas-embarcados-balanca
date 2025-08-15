#include <HX711.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ========== CONFIGURAÇÕES ==========
const char* ssid = "PocoX5PRO";
const char* password = "12345678";

const char* mqtt_server = "b07178fc6fac4463b89886cd7fc1892e.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "balanca_bot";
const char* mqtt_password = "balancaBot123";
const char* mqtt_client_id = "Balança";

const char* topic_peso = "balanca/peso";
const char* topic_status = "balanca/status";
// =====================================

// Conexões do HX711 
const int CELULA_DADO = 16;  // GPIO16 (DT)
const int CELULA_CLOCK = 17; // GPIO17 (SCK)

// Peso conhecido para calibração (em kg)
const float PESO_CALIBRACAO = 1.1; // Use 1.0 para 1kg, 0.5 para 500g, etc.

HX711 balanca;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Conecta ao WiFi
  Serial.println("Conectando ao WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print(".");
  }
  Serial.println("WiFi Conectado.");
  delay(500);

  // Configura MQTT
  espClient.setInsecure(); // ignora verificação SSL
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
 
  // Inicializa a balança
  Serial.println("Iniciando balança digital...");
  balanca.begin(CELULA_DADO, CELULA_CLOCK);
  
  Serial.println("Realizando tara...");
  balanca.tare(20); // Realiza a tara com 20 leituras
  Serial.println("Tara concluída!");
  mqttPublishWithQoS(topic_status, "Tara concluída", 1);
  delay(500);
  
  // Calibração da Balança
  Serial.print("Em até 10seg coloque um peso conhecido de ");
  Serial.print(PESO_CALIBRACAO);
  Serial.println(" kg na balança");
  Serial.println("Calibrando...");
  mqttPublishWithQoS(topic_status, "Aguardando peso para calibração", 1);
  delay(10000); // Tempo para colocar o peso
  
  // Realiza a calibração
  calibrarBalanca(PESO_CALIBRACAO);
  
  Serial.println("Calibração concluída!");
  mqttPublishWithQoS(topic_status, "Calibração concluída", 1);
}

// ========== LOOP ==========
void loop() {
  if (!mqttClient.connected()) {
    reconectarMQTT();
  }
  mqttClient.loop();

  static unsigned long lastPublish = 0;
  if (millis() - lastPublish > 2000) { // Publica a cada 2 segundos
    float peso = balanca.get_units(10);
    
    Serial.print("Peso: ");
    Serial.print(peso, 1);
    Serial.println(" kg");
    
    // Publica no MQTT
    char payload[20];
    dtostrf(peso, 1, 1, payload); // Converte float para string
    mqttPublish(topic_peso, payload);
    
    lastPublish = millis();
  }
}

// Função para calibrar a balança
void calibrarBalanca(float pesoConhecido) {
  // Obtém a leitura bruta com o peso
  float leituraComPeso = balanca.get_value(20); // 20 leituras
  
  // Calcula o fator de escala
  float fatorEscala = leituraComPeso / pesoConhecido;
  
  // Aplica o fator de escala
  balanca.set_scale(fatorEscala);
  
  // Exibe informações da calibração
  Serial.print("Leitura bruta: ");
  Serial.println(leituraComPeso);
  Serial.print("Fator de escala calculado: ");
  Serial.println(fatorEscala);

  // Publica informações no broker MQTT
  char calibMsg[50];
  snprintf(calibMsg, sizeof(calibMsg), "Calibrado: fator=%.2f", fatorEscala);
  mqttPublishWithQoS(topic_status, calibMsg, 1);
}

// Função para publicar mensagem em tópicos do broker MQTT
void mqttPublish(const char* topic, const char* payload) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, payload);
  }
}

// Função para publicação com QoS específico
void mqttPublishWithQoS(const char* topic, const char* payload, int qos) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, payload, true); // retained=true pode ser útil para status
    // Nota: A biblioteca PubSubClient padrão não suporta QoS diretamente
    // Esta é uma implementação simplificada
  }
}

// Função para manipular mensagens recebidas no broker MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Manipule mensagens recebidas aqui se necessário
}

// Função para reconectar ao broker MQTT
void reconectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.println("Conectando ao MQTT...");
    if (mqttClient.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("Conectado ao MQTT");
      mqttPublish(topic_status, "Conectado ao broker MQTT");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Tentando novamente em 5s...");
      delay(5000);
    }
  }
}