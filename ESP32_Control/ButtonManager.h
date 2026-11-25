#pragma once

#include <Arduino.h>

#include "Config.h"

// Pulsador entre GPIO y GND con INPUT_PULLUP (LOW = pulsado) y antirrebote por tiempo.
class DebouncedButton {
 public:
  void begin(uint8_t pin);
  void update(uint32_t now);

  bool isPressed() const { return stablePressed_; }
  bool justPressed() const { return justPressed_; }  // Solo durante la actualización del flanco.
  uint32_t pressedAt() const { return pressedAt_; }

 private:
  uint8_t pin_ = 0;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool justPressed_ = false;
  uint32_t lastRawChange_ = 0;
  uint32_t pressedAt_ = 0;
};

class ButtonManager {
 public:
  void begin();
  void update(uint32_t now);

  // Bit i = el botón de envío i se pulsó desde la última llamada. Un único evento por
  // pulsación: mantenerlo presionado no genera repeticiones.
  uint8_t takePressedMask();
  // true una sola vez cuando el botón de configuración lleva CONFIG_HOLD_MS presionado.
  bool takeConfigLongPress();

 private:
  DebouncedButton actions_[cfg::BUTTON_COUNT];
  DebouncedButton config_;
  uint8_t pressedMask_ = 0;
  bool configLongPress_ = false;
  bool configHoldReported_ = false;
};
