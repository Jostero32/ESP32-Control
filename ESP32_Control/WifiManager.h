#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#include <vector>

#include "StorageManager.h"

enum class WifiState : uint8_t {
  Idle,          // Radio sin iniciar.
  Connecting,    // Intento de conexión en curso.
  Connected,     // Estación conectada con IP.
  Disconnected,  // Sin conexión; se reintenta periódicamente.
  AccessPoint,   // Portal de configuración activo.
};

struct ScanResult {
  String ssid;
  int32_t rssi = 0;
  bool secure = false;
};

// Gestión de la radio: estación con reconexión no bloqueante, Access Point y escaneo
// asíncrono. Nunca activa el AP por sí mismo: eso lo decide main.cpp.
class WifiManager {
 public:
  void begin();
  void beginStation(const WifiCredentials& credentials);
  bool startAccessPoint();
  void update(uint32_t now);
  void shutdown();

  WifiState state() const { return state_; }
  bool isConnected() const { return state_ == WifiState::Connected; }
  const String& apSsid() const { return apSsid_; }
  IPAddress apIp() const;
  static const char* stateName(WifiState state);

  // Escaneo (solo en modo AP). No bloquea: el resultado se recoge en update().
  bool startScan();
  bool isScanning() const { return scanning_; }
  bool hasScanResults() const { return scanDone_; }
  const std::vector<ScanResult>& scanResults() const { return results_; }

 private:
  void startConnectAttempt(uint32_t now);
  void setState(WifiState state);
  void onConnected();
  void pollScan();

  WifiState state_ = WifiState::Idle;
  WifiCredentials credentials_;
  String apSsid_;
  uint32_t attemptStart_ = 0;
  uint32_t retryAt_ = 0;
  uint32_t attemptCount_ = 0;

  bool scanning_ = false;
  bool scanDone_ = false;
  std::vector<ScanResult> results_;
};
