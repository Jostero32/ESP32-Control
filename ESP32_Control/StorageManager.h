#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "Config.h"

struct WifiCredentials {
  String ssid;
  String password;  // Vacía para redes abiertas.
};

struct QueryParam {
  String name;
  String value;
};

struct ApiConfig {
  String baseUrl;
  // Parámetros fijos comunes a todos los botones, en el orden en que se envían.
  QueryParam params[cfg::MAX_EXTRA_PARAMS];
  uint8_t paramCount = 0;
  // Parámetro propio de cada botón: valueParam=buttonValues[i].
  String valueParam;
  String buttonValues[cfg::BUTTON_COUNT];
};

struct AdminSecret {
  uint8_t salt[cfg::ADMIN_SALT_LEN];
  uint8_t hash[cfg::ADMIN_HASH_LEN];
  // Contraseña generada en el primer arranque. Se conserva solo hasta que el
  // administrador la cambia, para poder mostrarla por Serial (aprovisionamiento).
  String initialPassword;
};

bool validateWifiCredentials(const WifiCredentials& credentials, String& error);
bool validateApiConfig(const ApiConfig& config, String& error);

// Persistencia en NVS (Preferences). La configuración HTTP se mantiene también en
// RAM protegida por mutex, porque la lee la tarea HTTP mientras el portal la puede modificar.
class StorageManager {
 public:
  bool begin();

  // Wi-Fi: la existencia de red guardada se marca con una clave propia, no con la
  // contraseña, para admitir redes abiertas.
  bool hasWifiCredentials() const { return wifiConfigured_; }
  bool loadWifiCredentials(WifiCredentials& out);
  bool saveWifiCredentials(const WifiCredentials& credentials);

  ApiConfig apiConfig() const;
  // Completa = todos los campos presentes y URL válida. `missing` recibe los campos que faltan.
  bool isApiConfigComplete(String* missing = nullptr) const;
  bool saveApiConfig(const ApiConfig& config);

  bool loadAdminSecret(AdminSecret& out);
  bool saveAdminSecret(const AdminSecret& secret);

 private:
  SemaphoreHandle_t mutex_ = nullptr;
  ApiConfig api_;
  bool wifiConfigured_ = false;
};
