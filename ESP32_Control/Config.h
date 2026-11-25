#pragma once

#include <Arduino.h>

// SOLO PARA DESARROLLO: 1 = las peticiones HTTPS NO validan el certificado del servidor
// (WiFiClientSecure::setInsecure). Debe ser 0 en producción; ver CaBundle.h.
#define API_TLS_INSECURE 0

// Constantes de hardware, tiempos y límites del firmware.
// Los GPIO son fijos por diseño: no se pueden modificar desde la interfaz web.
namespace cfg {

// ---------------------------------------------------------------- GPIO
constexpr uint8_t PIN_CONFIG_BUTTON = 23;  // Mantener 5 s para activar el Access Point.
constexpr uint8_t PIN_LED = 2;             // LED integrado en la mayoría de DevKit V1.
constexpr bool LED_ACTIVE_HIGH = true;

constexpr uint8_t BUTTON_COUNT = 4;
constexpr uint8_t PIN_BUTTONS[BUTTON_COUNT] = {18, 19, 21, 22};

// ---------------------------------------------------------------- Botones
constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t CONFIG_HOLD_MS = 5000;

// ---------------------------------------------------------------- Wi-Fi estación
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;  // Duración máxima de un intento.
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;   // Espera entre intentos fallidos.
constexpr uint32_t WIFI_LOST_RETRY_DELAY_MS = 1000;  // Primer reintento tras perder la red.

// ---------------------------------------------------------------- Access Point / portal
constexpr const char* AP_SSID_PREFIX = "ESP32-CONFIG-";
// Vacío = red abierta (habitual en portales cautivos). Si se define, mínimo 8 caracteres (WPA2).
constexpr const char* AP_PASSWORD = "";
constexpr uint8_t AP_CHANNEL = 1;
constexpr uint8_t AP_MAX_CLIENTS = 4;
constexpr uint8_t AP_IP[4] = {192, 168, 4, 1};
constexpr const char* PORTAL_HOSTNAME = "wifi.com";
constexpr uint16_t DNS_PORT = 53;
constexpr uint16_t HTTP_PORT = 80;
constexpr size_t MAX_SCAN_RESULTS = 30;
constexpr uint32_t RESTART_DELAY_MS = 1500;  // Margen para entregar la respuesta antes de reiniciar.

// ---------------------------------------------------------------- Cliente HTTP
constexpr uint32_t HTTP_CONNECT_TIMEOUT_MS = 5000;
constexpr uint16_t HTTP_RESPONSE_TIMEOUT_MS = 5000;
constexpr uint32_t TLS_HANDSHAKE_TIMEOUT_S = 8;
constexpr uint8_t EVENT_QUEUE_LENGTH = 50;  // Pulsaciones nuevas pendientes de enviar.
// Reintentos: una petición que falla por red, timeout, 408 o 5xx se aparta a una lista
// propia y se repite más tarde, sin ocupar la cola ni retrasar las pulsaciones nuevas.
// 200 y 409 son respuestas definitivas; el resto de códigos 3xx/4xx (también el 429, porque
// reintentar gasta más cupo del límite de la API) se descartan sin reintentar.
constexpr uint8_t HTTP_MAX_ATTEMPTS = 3;            // Incluye el primer intento.
constexpr uint8_t HTTP_RETRY_SLOTS = 16;            // Eventos esperando reintento a la vez.
constexpr uint32_t HTTP_RETRY_BASE_DELAY_MS = 2000; // 2 s, 4 s, 8 s, 16 s...
constexpr uint32_t HTTP_RETRY_MAX_DELAY_MS = 30000;
// Una pulsación más antigua que esto se descarta sin enviarla: con una red lenta o caída
// la cola no se llena de pulsaciones viejas y las nuevas no esperan detrás de ellas.
constexpr uint32_t HTTP_EVENT_MAX_AGE_MS = 30000;
// La conexión reutilizada se cierra tras este tiempo sin peticiones. Apache la corta a los
// 5 s; así la primera pulsación tras un rato siempre abre una conexión limpia.
constexpr uint32_t HTTP_KEEPALIVE_IDLE_MS = 4000;
constexpr uint32_t HTTP_TASK_STACK = 12288;  // TLS (mbedTLS) necesita una pila holgada.
constexpr UBaseType_t HTTP_TASK_PRIORITY = 1;
constexpr BaseType_t HTTP_TASK_CORE = 1;
// Espera máxima a que termine una petición en curso antes de cambiar a modo AP.
// Cubre conexión + handshake TLS + respuesta.
constexpr uint32_t AP_SWITCH_MAX_WAIT_MS = 20000;
// Vigilante: red de seguridad si una petición lleva más de este tiempo sin terminar
// (ya no debería ocurrir con la pausa del loop): se reinicia el ESP32 en modo normal.
// Una petición legítima, en el peor caso y con su repetición, ronda los 45 s.
constexpr uint32_t HTTP_HANG_RESTART_MS = 60000;

// ---------------------------------------------------------------- Límites de entrada (bytes)
constexpr size_t MAX_WIFI_SSID_LEN = 32;
constexpr size_t MIN_WIFI_PASS_LEN = 8;
constexpr size_t MAX_WIFI_PASS_LEN = 64;  // 63 caracteres o 64 dígitos hexadecimales.
constexpr size_t MAX_URL_LEN = 256;
constexpr size_t MAX_PARAM_NAME_LEN = 32;
constexpr size_t MAX_PARAM_VALUE_LEN = 64;
constexpr size_t MAX_BUTTON_VALUE_LEN = 64;
// Parámetros adicionales (nombre=valor) que se envían en todas las peticiones,
// p. ej. dispositivo, clave o codificación. Se configuran desde /admin.
constexpr uint8_t MAX_EXTRA_PARAMS = 8;
constexpr size_t MIN_ADMIN_PASS_LEN = 8;
constexpr size_t MAX_ADMIN_PASS_LEN = 64;

// ---------------------------------------------------------------- Autenticación administrativa
constexpr size_t ADMIN_SALT_LEN = 16;
constexpr size_t ADMIN_HASH_LEN = 32;
constexpr uint32_t ADMIN_PBKDF2_ITERATIONS = 4096;
// Contraseña inicial de /admin. Se muestra por Serial y en /admin se avisa de que debe
// cambiarse hasta que el administrador defina la suya.
constexpr const char* ADMIN_INITIAL_PASSWORD = "Admin123!";
constexpr uint32_t ADMIN_SESSION_TIMEOUT_MS = 10UL * 60UL * 1000UL;  // Inactividad.
constexpr uint8_t ADMIN_MAX_FAILED_ATTEMPTS = 5;
constexpr uint32_t ADMIN_LOCKOUT_MS = 30000;

}  // namespace cfg
