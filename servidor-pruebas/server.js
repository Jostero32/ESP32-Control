'use strict';

// Servidor de pruebas para ESP32-Control.
// Registra las peticiones GET que envían los botones y las muestra en un panel web.
// Sin dependencias: solo Node.js (18 o superior).
//
//   node server.js            -> puerto 3000
//   PORT=8080 node server.js  -> otro puerto (PowerShell: $env:PORT=8080; node server.js)
//
// Rutas:
//   /panel                 Panel web (index.html)
//   /panel/api/eventos     GET lista de eventos · DELETE vacía la lista
//   /panel/api/modo        POST ?modo=200|409|404|500|timeout  (respuesta a simular)
//   cualquier otra ruta    Se registra como petición del ESP32 (p. ej. /evento?dispositivo=..&valor=..)

const http = require('http');
const fs = require('fs');
const os = require('os');
const path = require('path');

const PORT = Number(process.env.PORT) || 3000;
const MAX_EVENTS = 200;
const TIMEOUT_HOLD_MS = 20000;  // En modo "timeout" la conexión se mantiene sin responder.
const MODES = ['200', '409', '404', '500', 'timeout'];

const events = [];
let nextId = 1;
let responseMode = '200';

function lanAddresses() {
  const result = [];
  for (const [name, list] of Object.entries(os.networkInterfaces())) {
    for (const addr of list || []) {
      if (addr.family === 'IPv4' && !addr.internal) result.push({ name, address: addr.address });
    }
  }
  return result;
}

function sendJson(res, code, body) {
  res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(body));
}

function timestamp() {
  return new Date().toLocaleTimeString('es', { hour12: false }) + '.' +
    String(new Date().getMilliseconds()).padStart(3, '0');
}

function recordEvent(req, url) {
  const event = {
    id: nextId++,
    time: new Date().toISOString(),
    ip: (req.socket.remoteAddress || '').replace(/^::ffff:/, ''),
    method: req.method,
    path: url.pathname,
    query: Object.fromEntries(url.searchParams),
    rawUrl: req.url,
    userAgent: req.headers['user-agent'] || '',
    response: responseMode,
  };
  events.unshift(event);
  if (events.length > MAX_EVENTS) events.pop();

  const params = Object.entries(event.query).map(([k, v]) => `${k}=${JSON.stringify(v)}`).join(' ');
  console.log(`[${timestamp()}] #${event.id} ${event.method} ${event.rawUrl}  desde ${event.ip}` +
    `\n            parámetros: ${params || '(ninguno)'}  ->  respuesta simulada: ${responseMode}`);
  return event;
}

function respondToDevice(res, event) {
  switch (responseMode) {
    case '409':
      return sendJson(res, 409, { ok: false, error: 'Simulado: ya registrado', id: event.id });
    case '404':
      return sendJson(res, 404, { ok: false, error: 'Simulado: no encontrado', id: event.id });
    case '500':
      return sendJson(res, 500, { ok: false, error: 'Simulado: error interno', id: event.id });
    case 'timeout':
      // No se responde: el ESP32 debe registrar un timeout (espera 5 s).
      setTimeout(() => res.destroy(), TIMEOUT_HOLD_MS);
      return undefined;
    default:
      return sendJson(res, 200, { ok: true, id: event.id, recibido: event.query });
  }
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');

  if (url.pathname === '/panel' || url.pathname === '/panel/') {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' });
    return fs.createReadStream(path.join(__dirname, 'index.html')).pipe(res);
  }
  if (url.pathname === '/panel/api/eventos') {
    if (req.method === 'DELETE') {
      events.length = 0;
      console.log(`[${timestamp()}] Lista de eventos vaciada desde el panel`);
      return sendJson(res, 200, { ok: true });
    }
    return sendJson(res, 200, { events, mode: responseMode, port: PORT, addresses: lanAddresses() });
  }
  if (url.pathname === '/panel/api/modo' && req.method === 'POST') {
    const mode = url.searchParams.get('modo');
    if (!MODES.includes(mode)) return sendJson(res, 400, { ok: false, error: 'Modo no válido' });
    responseMode = mode;
    console.log(`[${timestamp()}] Respuesta simulada cambiada a: ${mode}`);
    return sendJson(res, 200, { ok: true, mode });
  }
  if (url.pathname === '/favicon.ico') {
    res.writeHead(204);
    return res.end();
  }
  if (req.method !== 'GET') return sendJson(res, 405, { ok: false, error: 'Solo se aceptan peticiones GET' });

  return respondToDevice(res, recordEvent(req, url));
});

server.listen(PORT, '0.0.0.0', () => {
  console.log('Servidor de pruebas ESP32-Control');
  console.log(`  Panel:            http://localhost:${PORT}/panel`);
  const addresses = lanAddresses();
  if (addresses.length === 0) {
    console.log('  (No se encontró ninguna IP de red local: conecta el PC a la misma red Wi-Fi que el ESP32)');
  }
  for (const { name, address } of addresses) {
    console.log(`  URL para /admin:  http://${address}:${PORT}/evento    (${name})`);
  }
  console.log('Esperando peticiones... (Ctrl+C para salir)\n');
});

server.on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.error(`El puerto ${PORT} está ocupado. Usa otro: PORT=3001 node server.js`);
  } else {
    console.error(err);
  }
  process.exit(1);
});
