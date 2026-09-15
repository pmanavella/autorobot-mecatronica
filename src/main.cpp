/*
  Proyecto 1 - Seminario de Mecatrónica

  Qué hace este código:
  - Controla 2 motores DC a través de un driver L9110S (PWM + dirección).
  - Lee un sensor ultrasónico (HC-SR04 o similar) para medir distancia al frente.
  - Hace parpadear un LED SOLO mientras las ruedas se están moviendo
    (se apaga cuando el robot está detenido).
  - Comportamiento simple: avanza, y si detecta un obstáculo cerca, se
    detiene y gira para esquivarlo.

  ---------------------------------------------------------------
  IMPORTANTE: los números de pin de abajo son una propuesta.
  Tenés que revisar tu cableado real (qué GPIO del ESP32 va a cada
  cable del L9110S, del sensor y del LED) y ajustar esta sección.
  ---------------------------------------------------------------
*/

#include <Arduino.h>

// ---------- PINES: MOTOR A (rueda izquierda) ----------
// L9110S: cada motor tiene 2 entradas (A-IA / A-IB). Para avanzar,
// una va en PWM y la otra en LOW; para retroceder, se invierten.
#define MOTOR_A_IN1 27
#define MOTOR_A_IN2 26

// ---------- PINES: MOTOR B (rueda derecha) ----------
#define MOTOR_B_IN1 25
#define MOTOR_B_IN2 33

// ---------- PINES: SENSOR ULTRASÓNICO ----------
#define TRIG_PIN 5
#define ECHO_PIN 18

// ---------- PIN: LED indicador de movimiento ----------
// Mientras no tengas un LED físico conectado, podés dejar este pin
// apuntando al LED integrado de la placa (GPIO2) para probar la lógica.
// Cuando conectes un LED externo (con su resistencia, ~220-330 ohm),
// cambiá este número al GPIO que uses.
#define LED_PIN 2

// ---------- CANALES PWM (ESP32 LEDC) ----------
#define CH_MOTOR_A1 0
#define CH_MOTOR_A2 1
#define CH_MOTOR_B1 2
#define CH_MOTOR_B2 3
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8   // 0-255

// ---------- PARÁMETROS DE COMPORTAMIENTO ----------
const int VELOCIDAD_CRUCERO = 180;   // 0-255
const int DISTANCIA_MINIMA_CM = 15;  // distancia a la que esquiva
const unsigned long BLINK_INTERVAL_MS = 250; // qué tan rápido parpadea el LED al moverse

// ---------- ESTADO INTERNO (para no bloquear con delay()) ----------
bool robotEnMovimiento = false;
bool ledEncendido = false;
unsigned long ultimoBlink = 0;

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

void avanzar(int velocidad) {
  ledcWrite(CH_MOTOR_A1, velocidad);
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, velocidad);
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = true;
}

void retroceder(int velocidad) {
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, velocidad);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, velocidad);
  robotEnMovimiento = true;
}

// Gira sobre su propio eje (una rueda adelante, otra atrás)
void girarDerecha(int velocidad) {
  ledcWrite(CH_MOTOR_A1, velocidad); // izquierda avanza
  ledcWrite(CH_MOTOR_A2, 0);
  ledcWrite(CH_MOTOR_B1, 0);
  ledcWrite(CH_MOTOR_B2, velocidad); // derecha retrocede
  robotEnMovimiento = true;
}

void girarIzquierda(int velocidad) {
  ledcWrite(CH_MOTOR_A1, 0);
  ledcWrite(CH_MOTOR_A2, velocidad); // izquierda retrocede
  ledcWrite(CH_MOTOR_B1, velocidad); // derecha avanza
  ledcWrite(CH_MOTOR_B2, 0);
  robotEnMovimiento = true;
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

  // pulseIn con timeout de 30ms (~5m de rango, de sobra)
  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return -1;

  long distancia = duracion * 0.0343 / 2; // velocidad del sonido
  return distancia;
}

// =================================================================
//  LED SINCRONIZADO CON EL MOVIMIENTO (no bloqueante)
// =================================================================

void actualizarLed() {
  if (!robotEnMovimiento) {
    // Robot detenido -> LED apagado
    if (ledEncendido) {
      digitalWrite(LED_PIN, LOW);
      ledEncendido = false;
    }
    return;
  }

  // Robot en movimiento -> parpadeo cada BLINK_INTERVAL_MS
  unsigned long ahora = millis();
  if (ahora - ultimoBlink >= BLINK_INTERVAL_MS) {
    ultimoBlink = ahora;
    ledEncendido = !ledEncendido;
    digitalWrite(LED_PIN, ledEncendido ? HIGH : LOW);
  }
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
}

void loop() {
  long distancia = medirDistanciaCm();

  if (distancia > 0 && distancia < DISTANCIA_MINIMA_CM) {
    // Obstáculo cerca: frenar y esquivar
    detenerMotores();
    actualizarLed();
    delay(200);

    retroceder(VELOCIDAD_CRUCERO);
    unsigned long t0 = millis();
    while (millis() - t0 < 400) actualizarLed(); // retrocede un toque, sin bloquear el LED

    girarDerecha(VELOCIDAD_CRUCERO);
    t0 = millis();
    while (millis() - t0 < 500) actualizarLed(); // gira, sin bloquear el LED

  } else {
    avanzar(VELOCIDAD_CRUCERO);
  }

  actualizarLed();

  // Debug por Serial (abrí el Monitor Serie en PlatformIO para ver esto)
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");
}