#include "SmartHome.h"

// Crea una instancia global de nuestra casa inteligente
SmartHome myHome;

void setup() {
  Serial.begin(115200);
  myHome.setup(); // Llama al método de configuración de la clase
}

void loop() {
  myHome.loop(); // Llama al método de bucle principal de la clase
}