#include "SmartHome.h"

// --- CONSTANTES ---
// (Movimos todas las constantes aquí para mantener el .h limpio)
#define PIR_PIN 23
#define LED_SALA_PIN 22
#define LED_HAB_PIN 21
#define LED_COCINA_PIN 19
#define SERVO_PUERTA_PRINCIPAL_PIN 18
#define SERVO_PUERTA_HAB_PIN 5
#define SERVO_PUERTA_COCINA_PIN 17
#define RFID_SS_PIN 15
#define RFID_RST_PIN 2
#define STEPS_PER_REV 2048
#define IN1 32
#define IN2 33
#define IN3 25
#define IN4 26

const char* WIFI_SSID = "Gabriel";
const char* WIFI_PASS = "1234567890";
const char* MQTT_BROKER = "a30eisqpnafvzg-ats.iot.us-east-1.amazonaws.com";
const char* THING_NAME  = "MiCasa";

const char* UPDATE_TOPIC = "$aws/things/MiCasa/shadow/update";
const char* UPDATE_DELTA_TOPIC = "$aws/things/MiCasa/shadow/update/delta";
const char* RFID_CHECK_REQUEST_TOPIC = "MiCasa/rfid/checkRequest";
const char* RFID_CHECK_RESPONSE_TOPIC = "MiCasa/rfid/checkResponse";

// Certificados (los mismos que antes)
extern const char AMAZON_ROOT_CA1[] PROGMEM;
extern const char CERTIFICATE[] PROGMEM;
extern const char PRIVATE_KEY[] PROGMEM;

// --- TRUCO PARA EL CALLBACK ---
// Necesitamos una instancia global accesible por la función estática
SmartHome* home_instance = nullptr;

void SmartHome::staticMqttCallback(char* topic, byte* payload, unsigned int length) {
  if (home_instance) {
    home_instance->_handleMqttCallback(topic, payload, length);
  }
}

// Interrupción del PIR
volatile bool motionDetectedFlag = false;
void IRAM_ATTR pirISR() {
  motionDetectedFlag = true;
}


// --- IMPLEMENTACIÓN DE LA CLASE SMARTHOME ---

SmartHome::SmartHome() : 
  _mqttClient(_wiFiClient), 
  _stepperMotor(STEPS_PER_REV, IN1, IN3, IN2, IN4), 
  _mfrc522(RFID_SS_PIN, RFID_RST_PIN) {
  home_instance = this; // Guardamos la instancia actual para el callback
}

void SmartHome::setup() {
  _setupHardware();
  _setupNetwork();
}

void SmartHome::loop() {
  if (!_mqttClient.connected()) {
    _reconnectMQTT();
  }
  _mqttClient.loop();
  
  _handleMotionSensor();
  _handleRfid();
}

void SmartHome::_setupHardware() {
  pinMode(LED_SALA_PIN, OUTPUT);
  pinMode(LED_HAB_PIN,  OUTPUT);
  pinMode(LED_COCINA_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT_PULLUP);

  _servoPuertaPrincipal.attach(SERVO_PUERTA_PRINCIPAL_PIN);
  _servoPuertaHab.attach(SERVO_PUERTA_HAB_PIN);
  _servoPuertaCocina.attach(SERVO_PUERTA_COCINA_PIN);

  _servoPuertaPrincipal.write(0);
  _servoPuertaHab.write(0);
  _servoPuertaCocina.write(0);

  SPI.begin();
  _mfrc522.PCD_Init();
  
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), pirISR, CHANGE);
  
  // Inicializar estados
  _luzSalaState = "OFF";
  _luzHabState = "OFF";
  _luzCocinaState = "OFF";
  _puertaPrincipalState = "CLOSED";
  _puertaHabState = "CLOSED";
  _puertaCocinaState = "CLOSED";
  _ventanaSalaState = "CLOSED";
  _movimientoSalaState = "NOT_DETECTED";
  _ventanaSalaAbierta = false;
}

void SmartHome::_setupNetwork() {
  Serial.print("Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  Serial.print("Configurando hora desde servidor NTP...");
  configTime(0, 0, "pool.ntp.org");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(" Falló al obtener la hora");
    return;
  }
  Serial.println(" Hora configurada correctamente.");

  _wiFiClient.setCACert(AMAZON_ROOT_CA1);
  _wiFiClient.setCertificate(CERTIFICATE);
  _wiFiClient.setPrivateKey(PRIVATE_KEY);

  _mqttClient.setServer(MQTT_BROKER, 8883);
  _mqttClient.setCallback(SmartHome::staticMqttCallback);
}

void SmartHome::_reconnectMQTT() {
  while (!_mqttClient.connected()) {
    Serial.print("Conectando MQTT...");
    if (_mqttClient.connect(THING_NAME)) {
      Serial.println("Conectado a AWS IoT.");
      _mqttClient.subscribe(UPDATE_DELTA_TOPIC);
      _mqttClient.subscribe(RFID_CHECK_RESPONSE_TOPIC);
      _publishShadowUpdate();
    } else {
      Serial.print("Fallo, rc="); Serial.print(_mqttClient.state());
      Serial.println(" Reintentando en 5s...");
      delay(5000);
    }
  }
}

void SmartHome::_publishShadowUpdate() {
  StaticJsonDocument<512> doc;
  JsonObject reported = doc.createNestedObject("state").createNestedObject("reported");
  reported["luz_sala"] = _luzSalaState;
  reported["luz_habitacion"] = _luzHabState;
  reported["luz_cocina"] = _luzCocinaState;
  reported["puerta_principal"] = _puertaPrincipalState;
  reported["puerta_habitacion"] = _puertaHabState;
  reported["puerta_cocina"] = _puertaCocinaState;
  reported["ventana_sala"] = _ventanaSalaState;
  reported["movimiento_sala"] = _movimientoSalaState;
  char buffer[512];
  serializeJson(doc, buffer);
  _mqttClient.publish(UPDATE_TOPIC, buffer);
  Serial.println("Shadow reportado:");
  Serial.println(buffer);
}

void SmartHome::_abrirVentanaSala() {
  if (!_ventanaSalaAbierta) {
    _stepperMotor.setSpeed(10);
    _stepperMotor.step(STEPS_PER_REV / 4);
    _ventanaSalaAbierta = true;
  }
}

void SmartHome::_cerrarVentanaSala() {
  if (_ventanaSalaAbierta) {
    _stepperMotor.setSpeed(10);
    _stepperMotor.step(-STEPS_PER_REV / 4);
    _ventanaSalaAbierta = false;
  }
}

void SmartHome::_handleMqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido en: ");
  Serial.println(topic);
  String topicStr = String(topic);

  if (topicStr.equals(RFID_CHECK_RESPONSE_TOPIC)) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload, length);
    String status = doc["status"];
    Serial.print("Respuesta de validación RFID: "); Serial.println(status);
    if (status == "VALID") {
      Serial.println("Acceso concedido. Abriendo puerta principal.");
      _servoPuertaPrincipal.write(90);
      _puertaPrincipalState = "OPEN";
      _publishShadowUpdate();
      delay(5000); // Mantener abierta por 5 segundos
      _servoPuertaPrincipal.write(0);
      _puertaPrincipalState = "CLOSED";
      _publishShadowUpdate();
    } else {
      Serial.println("Acceso denegado.");
    }
    return;
  }

  if (topicStr.equals(UPDATE_DELTA_TOPIC)) {
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, payload, length);
    if (!doc.containsKey("state")) return;
    JsonObject state = doc["state"];
    bool changed = false;

    if (state.containsKey("luz_sala")) {
      String desired = state["luz_sala"];
      if (desired != _luzSalaState) { digitalWrite(LED_SALA_PIN, desired == "ON" ? HIGH : LOW); _luzSalaState = desired; changed = true; }
    }
    if (state.containsKey("luz_habitacion")) {
      String desired = state["luz_habitacion"];
      if (desired != _luzHabState) { digitalWrite(LED_HAB_PIN, desired == "ON" ? HIGH : LOW); _luzHabState = desired; changed = true; }
    }
    if (state.containsKey("luz_cocina")) {
      String desired = state["luz_cocina"];
      if (desired != _luzCocinaState) { digitalWrite(LED_COCINA_PIN, desired == "ON" ? HIGH : LOW); _luzCocinaState = desired; changed = true; }
    }
    if (state.containsKey("puerta_habitacion")) {
      String desired = state["puerta_habitacion"];
      if (desired != _puertaHabState) { _servoPuertaHab.write(desired == "OPEN" ? 90 : 0); _puertaHabState = desired; changed = true; }
    }
    if (state.containsKey("puerta_cocina")) {
      String desired = state["puerta_cocina"];
      if (desired != _puertaCocinaState) { _servoPuertaCocina.write(desired == "OPEN" ? 90 : 0); _puertaCocinaState = desired; changed = true; }
    }
    if (state.containsKey("ventana_sala")) {
      String desired = state["ventana_sala"];
      if (desired != _ventanaSalaState) {
        if (desired == "OPEN") _abrirVentanaSala();
        else _cerrarVentanaSala();
        _ventanaSalaState = desired;
        changed = true;
      }
    }

    if (changed) {
      _publishShadowUpdate();
    }
  }
}

void SmartHome::_handleMotionSensor() {
  if (motionDetectedFlag) {
    motionDetectedFlag = false;
    String nuevoEstado = digitalRead(PIR_PIN) == HIGH ? "DETECTED" : "NOT_DETECTED";
    if (nuevoEstado != _movimientoSalaState) {
      _movimientoSalaState = nuevoEstado;
      Serial.print("movimiento_sala: ");
      Serial.println(_movimientoSalaState);
      _publishShadowUpdate();
    }
  }
}

void SmartHome::_handleRfid() {
  if (_mfrc522.PICC_IsNewCardPresent() && _mfrc522.PICC_ReadCardSerial()) {
    String card_uid = "";
    for (byte i = 0; i < _mfrc522.uid.size; i++) {
      char hex[3];
      sprintf(hex, "%02X", _mfrc522.uid.uidByte[i]);
      card_uid += hex;
    }
    Serial.print("Tarjeta detectada, UID: "); Serial.println(card_uid);

    StaticJsonDocument<128> doc;
    doc["card_uid"] = card_uid;
    char json_buffer[128];
    serializeJson(doc, json_buffer);
    _mqttClient.publish(RFID_CHECK_REQUEST_TOPIC, json_buffer);
    Serial.println("Enviando petición de validación de RFID a la nube...");
    _mfrc522.PICC_HaltA();
    delay(2000);
  }
}