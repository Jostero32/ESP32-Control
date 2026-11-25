#!/usr/bin/env node
/**
 * Simulador del portal del ESP32: sirve web/ y responde la API interna con datos de ejemplo.
 * Sirve para editar la interfaz y tomar capturas sin tener la placa conectada.
 *
 *   node tools/simulador-portal.js          → http://localhost:4000  y  http://localhost:4000/admin
 *
 * No usa dependencias. Cualquier contraseña no vacía inicia sesión en /admin
 * (en la placa real se valida contra el hash PBKDF2 guardado en NVS).
 * Lo que se guarda vive solo en memoria mientras el simulador está abierto.
 */
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { randomBytes } = require('node:crypto');

const PORT = Number(process.env.PORT || 4000);
const WEB = path.join(__dirname, '..', 'web');
const MIME = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8' };

const NETWORKS = [
  { ssid: 'MiCasa_2.4G', rssi: -48, secure: true },
  { ssid: 'Oficina-5G-ext', rssi: -61, secure: true },
  { ssid: 'Laboratorio FISEI', rssi: -70, secure: true },
  { ssid: 'Invitados', rssi: -77, secure: false },
  { ssid: 'TP-Link_8F2A', rssi: -86, secure: true },
];

const state = {
  token: null,
  initialPassword: true,
  config: {
    url: 'https://api.ejemplo.com/evento',
    params: [
      { name: 'dispositivo', value: 'ESP32_001' },
      { name: 'codificacion', value: 'utf-8' },
    ],
    valueParam: 'valor',
    buttons: ['1', '2', '3', '4'],
  },
};

const json = (res, code, body) => {
  res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(body));
};

const readForm = (req) =>
  new Promise((resolve) => {
    let raw = '';
    req.on('data', (c) => (raw += c));
    req.on('end', () => resolve(new URLSearchParams(raw)));
  });

function configStatus(c) {
  const missing = [];
  if (!c.url) missing.push('URL');
  if (!c.valueParam) missing.push('parámetro de los botones');
  if (c.buttons.some((b) => !b)) missing.push('valores de los botones');
  return missing.length ? { status: 'incomplete', missing: missing.join(', ') } : { status: 'valid' };
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const p = url.pathname;
  const authed = state.token && req.headers['x-auth-token'] === state.token;

  if (p === '/api/info') return json(res, 200, { ok: true, name: 'ESP32-CONFIG-3A7F' });
  if (p === '/api/scan') return json(res, 200, { ok: true, scanning: false, networks: NETWORKS });
  if (p === '/api/wifi' && req.method === 'POST') {
    const f = await readForm(req);
    return json(res, 200, { ok: true, message: `Red «${f.get('ssid')}» guardada. (Simulador: no se reinicia nada.)` });
  }
  if (p === '/api/admin/login' && req.method === 'POST') {
    const f = await readForm(req);
    if (!f.get('password')) return json(res, 401, { ok: false, error: 'Contraseña incorrecta' });
    state.token = randomBytes(16).toString('hex');
    return json(res, 200, { ok: true, token: state.token });
  }
  if (p.startsWith('/api/admin/') && !authed) return json(res, 401, { ok: false, error: 'Sesión no válida' });
  if (p === '/api/admin/config' && req.method === 'GET') {
    return json(res, 200, { ok: true, config: state.config, maxParams: 8, initialPassword: state.initialPassword, ...configStatus(state.config) });
  }
  if (p === '/api/admin/config' && req.method === 'POST') {
    const f = await readForm(req);
    const n = Number(f.get('paramCount') || 0);
    state.config = {
      url: f.get('url') || '',
      params: Array.from({ length: n }, (_, i) => ({ name: f.get('pn' + i) || '', value: f.get('pv' + i) || '' })),
      valueParam: f.get('valueParam') || '',
      buttons: ['b1', 'b2', 'b3', 'b4'].map((k) => f.get(k) || ''),
    };
    return json(res, 200, { ok: true, message: 'Configuración guardada', ...configStatus(state.config) });
  }
  if (p === '/api/admin/password' && req.method === 'POST') {
    state.initialPassword = false;
    return json(res, 200, { ok: true, message: 'Contraseña administrativa actualizada' });
  }
  if (p === '/api/admin/logout') {
    state.token = null;
    return json(res, 200, { ok: true });
  }

  const file = p === '/' ? 'index.html' : p === '/admin' ? 'admin.html' : p.slice(1);
  const abs = path.join(WEB, path.normalize(file));
  if (!abs.startsWith(WEB) || !fs.existsSync(abs)) {
    res.writeHead(404);
    return res.end('No encontrado');
  }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(abs)] || 'application/octet-stream' });
  fs.createReadStream(abs).pipe(res);
});

server.listen(PORT, () => {
  console.log(`Simulador del portal: http://localhost:${PORT}  ·  admin: http://localhost:${PORT}/admin`);
});
