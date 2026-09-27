#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #include "secrets.example.h"
#endif

// Asignación de pines 
#define SS_PIN    5
#define RST_PIN   21
#define SERVO_PIN 13
#define LED_PIN   2

// Parámetros individuales 
const byte UID_AUTORIZADO[4] = {0x30, 0x4F, 0x8E, 0xC0};
const unsigned long INTERVALO_MUESTREO = 850;
const unsigned long TIEMPO_BLOQUEO = 13000;
const unsigned long TIEMPO_ACCESO = 1000;

const int RECHAZOS_MAX_BLOQUEO = 3;
const int CONFIRMACION_ALARMA = 2;
const int ESTADO_SEGURO_ANGULO = 0;
const int ACCESO_CONCEDIDO_ANGULO = 90;

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* mqttServer = SECRET_MQTT_SERVER;
const int mqttPort = SECRET_MQTT_PORT;

const char* mqttClientId = SECRET_MQTT_CLIENT_ID;
const char* mqttUsername = SECRET_MQTT_USERNAME;
const char* mqttPassword = SECRET_MQTT_PASSWORD;

const char* channelId = SECRET_CHANNEL_ID;

WiFiClient espClient;
PubSubClient MQTTClient(espClient);

// Declaraciones de funciones
void conectToWiFi();
void conectToMQTT();
void guardarAccesoAutorizado(String uid = "");
void guardarAccesoDenegado(String uid = "");
void manejarAccesoAutorizado(String uid = "");
void manejarAccesoDenegado(String uid = "");

enum EstadoSistema {
  ESTADO_NORMAL,
  ESTADO_AUTORIZADO,
  ESTADO_BLOQUEADO
};

MFRC522 *rfid = nullptr;
EstadoSistema estadoActual = ESTADO_NORMAL;
int rechazosConsecutivos = 0;
int contadorAutorizados = 0;
int contadorDenegados = 0;
unsigned long ultimoMuestreoMs = 0;
unsigned long inicioBloqueoMs = 0;
unsigned long inicioAccesoMs = 0;
unsigned long ultimoParpadeoMs = 0;
bool estadoLedBloqueo = false;

// Control PWM del servomotor
void posicionarServo(int angulo) {
  int duty = map(angulo, 0, 180, 1638, 8192);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(SERVO_PIN, duty);
#else
  ledcWrite(0, duty);
#endif
}

bool compararUID(byte *readUid, byte uidLength) {
  if (uidLength != 4) return false;
  for (byte i = 0; i < 4; i++) {
    if (readUid[i] != UID_AUTORIZADO[i]) return false;
  }
  return true;
}

// Función que guarda los accesos autorizados 
void guardarAccesoAutorizado(String uid) {
  contadorAutorizados++;

  Serial.print(" [REGISTRO] Acceso Autorizado guardado. UID: ");
  Serial.print(uid.length() > 0 ? uid : "N/A");
  Serial.println(" | Total Autorizados: " + String(contadorAutorizados));

  if (WiFi.status() == WL_CONNECTED) {
    if (!MQTTClient.connected()) {
      conectToMQTT();
    }
    MQTTClient.loop();

    String topic = "channels/" + String(channelId) + "/publish";
    String payload = "field1=" + String(contadorAutorizados) + "&field2=" + String(contadorDenegados) + "&status=Acceso Autorizado UID: " + uid;
    
    if (MQTTClient.publish(topic.c_str(), payload.c_str())) {
      Serial.println("Registro de Acceso Autorizado publicado.");
    } else {
      Serial.println("Error al publicar registro.");
    }
  }
}

// Función que guarda los accesos denegados 
void guardarAccesoDenegado(String uid) {
  contadorDenegados++;

  Serial.print(" [REGISTRO] Acceso Denegado guardado. UID: ");
  Serial.print(uid.length() > 0 ? uid : "N/A");
  Serial.println(" | Total Denegados: " + String(contadorDenegados));

  if (WiFi.status() == WL_CONNECTED) {
    if (!MQTTClient.connected()) {
      conectToMQTT();
    }
    MQTTClient.loop();

    String topic = "channels/" + String(channelId) + "/publish";
    String payload = "field1=" + String(contadorAutorizados) + "&field2=" + String(contadorDenegados) + "&status=Acceso Denegado UID: " + uid;
    
    if (MQTTClient.publish(topic.c_str(), payload.c_str())) {
      Serial.println("Registro de Acceso Denegado publicado.");
    } else {
      Serial.println("Error al publicar registro.");
    }
  }
}

void manejarAccesoAutorizado(String uid) {
  rechazosConsecutivos = 0;
  estadoActual = ESTADO_AUTORIZADO;
  inicioAccesoMs = millis();

  digitalWrite(LED_PIN, HIGH);
  posicionarServo(ACCESO_CONCEDIDO_ANGULO);

  Serial.println("ACCESO AUTORIZADO - Credencial válida.");
  
  // Guardar el acceso autorizado
  guardarAccesoAutorizado(uid);
}

void manejarAccesoDenegado(String uid) {
  rechazosConsecutivos++;
  posicionarServo(ESTADO_SEGURO_ANGULO);

  Serial.println(" ACCESO DENEGADO (Rechazo " + String(rechazosConsecutivos) + " de " + String(RECHAZOS_MAX_BLOQUEO) + ")");

  // Guardar el acceso denegado
  guardarAccesoDenegado(uid);

  if (rechazosConsecutivos == CONFIRMACION_ALARMA) {
    Serial.println(" CONFIRMACIÓN DE ALARMA: 2 rechazos consecutivos!");
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(150);
      digitalWrite(LED_PIN, LOW);
      delay(150);
    }
  } else {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
  }

  if (rechazosConsecutivos >= RECHAZOS_MAX_BLOQUEO) {
    estadoActual = ESTADO_BLOQUEADO;
    inicioBloqueoMs = millis();
    ultimoParpadeoMs = millis();
    estadoLedBloqueo = false;
    Serial.println(" Sistema BLOQUEADO por " + String(TIEMPO_BLOQUEO / 1000) + " s. Servomotor en Estado Seguro (" + String(ESTADO_SEGURO_ANGULO) + "°).");
  }
}

void actualizarEstadoBloqueo() {
  unsigned long ahoraMs = millis();
  posicionarServo(ESTADO_SEGURO_ANGULO);

  if (ahoraMs - ultimoParpadeoMs >= 250) {
    ultimoParpadeoMs = ahoraMs;
    estadoLedBloqueo = !estadoLedBloqueo;
    digitalWrite(LED_PIN, estadoLedBloqueo ? HIGH : LOW);
  }

  if (ahoraMs - inicioBloqueoMs >= TIEMPO_BLOQUEO) {
    estadoActual = ESTADO_NORMAL;
    rechazosConsecutivos = 0;
    digitalWrite(LED_PIN, LOW);
    Serial.println(" Bloqueo de 13s finalizado. Estado restablecido");
  }
}

void verificarEstadoAcceso() {
  if (estadoActual == ESTADO_AUTORIZADO) {
    if (millis() - inicioAccesoMs >= TIEMPO_ACCESO) {
      estadoActual = ESTADO_NORMAL;
      posicionarServo(ESTADO_SEGURO_ANGULO);
      digitalWrite(LED_PIN, LOW);
    }
  }
}

void procesarLecturaRFID() {
  if (rfid == nullptr) return;
  if (!rfid->PICC_IsNewCardPresent()) return;
  if (!rfid->PICC_ReadCardSerial()) return;

  String uidLeido = "";
  for (byte i = 0; i < rfid->uid.size; i++) {
    if (rfid->uid.uidByte[i] < 0x10) uidLeido += "0";
    uidLeido += String(rfid->uid.uidByte[i], HEX);
    if (i < rfid->uid.size - 1) uidLeido += " ";
  }
  uidLeido.toUpperCase();

  Serial.println(" Tarjeta detectada. UID: " + uidLeido);

  if (compararUID(rfid->uid.uidByte, rfid->uid.size)) {
    manejarAccesoAutorizado(uidLeido);
  } else {
    manejarAccesoDenegado(uidLeido);
  }

  rfid->PICC_HaltA();
  rfid->PCD_StopCrypto1();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  conectToWiFi();

  MQTTClient.setServer(mqttServer, mqttPort);
  conectToMQTT();

  Serial.println(" Control de Acceso RFID listo. Esperando lectura...");

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(SERVO_PIN, 50, 16);
#else
  ledcSetup(0, 50, 16);
  ledcAttachPin(SERVO_PIN, 0);
#endif
  posicionarServo(ESTADO_SEGURO_ANGULO);

  rfid = new MFRC522(SS_PIN, RST_PIN);
  pinMode(SS_PIN, OUTPUT);
  digitalWrite(SS_PIN, HIGH);
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, HIGH);

  SPI.begin(18, 19, 23, SS_PIN);

  rfid->PCD_WriteRegister(rfid->TxModeReg, 0x00);
  rfid->PCD_WriteRegister(rfid->RxModeReg, 0x00);
  rfid->PCD_WriteRegister(rfid->ModWidthReg, 0x26);
  rfid->PCD_WriteRegister(rfid->TModeReg, 0x80);
  rfid->PCD_WriteRegister(rfid->TPrescalerReg, 0xA9);
  rfid->PCD_WriteRegister(rfid->TReloadRegH, 0x03);
  rfid->PCD_WriteRegister(rfid->TReloadRegL, 0xE8);
  rfid->PCD_WriteRegister(rfid->TxASKReg, 0x40);
  rfid->PCD_WriteRegister(rfid->ModeReg, 0x3D);
  rfid->PCD_AntennaOn();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!MQTTClient.connected()) {
      conectToMQTT();
    }
    MQTTClient.loop();
  }

  unsigned long ahoraMs = millis();

  if (estadoActual == ESTADO_BLOQUEADO) {
    actualizarEstadoBloqueo();
    return;
  }

  verificarEstadoAcceso();

  if (ahoraMs - ultimoMuestreoMs >= INTERVALO_MUESTREO) {
    ultimoMuestreoMs = ahoraMs;
    procesarLecturaRFID();
  }
}

void conectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.println("Conectando a wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Conectado a wifi!");
}

void conectToMQTT() {
  while (!MQTTClient.connected()) {
    Serial.print("Conectando a MQTT...");
    if(MQTTClient.connect(
      mqttClientId,
      mqttUsername,
      mqttPassword
    )) {
      Serial.println("conectado a MQTT!");
    }
    else{
      Serial.print("Fallo al conectar a MQTT. Estado: ");
      Serial.println(MQTTClient.state());
      delay(500);
    }
  }
}
