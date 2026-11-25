'use strict';

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
