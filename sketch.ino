#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h" 

// ===================== CONFIGURAÇÃO =====================
const char* MATRICULA = "0000000";                  
const char* MQTT_CLIENT_ID = "COLE_AQUI_SEU_CLIENT_ID";  
const char* MQTT_USUARIO   = "COLE_AQUI_SEU_USUARIO";
const char* MQTT_SENHA     = "COLE_AQUI_SEU_SENHA";      
const unsigned long CANAL_ID = 0000000;      
// ========================================================

const int PINO_DHT     = 15;
const int PINO_LED     = 2;    
const int PINO_BUZZER  = 4;

DHTesp       dht;
WiFiClient   wifi;
PubSubClient mqtt(wifi);

int  limite;
bool alarme = false;
unsigned long ultimaLeitura = 0;

int calcularLimite(const char* mat) {
  int n = strlen(mat);
  int ultimos = (n >= 2) ? atoi(mat + n - 2) : atoi(mat);
  return 24 + (ultimos % 6);
}

bool decidirAlarme(float t, bool alarmeAtual) {
  if (!alarmeAtual && t > limite)      return true;
  if (alarmeAtual  && t < limite - 1)  return false;
  return alarmeAtual;
}

void atualizarSaidas(bool ligado) {
  digitalWrite(PINO_LED, ligado ? HIGH : LOW);
  if (ligado) tone(PINO_BUZZER, 1000, 200);
}

bool conectarThingSpeak() {
  mqtt.setServer("mqtt3.thingspeak.com", 1883);
  bool ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA);
  Serial.println(ok ? "Conectado ao ThingSpeak" : "Falhou: confira as credenciais");
  return ok;
}

void publicarNoCanal(float t, float h, bool alarmeAtivo) {
  char topico[48], dados[96];
  snprintf(topico, sizeof(topico), "channels/%lu/publish", CANAL_ID);
  snprintf(dados, sizeof(dados), "field1=%.1f&field2=%.0f%s", t, h, alarmeAtivo ? "&status=ALERTA" : "");
  
  bool ok = mqtt.publish(topico, dados);
  Serial.printf("%s %s em %s\n", ok ? "Publicado" : "Falhou", dados, topico);
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  dht.setup(PINO_DHT, DHTesp::DHT22);
  
  limite = calcularLimite(MATRICULA);
  
  Serial.printf("Caminho C | matricula=%s | limite=%d C\n", MATRICULA, limite);

  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" conectado!");

  conectarThingSpeak();
}

void loop() {
  if (!mqtt.connected()) {
    conectarThingSpeak();
  }
  mqtt.loop();
  if (millis() - ultimaLeitura >= 20000) {
    ultimaLeitura = millis();
    TempAndHumidity leitura = dht.getTempAndHumidity();
    
    if (dht.getStatus() != DHTesp::ERROR_NONE) {
      Serial.println("Erro de leitura do DHT");
      return; 
    }

    alarme = decidirAlarme(leitura.temperature, alarme);
    atualizarSaidas(alarme);

    publicarNoCanal(leitura.temperature, leitura.humidity, alarme);
  }
}