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

#define SS_PIN    5
#define RST_PIN   21
#define SERVO_PIN 13
#define LED_PIN   2

const char* DEVICE_ID = "IOT-A6F8B4147B";
const char* VARIABLE_NOMBRE = "identificador RFID";
const char* UNIDAD_TIEMPO = "segundos de bloqueo";

const byte UID_AUTORIZADO[4] = {0x30, 0x4F, 0x8E, 0xC0};

const unsigned long INTERVALO_MUESTREO   = 850;   
const unsigned long PERIODO_TELEMETRIA   = 20000; // 20 segundos (20000 ms)
const unsigned long INTERVALO_RECONEXION = 6000;  
const unsigned long TIEMPO_BLOQUEO       = 13000; 
const unsigned long TIEMPO_ACCESO        = 1000;  

const int RECHAZOS_MAX_BLOQUEO   = 3;
const int CONFIRMACION_ALARMA    = 2;
const int ESTADO_SEGURO_ANGULO   = 0;  
const int ACCESO_CONCEDIDO_ANGULO = 90; 

// TOPICS MQTT ASIGNADOS 
const char* TOPIC_BASE      = "iot/a6f8b4147b";
const char* TOPIC_TELEMETRY = "iot/a6f8b4147b/telemetry";
const char* TOPIC_STATUS    = "iot/a6f8b4147b/status";
const char* TOPIC_COMMAND   = "iot/a6f8b4147b/command";
const char* TOPIC_ALERT     = "iot/a6f8b4147b/alert";

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* mqttServer = SECRET_MQTT_SERVER;
const int mqttPort = SECRET_MQTT_PORT;
const char* mqttClientId = SECRET_MQTT_CLIENT_ID;
const char* mqttUsername = SECRET_MQTT_USERNAME;
const char* mqttPassword = SECRET_MQTT_PASSWORD;

WiFiClient espClient;
PubSubClient MQTTClient(espClient);

enum ModoOperacion {
  MODO_AUTO,
  MODO_ABRIR,
  MODO_CERRAR
};

enum EstadoSistema {
  ESTADO_NORMAL,
  ESTADO_AUTORIZADO,
  ESTADO_BLOQUEADO
};

MFRC522 *rfid = nullptr;
ModoOperacion modoActual = MODO_AUTO;
EstadoSistema estadoActual = ESTADO_NORMAL;

int rechazosConsecutivos = 0;
unsigned long sequenceNumber = 1;

unsigned long ultimoMuestreoMs   = 0;
unsigned long ultimaTelemetriaMs = 0;
unsigned long ultimaReconexionMs = 0;
unsigned long inicioBloqueoMs    = 0;
unsigned long inicioAccesoMs     = 0;
unsigned long ultimoParpadeoMs   = 0;
bool estadoLedBloqueo            = false;

void posicionarServo(int angulo);
bool compararUID(byte *readUid, byte uidLength);
void procesarLecturaRFID();
void manejarAccesoAutorizado(String uid);
void manejarAccesoDenegado(String uid);
void actualizarEstadoBloqueo();
void verificarEstadoAcceso();
void publicarTelemetriaJSON();
void publicarAlertaInmediata(String evento, String detalles = "");
void procesarComandoMQTT(String comando);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void verificarConexionRed();


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


String obtenerNombreModo() {
  switch (modoActual) {
    case MODO_ABRIR:  return "ABRIR";
    case MODO_CERRAR: return "CERRAR";
    case MODO_AUTO:
    default:          return "AUTO";
  }
}


void publicarTelemetriaJSON() {
  if (!MQTTClient.connected()) return;

  float valorSegundos = 0.0;
  if (estadoActual == ESTADO_BLOQUEADO) {
    unsigned long transcurrido = millis() - inicioBloqueoMs;
    if (transcurrido < TIEMPO_BLOQUEO) {
      valorSegundos = (float)(TIEMPO_BLOQUEO - transcurrido) / 1000.0f;
    }
  }

  bool estaEnAlarma = (estadoActual == ESTADO_BLOQUEADO) || (rechazosConsecutivos >= CONFIRMACION_ALARMA);


  String payload = "{";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"variable\":\"" + String(VARIABLE_NOMBRE) + "\",";
  payload += "\"value\":" + String(valorSegundos, 1) + ",";
  payload += "\"unit\":\"" + String(UNIDAD_TIEMPO) + "\",";
  payload += "\"mode\":\"" + obtenerNombreModo() + "\",";
  payload += "\"alarm\":" + String(estaEnAlarma ? "true" : "false") + ",";
  payload += "\"sequence\":" + String(sequenceNumber++);
  payload += "}";

  if (MQTTClient.publish(TOPIC_TELEMETRY, payload.c_str())) {
    String formattedSerial = " MQTT TELEMETRÍA #" + String(sequenceNumber - 1) + "] {\r\n";
    formattedSerial += "  \"device_id\": \"" + String(DEVICE_ID) + "\",\r\n";
    formattedSerial += "  \"variable\": \"" + String(VARIABLE_NOMBRE) + "\",\r\n";
    formattedSerial += "  \"value\": " + String(valorSegundos, 1) + ",\r\n";
    formattedSerial += "  \"unit\": \"" + String(UNIDAD_TIEMPO) + "\",\r\n";
    formattedSerial += "  \"mode\": \"" + obtenerNombreModo() + "\",\r\n";
    formattedSerial += "  \"alarm\": " + String(estaEnAlarma ? "true" : "false") + ",\r\n";
    formattedSerial += "  \"sequence\": " + String(sequenceNumber - 1) + "\r\n}";
    Serial.println(formattedSerial);
  } else {
    Serial.println(" Fallo al publicar telemetría.");
  }
}


void publicarAlertaInmediata(String evento, String detalles) {
  if (!MQTTClient.connected()) return;

  String payload = "{";
  payload += "\"event\":\"" + evento + "\",";
  if (detalles.length() > 0) {
    payload += "\"details\":\"" + detalles + "\",";
  }

  MQTTClient.publish(TOPIC_ALERT, payload.c_str());

  String formattedSerial = " MQTT ALERTA INMEDIATA {\r\n";
  formattedSerial += "  \"event\": \"" + evento + "\",\r\n";
  if (detalles.length() > 0) {
    formattedSerial += "  \"details\": \"" + detalles + "\",\r\n";
  }

  Serial.println(formattedSerial);
}


void procesarComandoMQTT(String comando) {
  comando.trim();
  

  if (comando.startsWith("{") && comando.endsWith("}")) {
    int idx = comando.indexOf("\"command\"");
    if (idx != -1) {
      int startQuote = comando.indexOf("\"", idx + 9);
      if (startQuote != -1) {
        int endQuote = comando.indexOf("\"", startQuote + 1);
        if (endQuote != -1) {
          comando = comando.substring(startQuote + 1, endQuote);
        }
      }
    }
  }

  comando.toUpperCase();

  Serial.println(" MQTT RECIBIDO - Comando: '" + comando + "'");

  if (comando == "AUTO") {
    modoActual = MODO_AUTO;
    posicionarServo(ESTADO_SEGURO_ANGULO);
    digitalWrite(LED_PIN, LOW);
    Serial.println(" [MQTT RECIBIDO] Comando 'AUTO': Reactiva el control automático por sensor RFID (muestreo a 850 ms).");
    MQTTClient.publish(TOPIC_STATUS, "{\"status\":\"OK\",\"mode\":\"AUTO\",\"message\":\"Modo automatico activado\"}");
  }
  else if (comando == "ABRIR") {
    modoActual = MODO_ABRIR;
    posicionarServo(ACCESO_CONCEDIDO_ANGULO);
    digitalWrite(LED_PIN, HIGH);
    Serial.println(" [MQTT RECIBIDO] Comando 'ABRIR': Apertura manual activada. Servomotor a 90° y LED encendido.");
    MQTTClient.publish(TOPIC_STATUS, "{\"status\":\"OK\",\"mode\":\"ABRIR\",\"message\":\"Apertura manual activada (Servo 90deg)\"}");
  }
  else if (comando == "CERRAR") {
    modoActual = MODO_CERRAR;
    posicionarServo(ESTADO_SEGURO_ANGULO);
    digitalWrite(LED_PIN, LOW);
    Serial.println(" [MQTT RECIBIDO] Comando 'CERRAR': Cierre manual activado. Servomotor a 0° (Estado Seguro) y LED apagado.");
    MQTTClient.publish(TOPIC_STATUS, "{\"status\":\"OK\",\"mode\":\"CERRAR\",\"message\":\"Cierre manual activado (Servo 0deg)\"}");
  }
  else {
    Serial.println(" [MQTT RECHAZADO] Comando 'COMANDO_DESCONOCIDO' o inválido: '" + comando + "'. Rechazado sin mover el actuador ni reiniciar el ESP32.");

    String statusErr = "{\"status\":\"RECHAZADO\",\"error\":\"COMANDO_DESCONOCIDO\",\"command\":\"" + comando + "\"}";
    MQTTClient.publish(TOPIC_STATUS, statusErr.c_str());
  }
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  if (String(topic) == TOPIC_COMMAND) {
    procesarComandoMQTT(mensaje);
  }
}

void manejarAccesoAutorizado(String uid) {
  rechazosConsecutivos = 0;
  estadoActual = ESTADO_AUTORIZADO;
  inicioAccesoMs = millis();

  digitalWrite(LED_PIN, HIGH);
  posicionarServo(ACCESO_CONCEDIDO_ANGULO);

  Serial.println(" ACCESO AUTORIZADO - Credencial válida: " + uid);
  publicarAlertaInmediata("ACCESO_AUTORIZADO", "UID: " + uid);
}

void manejarAccesoDenegado(String uid) {
  rechazosConsecutivos++;
  posicionarServo(ESTADO_SEGURO_ANGULO);

  Serial.println(" ACCESO DENEGADO (Rechazo " + String(rechazosConsecutivos) + " de " + String(RECHAZOS_MAX_BLOQUEO) + ") - UID: " + uid);

  if (rechazosConsecutivos == 1) {
    publicarAlertaInmediata("RECHAZO_UNICO", "UID: " + uid + " | Rechazo 1 de 3");
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
  }
  else if (rechazosConsecutivos == CONFIRMACION_ALARMA) {
    Serial.println(" CONFIRMACIÓN DE ALARMA: 2 rechazos consecutivos!");
    publicarAlertaInmediata("CONFIRMACION_ALARMA", "2 rechazos consecutivos detectados");
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(150);
      digitalWrite(LED_PIN, LOW);
      delay(150);
    }
  }

  if (rechazosConsecutivos >= RECHAZOS_MAX_BLOQUEO) {
    estadoActual = ESTADO_BLOQUEADO;
    inicioBloqueoMs = millis();
    ultimoParpadeoMs = millis();
    estadoLedBloqueo = false;

    Serial.println(" SISTEMA BLOQUEADO por 13 segundos.");
    publicarAlertaInmediata("SISTEMA_BLOQUEADO", "3 rechazos consecutivos. Bloqueo 13s activado");
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
    Serial.println(" Bloqueo de 13s finalizado. Estado seguro restablecido.");
    
    if (MQTTClient.connected()) {
      MQTTClient.publish(TOPIC_STATUS, "{\"status\":\"OK\",\"event\":\"BLOQUEO_FINALIZADO\",\"message\":\"Sistema desbloqueado\"}");
    }
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

void verificarConexionRed() {
  unsigned long ahoraMs = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (ahoraMs - ultimaReconexionMs >= INTERVALO_RECONEXION) {
      ultimaReconexionMs = ahoraMs;
      Serial.println(" Intentando reconexión a Wi-Fi...");
      WiFi.begin(ssid, password);
    }
    return;
  }

  if (!MQTTClient.connected()) {
    if (ahoraMs - ultimaReconexionMs >= INTERVALO_RECONEXION) {
      ultimaReconexionMs = ahoraMs;
      Serial.print(" Intentando reconectar a Mosquitto (" + String(mqttServer) + ":" + String(mqttPort) + ")... ");
      
      if (MQTTClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
        Serial.println("¡Conectado exitosamente!");
        MQTTClient.subscribe(TOPIC_COMMAND);
        MQTTClient.publish(TOPIC_STATUS, "{\"status\":\"CONECTADO\",\"device_id\":\"IOT-A6F8B4147B\"}");
      } else {
        Serial.println("Fallo. Estado MQTT: " + String(MQTTClient.state()));
      }
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);

  WiFi.begin(ssid, password);

  MQTTClient.setServer(mqttServer, mqttPort);
  MQTTClient.setCallback(mqttCallback);

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

  Serial.println(" Sistema Inicializado. Esperando lecturas RFID o Comandos MQTT...");
}

void loop() {
  verificarConexionRed();
  if (MQTTClient.connected()) {
    MQTTClient.loop();
  }

  unsigned long ahoraMs = millis();

  if (estadoActual == ESTADO_BLOQUEADO) {
    actualizarEstadoBloqueo();
  } else {
    verificarEstadoAcceso();

    if (modoActual == MODO_AUTO && (ahoraMs - ultimoMuestreoMs >= INTERVALO_MUESTREO)) {
      ultimoMuestreoMs = ahoraMs;
      procesarLecturaRFID();
    }
  }

  if (ahoraMs - ultimaTelemetriaMs >= PERIODO_TELEMETRIA) {
    ultimaTelemetriaMs = ahoraMs;
    publicarTelemetriaJSON();
  }
}
