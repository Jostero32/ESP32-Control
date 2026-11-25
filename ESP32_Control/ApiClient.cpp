#include "ApiClient.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "CaBundle.h"  // Certificados raíz de confianza para HTTPS.
#include "Config.h"
#include "Log.h"
#include "UrlUtils.h"

namespace {

constexpr const char* TAG = "HTTP";

bool hasCaBundle() {
  // Puntero volatile: impide que el compilador resuelva strstr al compilar (aviso -Waddress).
  const char* volatile pem = CA_BUNDLE_PEM;
  return strstr(pem, "-----BEGIN CERTIFICATE-----") != nullptr;
}

// 409 (Conflict): la API recibió la petición pero no la registra (p. ej. "Sin cobro
// pendiente de evaluar"). Repetirla no cambia nada, así que no se reintenta.
bool isAccepted(int code) { return code == HTTP_CODE_OK || code == HTTP_CODE_CONFLICT; }

// Errores de red/timeout (códigos negativos de HTTPClient) y respuestas que indican un
// problema temporal del servidor. Un 400/404 no se arregla repitiendo la petición, y
// repetir tras un 429 solo gasta más cupo del límite de la API.
bool isRetryable(int code) { return code < 0 || code == HTTP_CODE_REQUEST_TIMEOUT || code >= 500; }

uint32_t retryDelayMs(uint8_t attemptsDone) {
  uint32_t delay = cfg::HTTP_RETRY_BASE_DELAY_MS;
  for (uint8_t i = 1; i < attemptsDone && delay < cfg::HTTP_RETRY_MAX_DELAY_MS; ++i) delay *= 2;
  return delay < cfg::HTTP_RETRY_MAX_DELAY_MS ? delay : cfg::HTTP_RETRY_MAX_DELAY_MS;
}

}  // namespace

bool ApiClient::begin(StorageManager& storage) {
  storage_ = &storage;

  http_.setReuse(true);  // Keep-alive: la conexión sigue abierta si el servidor lo permite.
  http_.setConnectTimeout(cfg::HTTP_CONNECT_TIMEOUT_MS);
  http_.setTimeout(cfg::HTTP_RESPONSE_TIMEOUT_MS);
  http_.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
#if API_TLS_INSECURE
  secureClient_.setInsecure();
#else
  secureClient_.setCACert(CA_BUNDLE_PEM);
#endif
  secureClient_.setHandshakeTimeout(cfg::TLS_HANDSHAKE_TIMEOUT_S);

  queue_ = xQueueCreate(cfg::EVENT_QUEUE_LENGTH, sizeof(ButtonEvent));
  if (queue_ == nullptr) {
    LOGF(TAG, "Error: no se pudo crear la cola de eventos");
    return false;
  }
  if (xTaskCreatePinnedToCore(&ApiClient::taskEntry, "http_worker", cfg::HTTP_TASK_STACK, this,
                              cfg::HTTP_TASK_PRIORITY, &task_, cfg::HTTP_TASK_CORE) != pdPASS) {
    LOGF(TAG, "Error: no se pudo crear la tarea HTTP");
    return false;
  }
#if API_TLS_INSECURE
  LOGF(TAG, "AVISO: API_TLS_INSECURE=1, los certificados HTTPS NO se validan. Solo para desarrollo.");
#else
  if (!hasCaBundle()) {
    LOGF(TAG, "Aviso: CaBundle.h no contiene certificados; las peticiones HTTPS se rechazarán");
  }
#endif
  return true;
}

ApiClient::EnqueueResult ApiClient::enqueue(uint8_t buttonIndex) {
  if (!enabled_.load()) return EnqueueResult::Disabled;
  const ButtonEvent event{buttonIndex, 0, millis(), 0};
  return xQueueSend(queue_, &event, 0) == pdTRUE ? EnqueueResult::Queued : EnqueueResult::QueueFull;
}

void ApiClient::stop() {
  enabled_.store(false);
  const UBaseType_t pending = uxQueueMessagesWaiting(queue_);
  xQueueReset(queue_);
  if (pending > 0) {
    LOGF(TAG, "%u evento(s) pendiente(s) descartado(s) por cambio de modo", static_cast<unsigned>(pending));
  }
}

void ApiClient::taskEntry(void* arg) { static_cast<ApiClient*>(arg)->run(); }

void ApiClient::run() {
  ButtonEvent event;
  for (;;) {
    if (!enabled_.load()) clearRetries();

    // Primero los reintentos que ya tocan; si no hay, se esperan pulsaciones nuevas
    // como mucho hasta que venza el siguiente reintento.
    const uint32_t now = millis();
    if (!takeDueRetry(now, event) &&
        xQueueReceive(queue_, &event, ticksUntilNextRetry(now)) != pdTRUE) {
      continue;
    }
    // busy_ se marca antes de comprobar enabled_ (y stop() hace lo contrario), así que
    // quien cambia de modo ve la petición en curso o la tarea ve el envío deshabilitado.
    busySince_.store(millis());
    busy_.store(true);
    if (process(event) == Outcome::Retry) scheduleRetry(event);
    busy_.store(false);
  }
}

void ApiClient::scheduleRetry(ButtonEvent event) {
  const unsigned number = event.button + 1;
  const uint8_t attemptsDone = event.attempt + 1;
  if (!enabled_.load()) return;
  if (attemptsDone >= cfg::HTTP_MAX_ATTEMPTS) {
    LOGF(TAG, "Botón %u: evento descartado tras %u intentos fallidos", number, attemptsDone);
    return;
  }
  if (retryCount_ >= cfg::HTTP_RETRY_SLOTS) {
    LOGF(TAG, "Botón %u: evento descartado (lista de reintentos llena, %u eventos)", number,
         cfg::HTTP_RETRY_SLOTS);
    return;
  }
  const uint32_t delay = retryDelayMs(attemptsDone);
  event.attempt = attemptsDone;
  event.retryAt = millis() + delay;
  retries_[retryCount_++] = event;
  LOGF(TAG, "Botón %u: intento %u/%u en %lu ms", number, attemptsDone + 1, cfg::HTTP_MAX_ATTEMPTS,
       static_cast<unsigned long>(delay));
}

bool ApiClient::takeDueRetry(uint32_t now, ButtonEvent& out) {
  int best = -1;
  for (uint8_t i = 0; i < retryCount_; ++i) {
    if (static_cast<int32_t>(now - retries_[i].retryAt) < 0) continue;  // Aún no toca.
    if (best < 0 || static_cast<int32_t>(retries_[i].retryAt - retries_[best].retryAt) < 0) best = i;
  }
  if (best < 0) return false;
  out = retries_[best];
  retries_[best] = retries_[--retryCount_];
  return true;
}

TickType_t ApiClient::ticksUntilNextRetry(uint32_t now) const {
  if (retryCount_ == 0) return portMAX_DELAY;
  int32_t wait = static_cast<int32_t>(retries_[0].retryAt - now);
  for (uint8_t i = 1; i < retryCount_; ++i) {
    const int32_t remaining = static_cast<int32_t>(retries_[i].retryAt - now);
    if (remaining < wait) wait = remaining;
  }
  return wait <= 0 ? 0 : pdMS_TO_TICKS(wait) + 1;
}

void ApiClient::clearRetries() {
  if (retryCount_ == 0) return;
  LOGF(TAG, "%u reintento(s) pendiente(s) descartado(s) por cambio de modo", retryCount_);
  retryCount_ = 0;
}

ApiClient::Outcome ApiClient::process(const ButtonEvent& event) {
  const unsigned number = event.button + 1;

  if (!enabled_.load()) {
    LOGF(TAG, "Botón %u: evento descartado (envío deshabilitado por cambio de modo)", number);
    return Outcome::Done;
  }
  const uint32_t age = millis() - event.queuedAt;
  if (age > cfg::HTTP_EVENT_MAX_AGE_MS) {
    LOGF(TAG, "Botón %u: evento descartado (%lu s de antigüedad, intento %u/%u)", number,
         static_cast<unsigned long>(age / 1000), event.attempt + 1, cfg::HTTP_MAX_ATTEMPTS);
    return Outcome::Done;
  }
  if (!WiFi.isConnected()) {
    LOGF(TAG, "Botón %u: sin conexión Wi-Fi", number);
    return Outcome::Retry;
  }
  String missing;
  if (!storage_->isApiConfigComplete(&missing)) {
    LOGF(TAG, "Botón %u: evento descartado (configuración HTTP incompleta; falta: %s)", number,
         missing.c_str());
    return Outcome::Done;
  }

  const ApiConfig config = storage_->apiConfig();  // Copia: admite cambios en caliente.
  const String requestUrl = url::buildRequestUrl(config.baseUrl, config.params, config.paramCount,
                                                 config.valueParam, config.buttonValues[event.button]);
  const bool https = url::isHttps(requestUrl);
  LOGF(TAG, "Botón %u: GET %s (intento %u/%u, %lu ms desde la pulsación)", number, requestUrl.c_str(),
       event.attempt + 1, cfg::HTTP_MAX_ATTEMPTS, static_cast<unsigned long>(millis() - event.queuedAt));

#if !API_TLS_INSECURE
  if (https && !hasCaBundle()) {
    LOGF(TAG, "Botón %u: evento descartado (HTTPS sin certificados raíz en CaBundle.h)", number);
    return Outcome::Done;
  }
#endif
  // Cambio de servidor o conexión inactiva: se empieza con una conexión limpia.
  if (config.baseUrl != connectedBaseUrl_ || millis() - lastRequestEnd_ > cfg::HTTP_KEEPALIVE_IDLE_MS) {
    closeConnection();
    connectedBaseUrl_ = config.baseUrl;
  }
  WiFiClient& client = https ? static_cast<WiFiClient&>(secureClient_) : plainClient_;
  const bool reused = client.connected();

  int code = 0;
  String body;
  const uint32_t started = millis();
  if (!sendGet(client, requestUrl, code, body)) {
    LOGF(TAG, "Botón %u: evento descartado (HTTPClient no aceptó la URL)", number);
    return Outcome::Done;
  }
  if (code < 0 && reused) {
    // El servidor pudo cerrar la conexión mientras estaba inactiva: se repite al momento
    // con una conexión nueva, sin gastar un intento.
    LOGF(TAG, "Botón %u: la conexión reutilizada falló (%s); se abre una nueva", number,
         HTTPClient::errorToString(code).c_str());
    closeConnection();
    sendGet(client, requestUrl, code, body);
  }
  const unsigned long elapsed = millis() - started;
  lastRequestEnd_ = millis();
  if (code < 0) closeConnection();

  if (isAccepted(code)) {
    LOGF(TAG, "Botón %u: %s, HTTP %d (%lu ms%s)", number,
         code == HTTP_CODE_OK ? "ÉXITO" : "RECHAZADO por la API, no se reintenta", code, elapsed,
         reused ? ", conexión reutilizada" : "");
    if (code != HTTP_CODE_OK && body.length() > 0) LOGF(TAG, "Botón %u: respuesta: %s", number, body.c_str());
    return Outcome::Done;
  }
  if (code > 0) {
    LOGF(TAG, "Botón %u: ERROR, la API respondió HTTP %d (%lu ms)%s", number, code, elapsed,
         isRetryable(code) ? "" : ". Evento descartado sin reintentar");
    if (body.length() > 0) LOGF(TAG, "Botón %u: respuesta: %s", number, body.c_str());
  } else {
    LOGF(TAG, "Botón %u: ERROR de conexión/timeout: %s (%d, %lu ms)", number,
         HTTPClient::errorToString(code).c_str(), code, elapsed);
  }
  return isRetryable(code) ? Outcome::Retry : Outcome::Done;
}

// Envía el GET y lee el cuerpo completo: si quedaran datos sin leer, la siguiente
// petición por la misma conexión los confundiría con su respuesta.
bool ApiClient::sendGet(WiFiClient& client, const String& requestUrl, int& code, String& body) {
  if (!client.connected()) client.stop();  // Limpia una sesión que cerró el servidor.
  if (!http_.begin(client, requestUrl)) return false;
  code = http_.GET();
  body = code > 0 ? http_.getString() : String();
  body.replace('\n', ' ');
  body.trim();
  if (body.length() > 160) body = body.substring(0, 160) + "...";
  http_.end();  // Con keep-alive deja la conexión abierta.
  return true;
}

void ApiClient::closeConnection() {
  plainClient_.stop();
  secureClient_.stop();
}
