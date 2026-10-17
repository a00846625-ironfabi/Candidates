#include <U8g2lib.h>
#include <Servo.h>

// servo
const int servoPin = 6;

const int echo1 = 31, trig1 = 30; // Frontal
const int echo2 = 33, trig2 = 32; // Izquierda
const int echo3 = 35, trig3 = 34; // Derecha

// pantalla
const int PIN_SDA = 41, PIN_SCL = 42;

// sensor de color
const int PIN_S0 = 36, PIN_S1 = 37, PIN_S2 = 39, PIN_S3 = 38, PIN_OUT = 40;

// motores
// Lado derecho
const int ENB1 = 3, IN3_1 = 24, IN4_1 = 25;   // M1
const int ENA3 = 2, IN1_3 = 22, IN2_3 = 23;   // M3

// Lado izquierdo
const int ENA2 = 4, IN1_2 = 26, IN2_2 = 27;   // M2
const int ENB4 = 5, IN3_4 = 28, IN4_4 = 29;   // M4

const int VELOCIDAD = 150;                 // velocidad de las ruedas 
const int DIST_OBSTACULO_4 = 15;
const int TIEMPO_GIRO_90 = 600;
const unsigned long TIMEOUT_US = 30000;

// tiempo
const unsigned long TIEMPO_ESPERA = 3000;
const unsigned long TIEMPO_AVANCE = 10000;

enum Estado { ESPERA, AVANZA, PARADO };
Estado estado = ESPERA;
unsigned long tEstado = 0;     // cuándo empezó el estado actual
unsigned long tPantalla = 0;   // última actualización de la pantalla

// servo
Servo servo;
bool servoMovido = false;

// pantalla
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C pantalla(U8G2_R0, PIN_SCL, PIN_SDA, U8X8_PIN_NONE);

// color
const int LIMITE_NADA = 250;
const unsigned long INTERVALO_COLOR = 300;
unsigned long ultimaLecturaColor = 0;
String colorActual = "---";

int leerCanal(bool s2, bool s3) {
  digitalWrite(PIN_S2, s2);
  digitalWrite(PIN_S3, s3);
  delay(10);
  return pulseIn(PIN_OUT, LOW, 30000);   // menor valor, más intensidad
}

String detectarColor() {
  int r = leerCanal(LOW, LOW);
  int g = leerCanal(HIGH, HIGH);
  int b = leerCanal(LOW, HIGH);

  Serial.print("R:"); Serial.print(r);
  Serial.print(" G:"); Serial.print(g);
  Serial.print(" B:"); Serial.println(b);

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

// ultrasónicos
long medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duracion = pulseIn(echo, HIGH, TIMEOUT_US);
  if (duracion == 0) return 400;        // sin eco, muy lejos
  return duracion * 0.034 / 2;          // convierte a cm
}

// motores
void ponerMotor(int in1, int in2, int en, bool adelante, int vel) {
  digitalWrite(in1, adelante ? HIGH : LOW);
  digitalWrite(in2, adelante ? LOW : HIGH);
  analogWrite(en, vel);
}

void izquierda(bool adelante, int vel) {
  ponerMotor(IN1_2, IN2_2, ENA2, adelante, vel);   // M2
  ponerMotor(IN3_4, IN4_4, ENB4, adelante, vel);   // M4
}

void derecha(bool adelante, int vel) {
  ponerMotor(IN3_1, IN4_1, ENB1, adelante, vel);   // M1
  ponerMotor(IN1_3, IN2_3, ENA3, adelante, vel);   // M3
}

void adelante() { izquierda(true, VELOCIDAD);  derecha(true, VELOCIDAD); }
void girarIzq() { izquierda(false, VELOCIDAD); derecha(true, VELOCIDAD); }
void girarDer() { izquierda(true, VELOCIDAD);  derecha(false, VELOCIDAD); }

void girar90Izq() { girarIzq(); delay(TIEMPO_GIRO_90); adelante(); }
void girar90Der() { girarDer(); delay(TIEMPO_GIRO_90); adelante(); }

void parar() {
  analogWrite(ENB1, 0); analogWrite(ENA3, 0);
  analogWrite(ENA2, 0); analogWrite(ENB4, 0);
  digitalWrite(IN3_1, LOW); digitalWrite(IN4_1, LOW);
  digitalWrite(IN1_3, LOW); digitalWrite(IN2_3, LOW);
  digitalWrite(IN1_2, LOW); digitalWrite(IN2_2, LOW);
  digitalWrite(IN3_4, LOW); digitalWrite(IN4_4, LOW);
}

// pantalla
void mostrar(const char* titulo, unsigned long restanteMs, const String &color) {
  char linea[24];
  snprintf(linea, sizeof(linea), "%s %lu.%lu s", titulo, restanteMs / 1000, (restanteMs % 1000) / 100);

  pantalla.clearBuffer();
  pantalla.setFont(u8g2_font_6x12_tr);
  pantalla.drawStr(0, 11, linea);
  pantalla.setFont(u8g2_font_logisoso16_tr);
  pantalla.drawStr(0, 31, color.c_str());
  pantalla.sendBuffer();
}

void setup() {
  Serial.begin(9600);

  // ultrasónicos
  pinMode(trig1, OUTPUT); pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT); pinMode(echo2, INPUT);
  pinMode(trig3, OUTPUT); pinMode(echo3, INPUT);

  // motores
  int pines[] = { ENB1, IN3_1, IN4_1, ENA3, IN1_3, IN2_3, ENA2, IN1_2, IN2_2, ENB4, IN3_4, IN4_4 };
  for (int i = 0; i < 12; i++) pinMode(pines[i], OUTPUT);
  parar();

  // sensor de color
  pinMode(PIN_S0, OUTPUT); pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT); pinMode(PIN_S3, OUTPUT);
  pinMode(PIN_OUT, INPUT);
  digitalWrite(PIN_S0, HIGH);
  digitalWrite(PIN_S1, LOW);

  // pantalla
  pantalla.begin();

  // servo
  servo.attach(servoPin);
  servo.write(0);

  estado = ESPERA;
  tEstado = millis();
}

void loop() {
  unsigned long transcurrido = millis() - tEstado;
  unsigned long restante = 0;

  switch (estado) {
    case ESPERA:
      if (transcurrido >= TIEMPO_ESPERA) {
        adelante();
        estado = AVANZA;
        tEstado = millis();
        restante = TIEMPO_AVANCE;
      } else {
        restante = TIEMPO_ESPERA - transcurrido;
      }
      break;

    case AVANZA:
      if (transcurrido >= TIEMPO_AVANCE) {
        parar();
        estado = PARADO;
        restante = 0;
      } else {
        restante = TIEMPO_AVANCE - transcurrido;
      }
      break;

    case PARADO:
      restante = 0;
      break;
  }

  // cambia la pantalla cada 100 ms
  if (millis() - tPantalla >= 100) {
    tPantalla = millis();
    if (estado == ESPERA)      mostrar("ESPERA", restante, "HOLIS");
    else if (estado == AVANZA) mostrar("AVANZA", restante, colorActual);
    else                       mostrar("PARADO", 0, colorActual);
  }
}