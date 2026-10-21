/*
  Proyecto 1 - Seminario de Mecatrónica

  Qué hace este código:
  - Controla 2 motores DC a través de un driver L9110S (PWM + dirección).
  - Lee un sensor ultrasónico (HC-SR04 o similar) para medir distancia al frente.
  - Hace parpadear un LED SOLO mientras las ruedas se están moviendo.
  - Crea su propia red WiFi ("AutoRobot") y sirve la página de control
    en http://192.168.4.1 (la página está en src/pagina.h).
  - Informa hacia dónde se mueve (para que la web haga sonar la marcha atrás
    por el parlante Bluetooth emparejado al celular).
  - Corrige la diferencia de velocidad entre las ruedas para que vaya recto
    (un ajuste para adelante y otro para atrás, se calibran desde la web y
    quedan guardados en la memoria de la ESP32 aunque se apague).
  - Dos modos:
      * Manual (M): el auto obedece las flechas de la web.
        Si hay un obstáculo cerca, no deja avanzar (sí retroceder o girar).
      * Automático (A): avanza solo y esquiva obstáculos.

  Pedidos que atiende (HTTP):
    /             -> la página de control
    /cmd?o=F      -> orden: F adelante, B atrás, L izquierda, R derecha, S parar,
                     A automático, M manual, V:10 nivel de velocidad (0-40),
                     OFF apagar, ON encender,
                     TA:5.0 ajuste de rectitud adelante, TR:3.0 ajuste atrás (en %)
    /estado       -> solo devuelve el estado
  Cada respuesta es el estado:
    "E:distancia,moviendo,led,modo,apagado,nivel,direccion,reversas,ajusteAdelante,ajusteAtras"
  ej: "E:42,1,0,M,0,10,B,3,5.0,3.0"
    direccion: F, B, L, R o S (la que tienen ahora los motores)
    reversas:  cuántas veces empezó a ir para atrás desde que se prendió.
               Sirve para que la web no se pierda un retroceso corto
               (en automático retrocede solo 400 ms).
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>   // memoria no volátil de la ESP32 (viene incluida, no hay que instalar nada)
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
#define TRIG_PIN 32
#define ECHO_PIN 35

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
// Velocidad: la web manda un nivel de 0 a 40, que acá se traduce a PWM en dos tramos:
//   nivel 0-20  -> PWM 190-210 (el rango en el que el auto anda bien)
//   nivel 20-40 -> PWM 210-255 (hasta el 100 % de duty)
const int VEL_PWM_MIN = 190;                    // nivel 0
const int VEL_PWM_MEDIO = 210;                  // nivel 20
const int VEL_PWM_MAX = 255;                    // nivel 40 (máximo del PWM de 8 bits)
const int NIVEL_MEDIO = 20;
const int NIVEL_MAX = 40;
const int NIVEL_INICIAL = 10;                   // arranca en PWM 200
const int DISTANCIA_MINIMA_CM = 25;             // distancia a la que esquiva / frena
const unsigned long BLINK_INTERVAL_MS = 250;    // parpadeo del LED al moverse
const unsigned long TIEMPO_SEGURIDAD_MS = 600;  // manual: sin órdenes -> frena
const unsigned long TIEMPO_SIN_WEB_MS = 3000;   // automático: si la web desaparece -> frena

// ---------- AJUSTE DE RECTITUD ----------
// Los dos motores no son idénticos: con el mismo PWM la rueda izquierda gira más rápido
// y el auto se va abriendo hacia la derecha. El ajuste es cuánto % más lenta queda la
// rueda rápida respecto de la lenta.
//   positivo -> la izquierda es la rápida (se frena la izquierda)
//   negativo -> la derecha es la rápida (se frena la derecha)
// Son solo los valores de arranque: después se calibran desde la página y se guardan.
// (primera prueba con 5.0 / 3.0: se pasaba y el auto tiraba a la izquierda)
const float AJUSTE_ADELANTE_INICIAL = 3.5;
const float AJUSTE_ATRAS_INICIAL    = 2.0;
const float AJUSTE_MAX = 20.0;

int nivelAPwm(int n) {
  if (n <= NIVEL_MEDIO)
    return VEL_PWM_MIN + n * (VEL_PWM_MEDIO - VEL_PWM_MIN) / NIVEL_MEDIO;
  return VEL_PWM_MEDIO + (n - NIVEL_MEDIO) * (VEL_PWM_MAX - VEL_PWM_MEDIO) / (NIVEL_MAX - NIVEL_MEDIO);
}

// ---------- ESTADO ----------
bool robotEnMovimiento = false;
bool ledEncendido = false;
unsigned long ultimoBlink = 0;
long distanciaActual = -1;

char modo = 'M';          // 'M' manual, 'A' automático
char ordenManual = 'S';   // última flecha recibida
int nivel = NIVEL_INICIAL;
int velocidad = nivelAPwm(NIVEL_INICIAL);
bool apagado = false;     // botón de apagado de la web: frena todo e ignora órdenes
unsigned long ultimaOrden = 0;     // última flecha / orden recibida
unsigned long ultimoContacto = 0;  // último pedido cualquiera de la web

// Marcha atrás (lo usa la web para reproducir el sonido)
char direccion = 'S';              // F, B, L, R o S: lo que están haciendo los motores
unsigned int contadorReversas = 0; // sube 1 cada vez que EMPIEZA a retroceder

float ajusteAdelante = AJUSTE_ADELANTE_INICIAL;
float ajusteAtras = AJUSTE_ATRAS_INICIAL;
Preferences memoria;

WebServer server(80);

// =================================================================
//  CONTROL DE MOTORES
// =================================================================

// Cada función de motor avisa hacia dónde va. Solo cuenta una reversa
// nueva cuando cambia a 'B' (no en cada vuelta del loop).
void marcarDireccion(char d) {
  if (d == 'B' && direccion != 'B') contadorReversas++;
  direccion = d;
}

void detenerMotores() {
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = false;
  marcarDireccion('S');
}

// Reparte la corrección entre las dos ruedas para que la velocidad promedio
// casi no cambie: la rueda lenta sube la mitad del ajuste (sin pasar de 255)
// y la rápida queda "ajuste" % por debajo de la lenta.
// Ej: v = 200, ajuste = 5  ->  lenta 205, rápida 194
void repartirVelocidad(int v, float ajuste, int &vIzq, int &vDer) {
  float a = fabs(ajuste);
  int lenta  = min(VEL_PWM_MAX, (int)(v * (1.0 + a / 200.0) + 0.5));
  int rapida = (int)(lenta * (1.0 - a / 100.0) + 0.5);
  if (ajuste >= 0) { vIzq = rapida; vDer = lenta; }   // izquierda rápida -> se frena
  else             { vIzq = lenta;  vDer = rapida; }  // derecha rápida -> se frena
}

void avanzar(int v) {
  int vIzq, vDer;
  repartirVelocidad(v, ajusteAdelante, vIzq, vDer);
  ledcWrite(CH_MOTOR_A1, vIzq);   // motor A = rueda izquierda
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, vDer);   // motor B = rueda derecha
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = true;
  marcarDireccion('F');
}

void retroceder(int v) {
  int vIzq, vDer;
  repartirVelocidad(v, ajusteAtras, vIzq, vDer);
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, vIzq);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, vDer);
  robotEnMovimiento = true;
  marcarDireccion('B');
}

// Gira sobre su propio eje (una rueda adelante, otra atrás)
void girarDerecha(int v) {
  ledcWrite(CH_MOTOR_A1, v);   // izquierda avanza
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, v);   // derecha retrocede
  robotEnMovimiento = true;
  marcarDireccion('R');
}

void girarIzquierda(int v) {
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, v);   // izquierda retrocede
  ledcWrite(CH_MOTOR_B1, v);   // derecha avanza
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = true;
  marcarDireccion('L');
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
  char msg[80];
  snprintf(msg, sizeof(msg), "E:%ld,%d,%d,%c,%d,%d,%c,%u,%.1f,%.1f",
           distanciaActual, robotEnMovimiento, ledEncendido, modo, apagado, nivel,
           direccion, contadorReversas, ajusteAdelante, ajusteAtras);
  return String(msg);
}

void procesarOrden(String orden) {
  orden.trim();
  if (orden.length() == 0) return;
  ultimaOrden = millis();

  if (orden.startsWith("V:")) {
    nivel = constrain(orden.substring(2).toInt(), 0, NIVEL_MAX);
    velocidad = nivelAPwm(nivel);
  } else if (orden.startsWith("TA:")) {
    ajusteAdelante = constrain(orden.substring(3).toFloat(), -AJUSTE_MAX, AJUSTE_MAX);
    memoria.putFloat("ajAdel", ajusteAdelante);
  } else if (orden.startsWith("TR:")) {
    ajusteAtras = constrain(orden.substring(3).toFloat(), -AJUSTE_MAX, AJUSTE_MAX);
    memoria.putFloat("ajAtras", ajusteAtras);
  } else if (orden == "OFF") {
    apagado = true;
    modo = 'M';
    ordenManual = 'S';
    detenerMotores();
  } else if (orden == "ON") {
    apagado = false;
    ordenManual = 'S';
  } else if (apagado) {
    // apagado: se ignoran flechas y cambio de modo hasta tocar "Encender"
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

  // Ajustes de rectitud guardados (si nunca se calibró, usa los iniciales)
  memoria.begin("robot", false);
  // Si quieren volver a los valores del código e ignorar lo guardado desde la web,
  // descomenten la línea siguiente, suban el código una vez y después vuelvan a comentarla.
  // memoria.clear();
  ajusteAdelante = memoria.getFloat("ajAdel", AJUSTE_ADELANTE_INICIAL);
  ajusteAtras    = memoria.getFloat("ajAtras", AJUSTE_ATRAS_INICIAL);

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

  if (apagado)          detenerMotores();
  else if (modo == 'A') modoAutomatico(obstaculo);
  else                  modoManual(obstaculo);

  actualizarLed();

  // Debug por Serial cada medio segundo
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint >= 500) {
    ultimoPrint = millis();
    Serial.println(textoEstado());
  }
}