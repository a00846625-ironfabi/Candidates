#include <U8g2lib.h>
#include <Servo.h>

// pantalla
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /*SCL=*/ 42, /*SDA=*/ 41, U8X8_PIN_NONE);

// servo
Servo servo;
const int servoPin = 6;
bool servoMovido = false;

// ultrasonicos
const int echo1 = 31, trig1 = 30; // Frontal
const int echo2 = 33, trig2 = 32; // Izquierda
const int echo3 = 35, trig3 = 34; // Derecha
const int echo4 = 43, trig4 = 44; // Pelota

// sensor de color
const int PIN_S0 = 36, PIN_S1 = 37, PIN_S2 = 39, PIN_S3 = 38, PIN_OUT = 40;
const int LIMITE_NADA = 250;
const unsigned long INTERVALO_COLOR = 300;
unsigned long ultimaLecturaColor = 0;
String colorActual = " ";

// motores
// Lado izquierdo (U3)
const int L_IN1 = 26, L_IN2 = 27, L_ENA = 4;
const int L_IN3 = 28, L_IN4 = 29, L_ENB = 5;

// Lado derecho (U2)
const int R_IN1 = 22, R_IN2 = 23, R_ENA = 2;
const int R_IN3 = 24, R_IN4 = 25, R_ENB = 3;

const int VELOCIDAD = 100;
const int DIST_OBSTACULO = 15;
const unsigned long TIMEOUT_US = 12000; // ~2 metros máx para no congelar el loop


void ponerMotor(int in1, int in2, int en, bool adelante, int vel) {
  digitalWrite(in1, adelante ? HIGH : LOW);
  digitalWrite(in2, adelante ? LOW : HIGH);
  analogWrite(en, vel);
}

void avanzar() {
  ponerMotor(L_IN1, L_IN2, L_ENA, true, VELOCIDAD);
  ponerMotor(L_IN3, L_IN4, L_ENB, true, VELOCIDAD);
  ponerMotor(R_IN1, R_IN2, R_ENA, true, VELOCIDAD);
  ponerMotor(R_IN3, R_IN4, R_ENB, true, VELOCIDAD);
}

void girarIzquierda() {
  ponerMotor(L_IN1, L_IN2, L_ENA, false, VELOCIDAD);
  ponerMotor(L_IN3, L_IN4, L_ENB, false, VELOCIDAD);
  ponerMotor(R_IN1, R_IN2, R_ENA, true, VELOCIDAD);
  ponerMotor(R_IN3, R_IN4, R_ENB, true, VELOCIDAD);
}

void girarDerecha() {
  ponerMotor(L_IN1, L_IN2, L_ENA, true, VELOCIDAD);
  ponerMotor(L_IN3, L_IN4, L_ENB, true, VELOCIDAD);
  ponerMotor(R_IN1, R_IN2, R_ENA, false, VELOCIDAD);
  ponerMotor(R_IN3, R_IN4, R_ENB, false, VELOCIDAD);
}

void parar() {
  analogWrite(L_ENA, 0); analogWrite(L_ENB, 0);
  analogWrite(R_ENA, 0); analogWrite(R_ENB, 0);
  digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, LOW); digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
}

// ultrasonicos y color
long medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duracion = pulseIn(echo, HIGH, TIMEOUT_US);
  if (duracion == 0) return 400;
  return duracion * 0.034 / 2;
}

int leerCanalColor(bool s2, bool s3) {
  digitalWrite(PIN_S2, s2);
  digitalWrite(PIN_S3, s3);
  delayMicroseconds(100);
  return pulseIn(PIN_OUT, LOW, 15000);
}

String detectarColor() {
  int r = leerCanalColor(LOW, LOW);
  int g = leerCanalColor(HIGH, HIGH);
  int b = leerCanalColor(LOW, HIGH);

  if (r == 0 || g == 0 || b == 0) return "---";
  if (r > LIMITE_NADA && g > LIMITE_NADA && b > LIMITE_NADA) return "---";

  float gr = (float)g / r;
  float br = (float)b / r;

  if (gr < 0.8 && br < 0.8 && abs(g - b) < 0.25 * max(g, b)) return "CIAN";
  if (gr < 1.3 && br > 1.5) return "AMARILLO";
  if (gr >= 1.3 && b > g * 1.1) return "NARANJA";
  if (gr >= 1.3 && g > b * 1.1) return "ROSA";

  return "---";
}

void mostrarColor(const String &texto) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(0, 24, texto.c_str());
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(9600);

  int pinesMotores[] = { L_IN1, L_IN2, L_ENA, L_IN3, L_IN4, L_ENB,
                         R_IN1, R_IN2, R_ENA, R_IN3, R_IN4, R_ENB };
  for (int i = 0; i < 12; i++) pinMode(pinesMotores[i], OUTPUT);
  parar();

  pinMode(trig1, OUTPUT); pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT); pinMode(echo2, INPUT);
  pinMode(trig3, OUTPUT); pinMode(echo3, INPUT);
  pinMode(trig4, OUTPUT); pinMode(echo4, INPUT);

  pinMode(PIN_S0, OUTPUT); pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT); pinMode(PIN_S3, OUTPUT);
  pinMode(PIN_OUT, INPUT);
  digitalWrite(PIN_S0, HIGH); digitalWrite(PIN_S1, LOW);

  u8g2.begin();
  mostrarColor("INICIANDO");

  servo.attach(servoPin);
  servo.write(0);

  delay(2000); // espera 2 segundos
}

void loop() {
  // 1. Lecturas de distancia
  long d1 = medirDistancia(trig1, echo1); // Delante
  long d2 = medirDistancia(trig2, echo2); // Izquierda
  long d3 = medirDistancia(trig3, echo3); // Derecha
  long d4 = medirDistancia(trig4, echo4); // Pelota

  // 2. Lógica de navegación con evitación de obstáculos
  if (d1 < DIST_OBSTACULO) {
    if (d2 > d3) {
      girarIzquierda();
    } else {
      girarDerecha();
    }
  } else {
    avanzar();
  }

  // 3. Captura de la pelota con Servo
  if (d4 < DIST_OBSTACULO && !servoMovido) {
    servo.write(90);
    servoMovido = true;
  }

  // 4. Muestreo de color sin bloquear el bucle principal
  if (millis() - ultimaLecturaColor >= INTERVALO_COLOR) {
    ultimaLecturaColor = millis();
    String c = detectarColor();
    if (c != colorActual) {
      colorActual = c;
      mostrarColor(colorActual);
    }
  }
}