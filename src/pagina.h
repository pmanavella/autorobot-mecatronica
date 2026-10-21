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
<title>Control Robot</title>
<style>
  :root {
    --bg: #0D1626; --ink: #16202F; --muted: #55627A;
    --txt-fondo: #E7ECF3; --txt-fondo-suave: #A9B6CC;   /* textos que van sobre el fondo azul oscuro */
    --body: #23314A; --body-hi: #33466B; --panel: #1A2539;
    --key: #F5C542; --key-lo: #C99A1A; --stop: #E8574C; --stop-lo: #A8352D;
    --led-off: #3B4A66; --led-on: #FF5A4E; --ok: #35D07F; --soft: #8E9AB0;
  }
  * { box-sizing: border-box; }
  html, body { margin: 0; min-height: 100%; }
  body {
    background: var(--bg); color: var(--txt-fondo);
    font-family: "Avenir Next", "Segoe UI", Roboto, system-ui, sans-serif;
    display: flex; flex-direction: column; align-items: center;
    padding: max(16px, env(safe-area-inset-top)) 16px max(20px, env(safe-area-inset-bottom));
    -webkit-user-select: none; user-select: none;
  }
  button { font: inherit; cursor: pointer; }
  button:focus-visible, input:focus-visible { outline: 3px solid var(--key); outline-offset: 3px; }

  header { width: 100%; max-width: 440px; margin-bottom: 18px; }
  h1 { font-size: 1.6rem; margin: 0; font-weight: 700; line-height: 1.1; }
  .estado { display: flex; align-items: center; gap: 8px; font-size: .9rem; color: var(--txt-fondo-suave); margin-top: 4px; }
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
  .pad.auto .tecla:not(.parar) { opacity: .55; }       /* en automático se atenúan las flechas, no el Parar */
  .pad:not(.auto) .tecla.parar { visibility: hidden; }  /* Parar solo existe en automático (mantiene su lugar en la grilla) */
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
  .apagar { display: block; width: 100%; margin-top: 22px; padding: 14px; border: 0; border-radius: 14px;
    background: var(--stop); color: #fff; font-weight: 700; font-size: 1rem; box-shadow: 0 5px 0 var(--stop-lo); }
  .apagar:active { transform: translateY(4px); box-shadow: 0 1px 0 var(--stop-lo); }
  .apagar.encender { background: var(--ok); color: var(--ink); box-shadow: 0 5px 0 #23945A; }
  .control.off .pad, .control.off .modos, .control.off .velocidad { opacity: .35; pointer-events: none; }
  .sonido { margin-top: 22px; background: var(--panel); border-radius: 14px; padding: 14px 16px; }
  .sonido h2 { font-size: 1rem; margin: 0 0 10px; font-weight: 700; display: flex; justify-content: space-between; align-items: center; gap: 8px; }
  .sonido h2 span { font-size: .8rem; font-weight: 600; color: var(--soft); }
  .sonido h2 span.ok { color: var(--ok); }
  .tipos { display: grid; grid-template-columns: repeat(3, 1fr); gap: 4px; padding: 4px; border-radius: 12px; background: var(--bg); }
  .tipos button { border: 0; border-radius: 9px; padding: 9px 4px; background: transparent; color: var(--soft); font-weight: 600; font-size: .9rem; }
  .tipos button[aria-pressed="true"] { background: #E7ECF3; color: var(--ink); }
  .opcCancion { margin-top: 12px; display: grid; gap: 10px; }
  .opcCancion[hidden] { display: none; }
  .archivo { display: flex; align-items: center; gap: 10px; }
  .archivo span { flex: 1; min-width: 0; font-size: .85rem; color: var(--soft); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .btnChico { border: 0; border-radius: 10px; padding: 9px 14px; background: var(--key); color: var(--ink); font-weight: 700; box-shadow: 0 4px 0 var(--key-lo); flex: none; }
  .btnChico:active { transform: translateY(3px); box-shadow: 0 1px 0 var(--key-lo); }
  .check { display: flex; align-items: center; gap: 8px; font-size: .9rem; }
  .check input { width: 18px; height: 18px; accent-color: var(--key); }
  .probar { width: 100%; margin-top: 12px; }
  .rectitud { margin-top: 14px; background: var(--panel); border-radius: 14px; padding: 12px 16px; }
  .rectitud summary { font-weight: 700; cursor: pointer; }
  .rectitud p { font-size: .85rem; color: var(--soft); margin: 10px 0 12px; line-height: 1.45; }
  .rectitud label { display: flex; justify-content: space-between; font-size: .9rem; margin: 12px 0 6px; }
  .rectitud output { font-weight: 700; color: var(--key); }
  .info { width: 100%; max-width: 440px; margin-top: 18px; font-size: .9rem; color: var(--txt-fondo-suave); line-height: 1.5; }
  .info p { margin: 0 0 6px; } .info strong { color: var(--txt-fondo); }
  .info h2 { font-size: 1.05rem; font-weight: 700; color: var(--txt-fondo); margin: 18px 0 6px; }
  @media (prefers-reduced-motion: reduce) { * { transition: none !important; } }
</style>
</head>
<body>

<header>
  <h1>Control Robot</h1>
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
    <label for="vel">Velocidad <output id="velTxt" for="vel">10</output></label>
    <input type="range" id="vel" min="0" max="40" step="1" value="10">
  </div>

  <section class="sonido" aria-label="Sonido de marcha atrás">
    <h2>Sonido de marcha atrás <span id="audioEstado">Tocá Probar para activarlo</span></h2>
    <div class="tipos" role="group" aria-label="Qué suena">
      <button data-tipo="pitido" aria-pressed="true">Pitido</button>
      <button data-tipo="cancion" aria-pressed="false">Canción</button>
      <button data-tipo="nada" aria-pressed="false">Nada</button>
    </div>
    <div class="opcCancion" id="opcCancion" hidden>
      <div class="archivo">
        <span id="nombreCancion">Ninguna canción elegida</span>
        <button class="btnChico" id="btnElegir">Elegir</button>
        <input type="file" id="archivo" accept="audio/*" hidden>
      </div>
      <label class="check"><input type="checkbox" id="cortar" checked> Cortar cuando deja de ir para atrás</label>
    </div>
    <button class="btnChico probar" id="btnProbar">&#128264; Probar sonido</button>
  </section>

  <details class="rectitud">
    <summary>Ajuste de rectitud</summary>
    <p>Si el auto se abre hacia la derecha, mové el ajuste a la derecha (frena la rueda izquierda).
       Si se abre hacia la izquierda, a la izquierda. Se guarda en el auto.</p>
    <label for="ajA">Adelante <output id="ajATxt" for="ajA">—</output></label>
    <input type="range" id="ajA" min="-20" max="20" step="0.5" value="0">
    <label for="ajR">Atrás <output id="ajRTxt" for="ajR">—</output></label>
    <input type="range" id="ajR" min="-20" max="20" step="0.5" value="0">
  </details>

  <button class="apagar" id="btnApagar">&#x23FB; Apagar</button>
</main>

<section class="info">
  <p>Última orden: <strong id="ultima">ninguna</strong></p>
  <h2>¿Cómo manejarlo?</h2>
  <p>Mantené apretada una flecha para mover el auto; al soltarla, frena.
     En modo automático avanza solo y esquiva obstáculos: para frenarlo aparece el botón Parar
     del medio (vuelve a manual), y tocar una flecha también lo pasa a manual.
     Con teclado: flechas o W A S D, y barra espaciadora para parar.
     Con "Apagar" el auto frena y no obedece nada hasta tocar "Encender".</p>
  <h2>Sonido de marcha atrás</h2>
  <p>Emparejá el celular con el parlante Bluetooth y subí el volumen multimedia.
     Al abrir esta página tocá <strong>Probar sonido</strong> una vez: los celulares no dejan
     que una página haga ruido hasta que la tocás. La canción elegida queda guardada en este celular.</p>
</section>

<script>
  const DISTANCIA_MINIMA = 25;  // igual que DISTANCIA_MINIMA_CM en main.cpp
  const NOMBRES = { F: 'Adelante', B: 'Atrás', L: 'Izquierda', R: 'Derecha', S: 'Parar', A: 'Modo automático', M: 'Modo manual' };
  const TECLAS  = { arrowup: 'F', w: 'F', arrowdown: 'B', s: 'B', arrowleft: 'L', a: 'L', arrowright: 'R', d: 'R', ' ': 'S' };
  const $ = id => document.getElementById(id);

  let dirActual = 'S', modo = 'M', modoLocalHasta = 0;
  let cola = [];            // órdenes sueltas que no se pueden perder (parar, modo, velocidad)
  let ocupado = false, apurar = false, ultimaRespuesta = 0;
  let apagado = false, apagadoLocalHasta = 0, nivelSincronizado = false;

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
    if (apagado) return;
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
    if (apagado && dir !== 'S') return;
    if (modo === 'A') { setModo('M'); modoLocalHasta = Date.now() + 800; }
    dirActual = dir;
    // Marcha atrás local: suena ya, sin esperar la respuesta de la ESP32
    if (dir === 'B') { retroLocalHasta = Date.now() + 700; empezarSonido(); }
    else retroLocalHasta = 0;
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

  // ================= Ajuste de rectitud =================
  let ajusteSincronizado = false;
  function textoAjuste(v) {
    v = Number(v);
    if (v === 0) return '0 %';
    return Math.abs(v).toFixed(1) + ' % ' + (v > 0 ? '(frena izq.)' : '(frena der.)');
  }
  [['ajA', 'TA:'], ['ajR', 'TR:']].forEach(([id, cmd]) => {
    $(id).addEventListener('input', () => { $(id + 'Txt').textContent = textoAjuste($(id).value); });
    $(id).addEventListener('change', () => { cola.push(cmd + $(id).value); tick(); });
  });

  // ================= Apagado =================
  function setApagado(a) {
    apagado = a;
    $('btnApagar').innerHTML = a ? '&#x23FB; Encender' : '&#x23FB; Apagar';
    $('btnApagar').classList.toggle('encender', a);
    document.querySelector('.control').classList.toggle('off', a);
  }
  $('btnApagar').addEventListener('click', () => {
    const a = !apagado;
    if (a) { dirActual = 'S'; marcarTecla(null); setModo('M'); }
    setApagado(a); apagadoLocalHasta = Date.now() + 800;
    $('ultima').textContent = a ? 'Apagado' : 'Encendido';
    cola.push(a ? 'OFF' : 'ON'); tick();
  });

  // ================= Estado que responde la ESP32 =================
  // Formato: "E:distancia,moviendo,led,modo,apagado,nivel,direccion,reversas,ajusteAdelante,ajusteAtras"
  //   ej: "E:42,1,0,M,0,10,B,3,5.0,3.0"
  function procesar(texto) {
    if (!texto.startsWith('E:')) return;
    const [d, mov, led, m, off, niv, dir, nRev, ajA, ajR] = texto.slice(2).split(',');
    if (ajR !== undefined && !ajusteSincronizado) {   // al abrir la página, toma los ajustes guardados en el auto
      $('ajA').value = ajA; $('ajATxt').textContent = textoAjuste(ajA);
      $('ajR').value = ajR; $('ajRTxt').textContent = textoAjuste(ajR);
      ajusteSincronizado = true;
    }
    if (dir !== undefined) revisarReversa(dir, Number(nRev));
    mostrarDistancia(Number(d));
    $('led').classList.toggle('on', led === '1');
    if ((m === 'A' || m === 'M') && m !== modo && Date.now() > modoLocalHasta) setModo(m);
    if (off !== undefined && (off === '1') !== apagado && Date.now() > apagadoLocalHasta) setApagado(off === '1');
    if (niv !== undefined && !nivelSincronizado) {   // al abrir la página, toma el nivel que tiene el auto
      $('vel').value = niv; $('velTxt').textContent = niv; nivelSincronizado = true;
    }
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
  // ================= Sonido de marcha atrás =================
  // El celular está emparejado al parlante Bluetooth: todo lo que suena acá sale por el parlante.
  // Se dispara de dos formas:
  //   1) al apretar la flecha Atrás en este celular (inmediato), y
  //   2) cuando la ESP32 avisa que está retrocediendo (cubre el modo automático).
  const SONIDO_MIN_MS = 1500;   // aunque retroceda 400 ms, que se llegue a escuchar
  const guardado = (k, def) => { try { return localStorage.getItem(k) ?? def; } catch (e) { return def; } };
  const guardar  = (k, v) => { try { localStorage.setItem(k, v); } catch (e) {} };

  let tipoSonido = guardado('tipoSonido', 'pitido');   // 'pitido' | 'cancion' | 'nada'
  let cortarSonido = guardado('cortarSonido', '1') === '1';
  let retroAuto = false, retroLocalHasta = 0, ultimasReversas = null;
  let sonando = false, sonidoMinHasta = 0;
  let ctxAudio = null, osc = null, gan = null, timerPitido = null;
  let audioListo = false, urlCancion = null;
  const cancion = new Audio();
  cancion.preload = 'auto';

  // --- Desbloqueo: los navegadores no reproducen nada hasta que el usuario toca la página ---
  function silencio() {   // un WAV de 0,05 s en silencio, para "estrenar" el <audio> en iPhone
    const n = 400, b = new Uint8Array(44 + n), v = new DataView(b.buffer);
    const t = (o, s) => [...s].forEach((c, i) => b[o + i] = c.charCodeAt(0));
    t(0, 'RIFF'); v.setUint32(4, 36 + n, true); t(8, 'WAVEfmt '); v.setUint32(16, 16, true);
    v.setUint16(20, 1, true); v.setUint16(22, 1, true); v.setUint32(24, 8000, true); v.setUint32(28, 8000, true);
    v.setUint16(32, 1, true); v.setUint16(34, 8, true); t(36, 'data'); v.setUint32(40, n, true); b.fill(128, 44);
    return URL.createObjectURL(new Blob([b], { type: 'audio/wav' }));
  }
  function desbloquearAudio() {
    try {
      if (!ctxAudio) ctxAudio = new (window.AudioContext || window.webkitAudioContext)();
      if (ctxAudio.state !== 'running') ctxAudio.resume();
    } catch (e) {}
    if (audioListo) return;
    if (!urlCancion) cancion.src = silencio();
    cancion.muted = true;
    cancion.play().then(() => {
      cancion.muted = false;
      if (!sonando) { cancion.pause(); cancion.currentTime = 0; }
      audioListo = true;
      $('audioEstado').textContent = 'Audio activado';
      $('audioEstado').classList.add('ok');
    }).catch(() => { cancion.muted = false; });
  }
  ['pointerdown', 'pointerup', 'touchend', 'click', 'keydown'].forEach(ev =>
    document.addEventListener(ev, desbloquearAudio, { capture: true }));

  // --- Pitido de camión: 1 kHz, 0,4 s sí / 0,4 s no, generado por el celular (no necesita archivo) ---
  function iniciarPitido() {
    if (!ctxAudio) return;
    ctxAudio.resume();
    gan = ctxAudio.createGain(); gan.gain.value = 0; gan.connect(ctxAudio.destination);
    osc = ctxAudio.createOscillator(); osc.type = 'square'; osc.frequency.value = 1050;
    osc.connect(gan); osc.start();
    let on = false;
    const alternar = () => { on = !on; if (gan) gan.gain.setTargetAtTime(on ? 0.25 : 0, ctxAudio.currentTime, 0.005); };
    alternar();
    timerPitido = setInterval(alternar, 400);
  }
  function detenerPitido() {
    clearInterval(timerPitido); timerPitido = null;
    if (osc) { try { osc.stop(); } catch (e) {} osc.disconnect(); osc = null; }
    if (gan) { gan.disconnect(); gan = null; }
  }

  // --- Canción elegida del celular ---
  function iniciarCancion() {
    if (!urlCancion || !cancion.paused) return;   // si sigue sonando (sin cortar), no la reinicia
    cancion.muted = false; cancion.currentTime = 0;
    cancion.play().catch(() => {});
  }
  function detenerCancion() { cancion.pause(); }

  // --- Lógica común ---
  function empezarSonido(minimoMs = SONIDO_MIN_MS) {
    if (sonando || tipoSonido === 'nada') return;
    sonando = true;
    sonidoMinHasta = Date.now() + minimoMs;
    if (tipoSonido === 'pitido') iniciarPitido(); else iniciarCancion();
  }
  function terminarSonido() { sonando = false; detenerPitido(); detenerCancion(); }

  function retrocediendo() { return retroAuto || Date.now() < retroLocalHasta; }

  // Se llama con cada respuesta de la ESP32
  function revisarReversa(dir, reversas) {
    const huboNueva = ultimasReversas !== null && reversas !== ultimasReversas;  // por si un retroceso cortito pasó entre dos consultas
    ultimasReversas = reversas;
    retroAuto = dir === 'B';
    if (retroAuto || huboNueva) empezarSonido();
  }

  // Cada 100 ms: ¿hay que callarse?
  setInterval(() => {
    if (!sonando || retrocediendo() || Date.now() < sonidoMinHasta) return;
    if (tipoSonido === 'cancion' && !cortarSonido) { sonando = false; return; }   // la deja terminar
    terminarSonido();
  }, 100);

  // --- Guardar la canción en el celular (IndexedDB) para no elegirla cada vez ---
  function db() {
    return new Promise((ok, mal) => {
      const r = indexedDB.open('autoRobot', 1);
      r.onupgradeneeded = () => r.result.createObjectStore('archivos');
      r.onsuccess = () => ok(r.result); r.onerror = () => mal(r.error);
    });
  }
  async function guardarCancion(dato) {
    const base = await db();
    await new Promise((ok, mal) => {
      const tx = base.transaction('archivos', 'readwrite');
      tx.objectStore('archivos').put(dato, 'cancion');
      tx.oncomplete = ok; tx.onerror = () => mal(tx.error);
    });
  }
  async function leerCancion() {
    const base = await db();
    return new Promise((ok, mal) => {
      const r = base.transaction('archivos').objectStore('archivos').get('cancion');
      r.onsuccess = () => ok(r.result); r.onerror = () => mal(r.error);
    });
  }
  function usarCancion(blob, nombre) {
    if (urlCancion) URL.revokeObjectURL(urlCancion);
    urlCancion = URL.createObjectURL(blob);
    cancion.src = urlCancion;
    $('nombreCancion').textContent = nombre;
  }

  // --- Controles ---
  function mostrarTipo() {
    document.querySelectorAll('.tipos button').forEach(b => b.setAttribute('aria-pressed', b.dataset.tipo === tipoSonido));
    $('opcCancion').hidden = tipoSonido !== 'cancion';
    $('btnProbar').disabled = tipoSonido === 'nada';
    $('btnProbar').style.opacity = tipoSonido === 'nada' ? .4 : 1;
  }
  document.querySelectorAll('.tipos button').forEach(b => b.addEventListener('click', () => {
    terminarSonido();
    tipoSonido = b.dataset.tipo; guardar('tipoSonido', tipoSonido);
    mostrarTipo();
  }));
  $('cortar').checked = cortarSonido;
  $('cortar').addEventListener('change', () => { cortarSonido = $('cortar').checked; guardar('cortarSonido', cortarSonido ? '1' : '0'); });
  $('btnElegir').addEventListener('click', () => $('archivo').click());
  $('archivo').addEventListener('change', async () => {
    const f = $('archivo').files[0];
    if (!f) return;
    terminarSonido();
    const blob = new Blob([await f.arrayBuffer()], { type: f.type || 'audio/mpeg' });
    usarCancion(blob, f.name);
    try { await guardarCancion({ nombre: f.name, blob }); } catch (e) { /* si no se puede guardar, igual sirve en esta sesión */ }
  });
  $('btnProbar').addEventListener('click', () => {
    desbloquearAudio();
    terminarSonido();
    empezarSonido(3000);
  });

  mostrarTipo();
  leerCancion().then(d => { if (d && d.blob) usarCancion(d.blob, d.nombre); }).catch(() => {});

  tick();
</script>
</body>
</html>
)rawliteral";