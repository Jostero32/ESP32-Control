// ARCHIVO GENERADO por tools/generar_recursos_web.py a partir de la carpeta web/.
// No lo edite a mano: modifique web/ y vuelva a ejecutar el script.
#pragma once

#include <Arduino.h>

// web/index.html
const char WEB_INDEX_HTML[] PROGMEM = R"ESP32WEB(<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Configuración Wi-Fi</title>
  <link rel="stylesheet" href="/styles.css">
</head>
<body>
  <main class="container">
    <header class="header">
      <h1>Configuración Wi-Fi</h1>
      <p class="subtitle">Dispositivo: <strong id="deviceName">…</strong></p>
    </header>

    <section class="card">
      <div class="card-head">
        <h2>Redes disponibles (2.4 GHz)</h2>
        <button type="button" id="refreshBtn" class="btn btn-secondary btn-small">Actualizar</button>
      </div>
      <p id="scanStatus" class="hint">Buscando redes…</p>
      <ul id="networkList" class="network-list"></ul>
    </section>

    <section class="card">
      <h2>Conectar a una red</h2>
      <form id="wifiForm" autocomplete="off" novalidate>
        <label for="ssid">Nombre de la red (SSID)</label>
        <input id="ssid" name="ssid" type="text" maxlength="32" required
               placeholder="Selecciona una red o escribe una oculta"
               autocapitalize="none" autocorrect="off" spellcheck="false">
        <p class="hint">Para una red oculta, escribe su nombre exacto.</p>

        <label for="password">Contraseña</label>
        <div class="password-row">
          <input id="password" name="password" type="password" maxlength="64" autocomplete="new-password"
                 autocapitalize="none" autocorrect="off" spellcheck="false">
          <button type="button" id="togglePassword" class="btn btn-secondary btn-small">Mostrar</button>
        </div>
        <p id="passwordHint" class="hint">Déjala vacía solo si la red es abierta.</p>

        <button type="submit" id="saveBtn" class="btn btn-primary">Guardar y reiniciar</button>
      </form>
      <div id="message" class="message" role="status" hidden></div>
    </section>

    <p class="footer">Si esta página no se abrió automáticamente, visita http://192.168.4.1</p>
  </main>
  <script src="/wifi.js"></script>
</body>
</html>
)ESP32WEB";

// web/admin.html
const char WEB_ADMIN_HTML[] PROGMEM = R"ESP32WEB(<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="robots" content="noindex">
  <title>Administración</title>
  <link rel="stylesheet" href="/styles.css">
</head>
<body>
  <main class="container">
    <header class="header">
      <h1>Administración</h1>
      <p class="subtitle">Configuración de las peticiones HTTP</p>
    </header>

    <!-- Acceso -->
    <section id="loginView" class="card">
      <h2>Acceso</h2>
      <form id="loginForm" novalidate>
        <label for="loginPassword">Contraseña administrativa</label>
        <input id="loginPassword" type="password" maxlength="64" autocomplete="current-password" required>
        <button type="submit" id="loginBtn" class="btn btn-primary">Entrar</button>
      </form>
      <div id="loginMessage" class="message" role="status" hidden></div>
    </section>

    <!-- Configuración (solo tras autenticarse) -->
    <div id="configView" hidden>
      <div id="initialNotice" class="message warning" hidden>
        Estás usando la contraseña inicial de la placa. Cámbiala en la sección «Contraseña administrativa».
      </div>

      <form id="configForm" novalidate>
        <section class="card">
          <div class="card-head">
            <h2>Datos de la API</h2>
            <span id="statusBadge" class="status-badge"></span>
          </div>
          <label for="url">URL de destino</label>
          <input id="url" name="url" type="url" maxlength="256" placeholder="https://api.ejemplo.com/evento"
                 autocapitalize="none" autocorrect="off" spellcheck="false">
        </section>

        <section class="card">
          <h2>Parámetros adicionales</h2>
          <p class="hint">Se envían en todas las peticiones, en este orden (p. ej. dispositivo, clave, codificación).
            Las filas vacías se ignoran.</p>
          <div id="paramHead" class="param-head" aria-hidden="true" hidden><span>Nombre</span><span>Valor</span></div>
          <div id="paramList" class="param-list"></div>
          <button type="button" id="addParamBtn" class="btn btn-secondary btn-small">+ Añadir parámetro</button>
        </section>

        <section class="card">
          <h2>Datos de los botones</h2>
          <label for="valueParam">Nombre del parámetro de los botones</label>
          <input id="valueParam" name="valueParam" type="text" maxlength="32" placeholder="valor"
                 autocapitalize="none" autocorrect="off" spellcheck="false">
          <div class="grid-2">
            <div>
              <label for="b1">Valor del botón 1</label>
              <input id="b1" name="b1" type="text" maxlength="64" placeholder="1" autocapitalize="none">
            </div>
            <div>
              <label for="b2">Valor del botón 2</label>
              <input id="b2" name="b2" type="text" maxlength="64" placeholder="2" autocapitalize="none">
            </div>
            <div>
              <label for="b3">Valor del botón 3</label>
              <input id="b3" name="b3" type="text" maxlength="64" placeholder="3" autocapitalize="none">
            </div>
            <div>
              <label for="b4">Valor del botón 4</label>
              <input id="b4" name="b4" type="text" maxlength="64" placeholder="4" autocapitalize="none">
            </div>
          </div>
        </section>

        <section class="card">
          <h2>Vista previa de peticiones</h2>
          <p class="hint">Así quedará la petición GET de cada botón. La vista previa no envía ninguna petición.</p>
          <ol id="preview" class="preview-list"></ol>
        </section>

        <button type="submit" id="saveConfigBtn" class="btn btn-primary">Guardar configuración</button>
        <div id="configMessage" class="message" role="status" hidden></div>
      </form>

      <section class="card">
        <h2>Contraseña administrativa</h2>
        <form id="passwordForm" novalidate>
          <label for="currentPassword">Contraseña actual</label>
          <input id="currentPassword" type="password" maxlength="64" autocomplete="current-password">
          <label for="newPassword">Nueva contraseña (8 a 64 caracteres)</label>
          <input id="newPassword" type="password" maxlength="64" autocomplete="new-password">
          <label for="confirmPassword">Repite la nueva contraseña</label>
          <input id="confirmPassword" type="password" maxlength="64" autocomplete="new-password">
          <button type="submit" id="passwordBtn" class="btn btn-primary">Cambiar contraseña</button>
        </form>
        <div id="passwordMessage" class="message" role="status" hidden></div>
      </section>

      <div class="actions">
        <button type="button" id="logoutBtn" class="btn btn-secondary btn-small">Cerrar sesión</button>
      </div>
    </div>
  </main>
  <script src="/admin.js"></script>
</body>
</html>
)ESP32WEB";

// web/styles.css
const char WEB_STYLES_CSS[] PROGMEM = R"ESP32WEB(:root {
  --bg: #f3f5f9;
  --card: #ffffff;
  --text: #1c2430;
  --muted: #5d6878;
  --border: #dde2ea;
  --primary: #2563eb;
  --primary-text: #ffffff;
  --secondary: #e8edf5;
  --success-bg: #e7f6ec;
  --success: #17663a;
  --error-bg: #fdecec;
  --error: #a32020;
  --warning-bg: #fff6e0;
  --warning: #7a5600;
  --focus: rgba(37, 99, 235, 0.35);
  --radius: 12px;
}

@media (prefers-color-scheme: dark) {
  :root {
    --bg: #11161d;
    --card: #1a212b;
    --text: #e6ebf2;
    --muted: #9aa5b5;
    --border: #2c3542;
    --primary: #3b82f6;
    --secondary: #263040;
    --success-bg: #12301f;
    --success: #7fd6a0;
    --error-bg: #3a1717;
    --error: #ff9c9c;
    --warning-bg: #3a2e10;
    --warning: #f3cd72;
    --focus: rgba(59, 130, 246, 0.45);
  }
}

* {
  box-sizing: border-box;
}

html {
  -webkit-text-size-adjust: 100%;
}

body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  font-size: 16px;
  line-height: 1.45;
}

.container {
  max-width: 560px;
  margin: 0 auto;
  padding: 20px 16px 32px;
}

.header {
  margin-bottom: 16px;
}

h1 {
  font-size: 1.5rem;
  margin: 0 0 4px;
}

h2 {
  font-size: 1.05rem;
  margin: 0 0 12px;
}

.subtitle,
.hint,
.footer {
  color: var(--muted);
  font-size: 0.9rem;
  margin: 4px 0 0;
}

.footer {
  text-align: center;
  margin-top: 20px;
}

.card {
  background: var(--card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 16px;
  margin-bottom: 16px;
}

.card-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  margin-bottom: 8px;
}

.card-head h2 {
  margin: 0;
}

label {
  display: block;
  font-weight: 600;
  font-size: 0.9rem;
  margin: 12px 0 6px;
}

input[type="text"],
input[type="password"],
input[type="url"] {
  width: 100%;
  padding: 11px 12px;
  font-size: 1rem;
  color: var(--text);
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 8px;
  outline: none;
}

input:focus {
  border-color: var(--primary);
  box-shadow: 0 0 0 3px var(--focus);
}

.grid-2 {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 0 12px;
}

@media (max-width: 380px) {
  .grid-2 {
    grid-template-columns: 1fr;
  }
}

.param-list {
  margin-bottom: 12px;
}

.param-row {
  display: grid;
  grid-template-columns: 1fr 1fr 40px;
  gap: 8px;
  align-items: center;
  margin-top: 8px;
}

.param-row input {
  min-width: 0;
}

.param-head {
  display: grid;
  grid-template-columns: 1fr 1fr 40px;
  gap: 8px;
  margin-top: 12px;
  font-weight: 600;
  font-size: 0.9rem;
}

.btn-icon {
  width: 40px;
  height: 40px;
  padding: 0;
  font-size: 1.2rem;
  line-height: 1;
}

.password-row {
  display: flex;
  gap: 8px;
}

.password-row input {
  flex: 1;
  min-width: 0;
}

.btn {
  appearance: none;
  border: 0;
  border-radius: 8px;
  padding: 12px 16px;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
}

.btn:disabled {
  opacity: 0.55;
  cursor: default;
}

.btn-primary {
  width: 100%;
  margin-top: 16px;
  background: var(--primary);
  color: var(--primary-text);
}

.btn-secondary {
  background: var(--secondary);
  color: var(--text);
}

.btn-small {
  padding: 8px 12px;
  font-size: 0.9rem;
  white-space: nowrap;
}

.network-list {
  list-style: none;
  margin: 8px 0 0;
  padding: 0;
  max-height: 320px;
  overflow-y: auto;
}

.network {
  width: 100%;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 12px;
  margin-bottom: 6px;
  background: var(--bg);
  color: var(--text);
  border: 1px solid var(--border);
  border-radius: 8px;
  font-size: 1rem;
  text-align: left;
  cursor: pointer;
}

.network.selected {
  border-color: var(--primary);
  box-shadow: 0 0 0 2px var(--focus);
}

.network-name {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.network-meta {
  display: flex;
  align-items: center;
  gap: 8px;
  color: var(--muted);
  font-size: 0.85rem;
  flex-shrink: 0;
}

.bars {
  display: inline-flex;
  align-items: flex-end;
  gap: 2px;
  height: 14px;
}

.bars i {
  display: block;
  width: 4px;
  background: var(--border);
  border-radius: 1px;
}

.bars i:nth-child(1) { height: 25%; }
.bars i:nth-child(2) { height: 50%; }
.bars i:nth-child(3) { height: 75%; }
.bars i:nth-child(4) { height: 100%; }

.bars i.on {
  background: var(--primary);
}

.message {
  margin-top: 14px;
  padding: 12px;
  border-radius: 8px;
  font-size: 0.95rem;
}

.message.success {
  background: var(--success-bg);
  color: var(--success);
}

.message.error {
  background: var(--error-bg);
  color: var(--error);
}

.message.warning {
  background: var(--warning-bg);
  color: var(--warning);
}

.message.info {
  background: var(--secondary);
  color: var(--text);
}

.preview-list {
  margin: 8px 0 0;
  padding-left: 0;
  list-style: none;
}

.preview-list li {
  margin-bottom: 10px;
}

.preview-label {
  font-weight: 600;
  font-size: 0.85rem;
  display: block;
  margin-bottom: 4px;
}

.preview-url {
  display: block;
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  font-size: 0.82rem;
  word-break: break-all;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 6px;
  padding: 8px;
}

.preview-url.invalid {
  color: var(--error);
}

.status-badge {
  display: inline-block;
  padding: 2px 10px;
  border-radius: 999px;
  font-size: 0.8rem;
  font-weight: 600;
}

.status-badge.valid {
  background: var(--success-bg);
  color: var(--success);
}

.status-badge.incomplete {
  background: var(--warning-bg);
  color: var(--warning);
}

#configForm {
  margin-bottom: 16px;
}

.actions {
  display: flex;
  justify-content: flex-end;
  margin-bottom: 16px;
}

[hidden] {
  display: none !important;
}
)ESP32WEB";

// web/wifi.js
const char WEB_WIFI_JS[] PROGMEM = R"ESP32WEB('use strict';

(function () {
  const MAX_SSID_BYTES = 32;
  const SCAN_POLL_MS = 1000;

  const $ = (id) => document.getElementById(id);
  const form = $('wifiForm');
  const ssidInput = $('ssid');
  const passwordInput = $('password');
  const saveBtn = $('saveBtn');
  const refreshBtn = $('refreshBtn');
  const networkList = $('networkList');
  const scanStatus = $('scanStatus');
  const passwordHint = $('passwordHint');

  let networks = [];
  let pollTimer = null;
  let saved = false;

  const byteLength = (text) => new TextEncoder().encode(text).length;

  function showMessage(text, type) {
    const box = $('message');
    box.textContent = text;
    box.className = 'message ' + type;
    box.hidden = false;
  }

  function signalLevel(rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -78) return 2;
    return 1;
  }

  function knownNetwork(ssid) {
    return networks.find((n) => n.ssid === ssid);
  }

  function updatePasswordHint() {
    const net = knownNetwork(ssidInput.value);
    if (net && !net.secure) {
      passwordHint.textContent = 'Red abierta: no necesita contraseña.';
    } else if (net) {
      passwordHint.textContent = 'Red protegida: introduce su contraseña (8 a 63 caracteres).';
    } else {
      passwordHint.textContent = 'Déjala vacía solo si la red es abierta.';
    }
  }

  function markSelected() {
    networkList.querySelectorAll('.network').forEach((btn) => {
      btn.classList.toggle('selected', btn.dataset.ssid === ssidInput.value);
    });
  }

  function renderNetworks() {
    networkList.textContent = '';
    networks.forEach((net) => {
      const li = document.createElement('li');
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'network';
      btn.dataset.ssid = net.ssid;

      const name = document.createElement('span');
      name.className = 'network-name';
      name.textContent = net.ssid;  // textContent: el SSID nunca se interpreta como HTML.

      const meta = document.createElement('span');
      meta.className = 'network-meta';
      const detail = document.createElement('span');
      detail.textContent = (net.secure ? '🔒 ' : 'Abierta · ') + net.rssi + ' dBm';
      const bars = document.createElement('span');
      bars.className = 'bars';
      bars.setAttribute('aria-hidden', 'true');
      const level = signalLevel(net.rssi);
      for (let i = 1; i <= 4; i++) {
        const bar = document.createElement('i');
        if (i <= level) bar.className = 'on';
        bars.appendChild(bar);
      }
      meta.appendChild(detail);
      meta.appendChild(bars);

      btn.appendChild(name);
      btn.appendChild(meta);
      btn.addEventListener('click', () => {
        ssidInput.value = net.ssid;
        markSelected();
        updatePasswordHint();
        if (net.secure) passwordInput.focus();
      });
      li.appendChild(btn);
      networkList.appendChild(li);
    });
    markSelected();
  }

  async function loadScan(refresh) {
    clearTimeout(pollTimer);
    refreshBtn.disabled = true;
    try {
      const res = await fetch('/api/scan' + (refresh ? '?refresh=1' : ''), { cache: 'no-store' });
      const data = await res.json();
      networks = Array.isArray(data.networks) ? data.networks : [];
      renderNetworks();
      updatePasswordHint();
      if (data.scanning) {
        scanStatus.textContent = networks.length ? 'Actualizando lista de redes…' : 'Buscando redes…';
        pollTimer = setTimeout(() => loadScan(false), SCAN_POLL_MS);
        return;
      }
      scanStatus.textContent = networks.length
        ? networks.length + (networks.length === 1 ? ' red encontrada.' : ' redes encontradas.')
        : 'No se encontraron redes. Pulsa «Actualizar» o escribe el nombre manualmente.';
    } catch (err) {
      scanStatus.textContent = 'No se pudo obtener la lista de redes. Pulsa «Actualizar».';
    }
    refreshBtn.disabled = saved;
  }

  function validate(ssid, password) {
    if (!ssid) return 'Selecciona una red o escribe su nombre.';
    if (byteLength(ssid) > MAX_SSID_BYTES) return 'El nombre de la red no puede superar 32 bytes.';
    const len = password.length;
    if (len === 0) {
      const net = knownNetwork(ssid);
      if (net && net.secure) return 'La red seleccionada está protegida: introduce su contraseña.';
      return null;
    }
    if (len === 64) {
      return /^[0-9a-fA-F]{64}$/.test(password)
        ? null
        : 'Una contraseña de 64 caracteres debe ser una clave hexadecimal.';
    }
    if (len < 8 || len > 63) return 'La contraseña debe tener entre 8 y 63 caracteres.';
    return null;
  }

  async function save(event) {
    event.preventDefault();
    const ssid = ssidInput.value;
    const password = passwordInput.value;
    const error = validate(ssid, password);
    if (error) {
      showMessage(error, 'error');
      return;
    }

    saveBtn.disabled = true;
    showMessage('Guardando…', 'info');
    try {
      const res = await fetch('/api/wifi', {
        method: 'POST',
        body: new URLSearchParams({ ssid: ssid, password: password }),
      });
      const data = await res.json();
      if (!res.ok || !data.ok) throw new Error(data.error || 'Error ' + res.status);
      saved = true;
      clearTimeout(pollTimer);
      refreshBtn.disabled = true;
      ssidInput.disabled = true;
      passwordInput.disabled = true;
      showMessage(data.message + ' Esta red de configuración desaparecerá en unos segundos.', 'success');
    } catch (err) {
      saveBtn.disabled = false;
      showMessage('No se pudo guardar: ' + err.message, 'error');
    }
  }

  async function loadInfo() {
    try {
      const res = await fetch('/api/info', { cache: 'no-store' });
      const data = await res.json();
      if (data.name) $('deviceName').textContent = data.name;
    } catch (err) {
      $('deviceName').textContent = 'ESP32';
    }
  }

  $('togglePassword').addEventListener('click', (e) => {
    const show = passwordInput.type === 'password';
    passwordInput.type = show ? 'text' : 'password';
    e.currentTarget.textContent = show ? 'Ocultar' : 'Mostrar';
  });
  ssidInput.addEventListener('input', () => {
    markSelected();
    updatePasswordHint();
  });
  refreshBtn.addEventListener('click', () => loadScan(true));
  form.addEventListener('submit', save);

  loadInfo();
  loadScan(true);
})();
)ESP32WEB";

// web/admin.js
const char WEB_ADMIN_JS[] PROGMEM = R"ESP32WEB('use strict';

(function () {
  // Límites en bytes; deben coincidir con ESP32_Control/Config.h (el firmware vuelve a validar).
  const LIMITS = { url: 256, param: 32, value: 64, adminMin: 8, adminMax: 64 };
  const BUTTONS = ['b1', 'b2', 'b3', 'b4'];
  const CONFIG_FIELDS = ['url', 'valueParam'].concat(BUTTONS);
  // Ejemplos para las primeras filas de parámetros adicionales.
  const PARAM_PLACEHOLDERS = [['dispositivo', 'ESP32_001'], ['clave', 'abc123'], ['codificacion', 'utf-8']];
  let maxParams = 8;  // Lo actualiza el firmware en /api/admin/config.

  const $ = (id) => document.getElementById(id);
  const encoder = new TextEncoder();
  let token = null;

  // ------------------------------------------------------------ Utilidades

  function showMessage(id, text, type) {
    const box = $(id);
    box.textContent = text;
    box.className = 'message ' + type;
    box.hidden = false;
  }

  function hideMessage(id) {
    $(id).hidden = true;
  }

  const byteLength = (text) => encoder.encode(text).length;
  const fieldValue = (id) => $(id).value.trim();

  // ------------------------------------------------------------ Lógica de URL
  // Réplica de ESP32_Control/UrlUtils.cpp para que la vista previa coincida con lo que envía el ESP32.

  function encodeComponent(text) {
    let out = '';
    for (const b of encoder.encode(text)) {
      const isUnreserved =
        (b >= 0x41 && b <= 0x5a) || (b >= 0x61 && b <= 0x7a) || (b >= 0x30 && b <= 0x39) ||
        b === 0x2d || b === 0x5f || b === 0x2e || b === 0x7e;
      out += isUnreserved ? String.fromCharCode(b) : '%' + b.toString(16).toUpperCase().padStart(2, '0');
    }
    return out;
  }

  function schemeLength(u) {
    if (u.startsWith('https://')) return 8;
    if (u.startsWith('http://')) return 7;
    return 0;
  }

  function authorityEnd(u, start) {
    for (let i = start; i < u.length; i++) {
      if (u[i] === '/' || u[i] === '?') return i;
    }
    return u.length;
  }

  function validateBaseUrl(u) {
    if (!u) return 'La URL está vacía';
    if (u.length > LIMITS.url) return 'La URL supera ' + LIMITS.url + ' caracteres';
    for (let i = 0; i < u.length; i++) {
      const c = u.charCodeAt(i);
      if (c <= 0x20 || c >= 0x7f) {
        return 'La URL contiene espacios o caracteres no ASCII (deben ir codificados con %XX)';
      }
      if (u[i] === '#') return 'La URL no debe contener un fragmento (#)';
    }
    const start = schemeLength(u);
    if (!start) return 'La URL debe comenzar por http:// o https:// (en minúsculas)';
    const authority = u.substring(start, authorityEnd(u, start));
    const hostPort = authority.substring(authority.lastIndexOf('@') + 1);
    const colon = hostPort.indexOf(':');
    const host = colon >= 0 ? hostPort.substring(0, colon) : hostPort;
    if (!host) return 'La URL no contiene un host';
    if (!/^[A-Za-z0-9._-]+$/.test(host)) return 'El host de la URL contiene caracteres no válidos';
    if (colon >= 0) {
      const port = hostPort.substring(colon + 1);
      if (!/^[0-9]{1,5}$/.test(port) || Number(port) < 1 || Number(port) > 65535) {
        return 'El puerto de la URL no es válido';
      }
    }
    return null;
  }

  function buildRequestUrl(base, params, valueParam, value) {
    const end = authorityEnd(base, schemeLength(base));
    let out = end < base.length && base[end] === '/' ? base : base.substring(0, end) + '/' + base.substring(end);
    if (out.indexOf('?') < 0) out += '?';
    else if (!out.endsWith('?') && !out.endsWith('&')) out += '&';
    params.forEach((p) => { out += encodeComponent(p.name) + '=' + encodeComponent(p.value) + '&'; });
    return out + encodeComponent(valueParam) + '=' + encodeComponent(value);
  }

  // ------------------------------------------------------------ Validación

  function checkText(value, label, max) {
    if (!value) return 'El campo "' + label + '" es obligatorio';
    if (byteLength(value) > max) return 'El campo "' + label + '" supera ' + max + ' caracteres';
    if (/[\u0000-\u001f\u007f]/.test(value)) return 'El campo "' + label + '" contiene caracteres de control';
    return null;
  }

  function validateParams(params) {
    if (params.length > maxParams) return 'Se admiten como máximo ' + maxParams + ' parámetros adicionales';
    for (let i = 0; i < params.length; i++) {
      const p = params[i];
      const label = 'Parámetro ' + (i + 1);
      const error = checkText(p.name, label + ': nombre', LIMITS.param) ||
        checkText(p.value, label + ': valor', LIMITS.value);
      if (error) return error;
      if (params.slice(0, i).some((q) => q.name === p.name)) return 'El parámetro "' + p.name + '" está repetido';
    }
    return null;
  }

  function validateValueParam(c) {
    return (
      checkText(c.valueParam, 'Parámetro de los botones', LIMITS.param) ||
      (c.params.some((p) => p.name === c.valueParam)
        ? 'El parámetro de los botones ("' + c.valueParam + '") no puede coincidir con un parámetro adicional'
        : null)
    );
  }

  function validateConfig(c) {
    return (
      checkText(c.url, 'URL de la API', LIMITS.url) ||
      validateBaseUrl(c.url) ||
      validateParams(c.params) ||
      validateValueParam(c) ||
      BUTTONS.map((b, i) => checkText(c[b], 'Valor del botón ' + (i + 1), LIMITS.value)).find(Boolean) ||
      null
    );
  }

  // Filas con nombre y valor vacíos se ignoran.
  function readParams() {
    const params = [];
    $('paramList').querySelectorAll('.param-row').forEach((row) => {
      const inputs = row.querySelectorAll('input');
      const p = { name: inputs[0].value.trim(), value: inputs[1].value.trim() };
      if (p.name || p.value) params.push(p);
    });
    return params;
  }

  function readForm() {
    const c = {};
    CONFIG_FIELDS.forEach((f) => { c[f] = fieldValue(f); });
    c.params = readParams();
    return c;
  }

  function toRequestBody(c) {
    const body = new URLSearchParams();
    CONFIG_FIELDS.forEach((f) => body.append(f, c[f]));
    body.append('paramCount', String(c.params.length));
    c.params.forEach((p, i) => {
      body.append('pn' + i, p.name);
      body.append('pv' + i, p.value);
    });
    return body;
  }

  // ------------------------------------------------------------ Parámetros adicionales

  function updateParamControls() {
    const count = $('paramList').children.length;
    $('paramHead').hidden = count === 0;
    $('addParamBtn').disabled = count >= maxParams;
  }

  function createParamInput(value, placeholder, maxLength, ariaLabel) {
    const input = document.createElement('input');
    input.type = 'text';
    input.value = value;
    input.placeholder = placeholder;
    input.maxLength = maxLength;
    input.setAttribute('aria-label', ariaLabel);
    input.autocapitalize = 'none';
    input.setAttribute('autocorrect', 'off');
    input.spellcheck = false;
    return input;
  }

  function addParamRow(name, value) {
    const list = $('paramList');
    if (list.children.length >= maxParams) return null;
    const example = PARAM_PLACEHOLDERS[list.children.length] || ['nombre', 'valor'];
    const row = document.createElement('div');
    row.className = 'param-row';
    const nameInput = createParamInput(name || '', example[0], LIMITS.param, 'Nombre del parámetro');
    const valueInput = createParamInput(value || '', example[1], LIMITS.value, 'Valor del parámetro');
    const remove = document.createElement('button');
    remove.type = 'button';
    remove.className = 'btn btn-secondary btn-icon';
    remove.textContent = '×';
    remove.title = 'Quitar parámetro';
    remove.setAttribute('aria-label', 'Quitar parámetro');
    remove.addEventListener('click', () => {
      row.remove();
      updateParamControls();
      hideMessage('configMessage');
      updatePreview();
    });
    row.appendChild(nameInput);
    row.appendChild(valueInput);
    row.appendChild(remove);
    list.appendChild(row);
    updateParamControls();
    return nameInput;
  }

  // ------------------------------------------------------------ Vista previa

  function updatePreview() {
    const c = readForm();
    const list = $('preview');
    list.textContent = '';
    const commonError = (c.url ? validateBaseUrl(c.url) : 'Falta la URL de destino') ||
      validateParams(c.params) ||
      (c.valueParam ? validateValueParam(c) : 'Falta el nombre del parámetro de los botones');

    BUTTONS.forEach((b, i) => {
      const li = document.createElement('li');
      const label = document.createElement('span');
      label.className = 'preview-label';
      label.textContent = 'Botón ' + (i + 1);
      const code = document.createElement('code');
      code.className = 'preview-url';

      let problem = commonError;
      if (!problem && !c[b]) problem = 'Falta el valor del botón ' + (i + 1);
      if (problem) {
        code.classList.add('invalid');
        code.textContent = problem;
      } else {
        code.textContent = 'GET ' + buildRequestUrl(c.url, c.params, c.valueParam, c[b]);
      }
      li.appendChild(label);
      li.appendChild(code);
      list.appendChild(li);
    });
  }

  // ------------------------------------------------------------ Comunicación con el ESP32

  async function api(path, options) {
    const opts = Object.assign({ cache: 'no-store', headers: {} }, options || {});
    if (token) opts.headers['X-Auth-Token'] = token;
    const res = await fetch(path, opts);
    let data = {};
    try {
      data = await res.json();
    } catch (e) {
      data = { ok: false, error: 'Respuesta no válida del dispositivo' };
    }
    if (res.status === 401 && token) {
      showLogin('La sesión ha expirado. Vuelve a iniciar sesión.');
      throw new Error(data.error || 'Sesión no válida');
    }
    if (!res.ok || !data.ok) throw new Error(data.error || 'Error ' + res.status);
    return data;
  }

  function showLogin(message) {
    token = null;
    $('configView').hidden = true;
    $('loginView').hidden = false;
    $('loginPassword').value = '';
    if (message) showMessage('loginMessage', message, 'warning');
  }

  function setStatus(status, missing) {
    const badge = $('statusBadge');
    if (status === 'valid') {
      badge.className = 'status-badge valid';
      badge.textContent = 'Configuración válida';
      badge.title = '';
    } else {
      badge.className = 'status-badge incomplete';
      badge.textContent = 'Configuración incompleta';
      badge.title = missing ? 'Falta: ' + missing : '';
    }
  }

  async function loadConfig() {
    const data = await api('/api/admin/config');
    const c = data.config;
    if (data.maxParams) maxParams = data.maxParams;
    $('url').value = c.url;
    $('paramList').textContent = '';
    (c.params || []).forEach((p) => addParamRow(p.name, p.value));
    if (!$('paramList').children.length) addParamRow('', '');  // Fila vacía de ejemplo.
    updateParamControls();
    $('valueParam').value = c.valueParam;
    BUTTONS.forEach((b, i) => { $(b).value = c.buttons[i] || ''; });
    setStatus(data.status, data.missing);
    $('initialNotice').hidden = !data.initialPassword;
    updatePreview();
  }

  async function login(event) {
    event.preventDefault();
    const password = $('loginPassword').value;
    if (!password) {
      showMessage('loginMessage', 'Introduce la contraseña administrativa.', 'error');
      return;
    }
    $('loginBtn').disabled = true;
    try {
      const data = await api('/api/admin/login', {
        method: 'POST',
        body: new URLSearchParams({ password: password }),
      });
      token = data.token;
      hideMessage('loginMessage');
      await loadConfig();
      $('loginView').hidden = true;
      $('configView').hidden = false;
    } catch (err) {
      showLogin();
      showMessage('loginMessage', err.message, 'error');
    } finally {
      $('loginBtn').disabled = false;
      $('loginPassword').value = '';
    }
  }

  async function saveConfig(event) {
    event.preventDefault();
    const c = readForm();
    const error = validateConfig(c);
    if (error) {
      showMessage('configMessage', error, 'error');
      return;
    }
    $('saveConfigBtn').disabled = true;
    try {
      const data = await api('/api/admin/config', { method: 'POST', body: toRequestBody(c) });
      showMessage('configMessage', data.message, 'success');
      setStatus('valid', '');
    } catch (err) {
      showMessage('configMessage', 'No se pudo guardar: ' + err.message, 'error');
    } finally {
      $('saveConfigBtn').disabled = false;
    }
  }

  async function changePassword(event) {
    event.preventDefault();
    const current = $('currentPassword').value;
    const next = $('newPassword').value;
    const confirm = $('confirmPassword').value;
    let error = null;
    if (!current) error = 'Introduce la contraseña actual.';
    else if (byteLength(next) < LIMITS.adminMin || byteLength(next) > LIMITS.adminMax) {
      error = 'La nueva contraseña debe tener entre 8 y 64 caracteres.';
    } else if (next !== confirm) error = 'Las contraseñas nuevas no coinciden.';
    if (error) {
      showMessage('passwordMessage', error, 'error');
      return;
    }
    $('passwordBtn').disabled = true;
    try {
      const data = await api('/api/admin/password', {
        method: 'POST',
        body: new URLSearchParams({ current: current, next: next }),
      });
      showMessage('passwordMessage', data.message, 'success');
      $('initialNotice').hidden = true;
      $('passwordForm').reset();
    } catch (err) {
      showMessage('passwordMessage', 'No se pudo cambiar: ' + err.message, 'error');
    } finally {
      $('passwordBtn').disabled = false;
    }
  }

  async function logout() {
    try {
      await api('/api/admin/logout', { method: 'POST' });
    } catch (e) {
      // La sesión se descarta en el navegador igualmente.
    }
    showLogin('Sesión cerrada.');
  }

  $('loginForm').addEventListener('submit', login);
  $('configForm').addEventListener('submit', saveConfig);
  $('configForm').addEventListener('input', () => {
    hideMessage('configMessage');
    updatePreview();
  });
  $('addParamBtn').addEventListener('click', () => {
    const input = addParamRow('', '');
    if (input) input.focus();
    updatePreview();
  });
  $('passwordForm').addEventListener('submit', changePassword);
  $('logoutBtn').addEventListener('click', logout);
})();
)ESP32WEB";
