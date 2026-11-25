#pragma once

#include <Arduino.h>

#include "StorageManager.h"

// Utilidades para validar la URL base y construir las peticiones GET.
// web/admin.js replica esta misma lógica para la "Vista previa de peticiones".
namespace url {

// Codificación de componente (RFC 3986): solo A-Z a-z 0-9 - _ . ~ quedan sin codificar.
String encodeComponent(const String& value);

// Comprueba esquema http:// o https:// (minúsculas), host, puerto y caracteres permitidos.
bool validateBaseUrl(const String& baseUrl, String& error);

bool isHttps(const String& requestUrl);

// Añade los parámetros adicionales y, al final, valueParam=value a la URL base,
// conservando los parámetros que ya tuviera.
String buildRequestUrl(const String& baseUrl, const QueryParam* params, uint8_t paramCount,
                       const String& valueParam, const String& value);

}  // namespace url
