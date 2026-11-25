#include "AdminAuth.h"

#include <esp_random.h>
#include <mbedtls/md.h>

#include "Log.h"

namespace {

constexpr const char* TAG = "ADMIN";
constexpr size_t kTokenBytes = 16;

// PBKDF2-HMAC-SHA256 con un único bloque de salida (32 bytes).
bool deriveKey(const String& password, const uint8_t* salt, uint8_t out[cfg::ADMIN_HASH_LEN]) {
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  if (info == nullptr || mbedtls_md_setup(&ctx, info, 1) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }

  const auto* key = reinterpret_cast<const unsigned char*>(password.c_str());
  const size_t keyLen = password.length();
  uint8_t block[cfg::ADMIN_SALT_LEN + 4];
  memcpy(block, salt, cfg::ADMIN_SALT_LEN);
  block[cfg::ADMIN_SALT_LEN + 0] = 0;
  block[cfg::ADMIN_SALT_LEN + 1] = 0;
  block[cfg::ADMIN_SALT_LEN + 2] = 0;
  block[cfg::ADMIN_SALT_LEN + 3] = 1;

  uint8_t u[cfg::ADMIN_HASH_LEN];
  bool ok = mbedtls_md_hmac_starts(&ctx, key, keyLen) == 0 &&
            mbedtls_md_hmac_update(&ctx, block, sizeof(block)) == 0 &&
            mbedtls_md_hmac_finish(&ctx, u) == 0;
  memcpy(out, u, sizeof(u));
  for (uint32_t i = 1; ok && i < cfg::ADMIN_PBKDF2_ITERATIONS; ++i) {
    ok = mbedtls_md_hmac_reset(&ctx) == 0 && mbedtls_md_hmac_update(&ctx, u, sizeof(u)) == 0 &&
         mbedtls_md_hmac_finish(&ctx, u) == 0;
    for (size_t j = 0; j < sizeof(u); ++j) out[j] ^= u[j];
  }
  mbedtls_md_free(&ctx);
  return ok;
}

bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len) {
  uint8_t diff = 0;
  for (size_t i = 0; i < len; ++i) diff |= a[i] ^ b[i];
  return diff == 0;
}

bool constantTimeEquals(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  return constantTimeEquals(reinterpret_cast<const uint8_t*>(a.c_str()),
                            reinterpret_cast<const uint8_t*>(b.c_str()), a.length());
}

String randomToken() {
  uint8_t bytes[kTokenBytes];
  esp_fill_random(bytes, sizeof(bytes));
  static const char kHex[] = "0123456789abcdef";
  String out;
  out.reserve(kTokenBytes * 2);
  for (uint8_t b : bytes) {
    out += kHex[b >> 4];
    out += kHex[b & 0x0F];
  }
  return out;
}

bool hasControlChars(const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c < 0x20 || c == 0x7F) return true;
  }
  return false;
}

}  // namespace

bool AdminAuth::begin() {
  if (storage_.loadAdminSecret(secret_)) {
    provisioned_ = true;
    // Una contraseña definida por el administrador se respeta siempre. Solo se sustituye
    // una contraseña inicial distinta de la actual que nunca se cambió (las versiones
    // anteriores del firmware generaban una aleatoria).
    if (!usingInitialPassword() || secret_.initialPassword == cfg::ADMIN_INITIAL_PASSWORD) {
      printProvisioningInfo();
      return true;
    }
    LOGF(TAG, "Contraseña inicial anterior sin cambiar: se sustituye por la inicial actual");
  }

  AdminSecret fresh{};
  esp_fill_random(fresh.salt, sizeof(fresh.salt));
  fresh.initialPassword = cfg::ADMIN_INITIAL_PASSWORD;
  if (!deriveKey(fresh.initialPassword, fresh.salt, fresh.hash) || !storage_.saveAdminSecret(fresh)) {
    LOGF(TAG, "Error: no se pudo aprovisionar la contraseña administrativa");
    return false;
  }
  secret_ = fresh;
  provisioned_ = true;
  LOGF(TAG, "Contraseña administrativa inicial configurada");
  printProvisioningInfo();
  return true;
}

void AdminAuth::printProvisioningInfo() const {
  if (!provisioned_ || !usingInitialPassword()) return;
  Serial.printf(
      "\n==================== APROVISIONAMIENTO /admin ====================\n"
      "  Contraseña administrativa inicial: %s\n"
      "  Cámbiela desde /admin; después dejará de mostrarse aquí.\n"
      "===================================================================\n\n",
      secret_.initialPassword.c_str());
}

bool AdminAuth::verifyPassword(const String& password) const {
  uint8_t candidate[cfg::ADMIN_HASH_LEN];
  if (!deriveKey(password, secret_.salt, candidate)) return false;
  return constantTimeEquals(candidate, secret_.hash, sizeof(candidate));
}

bool AdminAuth::isLockedOut(uint32_t now, uint32_t& retryAfterSeconds) const {
  if (!lockedOut_) return false;
  const uint32_t elapsed = now - lockoutStart_;
  if (elapsed >= cfg::ADMIN_LOCKOUT_MS) return false;
  retryAfterSeconds = (cfg::ADMIN_LOCKOUT_MS - elapsed + 999) / 1000;
  return true;
}

void AdminAuth::registerFailure(uint32_t now) {
  if (++failedAttempts_ >= cfg::ADMIN_MAX_FAILED_ATTEMPTS) {
    lockedOut_ = true;
    lockoutStart_ = now;
    failedAttempts_ = 0;
    LOGF(TAG, "Demasiados intentos fallidos: acceso bloqueado %lu s",
         static_cast<unsigned long>(cfg::ADMIN_LOCKOUT_MS / 1000));
  }
}

AdminAuth::LoginResult AdminAuth::login(const String& password, String& token,
                                        uint32_t& retryAfterSeconds) {
  if (!provisioned_) return LoginResult::NotProvisioned;
  const uint32_t now = millis();
  if (isLockedOut(now, retryAfterSeconds)) return LoginResult::LockedOut;
  lockedOut_ = false;

  if (password.length() == 0 || password.length() > cfg::MAX_ADMIN_PASS_LEN || !verifyPassword(password)) {
    LOGF(TAG, "Intento de acceso rechazado: contraseña incorrecta");
    registerFailure(now);
    return LoginResult::InvalidPassword;
  }

  failedAttempts_ = 0;
  sessionToken_ = randomToken();  // Una única sesión activa: invalida la anterior.
  sessionLastUse_ = now;
  token = sessionToken_;
  LOGF(TAG, "Acceso administrativo concedido");
  return LoginResult::Ok;
}

bool AdminAuth::checkToken(const String& token) {
  if (sessionToken_.length() == 0 || token.length() == 0) return false;
  const uint32_t now = millis();
  if (now - sessionLastUse_ >= cfg::ADMIN_SESSION_TIMEOUT_MS) {
    LOGF(TAG, "Sesión administrativa expirada por inactividad");
    sessionToken_ = "";
    return false;
  }
  if (!constantTimeEquals(token, sessionToken_)) return false;
  sessionLastUse_ = now;
  return true;
}

void AdminAuth::logout() {
  sessionToken_ = "";
  LOGF(TAG, "Sesión administrativa cerrada");
}

bool AdminAuth::changePassword(const String& current, const String& next, String& error) {
  const uint32_t now = millis();
  uint32_t retryAfter = 0;
  if (isLockedOut(now, retryAfter)) {
    error = "Demasiados intentos fallidos. Espera " + String(retryAfter) + " s";
    return false;
  }
  if (!verifyPassword(current)) {
    registerFailure(now);
    error = "La contraseña actual no es correcta";
    return false;
  }
  if (next.length() < cfg::MIN_ADMIN_PASS_LEN || next.length() > cfg::MAX_ADMIN_PASS_LEN) {
    error = "La nueva contraseña debe tener entre " + String(cfg::MIN_ADMIN_PASS_LEN) + " y " +
            String(cfg::MAX_ADMIN_PASS_LEN) + " caracteres";
    return false;
  }
  if (hasControlChars(next)) {
    error = "La nueva contraseña contiene caracteres no válidos";
    return false;
  }
  if (next == current) {
    error = "La nueva contraseña debe ser distinta de la actual";
    return false;
  }

  AdminSecret updated{};
  esp_fill_random(updated.salt, sizeof(updated.salt));
  if (!deriveKey(next, updated.salt, updated.hash) || !storage_.saveAdminSecret(updated)) {
    error = "No se pudo guardar la nueva contraseña";
    return false;
  }
  secret_ = updated;  // initialPassword vacía: deja de imprimirse por Serial.
  failedAttempts_ = 0;
  LOGF(TAG, "Contraseña administrativa actualizada");
  return true;
}
