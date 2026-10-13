#include <U8g2lib.h> // pantalla
#include <Servo.h>

// servo
const int servoPin = 6;

// ultra 1 (delante)
const int echo1 = 31;
const int trig1 = 30;

// ultra 2 (izquierda)
const int echo2 = 33;
const int trig2 = 32;

// ultra 3 (derecha)
const int echo3 = 35;
const int trig3 = 34;

// ultra 4 (pelota)
const int echo4 = 43;
const int trig4 = 44; // por definir

// pantalla
const int PIN_SDA = 41;
const int PIN_SCL = 42;

// sensor de color
const int PIN_S0 = 36;
const int PIN_S1 = 37;
const int PIN_S2 = 39;
const int PIN_S3 = 38;
const int PIN_OUT = 40;

// motor M1 (Izquierdo 1)
const int ENB1 = 3;
const int IN3_1 = 24;
const int IN4_1 = 25;

// motor M2 (Derecho 1)
const int ENA2 = 4;
const int IN1_2 = 26;
const int IN2_2 = 27;

// motor M3 (Izquierdo 2)
const int ENA3 = 2;
const int IN1_3 = 22;
const int IN2_3 = 23;

// motor M4 (Derecho 2)
const int ENB4 = 5;
const int IN3_4 = 28;
const int IN4_4 = 29;

const int VELOCIDAD = 50;            // velocidad de las ruedas
const int DIST_OBSTACULO_4 = 15;         
const int TIEMPO_GIRO_90 = 600;      // tiempo de giro 
const unsigned long TIMEOUT_US = 30000;

// inicializamos el servo 
Servo servo;
bool servoMovido = false;    

// pantalla con sensor 
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C pantalla(U8G2_R0, PIN_SCL, PIN_SDA, U8X8_PIN_NONE); 

const int LIMITE_NADA = 250;                
const unsigned long INTERVALO_COLOR = 300;   // cada cuántos ms lee el color
unsigned long ultimaLecturaColor = 0;
String colorActual = " ";

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

  // nada cerca
  if (r == 0 || g == 0 || b == 0) return "---";
  if (r > LIMITE_NADA && g > LIMITE_NADA && b > LIMITE_NADA) return "---";

  float gr = (float)g / r;   
  float br = (float)b / r;   

  // cian
  if (gr < 0.8 && br < 0.8 && abs(g - b) < 0.25 * max(g, b)) return "CIAN";

  // amarillo
  if (gr < 1.3 && br > 1.5) return "AMARILLO";

  // naranja
  if (gr >= 1.3 && b > g * 1.1) return "NARANJA";

  // rosa
  if (gr >= 1.3 && g > b * 1.1) return "ROSA";

  return "---";
}

void mostrarColor(const String &texto) {
  pantalla.clearBuffer();
  pantalla.setFont(u8g2_font_ncenB14_tr);
  pantalla.drawStr(0, 24, texto.c_str());
  pantalla.sendBuffer();
}

// ultrasonicos
long medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duracion = pulseIn(echo, HIGH, TIMEOUT_US);
  if (duracion == 0) return 400;        // sin eco, muy lejos
  return duracion * 0.034 / 2;          // se convierte a cm
}

// motores - Lado izquierdo unificado (M1 y M3)
void izquierdaAdelante(int vel) {
  // M1
  analogWrite(ENB1, vel); digitalWrite(IN3_1, HIGH); digitalWrite(IN4_1, LOW);
  // M3
  analogWrite(ENA3, vel); digitalWrite(IN1_3, HIGH); digitalWrite(IN2_3, LOW);
}
void izquierdaAtras(int vel) {
  // M1
  analogWrite(ENB1, vel); digitalWrite(IN3_1, LOW); digitalWrite(IN4_1, HIGH);
  // M3
  analogWrite(ENA3, vel); digitalWrite(IN1_3, LOW); digitalWrite(IN2_3, HIGH);
}

// motores - Lado derecho unificado (M2 y M4)
void derechaAdelante(int vel) {
  // M2
  analogWrite(ENA2, vel); digitalWrite(IN1_2, HIGH); digitalWrite(IN2_2, LOW);
  // M4
  analogWrite(ENB4, vel); digitalWrite(IN3_4, HIGH); digitalWrite(IN4_4, LOW);
}
void derechaAtras(int vel) {
  // M2
  analogWrite(ENA2, vel); digitalWrite(IN1_2, LOW); digitalWrite(IN2_2, HIGH);
  // M4
  analogWrite(ENB4, vel); digitalWrite(IN3_4, LOW); digitalWrite(IN4_4, HIGH);
}

void adelante()  { izquierdaAdelante(VELOCIDAD); derechaAdelante(VELOCIDAD); }
void girarIzq()  { izquierdaAtras(VELOCIDAD);    derechaAdelante(VELOCIDAD); }
void girarDer()  { izquierdaAdelante(VELOCIDAD); derechaAtras(VELOCIDAD); }

void girar90Izq() {
  girarIzq();
  delay(TIEMPO_GIRO_90);
  adelante();
}

void girar90Der() {
  girarDer();
  delay(TIEMPO_GIRO_90);
  adelante();
}

void setup() {
  Serial.begin(9600);

  // ultrasónicos
  pinMode(trig1, OUTPUT); pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT); pinMode(echo2, INPUT);
  pinMode(trig3, OUTPUT); pinMode(echo3, INPUT);
  pinMode(trig4, OUTPUT); pinMode(echo4, INPUT);

  // todos los motores
  pinMode(ENB1, OUTPUT); pinMode(IN3_1, OUTPUT); pinMode(IN4_1, OUTPUT);
  pinMode(ENA2, OUTPUT); pinMode(IN1_2, OUTPUT); pinMode(IN2_2, OUTPUT);
  pinMode(ENA3, OUTPUT); pinMode(IN1_3, OUTPUT); pinMode(IN2_3, OUTPUT);
  pinMode(ENB4, OUTPUT); pinMode(IN3_4, OUTPUT); pinMode(IN4_4, OUTPUT);

  // sensor de color
  pinMode(PIN_S0, OUTPUT); pinMode(PIN_S1, OUTPUT);
  pinMode(PIN_S2, OUTPUT); pinMode(PIN_S3, OUTPUT);
  pinMode(PIN_OUT, INPUT);
  digitalWrite(PIN_S0, HIGH);   
  digitalWrite(PIN_S1, LOW);

  // pantalla
  pantalla.begin();
  mostrarColor("HOLIS");

  // servo
  servo.attach(servoPin);
  servo.write(0);

  delay(3000);   // espera 3 segundos
  adelante();    // arrancan los motores hacia adelante
}

void loop() {
  long d1 = medirDistancia(trig1, echo1);  // frente
  long d2 = medirDistancia(trig2, echo2);  // izquierda
  long d3 = medirDistancia(trig3, echo3);  // derecha
  long d4 = medirDistancia(trig4, echo4);  // sensor de pelota

  // decidir hacia dónde ir
  if (d1 >= d2 && d1 >= d3) {
    adelante();
  } else if (d2 > d1) {
    girar90Izq();
  } else {
    girar90Der();
  }

  // mecanismo para capturar la pelota
  if (d4 < DIST_OBSTACULO_4 && !servoMovido) {
    servo.write(90);
    servoMovido = true;
  }

  // mostrar el color
  if (millis() - ultimaLecturaColor >= INTERVALO_COLOR) {
    ultimaLecturaColor = millis();
    String c = detectarColor();
    if (c != colorActual) {
      colorActual = c;
      mostrarColor(colorActual);
    }
  }

  delay(50);
}