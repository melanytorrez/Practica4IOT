#include "SmartHome.h"

SmartHome myHome;

void setup() {
  Serial.begin(115200);
  myHome.setup();
}

void loop() {
  myHome.loop();
}