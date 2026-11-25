#include "StorageManager.h"

#include <Preferences.h>

#include "Log.h"
#include "UrlUtils.h"

namespace {

constexpr const char* TAG = "NVS";

constexpr const char* NS_WIFI = "wifi";
constexpr const char* NS_API = "api";
constexpr const char* NS_ADMIN = "admin";

constexpr const char* KEY_WIFI_CONFIGURED = "ok";
constexpr const char* KEY_WIFI_SSID = "ssid";
constexpr const char* KEY_WIFI_PASS = "pass";

constexpr const char* KEY_API_URL = "url";
constexpr const char* KEY_API_VALUE_PARAM = "p_val";
constexpr const char* KEY_API_BUTTON[cfg::BUTTON_COUNT] = {"b1", "b2", "b3", "b4"};
constexpr const char* KEY_API_PARAM_COUNT = "p_cnt";
// Claves de cada parámetro adicional: "pn<i>" (nombre) y "pv<i>" (valor).
constexpr const char* KEY_API_PARAM_NAME_PREFIX = "pn";
constexpr const char* KEY_API_PARAM_VALUE_PREFIX = "pv";
// Versiones anteriores guardaban un único parámetro fijo "dispositivo"; se migra al arrancar.
constexpr const char* KEY_API_LEGACY_DEVICE_ID = "dev_id";
constexpr const char* KEY_API_LEGACY_DEVICE_PARAM = "p_dev";

constexpr const char* KEY_ADMIN_SALT = "salt";
constexpr const char* KEY_ADMIN_HASH = "hash";
constexpr const char* KEY_ADMIN_INITIAL = "init";

class Lock {
 public:
  explicit Lock(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(m_); }

 private:
  SemaphoreHandle_t m_;
};

String readString(Preferences& prefs, const char* key) {
  return prefs.isKey(key) ? prefs.getString(key, "") : String();
}

// Escribe solo si el valor cambia, para no desgastar la flash innecesariamente.
bool putStringIfChanged(Preferences& prefs, const char* key, const String& value) {
  if (prefs.isKey(key) && prefs.getString(key, "") == value) return true;
  const size_t written = prefs.putString(key, value);
  // putString devuelve 0 también al guardar con éxito una cadena vacía.
  return value.length() == 0 ? prefs.isKey(key) : written == value.length();
}

bool putBytesIfChanged(Preferences& prefs, const char* key, const uint8_t* data, size_t len) {
  if (prefs.isKey(key) && prefs.getBytesLength(key) == len) {
    uint8_t current[64];
    if (len <= sizeof(current) && prefs.getBytes(key, current, len) == len &&
        memcmp(current, data, len) == 0) {
      return true;
    }
  }
  return prefs.putBytes(key, data, len) == len;
}

bool hasControlChars(const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c < 0x20 || c == 0x7F) return true;
  }
  return false;
}

bool checkText(const String& value, const char* label, size_t maxLen, String& error) {
  if (value.length() == 0) {
    error = String("El campo \"") + label + "\" es obligatorio";
    return false;
  }
  if (value.length() > maxLen) {
    error = String("El campo \"") + label + "\" supera " + String(maxLen) + " caracteres";
    return false;
  }
  if (hasControlChars(value)) {
    error = String("El campo \"") + label + "\" contiene caracteres de control";
    return false;
  }
  return true;
}

bool isHexString(const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    if (!isxdigit(static_cast<unsigned char>(value[i]))) return false;
  }
  return true;
}

void appendMissing(String& list, const char* name) {
  if (list.length() > 0) list += ", ";
  list += name;
}

String paramKey(const char* prefix, uint8_t index) { return String(prefix) + String(index); }

// Pasa el antiguo parámetro fijo (p_dev=dev_id) a la lista de parámetros adicionales.
void migrateLegacyDeviceParam(Preferences& prefs) {
  if (prefs.isKey(KEY_API_PARAM_COUNT)) return;
  const String name = readString(prefs, KEY_API_LEGACY_DEVICE_PARAM);
  const String value = readString(prefs, KEY_API_LEGACY_DEVICE_ID);
  uint8_t count = 0;
  if (name.length() > 0 || value.length() > 0) {
    if (!putStringIfChanged(prefs, paramKey(KEY_API_PARAM_NAME_PREFIX, 0).c_str(), name) ||
        !putStringIfChanged(prefs, paramKey(KEY_API_PARAM_VALUE_PREFIX, 0).c_str(), value)) {
      LOGF(TAG, "Error: no se pudo migrar el parámetro dispositivo");
      return;
    }
    count = 1;
  }
  if (prefs.putUChar(KEY_API_PARAM_COUNT, count) != 1) return;
  prefs.remove(KEY_API_LEGACY_DEVICE_PARAM);
  prefs.remove(KEY_API_LEGACY_DEVICE_ID);
  if (count > 0) LOGF(TAG, "Parámetro \"%s\" migrado a parámetros adicionales", name.c_str());
}

}  // namespace

bool validateWifiCredentials(const WifiCredentials& credentials, String& error) {
  if (credentials.ssid.length() == 0) {
    error = "El nombre de la red (SSID) es obligatorio";
    return false;
  }
  if (credentials.ssid.length() > cfg::MAX_WIFI_SSID_LEN) {
    error = "El SSID no puede superar " + String(cfg::MAX_WIFI_SSID_LEN) + " bytes";
    return false;
  }
  const size_t passLen = credentials.password.length();
  if (passLen == 0) return true;  // Red abierta.
  if (passLen == cfg::MAX_WIFI_PASS_LEN) {
    if (!isHexString(credentials.password)) {
      error = "Una contraseña de 64 caracteres debe ser hexadecimal (clave PSK)";
      return false;
    }
    return true;
  }
  if (passLen < cfg::MIN_WIFI_PASS_LEN || passLen > cfg::MAX_WIFI_PASS_LEN - 1) {
    error = "La contraseña debe tener entre 8 y 63 caracteres (o estar vacía si la red es abierta)";
    return false;
  }
  if (hasControlChars(credentials.password)) {
    error = "La contraseña contiene caracteres no válidos";
    return false;
  }
  return true;
}

bool validateApiConfig(const ApiConfig& config, String& error) {
  if (!checkText(config.baseUrl, "URL de la API", cfg::MAX_URL_LEN, error)) return false;
  if (!url::validateBaseUrl(config.baseUrl, error)) return false;
  if (config.paramCount > cfg::MAX_EXTRA_PARAMS) {
    error = "Se admiten como máximo " + String(cfg::MAX_EXTRA_PARAMS) + " parámetros adicionales";
    return false;
  }
  for (uint8_t i = 0; i < config.paramCount; ++i) {
    const QueryParam& param = config.params[i];
    const String label = "Parámetro " + String(i + 1);
    if (!checkText(param.name, (label + ": nombre").c_str(), cfg::MAX_PARAM_NAME_LEN, error) ||
        !checkText(param.value, (label + ": valor").c_str(), cfg::MAX_PARAM_VALUE_LEN, error)) {
      return false;
    }
    for (uint8_t j = 0; j < i; ++j) {
      if (config.params[j].name == param.name) {
        error = "El parámetro \"" + param.name + "\" está repetido";
        return false;
      }
    }
  }
  if (!checkText(config.valueParam, "Parámetro de los botones", cfg::MAX_PARAM_NAME_LEN, error)) {
    return false;
  }
  for (uint8_t i = 0; i < config.paramCount; ++i) {
    if (config.params[i].name == config.valueParam) {
      error = "El parámetro de los botones (\"" + config.valueParam +
              "\") no puede coincidir con un parámetro adicional";
      return false;
    }
  }
  for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) {
    const String label = "Valor del botón " + String(i + 1);
    if (!checkText(config.buttonValues[i], label.c_str(), cfg::MAX_BUTTON_VALUE_LEN, error)) {
      return false;
    }
  }
  return true;
}

bool StorageManager::begin() {
  mutex_ = xSemaphoreCreateMutex();
  if (mutex_ == nullptr) {
    LOGF(TAG, "Error: no se pudo crear el mutex de configuración");
    return false;
  }

  Preferences prefs;
  if (prefs.begin(NS_WIFI, false)) {
    wifiConfigured_ = prefs.getBool(KEY_WIFI_CONFIGURED, false) && prefs.isKey(KEY_WIFI_SSID);
    prefs.end();
  } else {
    LOGF(TAG, "Error: no se pudo abrir el espacio '%s'", NS_WIFI);
  }

  if (prefs.begin(NS_API, false)) {
    Lock lock(mutex_);
    migrateLegacyDeviceParam(prefs);
    api_.baseUrl = readString(prefs, KEY_API_URL);
    api_.paramCount = prefs.getUChar(KEY_API_PARAM_COUNT, 0);
    if (api_.paramCount > cfg::MAX_EXTRA_PARAMS) api_.paramCount = cfg::MAX_EXTRA_PARAMS;
    for (uint8_t i = 0; i < api_.paramCount; ++i) {
      api_.params[i].name = readString(prefs, paramKey(KEY_API_PARAM_NAME_PREFIX, i).c_str());
      api_.params[i].value = readString(prefs, paramKey(KEY_API_PARAM_VALUE_PREFIX, i).c_str());
    }
    api_.valueParam = readString(prefs, KEY_API_VALUE_PARAM);
    for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) {
      api_.buttonValues[i] = readString(prefs, KEY_API_BUTTON[i]);
    }
    prefs.end();
  } else {
    LOGF(TAG, "Error: no se pudo abrir el espacio '%s'", NS_API);
  }
  return true;
}

bool StorageManager::loadWifiCredentials(WifiCredentials& out) {
  if (!wifiConfigured_) return false;
  Preferences prefs;
  if (!prefs.begin(NS_WIFI, true)) return false;
  out.ssid = readString(prefs, KEY_WIFI_SSID);
  out.password = readString(prefs, KEY_WIFI_PASS);
  prefs.end();
  return out.ssid.length() > 0;
}

bool StorageManager::saveWifiCredentials(const WifiCredentials& credentials) {
  Preferences prefs;
  if (!prefs.begin(NS_WIFI, false)) return false;
  // La marca de "configurado" se escribe al final: un corte de alimentación a
  // mitad de escritura no deja una red a medio guardar marcada como válida.
  bool ok = putStringIfChanged(prefs, KEY_WIFI_SSID, credentials.ssid) &&
            putStringIfChanged(prefs, KEY_WIFI_PASS, credentials.password);
  if (ok && !prefs.getBool(KEY_WIFI_CONFIGURED, false)) {
    ok = prefs.putBool(KEY_WIFI_CONFIGURED, true) == 1;
  }
  prefs.end();
  if (ok) wifiConfigured_ = true;
  LOGF(TAG, "Credenciales Wi-Fi %s (SSID \"%s\")", ok ? "guardadas" : "NO guardadas",
       credentials.ssid.c_str());
  return ok;
}

ApiConfig StorageManager::apiConfig() const {
  Lock lock(mutex_);
  return api_;
}

bool StorageManager::isApiConfigComplete(String* missing) const {
  const ApiConfig config = apiConfig();
  String list;
  String urlError;
  if (config.baseUrl.length() == 0) {
    appendMissing(list, "URL");
  } else if (!url::validateBaseUrl(config.baseUrl, urlError)) {
    appendMissing(list, "URL válida");
  }
  for (uint8_t i = 0; i < config.paramCount; ++i) {
    if (config.params[i].name.length() == 0 || config.params[i].value.length() == 0) {
      appendMissing(list, (String("parámetro adicional ") + String(i + 1)).c_str());
    }
  }
  if (config.valueParam.length() == 0) appendMissing(list, "parámetro de los botones");
  for (uint8_t i = 0; i < cfg::BUTTON_COUNT; ++i) {
    if (config.buttonValues[i].length() == 0) {
      appendMissing(list, (String("valor del botón ") + String(i + 1)).c_str());
    }
  }
  if (missing != nullptr) *missing = list;
  return list.length() == 0;
}

bool StorageManager::saveApiConfig(const ApiConfig& config) {
  Preferences prefs;
  if (!prefs.begin(NS_API, false)) return false;
  bool ok = putStringIfChanged(prefs, KEY_API_URL, config.baseUrl) &&
            putStringIfChanged(prefs, KEY_API_VALUE_PARAM, config.valueParam);
  for (uint8_t i = 0; ok && i < cfg::BUTTON_COUNT; ++i) {
    ok = putStringIfChanged(prefs, KEY_API_BUTTON[i], config.buttonValues[i]);
  }
  for (uint8_t i = 0; ok && i < config.paramCount; ++i) {
    ok = putStringIfChanged(prefs, paramKey(KEY_API_PARAM_NAME_PREFIX, i).c_str(), config.params[i].name) &&
         putStringIfChanged(prefs, paramKey(KEY_API_PARAM_VALUE_PREFIX, i).c_str(), config.params[i].value);
  }
  if (ok && (!prefs.isKey(KEY_API_PARAM_COUNT) || prefs.getUChar(KEY_API_PARAM_COUNT, 0) != config.paramCount)) {
    ok = prefs.putUChar(KEY_API_PARAM_COUNT, config.paramCount) == 1;
  }
  // Se borran los parámetros sobrantes de una configuración anterior con más elementos.
  for (uint8_t i = config.paramCount; ok && i < cfg::MAX_EXTRA_PARAMS; ++i) {
    const String nameKey = paramKey(KEY_API_PARAM_NAME_PREFIX, i);
    const String valueKey = paramKey(KEY_API_PARAM_VALUE_PREFIX, i);
    if (prefs.isKey(nameKey.c_str())) prefs.remove(nameKey.c_str());
    if (prefs.isKey(valueKey.c_str())) prefs.remove(valueKey.c_str());
  }
  prefs.end();
  if (ok) {
    Lock lock(mutex_);
    api_ = config;  // Se aplica en caliente: la tarea HTTP lee una copia en cada petición.
  }
  LOGF(TAG, "Configuración HTTP %s", ok ? "guardada" : "NO guardada (error de NVS)");
  return ok;
}

bool StorageManager::loadAdminSecret(AdminSecret& out) {
  Preferences prefs;
  if (!prefs.begin(NS_ADMIN, false)) return false;
  const bool ok = prefs.getBytesLength(KEY_ADMIN_SALT) == cfg::ADMIN_SALT_LEN &&
                  prefs.getBytesLength(KEY_ADMIN_HASH) == cfg::ADMIN_HASH_LEN &&
                  prefs.getBytes(KEY_ADMIN_SALT, out.salt, cfg::ADMIN_SALT_LEN) == cfg::ADMIN_SALT_LEN &&
                  prefs.getBytes(KEY_ADMIN_HASH, out.hash, cfg::ADMIN_HASH_LEN) == cfg::ADMIN_HASH_LEN;
  out.initialPassword = ok ? readString(prefs, KEY_ADMIN_INITIAL) : String();
  prefs.end();
  return ok;
}

bool StorageManager::saveAdminSecret(const AdminSecret& secret) {
  Preferences prefs;
  if (!prefs.begin(NS_ADMIN, false)) return false;
  bool ok = putBytesIfChanged(prefs, KEY_ADMIN_SALT, secret.salt, cfg::ADMIN_SALT_LEN) &&
            putBytesIfChanged(prefs, KEY_ADMIN_HASH, secret.hash, cfg::ADMIN_HASH_LEN);
  if (ok) {
    if (secret.initialPassword.length() > 0) {
      ok = putStringIfChanged(prefs, KEY_ADMIN_INITIAL, secret.initialPassword);
    } else if (prefs.isKey(KEY_ADMIN_INITIAL)) {
      ok = prefs.remove(KEY_ADMIN_INITIAL);
    }
  }
  prefs.end();
  return ok;
}
