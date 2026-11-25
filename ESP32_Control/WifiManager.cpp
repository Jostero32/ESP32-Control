#include "WifiManager.h"

#include <WiFi.h>

#include <algorithm>

#include "Config.h"
#include "Log.h"

namespace {
constexpr const char* TAG = "WIFI";

bool reached(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }

// Desconecta la estación sin bloquear. En el core 3.x, WiFi.disconnect() espera hasta
// 100 ms a que se complete; disconnectAsync() no espera. En 2.x disconnect() ya es inmediato.
void disconnectStation(bool eraseDriverConfig) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  WiFi.disconnectAsync(false, eraseDriverConfig);
#else
  WiFi.disconnect(false, eraseDriverConfig);
#endif
}
}  // namespace

void WifiManager::begin() {
  // Las credenciales viven solo en nuestro espacio NVS; el SDK no guarda su propia copia
  // ni se conecta por su cuenta.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  // MAC base de fábrica (eFuse, igual a la MAC de la estación). A diferencia de
  // WiFi.macAddress(), es válida antes de iniciar la radio en los cores 2.x y 3.x.
  // Los bytes se almacenan del menos al más significativo: byte i = bits 8i..8i+7.
  const uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", static_cast<unsigned>((mac >> 32) & 0xFF),
           static_cast<unsigned>((mac >> 40) & 0xFF));
  apSsid_ = String(cfg::AP_SSID_PREFIX) + suffix;
}

IPAddress WifiManager::apIp() const {
  return IPAddress(cfg::AP_IP[0], cfg::AP_IP[1], cfg::AP_IP[2], cfg::AP_IP[3]);
}

const char* WifiManager::stateName(WifiState state) {
  switch (state) {
    case WifiState::Idle:
      return "INACTIVO";
    case WifiState::Connecting:
      return "CONECTANDO";
    case WifiState::Connected:
      return "CONECTADO";
    case WifiState::Disconnected:
      return "DESCONECTADO";
    case WifiState::AccessPoint:
      return "ACCESS_POINT";
  }
  return "?";
}

void WifiManager::setState(WifiState state) {
  if (state == state_) return;
  LOGF(TAG, "Estado: %s -> %s", stateName(state_), stateName(state));
  state_ = state;
}

void WifiManager::beginStation(const WifiCredentials& credentials) {
  credentials_ = credentials;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // Los reintentos los gestiona update().
  startConnectAttempt(millis());
}

void WifiManager::startConnectAttempt(uint32_t now) {
  ++attemptCount_;
  LOGF(TAG, "Conectando a \"%s\" (intento %lu)...", credentials_.ssid.c_str(),
       static_cast<unsigned long>(attemptCount_));
  const char* pass = credentials_.password.length() > 0 ? credentials_.password.c_str() : nullptr;
  WiFi.begin(credentials_.ssid.c_str(), pass);  // No bloqueante.
  attemptStart_ = now;
  setState(WifiState::Connecting);
}

void WifiManager::onConnected() {
  attemptCount_ = 0;
  setState(WifiState::Connected);
  LOGF(TAG, "Conectado a \"%s\". IP %s, RSSI %d dBm", credentials_.ssid.c_str(),
       WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void WifiManager::update(uint32_t now) {
  switch (state_) {
    case WifiState::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        onConnected();
      } else if (now - attemptStart_ >= cfg::WIFI_CONNECT_TIMEOUT_MS) {
        LOGF(TAG, "No se pudo conectar (estado %d). Nuevo intento en %lu s", WiFi.status(),
             static_cast<unsigned long>(cfg::WIFI_RETRY_INTERVAL_MS / 1000));
        disconnectStation(false);
        retryAt_ = now + cfg::WIFI_RETRY_INTERVAL_MS;
        setState(WifiState::Disconnected);
      }
      break;

    case WifiState::Connected:
      if (WiFi.status() != WL_CONNECTED) {
        LOGF(TAG, "Conexión Wi-Fi perdida (estado %d)", WiFi.status());
        retryAt_ = now + cfg::WIFI_LOST_RETRY_DELAY_MS;
        setState(WifiState::Disconnected);
      }
      break;

    case WifiState::Disconnected:
      if (WiFi.status() == WL_CONNECTED) {
        onConnected();
      } else if (reached(now, retryAt_)) {
        startConnectAttempt(now);
      }
      break;

    case WifiState::AccessPoint:
      pollScan();
      break;

    case WifiState::Idle:
      break;
  }
}

bool WifiManager::startAccessPoint() {
  LOGF(TAG, "Iniciando Access Point \"%s\"", apSsid_.c_str());
  WiFi.setAutoReconnect(false);
  // eraseap=true vacía solo la configuración STA en RAM del driver (persistent(false)),
  // no las credenciales guardadas en NVS. Evita que el reintento único que el core hace
  // tras un primer fallo de conexión vuelva a conectar la STA con el portal activo.
  if (WiFi.getMode() & WIFI_MODE_STA) disconnectStation(true);
  // AP+STA: la interfaz STA no se conecta a ninguna red, solo se usa para escanear.
  WiFi.mode(WIFI_AP_STA);

  const char* pass = strlen(cfg::AP_PASSWORD) >= 8 ? cfg::AP_PASSWORD : nullptr;
  const bool started = WiFi.softAP(apSsid_.c_str(), pass, cfg::AP_CHANNEL, 0, cfg::AP_MAX_CLIENTS);
  const IPAddress ip = apIp();
  const IPAddress mask(255, 255, 255, 0);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  // El core 3.x solo anuncia un DNS por DHCP si se indica: se anuncia el propio ESP32
  // para que los clientes usen el DNS cautivo. (IPAddress() = rango DHCP por defecto.)
  const bool configured = started && WiFi.softAPConfig(ip, ip, mask, IPAddress(), ip);
#else
  // En el core 2.x el servidor DHCP ya anuncia la IP del AP como DNS.
  const bool configured = started && WiFi.softAPConfig(ip, ip, mask);
#endif
  setState(WifiState::AccessPoint);

  if (!started || !configured) {
    LOGF(TAG, "Error al iniciar el Access Point (softAP=%d, config=%d)", started, configured);
    return false;
  }
  LOGF(TAG, "Access Point activo: SSID \"%s\" (%s), IP %s", apSsid_.c_str(),
       pass ? "WPA2" : "abierta", WiFi.softAPIP().toString().c_str());
  return true;
}

bool WifiManager::startScan() {
  if (state_ != WifiState::AccessPoint) return false;
  if (scanning_) return true;
  const int16_t result = WiFi.scanNetworks(true /* async */, false /* ocultas */);
  scanning_ = result == WIFI_SCAN_RUNNING;
  if (!scanning_) LOGF(TAG, "No se pudo iniciar el escaneo (código %d)", result);
  return scanning_;
}

void WifiManager::pollScan() {
  if (!scanning_) return;
  const int16_t count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) return;
  scanning_ = false;
  if (count < 0) {
    LOGF(TAG, "El escaneo de redes falló (código %d)", count);
    return;
  }

  results_.clear();
  for (int16_t i = 0; i < count; ++i) {
    ScanResult entry;
    entry.ssid = WiFi.SSID(i);
    if (entry.ssid.length() == 0) continue;  // Redes ocultas: se escriben manualmente.
    entry.rssi = WiFi.RSSI(i);
    entry.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    auto existing = std::find_if(results_.begin(), results_.end(),
                                 [&](const ScanResult& r) { return r.ssid == entry.ssid; });
    if (existing == results_.end()) {
      results_.push_back(entry);
    } else if (entry.rssi > existing->rssi) {
      *existing = entry;  // Varios puntos de acceso con el mismo SSID: se queda el más fuerte.
    }
  }
  WiFi.scanDelete();
  std::sort(results_.begin(), results_.end(),
            [](const ScanResult& a, const ScanResult& b) { return a.rssi > b.rssi; });
  if (results_.size() > cfg::MAX_SCAN_RESULTS) results_.resize(cfg::MAX_SCAN_RESULTS);
  scanDone_ = true;
  LOGF(TAG, "Escaneo completado: %u redes", static_cast<unsigned>(results_.size()));
}

void WifiManager::shutdown() {
  WiFi.scanDelete();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, false);
}
