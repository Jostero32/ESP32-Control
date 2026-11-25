#include "ConfigPortal.h"

#include <WiFi.h>

#include "Config.h"
#include "Log.h"
#include "WebAssets.h"  // Interfaz web en PROGMEM, generada desde web/.

namespace {

constexpr const char* TAG = "PORTAL";
constexpr const char* kAuthHeader = "X-Auth-Token";

// Rutas de detección de portal cautivo. Cualquier respuesta distinta de la esperada
// (204, "Success", "Microsoft Connect Test"...) hace que el sistema abra el portal.
constexpr const char* kCaptiveProbes[] = {
    "/generate_204",               // Android / Chrome
    "/gen_204",                    // Android
    "/hotspot-detect.html",        // iOS / macOS
    "/library/test/success.html",  // iOS antiguos
    "/connecttest.txt",            // Windows 10/11
    "/ncsi.txt",                   // Windows antiguos
    "/redirect",                   // Windows
    "/canonical.html",             // Firefox
    "/success.txt",                // Firefox
};

String jsonString(const String& value) {
  String out;
  out.reserve(value.length() + 2);
  out += '"';
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(c));
          out += escaped;
        } else {
          out += c;
        }
    }
  }
  out += '"';
  return out;
}

const char* jsonBool(bool value) { return value ? "true" : "false"; }

}  // namespace

ConfigPortal::ConfigPortal(StorageManager& storage, WifiManager& wifi, AdminAuth& auth)
    : storage_(storage), wifi_(wifi), auth_(auth), server_(cfg::HTTP_PORT) {}

void ConfigPortal::begin() {
  if (active_) return;
  const IPAddress ip = wifi_.apIp();
  apIpText_ = ip.toString();

  dns_.setErrorReplyCode(DNSReplyCode::NoError);
  dns_.start(cfg::DNS_PORT, "*", ip);  // Todas las consultas resuelven al ESP32.

  static const char* headers[] = {kAuthHeader};
  server_.collectHeaders(headers, 1);
  registerRoutes();
  server_.begin();
  active_ = true;
  LOGF(TAG, "Portal activo: http://%s/ (o http://%s/ si el DNS del cliente lo permite)",
       apIpText_.c_str(), cfg::PORTAL_HOSTNAME);
}

void ConfigPortal::stop() {
  if (!active_) return;
  server_.stop();
  dns_.stop();
  active_ = false;
  LOGF(TAG, "Portal detenido");
}

void ConfigPortal::update(uint32_t now) {
  if (!active_) return;
  dns_.processNextRequest();
  server_.handleClient();

  if (restartPending_ && static_cast<int32_t>(now - restartAt_) >= 0) {
    LOGF(TAG, "Reiniciando para conectar con la nueva red...");
    Serial.flush();
    stop();
    wifi_.shutdown();
    ESP.restart();
  }
}

void ConfigPortal::route(const char* uri, HTTPMethod method, Handler handler) {
  server_.on(uri, method, [this, handler]() {
    if (!isPortalHost()) {
      redirectToPortal();
      return;
    }
    (this->*handler)();
  });
}

void ConfigPortal::registerRoutes() {
  route("/", HTTP_GET, &ConfigPortal::handleIndex);
  route("/styles.css", HTTP_GET, &ConfigPortal::handleStyles);
  route("/wifi.js", HTTP_GET, &ConfigPortal::handleWifiScript);
  route("/api/info", HTTP_GET, &ConfigPortal::handleInfo);
  route("/api/scan", HTTP_GET, &ConfigPortal::handleScan);
  route("/api/wifi", HTTP_POST, &ConfigPortal::handleWifiSave);

  route("/admin", HTTP_GET, &ConfigPortal::handleAdminPage);
  route("/admin/", HTTP_GET, &ConfigPortal::handleAdminPage);
  route("/admin.js", HTTP_GET, &ConfigPortal::handleAdminScript);
  route("/api/admin/login", HTTP_POST, &ConfigPortal::handleAdminLogin);
  route("/api/admin/logout", HTTP_POST, &ConfigPortal::handleAdminLogout);
  route("/api/admin/config", HTTP_GET, &ConfigPortal::handleAdminGetConfig);
  route("/api/admin/config", HTTP_POST, &ConfigPortal::handleAdminSaveConfig);
  route("/api/admin/password", HTTP_POST, &ConfigPortal::handleAdminPassword);

  for (const char* probe : kCaptiveProbes) {
    server_.on(probe, HTTP_ANY, [this]() { redirectToPortal(); });
  }
  server_.onNotFound([this]() { handleNotFound(); });
}

bool ConfigPortal::isPortalHost() {
  String host = server_.hostHeader();
  const int colon = host.indexOf(':');
  if (colon >= 0) host = host.substring(0, colon);
  if (host.length() == 0) return true;  // HTTP/1.0 sin cabecera Host.
  return host == apIpText_ || host.equalsIgnoreCase(cfg::PORTAL_HOSTNAME);
}

void ConfigPortal::redirectToPortal() {
  server_.sendHeader("Location", "http://" + apIpText_ + "/", true);
  server_.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server_.send(302, "text/plain", "");
}

void ConfigPortal::sendJson(int code, const String& json) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json; charset=utf-8", json);
}

void ConfigPortal::sendError(int code, const String& message) {
  sendJson(code, "{\"ok\":false,\"error\":" + jsonString(message) + "}");
}

void ConfigPortal::sendStatic(const char* content, const char* contentType) {
  server_.sendHeader("Cache-Control", "no-cache");
  server_.send_P(200, contentType, content);
}

bool ConfigPortal::requireAdmin() {
  if (auth_.checkToken(server_.header(kAuthHeader))) return true;
  sendError(401, "Sesión no válida o expirada. Vuelve a iniciar sesión.");
  return false;
}

// ------------------------------------------------------------------ Páginas

void ConfigPortal::handleIndex() { sendStatic(WEB_INDEX_HTML, "text/html; charset=utf-8"); }
void ConfigPortal::handleAdminPage() { sendStatic(WEB_ADMIN_HTML, "text/html; charset=utf-8"); }
void ConfigPortal::handleStyles() { sendStatic(WEB_STYLES_CSS, "text/css; charset=utf-8"); }
void ConfigPortal::handleWifiScript() { sendStatic(WEB_WIFI_JS, "application/javascript; charset=utf-8"); }
void ConfigPortal::handleAdminScript() { sendStatic(WEB_ADMIN_JS, "application/javascript; charset=utf-8"); }

void ConfigPortal::handleNotFound() {
  if (!isPortalHost()) {
    redirectToPortal();
    return;
  }
  server_.send(404, "text/plain; charset=utf-8", "404: recurso no encontrado");
}

// ------------------------------------------------------------------ API Wi-Fi

void ConfigPortal::handleInfo() {
  sendJson(200, "{\"ok\":true,\"name\":" + jsonString(wifi_.apSsid()) + "}");
}

void ConfigPortal::handleScan() {
  const bool refresh = server_.hasArg("refresh");
  if (!wifi_.isScanning() && (refresh || !wifi_.hasScanResults())) wifi_.startScan();

  String json = "{\"ok\":true,\"scanning\":";
  json += jsonBool(wifi_.isScanning());
  json += ",\"networks\":[";
  bool first = true;
  for (const ScanResult& net : wifi_.scanResults()) {
    if (!first) json += ',';
    first = false;
    json += "{\"ssid\":" + jsonString(net.ssid) + ",\"rssi\":" + String(net.rssi) +
            ",\"secure\":" + jsonBool(net.secure) + "}";
  }
  json += "]}";
  sendJson(200, json);
}

void ConfigPortal::handleWifiSave() {
  if (restartPending_) {
    sendError(409, "Ya se guardó una red; el dispositivo se está reiniciando");
    return;
  }
  if (!server_.hasArg("ssid")) {
    sendError(400, "Falta el SSID");
    return;
  }
  WifiCredentials credentials;
  credentials.ssid = server_.arg("ssid");
  credentials.password = server_.arg("password");

  String error;
  if (!validateWifiCredentials(credentials, error)) {
    sendError(400, error);
    return;
  }
  if (!storage_.saveWifiCredentials(credentials)) {
    sendError(500, "No se pudo guardar la configuración en la memoria del dispositivo");
    return;
  }

  sendJson(200, "{\"ok\":true,\"message\":" +
                    jsonString("Red \"" + credentials.ssid +
                               "\" guardada. El dispositivo se reiniciará e intentará conectarse.") +
                    "}");
  restartPending_ = true;
  restartAt_ = millis() + cfg::RESTART_DELAY_MS;
}

// ------------------------------------------------------------------ API administrativa

void ConfigPortal::handleAdminLogin() {
  String token;
  uint32_t retryAfter = 0;
  switch (auth_.login(server_.arg("password"), token, retryAfter)) {
    case AdminAuth::LoginResult::Ok:
      sendJson(200, "{\"ok\":true,\"token\":" + jsonString(token) +
                        ",\"initialPassword\":" + jsonBool(auth_.usingInitialPassword()) + "}");
      break;
    case AdminAuth::LoginResult::LockedOut:
      sendError(429, "Demasiados intentos fallidos. Espera " + String(retryAfter) + " s.");
      break;
    case AdminAuth::LoginResult::NotProvisioned:
      sendError(503, "La autenticación administrativa no está disponible (error de NVS)");
      break;
    case AdminAuth::LoginResult::InvalidPassword:
    default:
      sendError(401, "Contraseña incorrecta");
      break;
  }
}

void ConfigPortal::handleAdminLogout() {
  if (!requireAdmin()) return;
  auth_.logout();
  sendJson(200, "{\"ok\":true}");
}

void ConfigPortal::handleAdminGetConfig() {
  if (!requireAdmin()) return;
  const ApiConfig config = storage_.apiConfig();
  String missing;
  const bool complete = storage_.isApiConfigComplete(&missing);

  String json = "{\"ok\":true,\"status\":";
  json += complete ? "\"valid\"" : "\"incomplete\"";
  json += ",\"missing\":" + jsonString(missing);
  json += ",\"initialPassword\":";
  json += jsonBool(auth_.usingInitialPassword());
  json += ",\"maxParams\":" + String(cfg::MAX_EXTRA_PARAMS);
  json += ",\"config\":{\"url\":" + jsonString(config.baseUrl);
  json += ",\"params\":[";
  for (uint8_t i = 0; i < config.paramCount; ++i) {
    if (i > 0) json += ',';
    json += "{\"name\":" + jsonString(config.params[i].name) +
            ",\"value\":" + jsonString(config.params[i].value) + "}";
  }
  json += "]";
  json += ",\"valueParam\":" + jsonString(config.valueParam);
  json += ",\"buttons\":[";
  for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) {
    if (i > 0) json += ',';
    json += jsonString(config.buttonValues[i]);
  }
  json += "]}}";
  sendJson(200, json);
}

void ConfigPortal::handleAdminSaveConfig() {
  if (!requireAdmin()) return;

  static const char* const kButtonArgs[cfg::BUTTON_COUNT] = {"b1", "b2", "b3", "b4"};
  const char* required[] = {"url", "valueParam", "paramCount"};
  for (const char* name : required) {
    if (!server_.hasArg(name)) {
      sendError(400, String("Falta el campo ") + name);
      return;
    }
  }

  // Parámetros adicionales: paramCount y, para cada i, pn<i> (nombre) y pv<i> (valor).
  const String countText = server_.arg("paramCount");
  bool countOk = countText.length() >= 1 && countText.length() <= 2;
  for (size_t i = 0; countOk && i < countText.length(); ++i) countOk = isDigit(countText[i]);
  const long count = countOk ? countText.toInt() : -1;
  if (count < 0 || count > cfg::MAX_EXTRA_PARAMS) {
    sendError(400, "Se admiten como máximo " + String(cfg::MAX_EXTRA_PARAMS) + " parámetros adicionales");
    return;
  }

  ApiConfig config;
  config.baseUrl = server_.arg("url");
  config.valueParam = server_.arg("valueParam");
  config.baseUrl.trim();
  config.valueParam.trim();
  config.paramCount = static_cast<uint8_t>(count);
  for (uint8_t i = 0; i < config.paramCount; ++i) {
    config.params[i].name = server_.arg("pn" + String(i));
    config.params[i].value = server_.arg("pv" + String(i));
    config.params[i].name.trim();
    config.params[i].value.trim();
  }
  for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) {
    config.buttonValues[i] = server_.arg(kButtonArgs[i]);
    config.buttonValues[i].trim();
  }

  String error;
  if (!validateApiConfig(config, error)) {
    sendError(400, error);
    return;
  }
  if (!storage_.saveApiConfig(config)) {
    sendError(500, "No se pudo guardar la configuración en la memoria del dispositivo");
    return;
  }
  sendJson(200,
           "{\"ok\":true,\"message\":" +
               jsonString("Configuración guardada y aplicada. Para volver al modo normal, guarda una "
                          "red Wi-Fi o reinicia el dispositivo.") +
               "}");
}

void ConfigPortal::handleAdminPassword() {
  if (!requireAdmin()) return;
  String error;
  if (!auth_.changePassword(server_.arg("current"), server_.arg("next"), error)) {
    sendError(400, error);
    return;
  }
  sendJson(200, "{\"ok\":true,\"message\":\"Contraseña administrativa actualizada\"}");
}
