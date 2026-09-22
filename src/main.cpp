/*
  Proyecto 1 - Seminario de Mecatrónica

  Qué hace este código:
  - Controla 2 motores DC a través de un driver L9110S (PWM + dirección).
  - Lee un sensor ultrasónico (HC-SR04 o similar) para medir distancia al frente.
  - Hace parpadear un LED SOLO mientras las ruedas se están moviendo.
  - Se controla desde una página web por Bluetooth (BLE), con dos modos:
      * Manual (M): el auto obedece las flechas de la web.
        Si hay un obstáculo cerca, no deja avanzar (sí retroceder o girar).
      * Automático (A): avanza solo y esquiva obstáculos.
  - Le manda a la web la distancia medida, si se mueve, el LED y el modo.

  Órdenes que recibe desde la web (texto):
    F = adelante, B = atrás, L = izquierda, R = derecha, S = parar
    A = modo automático, M = modo manual
    V:180 = cambiar velocidad (0-255)

  Estado que envía a la web cada 200 ms:
    "E:distancia,moviendo,led,modo"   ej: "E:42,1,0,M"
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---------- PINES: MOTOR A (rueda izquierda) ----------
#define MOTOR_A_IN1 27
#define MOTOR_A_IN2 26

// ---------- PINES: MOTOR B (rueda derecha) ----------
#define MOTOR_B_IN1 25
#define MOTOR_B_IN2 33

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

// ---------- BLUETOOTH (tienen que coincidir con la web) ----------
#define NOMBRE_BLE    "AutoRobot"
#define SERVICIO_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID       "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  // web -> ESP32
#define TX_UUID       "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  // ESP32 -> web

// ---------- PARÁMETROS DE COMPORTAMIENTO ----------
const int VELOCIDAD_CRUCERO = 180;              // velocidad inicial (0-255)
const int DISTANCIA_MINIMA_CM = 15;             // distancia a la que esquiva / frena
const unsigned long BLINK_INTERVAL_MS = 250;    // parpadeo del LED al moverse
const unsigned long TIEMPO_SEGURIDAD_MS = 600;  // en manual: sin órdenes -> frena

// ---------- ESTADO INTERNO ----------
bool robotEnMovimiento = false;
bool ledEncendido = false;
unsigned long ultimoBlink = 0;
long distanciaActual = -1;

// ---------- ESTADO QUE CAMBIA LA WEB (se modifica desde el Bluetooth) ----------
BLECharacteristic *txChar = nullptr;
volatile bool conectado = false;
volatile char modo = 'M';          // 'M' manual, 'A' automático
volatile char ordenManual = 'S';   // última flecha apretada en la web
volatile int velocidad = VELOCIDAD_CRUCERO;
volatile unsigned long ultimaOrden = 0;

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
//  COMUNICACIÓN CON LA WEB
// =================================================================

// Cada 200 ms le cuenta a la web qué está pasando (y lo imprime por Serial)
void enviarEstado() {
  static unsigned long ultimoEnvio = 0;
  unsigned long ahora = millis();
  if (ahora - ultimoEnvio < 200) return;
  ultimoEnvio = ahora;

  char msg[32];
  snprintf(msg, sizeof(msg), "E:%ld,%d,%d,%c",
           distanciaActual, robotEnMovimiento, ledEncendido, (char)modo);

  if (conectado && txChar != nullptr) {
    txChar->setValue((uint8_t *)msg, strlen(msg));
    txChar->notify();
  }
  Serial.println(msg);  // debug en el Monitor Serie
}

class ServidorCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) {
    conectado = true;
    Serial.println("Web conectada");
  }
  void onDisconnect(BLEServer *s) {
    conectado = false;
    modo = 'M';           // por seguridad, si se pierde la web, frena
    ordenManual = 'S';
    Serial.println("Web desconectada");
    BLEDevice::startAdvertising();  // para poder volver a conectar
  }
};

class OrdenesCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    String orden = c->getValue().c_str();
    orden.trim();
    ultimaOrden = millis();

    if (orden.startsWith("V:")) {
      velocidad = constrain(orden.substring(2).toInt(), 0, 255);
    } else if (orden == "A") {
      modo = 'A';
    } else if (orden == "M") {
      modo = 'M';
      ordenManual = 'S';
    } else if (orden.length() == 1 && strchr("FBLRS", orden[0])) {
      modo = 'M';          // tocar una flecha toma el control manual
      ordenManual = orden[0];
    }
  }
};

void iniciarBluetooth() {
  BLEDevice::init(NOMBRE_BLE);
  BLEServer *servidor = BLEDevice::createServer();
  servidor->setCallbacks(new ServidorCB());

  BLEService *servicio = servidor->createService(SERVICIO_UUID);
  txChar = servicio->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  txChar->addDescriptor(new BLE2902());

  BLECharacteristic *rx = servicio->createCharacteristic(
      RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  rx->setCallbacks(new OrdenesCB());
  servicio->start();

  BLEDevice::getAdvertising()->setScanResponse(true);
  BLEDevice::startAdvertising();
}

// =================================================================
//  MODOS DE FUNCIONAMIENTO
// =================================================================

// Espera sin congelar el LED ni la web; se corta si el usuario pasa a manual
void esperar(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms && modo == 'A') {
    actualizarLed();
    enviarEstado();
    delay(1);
  }
}

// Su comportamiento original: avanza y esquiva
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
  unsigned long t = ultimaOrden;

  // Si la web deja de mandar órdenes (se soltó la flecha o se cortó), frena
  if (orden != 'S' && millis() - t > TIEMPO_SEGURIDAD_MS) {
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
  iniciarBluetooth();
  Serial.println("Robot listo. Esperando la web por Bluetooth...");
}

void loop() {
  // Medimos distancia cada 60 ms (el sensor necesita ese tiempo entre lecturas)
  static unsigned long ultimaMedicion = 0;
  if (millis() - ultimaMedicion >= 60) {
    ultimaMedicion = millis();
    distanciaActual = medirDistanciaCm();
  }
  bool obstaculo = distanciaActual > 0 && distanciaActual < DISTANCIA_MINIMA_CM;

  if (modo == 'A') modoAutomatico(obstaculo);
  else             modoManual(obstaculo);

  actualizarLed();
  enviarEstado();
}