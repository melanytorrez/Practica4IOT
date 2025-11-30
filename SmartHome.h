#ifndef SMARTHOME_H
#define SMARTHOME_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <Stepper.h>
#include <SPI.h>
#include <MFRC522.h>
#include <time.h>

// Definición de la clase SmartHome
class SmartHome {
public:
  // Métodos públicos (la interfaz de nuestra clase)
  SmartHome(); // Constructor
  void setup();
  void loop();

private:
  // Atributos privados (encapsulamiento)
  
  // Clientes de red
  WiFiClientSecure _wiFiClient;
  PubSubClient _mqttClient;

  // Objetos de hardware
  Servo _servoPuertaPrincipal, _servoPuertaHab, _servoPuertaCocina;
  Stepper _stepperMotor;
  MFRC522 _mfrc522;
  
  // Estados lógicos de los dispositivos
  String _luzSalaState, _luzHabState, _luzCocinaState;
  String _puertaPrincipalState, _puertaHabState, _puertaCocinaState;
  String _ventanaSalaState;
  String _movimientoSalaState;
  bool _ventanaSalaAbierta;
  
  // Métodos privados (lógica interna)
  void _setupHardware();
  void _setupNetwork();
  void _reconnectMQTT();
  void _publishShadowUpdate();
  void _handleMqttCallback(char* topic, byte* payload, unsigned int length);
  void _handleRfid();
  void _handleMotionSensor();
  void _abrirVentanaSala();
  void _cerrarVentanaSala();

  // Función estática para el callback de MQTT
  static void staticMqttCallback(char* topic, byte* payload, unsigned int length);
};

#endif // SMARTHOME_H