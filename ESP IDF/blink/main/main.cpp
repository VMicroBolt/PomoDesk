#include <Arduino.h>

#include "../../../ESP IDF/blink/main/PomoDesk_V1_1.cpp"



extern "C" void app_main(void) {
  initArduino();
  setup();

  while (true) {
    loop();
  }
}