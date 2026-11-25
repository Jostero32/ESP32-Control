#include "LedManager.h"

namespace {

// Duraciones de cada paso; los pasos pares encienden el LED y los impares lo apagan.
struct Sequence {
  const uint16_t* durations;
  uint8_t count;
};

constexpr uint16_t kDisconnected[] = {500, 500};
constexpr uint16_t kAccessPoint[] = {100, 150, 100, 1000};

Sequence sequenceFor(LedPattern pattern) {
  switch (pattern) {
    case LedPattern::Disconnected:
      return {kDisconnected, 2};
    case LedPattern::AccessPoint:
      return {kAccessPoint, 4};
    case LedPattern::Connected:
    default:
      return {nullptr, 0};  // Encendido fijo.
  }
}

}  // namespace

void LedManager::begin(uint8_t pin, bool activeHigh) {
  pin_ = pin;
  activeHigh_ = activeHigh;
  pinMode(pin_, OUTPUT);
  apply(LedPattern::Disconnected, millis());
}

void LedManager::setPattern(LedPattern pattern, uint32_t now) {
  if (pattern != pattern_) apply(pattern, now);
}

void LedManager::apply(LedPattern pattern, uint32_t now) {
  pattern_ = pattern;
  step_ = 0;
  stepStart_ = now;
  write(true);  // Los tres patrones empiezan con el LED encendido.
}

void LedManager::update(uint32_t now) {
  const Sequence seq = sequenceFor(pattern_);
  if (seq.count == 0 || now - stepStart_ < seq.durations[step_]) return;
  stepStart_ += seq.durations[step_];
  step_ = (step_ + 1) % seq.count;
  // Si el loop se retrasó más de un paso completo, se resincroniza en lugar de
  // encadenar pasos atrasados.
  if (now - stepStart_ >= seq.durations[step_]) stepStart_ = now;
  write(step_ % 2 == 0);
}

void LedManager::write(bool on) { digitalWrite(pin_, (on == activeHigh_) ? HIGH : LOW); }
