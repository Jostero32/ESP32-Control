#pragma once

#include <Arduino.h>

// Registro por Serial. Se usa una única llamada a printf por línea para que los
// mensajes del loop principal y de la tarea HTTP no se intercalen a mitad de línea.
#define LOGF(tag, fmt, ...) \
  Serial.printf("[%9lu] %-6s " fmt "\n", static_cast<unsigned long>(millis()), tag, ##__VA_ARGS__)
