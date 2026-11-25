#include "ButtonManager.h"

void DebouncedButton::begin(uint8_t pin) {
  pin_ = pin;
  pinMode(pin_, INPUT_PULLUP);
  // Estado inicial "suelto": un botón ya presionado al arrancar se registra como una
  // pulsación cuando su lectura se estabiliza.
  rawPressed_ = false;
  stablePressed_ = false;
  lastRawChange_ = millis();
}

void DebouncedButton::update(uint32_t now) {
  justPressed_ = false;
  const bool raw = digitalRead(pin_) == LOW;
  if (raw != rawPressed_) {
    rawPressed_ = raw;
    lastRawChange_ = now;
  }
  if (raw != stablePressed_ && now - lastRawChange_ >= cfg::DEBOUNCE_MS) {
    stablePressed_ = raw;
    if (stablePressed_) {
      justPressed_ = true;
      pressedAt_ = now;
    }
  }
}

void ButtonManager::begin() {
  for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) actions_[i].begin(cfg::PIN_BUTTONS[i]);
  config_.begin(cfg::PIN_CONFIG_BUTTON);
}

void ButtonManager::update(uint32_t now) {
  for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) {
    actions_[i].update(now);
    if (actions_[i].justPressed()) pressedMask_ |= static_cast<uint8_t>(1U << i);
  }

  config_.update(now);
  if (!config_.isPressed()) {
    configHoldReported_ = false;
  } else if (!configHoldReported_ && now - config_.pressedAt() >= cfg::CONFIG_HOLD_MS) {
    configHoldReported_ = true;  // No se repite hasta soltar y volver a pulsar.
    configLongPress_ = true;
  }
}

uint8_t ButtonManager::takePressedMask() {
  const uint8_t mask = pressedMask_;
  pressedMask_ = 0;
  return mask;
}

bool ButtonManager::takeConfigLongPress() {
  const bool fired = configLongPress_;
  configLongPress_ = false;
  return fired;
}
