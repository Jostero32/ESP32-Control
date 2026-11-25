#pragma once

#include <Arduino.h>

#include "StorageManager.h"

// Autenticación de la página /admin.
//  - En el primer arranque se configura la contraseña inicial cfg::ADMIN_INITIAL_PASSWORD
//    (Admin123!) y se muestra por Serial
//    hasta que el administrador la cambia.
//  - En NVS solo se guarda PBKDF2-HMAC-SHA256(contraseña, sal).
//  - Tras iniciar sesión se entrega un token aleatorio que vive solo en RAM.
class AdminAuth {
 public:
  enum class LoginResult { Ok, InvalidPassword, LockedOut, NotProvisioned };

  explicit AdminAuth(StorageManager& storage) : storage_(storage) {}

  // Llamar con la radio ya iniciada: el RNG hardware solo es criptográficamente
  // aleatorio cuando el Wi-Fi o el Bluetooth están activos.
  bool begin();
  void printProvisioningInfo() const;
  bool usingInitialPassword() const { return secret_.initialPassword.length() > 0; }

  LoginResult login(const String& password, String& token, uint32_t& retryAfterSeconds);
  bool checkToken(const String& token);
  void logout();
  bool changePassword(const String& current, const String& next, String& error);

 private:
  bool verifyPassword(const String& password) const;
  bool isLockedOut(uint32_t now, uint32_t& retryAfterSeconds) const;
  void registerFailure(uint32_t now);

  StorageManager& storage_;
  AdminSecret secret_{};
  bool provisioned_ = false;

  String sessionToken_;
  uint32_t sessionLastUse_ = 0;

  uint8_t failedAttempts_ = 0;
  bool lockedOut_ = false;
  uint32_t lockoutStart_ = 0;
};
