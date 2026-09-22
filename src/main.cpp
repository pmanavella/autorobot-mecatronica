/*
  Proyecto 1 - Seminario de Mecatrónica

  Qué hace este código:
  - Controla 2 motores DC a través de un driver L9110S (PWM + dirección).
  - Lee un sensor ultrasónico (HC-SR04 o similar) para medir distancia al frente.
  - Hace parpadear un LED SOLO mientras las ruedas se están moviendo.
  - Crea su propia red WiFi ("AutoRobot") y sirve la página de control
    en http://192.168.4.1 (la página está en src/pagina.h).
  - Dos modos:
      * Manual (M): el auto obedece las flechas de la web.
        Si hay un obstáculo cerca, no deja avanzar (sí retroceder o girar).
      * Automático (A): avanza solo y esquiva obstáculos.

  Pedidos que atiende (HTTP):
    /             -> la página de control
    /cmd?o=F      -> orden: F adelante, B atrás, L izquierda, R derecha, S parar,
                     A automático, M manual, V:180 velocidad (0-255)
    /estado       -> solo devuelve el estado
  Cada respuesta es el estado: "E:distancia,moviendo,led,modo"  ej: "E:42,1,0,M"
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "pagina.h"

// ---------- PINES: MOTOR A (rueda izquierda) ----------
// IN1 e IN2 invertidos respecto al cableado original porque
// los motores giraban al revés (adelante iba hacia atrás).
#define MOTOR_A_IN1 26
#define MOTOR_A_IN2 27

// ---------- PINES: MOTOR B (rueda derecha) ----------
#define MOTOR_B_IN1 33
#define MOTOR_B_IN2 25

// ---------- PINES: SENSOR ULTRASÓNICO ----------
#define TRIG_PIN 5
#define ECHO_PIN 18

// ---------- PIN: LED indicador de movimiento ----------
// GPIO2 = LED integrado de la placa. Cuando conecten un LED externo
// (con resistencia de 220-330 ohm), cambien este número.
#define LED_PIN 2

// ---------- CANALES PWM (ESP32 LEDC) ----------
#define CH_MOTOR_A1 0
#define CH_MOTOR_A2 1
#define CH_MOTOR_B1 2
#define CH_MOTOR_B2 3
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8   // 0-255

// ---------- RED WIFI QUE CREA EL AUTO ----------
const char *WIFI_NOMBRE = "AutoRobot";
const char *WIFI_CLAVE  = "robot1234";   // mínimo 8 caracteres

// ---------- PARÁMETROS DE COMPORTAMIENTO ----------
const int VELOCIDAD_CRUCERO = 180;              // velocidad inicial (0-255)
const int DISTANCIA_MINIMA_CM = 15;             // distancia a la que esquiva / frena
const unsigned long BLINK_INTERVAL_MS = 250;    // parpadeo del LED al moverse
const unsigned long TIEMPO_SEGURIDAD_MS = 600;  // manual: sin órdenes -> frena
const unsigned long TIEMPO_SIN_WEB_MS = 3000;   // automático: si la web desaparece -> frena

// ---------- ESTADO ----------
bool robotEnMovimiento = false;
bool ledEncendido = false;
unsigned long ultimoBlink = 0;
long distanciaActual = -1;

char modo = 'M';          // 'M' manual, 'A' automático
char ordenManual = 'S';   // última flecha recibida
int velocidad = VELOCIDAD_CRUCERO;
unsigned long ultimaOrden = 0;     // última flecha / orden recibida
unsigned long ultimoContacto = 0;  // último pedido cualquiera de la web

WebServer server(80);

// =================================================================
//  CONTROL DE MOTORES
// =================================================================

void detenerMotores() {
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = false;
}

void avanzar(int v) {
  ledcWrite(CH_MOTOR_A1, v);
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, v);
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = true;
}

void retroceder(int v) {
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, v);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, v);
  robotEnMovimiento = true;
}

// Gira sobre su propio eje (una rueda adelante, otra atrás)
void girarDerecha(int v) {
  ledcWrite(CH_MOTOR_A1, v);   // izquierda avanza
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, v);   // derecha retrocede
  robotEnMovimiento = true;
}

void girarIzquierda(int v) {
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, v);   // izquierda retrocede
  ledcWrite(CH_MOTOR_B1, v);   // derecha avanza
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = true;
}

void aplicarOrden(char orden, int v) {
  switch (orden) {
    case 'F': avanzar(v); break;
    case 'B': retroceder(v); break;
    case 'L': girarIzquierda(v); break;
    case 'R': girarDerecha(v); break;
    default:  detenerMotores(); break;
  }
}

// =================================================================
//  SENSOR ULTRASÓNICO
// =================================================================

// Devuelve la distancia en cm. Si no detecta nada (timeout), devuelve -1.
long medirDistanciaCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return -1;
  return duracion * 0.0343 / 2;
}

// =================================================================
//  LED SINCRONIZADO CON EL MOVIMIENTO (no bloqueante)
// =================================================================

void actualizarLed() {
  if (!robotEnMovimiento) {
    if (ledEncendido) {
      digitalWrite(LED_PIN, LOW);
      ledEncendido = false;
    }
    return;
  }
  unsigned long ahora = millis();
  if (ahora - ultimoBlink >= BLINK_INTERVAL_MS) {
    ultimoBlink = ahora;
    ledEncendido = !ledEncendido;
    digitalWrite(LED_PIN, ledEncendido ? HIGH : LOW);
  }
}

// =================================================================
//  SERVIDOR WEB
// =================================================================

String textoEstado() {
  char msg[32];
  snprintf(msg, sizeof(msg), "E:%ld,%d,%d,%c",
           distanciaActual, robotEnMovimiento, ledEncendido, modo);
  return String(msg);
}

void procesarOrden(String orden) {
  orden.trim();
  if (orden.length() == 0) return;
  ultimaOrden = millis();

  if (orden.startsWith("V:")) {
    velocidad = constrain(orden.substring(2).toInt(), 0, 255);
  } else if (orden == "A") {
    modo = 'A';
  } else if (orden == "M") {
    modo = 'M';
    ordenManual = 'S';
  } else if (orden.length() == 1 && strchr("FBLRS", orden[0])) {
    modo = 'M';            // tocar una flecha toma el control manual
    ordenManual = orden[0];
  }
}

void responderEstado() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", textoEstado());
}

void handlePagina() {
  server.send_P(200, "text/html", PAGINA);
}

void handleCmd() {
  ultimoContacto = millis();
  procesarOrden(server.arg("o"));
  responderEstado();
}

void handleEstado() {
  ultimoContacto = millis();
  responderEstado();
}

void iniciarWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NOMBRE, WIFI_CLAVE);
  // Menos potencia = menos consumo de corriente (alcanza de sobra para un aula)
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  server.on("/", handlePagina);
  server.on("/cmd", handleCmd);
  server.on("/estado", handleEstado);
  server.onNotFound([]() { server.send(404, "text/plain", "No existe"); });
  server.begin();

  Serial.print("Red WiFi: ");
  Serial.print(WIFI_NOMBRE);
  Serial.print("  |  Clave: ");
  Serial.println(WIFI_CLAVE);
  Serial.print("Abrir en el navegador: http://");
  Serial.println(WiFi.softAPIP());
}

// =================================================================
//  MODOS DE FUNCIONAMIENTO
// =================================================================

// Espera sin congelar el LED ni la web; se corta si el usuario pasa a manual
void esperar(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms && modo == 'A') {
    server.handleClient();
    actualizarLed();
    delay(1);
  }
}

// Comportamiento original: avanza y esquiva
void modoAutomatico(bool obstaculo) {
  if (!obstaculo) {
    avanzar(velocidad);
    return;
  }
  detenerMotores();
  esperar(200);
  if (modo != 'A') return;

  retroceder(velocidad);
  esperar(400);
  if (modo != 'A') return;

  girarDerecha(velocidad);
  esperar(500);
}

// Obedece a la web
void modoManual(bool obstaculo) {
  char orden = ordenManual;

  // Si la web deja de mandar la flecha (se soltó o se cortó el WiFi), frena
  if (orden != 'S' && millis() - ultimaOrden > TIEMPO_SEGURIDAD_MS) {
    ordenManual = 'S';
    orden = 'S';
  }
  // No deja avanzar contra un obstáculo
  if (orden == 'F' && obstaculo) orden = 'S';

  aplicarOrden(orden, velocidad);
}

// =================================================================
//  SETUP / LOOP
// =================================================================

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  ledcSetup(CH_MOTOR_A1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_MOTOR_A2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_MOTOR_B1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(CH_MOTOR_B2, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(MOTOR_A_IN1, CH_MOTOR_A1);
  ledcAttachPin(MOTOR_A_IN2, CH_MOTOR_A2);
  ledcAttachPin(MOTOR_B_IN1, CH_MOTOR_B1);
  ledcAttachPin(MOTOR_B_IN2, CH_MOTOR_B2);

  detenerMotores();
  Serial.println("Robot listo.");
  iniciarWifi();
}

void loop() {
  server.handleClient();

  // Medimos distancia cada 60 ms (el sensor necesita ese tiempo entre lecturas)
  static unsigned long ultimaMedicion = 0;
  if (millis() - ultimaMedicion >= 60) {
    ultimaMedicion = millis();
    distanciaActual = medirDistanciaCm();
  }
  bool obstaculo = distanciaActual > 0 && distanciaActual < DISTANCIA_MINIMA_CM;

  // Seguridad: si en automático la web deja de responder, frena
  if (modo == 'A' && millis() - ultimoContacto > TIEMPO_SIN_WEB_MS) {
    modo = 'M';
    ordenManual = 'S';
  }

  if (modo == 'A') modoAutomatico(obstaculo);
  else             modoManual(obstaculo);

  actualizarLed();

  // Debug por Serial cada medio segundo
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint >= 500) {
    ultimoPrint = millis();
    Serial.println(textoEstado());
  }
}