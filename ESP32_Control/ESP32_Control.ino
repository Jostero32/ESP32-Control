// ESP32-Control
// Cuatro botones que envían peticiones HTTP GET configurables, con portal cautivo
// para la configuración Wi-Fi y una página administrativa protegida.
//
// El loop principal no bloquea (solo cede 1 ms al final): botones, LED, Wi-Fi y portal se atienden en cada
// vuelta, y las peticiones HTTP se ejecutan en su propia tarea (ApiClient).

#include <Arduino.h>

#include "AdminAuth.h"
#include "ApiClient.h"
#include "ButtonManager.h"
#include "Config.h"
#include "ConfigPortal.h"
#include "LedManager.h"
#include "Log.h"
#include "StorageManager.h"
#include "WifiManager.h"

// Sin namespace anónimo: el preprocesador de Arduino genera prototipos automáticamente
// y no siempre los sitúa bien dentro de un namespace. Se usa `static` (enlace interno).

static constexpr const char* TAG = "APP";

// Modo de funcionamiento. El estado detallado de la conexión (conectando, conectado,
// desconectado) lo lleva WifiManager.
enum class AppMode : uint8_t {
  Station,        // Funcionamiento normal: botones -> HTTP.
  SwitchingToAp,  // Esperando a que termine la petición HTTP en curso.
  AccessPoint,    // Portal de configuración activo hasta guardar red o reiniciar.
};

static StorageManager storage;
static WifiManager wifi;
static AdminAuth adminAuth(storage);
static ApiClient apiClient;
static ButtonManager buttons;
static LedManager led;
static ConfigPortal portal(storage, wifi, adminAuth);

static AppMode mode = AppMode::Station;
static uint32_t apSwitchStartedAt = 0;

static void startAccessPointMode() {
  wifi.startAccessPoint();
  portal.begin();
  adminAuth.printProvisioningInfo();
  mode = AppMode::AccessPoint;
}

static void requestAccessPointMode(uint32_t now) {
  LOGF(TAG, "Botón de configuración mantenido %lu s: activando el modo Access Point",
       static_cast<unsigned long>(cfg::CONFIG_HOLD_MS / 1000));
  apiClient.stop();  // Deshabilita el envío y vacía la cola.
  apSwitchStartedAt = now;
  mode = AppMode::SwitchingToAp;
}

static void completeAccessPointSwitch(uint32_t now) {
  if (apiClient.isBusy()) {
    if (now - apSwitchStartedAt < cfg::AP_SWITCH_MAX_WAIT_MS) return;  // Sigue esperando.
    LOGF(TAG, "Aviso: la petición HTTP en curso no terminó a tiempo; se interrumpe");
  } else if (now != apSwitchStartedAt) {
    LOGF(TAG, "Operaciones HTTP detenidas (%lu ms)", static_cast<unsigned long>(now - apSwitchStartedAt));
  }
  startAccessPointMode();
}

// Solo en modo normal: en modo AP un reinicio cerraría el portal que se está usando.
static void checkHttpHang(uint32_t now) {
  if (mode != AppMode::Station || apiClient.busyFor(now) < cfg::HTTP_HANG_RESTART_MS) return;
  LOGF(TAG, "ERROR: la petición HTTP lleva %lu s bloqueada; reiniciando el dispositivo",
       static_cast<unsigned long>(apiClient.busyFor(now) / 1000));
  Serial.flush();
  ESP.restart();
}

static void handleButtonPress(uint8_t index) {
  const unsigned number = index + 1;
  if (mode != AppMode::Station) {
    LOGF(TAG, "Botón %u: evento descartado (modo Access Point)", number);
    return;
  }
  if (!wifi.isConnected()) {
    LOGF(TAG, "Botón %u: evento descartado (sin conexión Wi-Fi, estado %s)", number,
         WifiManager::stateName(wifi.state()));
    return;
  }
  String missing;
  if (!storage.isApiConfigComplete(&missing)) {
    LOGF(TAG, "Botón %u: evento descartado (configuración HTTP incompleta; falta: %s)", number,
         missing.c_str());
    return;
  }
  switch (apiClient.enqueue(index)) {
    case ApiClient::EnqueueResult::Queued:
      LOGF(TAG, "Botón %u: pulsación aceptada", number);
      break;
    case ApiClient::EnqueueResult::QueueFull:
      LOGF(TAG, "Botón %u: evento descartado (cola llena, %u eventos)", number, cfg::EVENT_QUEUE_LENGTH);
      break;
    case ApiClient::EnqueueResult::Disabled:
      LOGF(TAG, "Botón %u: evento descartado (envío HTTP deshabilitado)", number);
      break;
  }
}

static void updateLed(uint32_t now) {
  LedPattern pattern = LedPattern::Disconnected;
  if (mode != AppMode::Station) {
    pattern = LedPattern::AccessPoint;  // Prioridad sobre los demás patrones.
  } else if (wifi.isConnected()) {
    pattern = LedPattern::Connected;
  }
  led.setPattern(pattern, now);
  led.update(now);
}

static void logConfigurationStatus() {
  String missing;
  if (storage.isApiConfigComplete(&missing)) {
    LOGF(TAG, "Configuración HTTP: válida");
  } else {
    LOGF(TAG, "Configuración HTTP: incompleta (falta: %s). No se enviarán peticiones.", missing.c_str());
  }
}


void setup() {
  Serial.begin(115200);
  LOGF(TAG, "ESP32-Control iniciando");
  LOGF(TAG, "GPIO: botones %u/%u/%u/%u, configuración %u, LED %u", cfg::PIN_BUTTONS[0],
       cfg::PIN_BUTTONS[1], cfg::PIN_BUTTONS[2], cfg::PIN_BUTTONS[3], cfg::PIN_CONFIG_BUTTON,
       cfg::PIN_LED);

  led.begin(cfg::PIN_LED, cfg::LED_ACTIVE_HIGH);
  buttons.begin();
  storage.begin();
  wifi.begin();
  apiClient.begin(storage);

  WifiCredentials credentials;
  if (storage.hasWifiCredentials() && storage.loadWifiCredentials(credentials)) {
    LOGF(TAG, "Wi-Fi: credenciales guardadas para \"%s\"", credentials.ssid.c_str());
    wifi.beginStation(credentials);
    mode = AppMode::Station;
    apiClient.setEnabled(true);
  } else {
    LOGF(TAG, "Wi-Fi: no hay credenciales guardadas; se inicia el portal de configuración");
    startAccessPointMode();
  }

  // Con la radio ya activa, el RNG hardware es apto para generar la sal del hash.
  adminAuth.begin();
  logConfigurationStatus();
}

void loop() {
  const uint32_t now = millis();

  buttons.update(now);
  wifi.update(now);

  if (buttons.takeConfigLongPress()) {
    if (mode == AppMode::Station) {
      requestAccessPointMode(now);
    } else {
      LOGF(TAG, "Botón de configuración: el modo Access Point ya está activo");
    }
  }

  checkHttpHang(now);
  if (mode == AppMode::SwitchingToAp) completeAccessPointSwitch(now);
  if (mode == AppMode::AccessPoint) portal.update(now);

  const uint8_t pressed = buttons.takePressedMask();
  for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) {
    if (pressed & (1U << i)) handleButtonPress(i);
  }

  updateLed(now);

  // Imprescindible: la tarea HTTP comparte núcleo y prioridad con el loop. Sin esta pausa
  // las peticiones se quedan colgadas (comprobado en la placa, también con esp_http_client).
  delay(1);
}
