#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "AdminAuth.h"
#include "StorageManager.h"
#include "WifiManager.h"

// Portal cautivo (DNS + HTTP:80), activo solo en modo Access Point.
//
// Enrutado:
//  - Host ajeno (cualquier dominio resuelto por el DNS cautivo) -> 302 a http://192.168.4.1/
//  - Host propio (192.168.4.1 o wifi.com): se sirven las rutas reales y las desconocidas
//    devuelven 404. Así /admin nunca se ve afectada por las redirecciones del portal.
class ConfigPortal {
 public:
  ConfigPortal(StorageManager& storage, WifiManager& wifi, AdminAuth& auth);

  void begin();
  void stop();
  void update(uint32_t now);
  bool isActive() const { return active_; }

 private:
  using Handler = void (ConfigPortal::*)();

  void registerRoutes();
  void route(const char* uri, HTTPMethod method, Handler handler);
  bool isPortalHost();
  void redirectToPortal();
  void sendJson(int code, const String& json);
  void sendError(int code, const String& message);
  void sendStatic(const char* content, const char* contentType);
  bool requireAdmin();

  // Páginas y recursos
  void handleIndex();
  void handleAdminPage();
  void handleStyles();
  void handleWifiScript();
  void handleAdminScript();
  void handleNotFound();

  // API pública (configuración Wi-Fi)
  void handleInfo();
  void handleScan();
  void handleWifiSave();

  // API administrativa
  void handleAdminLogin();
  void handleAdminLogout();
  void handleAdminGetConfig();
  void handleAdminSaveConfig();
  void handleAdminPassword();

  StorageManager& storage_;
  WifiManager& wifi_;
  AdminAuth& auth_;
  WebServer server_;
  DNSServer dns_;
  String apIpText_;
  bool active_ = false;
  bool restartPending_ = false;
  uint32_t restartAt_ = 0;
};
