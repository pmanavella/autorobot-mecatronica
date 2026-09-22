// Página web de control que sirve la propia ESP32.
// Para cambiar la interfaz, editá el HTML que está entre R"rawliteral( y )rawliteral".
#pragma once
#include <Arduino.h>

const char PAGINA[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Control AutoRobot</title>
<style>
  :root {
    --bg: #DDE3EA; --ink: #16202F; --muted: #55627A;
    --body: #23314A; --body-hi: #33466B; --panel: #1A2539;
    --key: #F5C542; --key-lo: #C99A1A; --stop: #E8574C; --stop-lo: #A8352D;
    --led-off: #3B4A66; --led-on: #FF5A4E; --ok: #35D07F; --soft: #8E9AB0;
  }
  @media (prefers-color-scheme: dark) { :root { --bg: #10151D; --ink: #E7ECF3; --muted: #8E9AB0; } }
  * { box-sizing: border-box; }
  html, body { margin: 0; min-height: 100%; }
  body {
    background: var(--bg); color: var(--ink);
    font-family: "Avenir Next", "Segoe UI", Roboto, system-ui, sans-serif;
    display: flex; flex-direction: column; align-items: center;
    padding: max(16px, env(safe-area-inset-top)) 16px max(20px, env(safe-area-inset-bottom));
    -webkit-user-select: none; user-select: none;
  }
  button { font: inherit; cursor: pointer; }
  button:focus-visible, input:focus-visible { outline: 3px solid var(--key); outline-offset: 3px; }

  header { width: 100%; max-width: 440px; margin-bottom: 18px; }
  h1 { font-size: 1.6rem; margin: 0; font-weight: 700; line-height: 1.1; }
  .estado { display: flex; align-items: center; gap: 8px; font-size: .9rem; color: var(--muted); margin-top: 4px; }
  .punto { width: 10px; height: 10px; border-radius: 50%; background: var(--stop); flex: none; }
  .estado[data-s="on"] .punto { background: var(--ok); box-shadow: 0 0 8px var(--ok); }

  .control {
    width: 100%; max-width: 440px;
    background: linear-gradient(var(--body-hi), var(--body));
    border-radius: 28px 28px 48px 48px; padding: 20px 20px 28px; color: #E7ECF3;
    box-shadow: inset 0 2px 0 rgba(255,255,255,.12), 0 16px 32px rgba(16,24,40,.35);
  }
  .pantalla { background: var(--panel); border-radius: 14px; padding: 14px 16px; box-shadow: inset 0 2px 6px rgba(0,0,0,.45); }
  .fila { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
  .distancia { font-size: 2.1rem; font-weight: 700; line-height: 1; font-variant-numeric: tabular-nums; }
  .distancia small { font-size: .95rem; font-weight: 400; color: var(--soft); margin-left: 4px; }
  .luz { display: flex; align-items: center; gap: 8px; font-size: .8rem; color: var(--soft); text-align: right; }
  .led { width: 20px; height: 20px; border-radius: 50%; flex: none; background: var(--led-off); box-shadow: inset 0 -3px 4px rgba(0,0,0,.35); }
  .led.on { background: var(--led-on); box-shadow: 0 0 14px var(--led-on), inset 0 -3px 4px rgba(0,0,0,.2); }
  .barra { height: 8px; border-radius: 4px; background: #2C3A55; margin: 12px 0 8px; overflow: hidden; }
  .barra span { display: block; height: 100%; width: 0; background: var(--ok); border-radius: 4px; transition: width .2s, background .2s; }
  .nota { font-size: .85rem; color: var(--soft); min-height: 1.3em; }
  .nota.alerta { color: #FF8A80; font-weight: 600; }

  .modos { display: grid; grid-template-columns: 1fr 1fr; gap: 4px; margin-top: 16px; padding: 4px; border-radius: 12px; background: var(--panel); }
  .modos button { border: 0; border-radius: 9px; padding: 10px; background: transparent; color: var(--soft); font-weight: 600; }
  .modos button[aria-pressed="true"] { background: #E7ECF3; color: var(--ink); }

  .pad { display: grid; grid-template-columns: repeat(3, 1fr); grid-template-rows: repeat(3, 1fr); gap: 12px; width: min(100%, 290px); aspect-ratio: 1; margin: 26px auto 28px; transition: opacity .2s; }
  .pad.auto { opacity: .55; }
  .tecla {
    border: 0; border-radius: 18px; background: var(--key); color: var(--ink);
    box-shadow: 0 6px 0 var(--key-lo); font-size: 1.9rem; display: grid; place-items: center;
    touch-action: none; -webkit-tap-highlight-color: transparent; transition: transform .05s, box-shadow .05s;
  }
  .tecla.activa { transform: translateY(5px); box-shadow: 0 1px 0 var(--key-lo); }
  .tecla.parar { background: var(--stop); color: #fff; box-shadow: 0 6px 0 var(--stop-lo); border-radius: 50%; font-size: 1rem; font-weight: 700; }
  .tecla.parar.activa { box-shadow: 0 1px 0 var(--stop-lo); }
  [data-dir="F"] { grid-area: 1 / 2; } [data-dir="L"] { grid-area: 2 / 1; } [data-dir="S"] { grid-area: 2 / 2; }
  [data-dir="R"] { grid-area: 2 / 3; } [data-dir="B"] { grid-area: 3 / 2; }

  .velocidad label { display: flex; justify-content: space-between; font-size: .9rem; margin-bottom: 8px; }
  .velocidad output { font-weight: 700; color: var(--key); }
  input[type="range"] { width: 100%; accent-color: var(--key); }
  .info { width: 100%; max-width: 440px; margin-top: 18px; font-size: .9rem; color: var(--muted); line-height: 1.5; }
  .info p { margin: 0 0 6px; } .info strong { color: var(--ink); }
  @media (prefers-reduced-motion: reduce) { * { transition: none !important; } }
</style>
</head>
<body>

<header>
  <h1>AutoRobot</h1>
  <div class="estado" id="estado" data-s="off" role="status">
    <span class="punto"></span><span id="estadoTxt">Buscando el auto…</span>
  </div>
</header>

<main class="control">
  <section class="pantalla" aria-label="Sensor ultrasónico">
    <div class="fila">
      <div class="distancia" id="distancia">—<small>cm</small></div>
      <div class="luz"><span>Luz de<br>movimiento</span><span class="led" id="led"></span></div>
    </div>
    <div class="barra"><span id="barra"></span></div>
    <div class="nota" id="nota">Esperando datos del sensor.</div>
  </section>

  <div class="modos" role="group" aria-label="Modo de manejo">
    <button id="btnManual" aria-pressed="true">Manual</button>
    <button id="btnAuto" aria-pressed="false">Automático</button>
  </div>

  <div class="pad" id="pad">
    <button class="tecla" data-dir="F" aria-label="Adelante">&#9650;</button>
    <button class="tecla" data-dir="L" aria-label="Girar a la izquierda">&#9664;</button>
    <button class="tecla parar" data-dir="S" aria-label="Parar">Parar</button>
    <button class="tecla" data-dir="R" aria-label="Girar a la derecha">&#9654;</button>
    <button class="tecla" data-dir="B" aria-label="Atrás">&#9660;</button>
  </div>

  <div class="velocidad">
    <label for="vel">Velocidad <output id="velTxt" for="vel">180</output></label>
    <input type="range" id="vel" min="80" max="255" value="180">
  </div>
</main>

<section class="info">
  <p>Última orden: <strong id="ultima">ninguna</strong></p>
  <p>Mantené apretada una flecha para mover el auto; al soltarla, frena.
     En modo automático avanza solo y esquiva obstáculos; tocar una flecha lo vuelve a manual.
     Con teclado: flechas o W A S D, y barra espaciadora para parar.</p>
</section>

<script>
  const DISTANCIA_MINIMA = 15;  // igual que DISTANCIA_MINIMA_CM en main.cpp
  const NOMBRES = { F: 'Adelante', B: 'Atrás', L: 'Izquierda', R: 'Derecha', S: 'Parar', A: 'Modo automático', M: 'Modo manual' };
  const TECLAS  = { arrowup: 'F', w: 'F', arrowdown: 'B', s: 'B', arrowleft: 'L', a: 'L', arrowright: 'R', d: 'R', ' ': 'S' };
  const $ = id => document.getElementById(id);

  let dirActual = 'S', modo = 'M', modoLocalHasta = 0;
  let cola = [];            // órdenes sueltas que no se pueden perder (parar, modo, velocidad)
  let ocupado = false, apurar = false, ultimaRespuesta = 0;

  // ================= Comunicación con la ESP32 =================
  // Se hace un pedido a la vez. Cada respuesta trae el estado del auto.
  async function pedir(orden) {
    const ctrl = new AbortController();
    const t = setTimeout(() => ctrl.abort(), 1500);
    try {
      const url = orden ? '/cmd?o=' + encodeURIComponent(orden) : '/estado';
      const r = await fetch(url, { cache: 'no-store', signal: ctrl.signal });
      procesar(await r.text());
      ultimaRespuesta = Date.now();
    } catch (e) { /* sin respuesta: lo muestra actualizarConexion() */ }
    clearTimeout(t);
  }

  async function tick() {
    if (ocupado) { apurar = true; return; }
    ocupado = true; apurar = false;
    // prioridad: órdenes en cola > flecha apretada > solo preguntar el estado
    const orden = cola.length ? cola.shift() : (dirActual !== 'S' ? dirActual : null);
    await pedir(orden);
    ocupado = false;
    if (cola.length || apurar) tick();
  }
  setInterval(tick, 200);   // mientras se mantiene una flecha, la orden se repite; si deja de llegar, la ESP32 frena sola

  function actualizarConexion() {
    const ok = Date.now() - ultimaRespuesta < 1200;
    $('estado').dataset.s = ok ? 'on' : 'off';
    $('estadoTxt').textContent = ok ? 'Conectado al auto' : 'Sin respuesta. Revisá estar conectado a la red WiFi AutoRobot';
  }
  setInterval(actualizarConexion, 500);

  // ================= Modo =================
  function setModo(m) {
    modo = m;
    $('btnManual').setAttribute('aria-pressed', m === 'M');
    $('btnAuto').setAttribute('aria-pressed', m === 'A');
    $('pad').classList.toggle('auto', m === 'A');
  }
  function elegirModo(m) {
    dirActual = 'S'; marcarTecla(null);
    setModo(m); modoLocalHasta = Date.now() + 800;
    $('ultima').textContent = NOMBRES[m];
    cola.push(m); tick();
  }
  $('btnManual').addEventListener('click', () => elegirModo('M'));
  $('btnAuto').addEventListener('click', () => elegirModo('A'));

  // ================= Movimiento =================
  function marcarTecla(dir) {
    document.querySelectorAll('.tecla').forEach(t => t.classList.toggle('activa', t.dataset.dir === dir));
  }
  function mover(dir) {
    if (modo === 'A') { setModo('M'); modoLocalHasta = Date.now() + 800; }
    dirActual = dir;
    $('ultima').textContent = NOMBRES[dir];
    marcarTecla(dir);
    if (dir === 'S') {
      cola.push('S');
      setTimeout(() => { if (dirActual === 'S') marcarTecla(null); }, 150);
    }
    tick();
  }
  function soltar() { if (dirActual !== 'S') mover('S'); }

  document.querySelectorAll('.tecla').forEach(tecla => {
    const dir = tecla.dataset.dir;
    tecla.addEventListener('pointerdown', e => { e.preventDefault(); tecla.setPointerCapture(e.pointerId); mover(dir); });
    ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(ev =>
      tecla.addEventListener(ev, () => { if (dir !== 'S' && dirActual === dir) soltar(); }));
    tecla.addEventListener('contextmenu', e => e.preventDefault());
  });
  window.addEventListener('keydown', e => {
    const dir = TECLAS[e.key.toLowerCase()];
    if (!dir) return;
    e.preventDefault();
    if (!e.repeat) mover(dir);
  });
  window.addEventListener('keyup', e => {
    const dir = TECLAS[e.key.toLowerCase()];
    if (dir && dir === dirActual) soltar();
  });
  window.addEventListener('blur', soltar);
  document.addEventListener('visibilitychange', () => { if (document.hidden) soltar(); });

  // ================= Velocidad =================
  $('vel').addEventListener('input', () => { $('velTxt').textContent = $('vel').value; });
  $('vel').addEventListener('change', () => { cola.push('V:' + $('vel').value); tick(); });

  // ================= Estado que responde la ESP32 =================
  // Formato: "E:distancia,moviendo,led,modo"   ej: "E:42,1,0,M"
  function procesar(texto) {
    if (!texto.startsWith('E:')) return;
    const [d, mov, led, m] = texto.slice(2).split(',');
    mostrarDistancia(Number(d));
    $('led').classList.toggle('on', led === '1');
    if ((m === 'A' || m === 'M') && m !== modo && Date.now() > modoLocalHasta) setModo(m);
  }
  function mostrarDistancia(d) {
    const barra = $('barra'), nota = $('nota');
    nota.classList.remove('alerta');
    if (d < 0) {
      $('distancia').innerHTML = '+200<small>cm</small>';
      barra.style.width = '100%'; barra.style.background = 'var(--ok)';
      nota.textContent = 'Camino libre al frente.';
      return;
    }
    $('distancia').innerHTML = d + '<small>cm</small>';
    barra.style.width = Math.min(100, d) + '%';
    if (d < DISTANCIA_MINIMA) {
      barra.style.background = 'var(--stop)';
      nota.textContent = modo === 'A' ? 'Obstáculo cerca: esquivando.' : 'Obstáculo cerca: no puede avanzar.';
      nota.classList.add('alerta');
    } else if (d < DISTANCIA_MINIMA * 2) {
      barra.style.background = 'var(--key)';
      nota.textContent = 'Algo se acerca al frente.';
    } else {
      barra.style.background = 'var(--ok)';
      nota.textContent = 'Camino libre al frente.';
    }
  }
  tick();
</script>
</body>
</html>
)rawliteral";