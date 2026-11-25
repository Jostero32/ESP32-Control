#pragma once

#include <Arduino.h>

enum class LedPattern : uint8_t {
  Connected,     // Encendido fijo.
  Disconnected,  // 500 ms encendido / 500 ms apagado.
  AccessPoint,   // 100 on / 150 off / 100 on / 1000 off.
};

// Patrones temporizados con millis(), sin delay().
class LedManager {
 public:
  void begin(uint8_t pin, bool activeHigh);
  void setPattern(LedPattern pattern, uint32_t now);
  void update(uint32_t now);

 private:
  void apply(LedPattern pattern, uint32_t now);
  void write(bool on);

  uint8_t pin_ = 0;
  bool activeHigh_ = true;
  LedPattern pattern_ = LedPattern::Disconnected;
  uint8_t step_ = 0;
  uint32_t stepStart_ = 0;
};
