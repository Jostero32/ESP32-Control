#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>

#include "StorageManager.h"

// Envío de las peticiones GET en una tarea FreeRTOS propia, para que los timeouts de
// red no bloqueen los botones ni el LED. Los eventos esperan en una cola pequeña en
// RAM (sin persistencia). Los fallos transitorios se reintentan desde una lista aparte
// (solo la usa la tarea HTTP), de modo que no ocupan la cola de pulsaciones nuevas.
// La conexión con la API se mantiene abierta entre peticiones (keep-alive) para no
// repetir DNS + handshake TLS en cada pulsación.
class ApiClient {
 public:
  enum class EnqueueResult { Queued, QueueFull, Disabled };

  bool begin(StorageManager& storage);

  EnqueueResult enqueue(uint8_t buttonIndex);
  void setEnabled(bool enabled) { enabled_.store(enabled); }
  bool isEnabled() const { return enabled_.load(); }
  // Deshabilita el envío y descarta los eventos en cola (cambio de modo ordenado).
  void stop();
  // true mientras la tarea procesa un evento (petición en curso).
  bool isBusy() const { return busy_.load(); }
  // Milisegundos que lleva la petición en curso (0 si no hay ninguna).
  uint32_t busyFor(uint32_t now) const { return busy_.load() ? now - busySince_.load() : 0; }

 private:
  struct ButtonEvent {
    uint8_t button;
    uint8_t attempt;    // Intentos ya realizados.
    uint32_t queuedAt;
    uint32_t retryAt;   // Solo para eventos en la lista de reintentos.
  };

  enum class Outcome { Done, Retry };

  static void taskEntry(void* arg);
  void run();
  Outcome process(const ButtonEvent& event);
  bool sendGet(WiFiClient& client, const String& requestUrl, int& code, String& body);
  void closeConnection();

  // Lista de reintentos (solo desde la tarea HTTP).
  void scheduleRetry(ButtonEvent event);
  bool takeDueRetry(uint32_t now, ButtonEvent& out);
  TickType_t ticksUntilNextRetry(uint32_t now) const;
  void clearRetries();

  StorageManager* storage_ = nullptr;
  QueueHandle_t queue_ = nullptr;
  TaskHandle_t task_ = nullptr;
  ButtonEvent retries_[cfg::HTTP_RETRY_SLOTS];
  uint8_t retryCount_ = 0;

  // Conexión persistente (solo desde la tarea HTTP).
  HTTPClient http_;
  WiFiClient plainClient_;
  WiFiClientSecure secureClient_;
  String connectedBaseUrl_;  // URL base de la conexión abierta; si cambia, se cierra.
  uint32_t lastRequestEnd_ = 0;

  std::atomic<bool> enabled_{false};
  std::atomic<bool> busy_{false};
  std::atomic<uint32_t> busySince_{0};
};
