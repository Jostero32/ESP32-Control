'use strict';

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
