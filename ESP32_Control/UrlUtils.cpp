#include "UrlUtils.h"

#include "Config.h"

namespace url {
namespace {

constexpr char kHttp[] = "http://";
constexpr char kHttps[] = "https://";

bool isUnreserved(uint8_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
         c == '_' || c == '.' || c == '~';
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

size_t schemeLength(const String& u) {
  if (u.startsWith(kHttps)) return sizeof(kHttps) - 1;
  if (u.startsWith(kHttp)) return sizeof(kHttp) - 1;
  return 0;
}

// Índice donde termina la autoridad ([usuario@]host[:puerto]).
size_t authorityEnd(const String& u, size_t start) {
  for (size_t i = start; i < u.length(); ++i) {
    if (u[i] == '/' || u[i] == '?') return i;
  }
  return u.length();
}

// HTTPClient necesita una ruta explícita:
// "https://h" -> "https://h/"   y   "https://h?a=1" -> "https://h/?a=1".
String withExplicitPath(const String& u) {
  const size_t end = authorityEnd(u, schemeLength(u));
  if (end < u.length() && u[end] == '/') return u;
  String out = u.substring(0, end);
  out += '/';
  out += u.substring(end);
  return out;
}

}  // namespace

String encodeComponent(const String& value) {
  static const char kHex[] = "0123456789ABCDEF";
  String out;
  out.reserve(value.length() * 3);
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (isUnreserved(c)) {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += kHex[c >> 4];
      out += kHex[c & 0x0F];
    }
  }
  return out;
}

bool isHttps(const String& requestUrl) { return requestUrl.startsWith(kHttps); }

bool validateBaseUrl(const String& u, String& error) {
  if (u.length() == 0) {
    error = "La URL está vacía";
    return false;
  }
  if (u.length() > cfg::MAX_URL_LEN) {
    error = "La URL supera " + String(cfg::MAX_URL_LEN) + " caracteres";
    return false;
  }
  for (size_t i = 0; i < u.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(u[i]);
    if (c <= 0x20 || c >= 0x7F) {
      error = "La URL contiene espacios o caracteres no ASCII (deben ir codificados con %XX)";
      return false;
    }
    if (c == '#') {
      error = "La URL no debe contener un fragmento (#)";
      return false;
    }
  }

  const size_t start = schemeLength(u);
  if (start == 0) {
    error = "La URL debe comenzar por http:// o https:// (en minúsculas)";
    return false;
  }

  const String authority = u.substring(start, authorityEnd(u, start));
  const int at = authority.lastIndexOf('@');
  const String hostPort = at >= 0 ? authority.substring(at + 1) : authority;
  const int colon = hostPort.indexOf(':');
  const String host = colon >= 0 ? hostPort.substring(0, colon) : hostPort;

  if (host.length() == 0) {
    error = "La URL no contiene un host";
    return false;
  }
  for (size_t i = 0; i < host.length(); ++i) {
    const char c = host[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c) || c == '-' ||
                    c == '.' || c == '_';
    if (!ok) {
      error = "El host de la URL contiene caracteres no válidos";
      return false;
    }
  }

  if (colon >= 0) {
    const String port = hostPort.substring(colon + 1);
    bool ok = port.length() >= 1 && port.length() <= 5;
    for (size_t i = 0; ok && i < port.length(); ++i) ok = isDigit(port[i]);
    const long value = ok ? port.toInt() : 0;
    if (!ok || value < 1 || value > 65535) {
      error = "El puerto de la URL no es válido";
      return false;
    }
  }
  return true;
}

String buildRequestUrl(const String& baseUrl, const QueryParam* params, uint8_t paramCount,
                       const String& valueParam, const String& value) {
  String out = withExplicitPath(baseUrl);
  if (out.indexOf('?') < 0) {
    out += '?';
  } else if (!out.endsWith("?") && !out.endsWith("&")) {
    out += '&';
  }
  for (uint8_t i = 0; i < paramCount; ++i) {
    out += encodeComponent(params[i].name);
    out += '=';
    out += encodeComponent(params[i].value);
    out += '&';
  }
  out += encodeComponent(valueParam);
  out += '=';
  out += encodeComponent(value);
  return out;
}

}  // namespace url
