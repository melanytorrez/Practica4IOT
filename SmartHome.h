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

// Definition of the SmartHome class
class SmartHome {
public:
  // Public methods (the interface of our class)
  SmartHome(); // Constructor
  void setup();
  void loop();

private:
  // Private attributes (encapsulation)
  
  // Network clients
  WiFiClientSecure _wiFiClient;
  PubSubClient _mqttClient;

  // Hardware objects
  Servo _servoPuertaPrincipal, _servoPuertaHab, _servoPuertaCocina;
  Stepper _stepperMotor;
  MFRC522 _mfrc522;
  
  // Logical states of devices
  String _luzSalaState, _luzHabState, _luzCocinaState;
  String _puertaPrincipalState, _puertaHabState, _puertaCocinaState;
  String _ventanaSalaState;
  String _movimientoSalaState;
  bool _ventanaSalaAbierta;
  
  // Private methods (internal logic)
  void _setupHardware();
  void _setupNetwork();
  void _reconnectMQTT();
  void _publishShadowUpdate();
  void _handleMqttCallback(char* topic, byte* payload, unsigned int length);
  void _handleRfid();
  void _handleMotionSensor();
  void _abrirVentanaSala();
  void _cerrarVentanaSala();

  // Static function for the MQTT callback
  static void staticMqttCallback(char* topic, byte* payload, unsigned int length);
};

#endif // SMARTHOME_H