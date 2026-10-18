#include <U8g2lib.h>

U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, 42, 41, U8X8_PIN_NONE);

const int L_IN1 = 26, L_IN2 = 27, L_ENA = 4;   // Izquierdo 1 
const int L_IN3 = 24, L_IN4 = 25, L_ENB = 3;   // Izquierdo 2
const int R_IN1 = 28, R_IN2 = 29, R_ENA = 5;   // Derecho 1   
const int R_IN3 = 22, R_IN4 = 23, R_ENB = 2;   // Derecho 2   

const int VELOCIDAD = 90;        // avance
const int VELOCIDAD_GIRO = 100;   // giro

// ultras
const int TRIG_F = 50, ECHO_F = 51;   // frontal
const int TRIG_I = 32, ECHO_I = 33;   // izquierda
const int TRIG_D = 30, ECHO_D = 31;   // derecha

// sensor de color 
const int S0 = 36, S1 = 37, S2 = 39, S3 = 38, SENSOR_OUT = 40;

struct ColorRef { const char* nombre; int r, g, b; };
const ColorRef COLORES[] = {
  { "ROJO",     9, 16, 14 },
  { "AMARILLO", 8, 11, 13 },
  { "AZUL",    11, 13, 11 },
  { "VERDE",    8, 10, 12 },
  { "NARANJA", 7, 14, 11 }
};
const int NUM_COLORES = sizeof(COLORES) / sizeof(COLORES[0]);

const int MUESTRAS = 5;                 // lecturas para promediarse
const float UMBRAL_DIST2 = 30.0;        
const char* colorActual = "---";

const int DIST_OBSTACULO = 22;                 // cm
const unsigned long TIEMPO_GIRO_DER = 940;         // ms // giro a la derecha
const unsigned long TIEMPO_GIRO_IZQ = 480;         // ms // giro a la IZQUIERDA
const unsigned long TIEMPO_PAUSA = 450;        // ms

// tiempo
const unsigned long TIEMPO_ESPERA = 5000;
const unsigned long TIEMPO_AVANCE = 300;
enum Estado { ESPERA, AVANZA, REVISA, GIRA };
Estado estado = ESPERA;
int dirGiro = 1;                 // 1 = derecha, -1 = izquierda
unsigned long tEstado = 0;
unsigned long tPantalla = 0;
unsigned long tGiro = 0;

// motores
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
  Serial.println("Avanzando hacia adelante...");
}

void girarDerecha() {
  ponerMotor(L_IN1, L_IN2, L_ENA, true,  VELOCIDAD_GIRO);
  ponerMotor(L_IN3, L_IN4, L_ENB, true,  VELOCIDAD_GIRO);
  ponerMotor(R_IN1, R_IN2, R_ENA, false, VELOCIDAD_GIRO * 1.2);
  ponerMotor(R_IN3, R_IN4, R_ENB, false, VELOCIDAD_GIRO * 1.2);
  Serial.println("Girando hacia la derecha....");
}

void girarIzquierda() {
  ponerMotor(L_IN1, L_IN2, L_ENA, false, VELOCIDAD_GIRO);
  ponerMotor(L_IN3, L_IN4, L_ENB, false, VELOCIDAD_GIRO);
  ponerMotor(R_IN1, R_IN2, R_ENA, true,  VELOCIDAD_GIRO);
  ponerMotor(R_IN3, R_IN4, R_ENB, true,  VELOCIDAD_GIRO);
  Serial.println("Girando hacia la izquierda....");
}

void parar() {
  analogWrite(L_ENA, 0); analogWrite(L_ENB, 0);
  analogWrite(R_ENA, 0); analogWrite(R_ENB, 0);
  digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, LOW); digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
  Serial.println("Parando.....");
}

void iniciarAvance() {
  avanzar();
  estado = AVANZA;
  tEstado = millis();
  Serial.println("Iniciando Estado Avance");
}

// ultrasónicos
int medirCm(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  unsigned long dur = pulseIn(echo, HIGH, 25000UL);
  if (dur == 0) return 400;
  return (int)(dur * 0.034 / 2);
}

// sensor de color
// Promedia varias lecturas de un canal (filtro segun s2/s3)
float leerCanal(bool s2, bool s3) {
  digitalWrite(S2, s2);
  digitalWrite(S3, s3);
  delay(10);                                   // estabilizar el filtro
  float suma = 0;
  int validas = 0;
  for (int i = 0; i < MUESTRAS; i++) {
    unsigned long p = pulseIn(SENSOR_OUT, LOW, 30000UL);
    if (p > 0) { suma += p; validas++; }
  }
  return validas ? suma / validas : 0;
}

// Lee R, G, B y regresa el nombre del color mas cercano a tu calibracion
const char* detectarColor() {
  float r = leerCanal(LOW,  LOW);    // rojo
  float g = leerCanal(HIGH, HIGH);   // verde
  float b = leerCanal(LOW,  HIGH);   // azul

  int mejor = -1;
  float mejorDist = 1e9;
  for (int i = 0; i < NUM_COLORES; i++) {
    float dr = r - COLORES[i].r;
    float dg = g - COLORES[i].g;
    float db = b - COLORES[i].b;
    float d2 = dr * dr + dg * dg + db * db;
    if (d2 < mejorDist) { mejorDist = d2; mejor = i; }
  }

  // Para ajustar, abre el serial monitor
  Serial.print("R:"); Serial.print(r, 1);
  Serial.print(" G:"); Serial.print(g, 1);
  Serial.print(" B:"); Serial.print(b, 1);
  Serial.print(" -> ");
  Serial.print(mejor >= 0 ? COLORES[mejor].nombre : "?");
  Serial.print(" (d2="); Serial.print(mejorDist, 1); Serial.println(")");

  if (mejor < 0 || mejorDist > UMBRAL_DIST2) return "---";
  return COLORES[mejor].nombre;
}

// pantalla
void mostrar(const char* titulo, unsigned long restanteMs) {
  char num[12];
  snprintf(num, sizeof(num), "%lu.%lu s", restanteMs / 1000, (restanteMs % 1000) / 100);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(0, 11, titulo);
  u8g2.drawStr(70, 31, colorActual);          // color detectado
  u8g2.setFont(u8g2_font_logisoso16_tr);
  u8g2.drawStr(0, 31, num);
  u8g2.sendBuffer();
}

void setup() {
  int pines[] = { L_IN1, L_IN2, L_ENA, L_IN3, L_IN4, L_ENB, R_IN1, R_IN2, R_ENA, R_IN3, R_IN4, R_ENB };
  for (int i = 0; i < 12; i++) pinMode(pines[i], OUTPUT);

  pinMode(TRIG_F, OUTPUT); pinMode(ECHO_F, INPUT);
  pinMode(TRIG_I, OUTPUT); pinMode(ECHO_I, INPUT);
  pinMode(TRIG_D, OUTPUT); pinMode(ECHO_D, INPUT);

  pinMode(S0, OUTPUT); pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT); pinMode(S3, OUTPUT);
  pinMode(SENSOR_OUT, INPUT);
  digitalWrite(S0, HIGH);     // escala que fije
  digitalWrite(S1, LOW);

  Serial.begin(9600);
  parar();
  u8g2.begin();

  estado = ESPERA;
  tEstado = millis();
}

void loop() {
  unsigned long transcurrido = millis() - tEstado;
  unsigned long restante = 0;

  switch (estado) {
    case ESPERA:
      if (transcurrido >= TIEMPO_ESPERA) {
        iniciarAvance();
        restante = TIEMPO_AVANCE;
      } else {
        restante = TIEMPO_ESPERA - transcurrido;
      }
      break;

    case AVANZA:
      if (transcurrido >= TIEMPO_AVANCE) {
        parar();
        estado = REVISA;
        tEstado = millis();
      } else {
        restante = TIEMPO_AVANCE - transcurrido;
      }
      break;

    case REVISA:
      if (transcurrido >= TIEMPO_PAUSA) {
        colorActual = detectarColor();      // el carro esta parado

        int dF = medirCm(TRIG_F, ECHO_F);
        Serial.print(" La distancia sensor frontal es: ");
        Serial.println(dF);
        int dI = medirCm(TRIG_I, ECHO_I);
        Serial.print(" La distancia sensor izauierdo es: ");
        Serial.println(dI);
        int dD = medirCm(TRIG_D, ECHO_D);
        Serial.print(" La distancia sensor derecho es: ");
        Serial.println(dD);


        if (dF >= DIST_OBSTACULO) {
          iniciarAvance();
        } else {
          dirGiro = (dD > dI) ? 1 : -1;
          if (dirGiro == 1) girarDerecha(); else girarIzquierda();
          estado = GIRA;
          tGiro = millis();
        }
      }
      break;

    case GIRA:
      if (millis() - tGiro >= TIEMPO_GIRO) {
        parar();
        iniciarAvance();

      }
      break;
  }

  // refresh de la pantalla cada 100 ms
  if (millis() - tPantalla >= 100) {
    tPantalla = millis();
    if (estado == ESPERA) {
      colorActual = detectarColor();        // prueba de colores
      mostrar("ESPERANDO...", restante);
    }
    else if (estado == AVANZA) mostrar("AVANZANDO", restante);
    else if (estado == REVISA) mostrar("REVISANDO", 0);
    else mostrar(dirGiro == 1 ? "GIRO DERECHA" : "GIRO IZQUIERDA", 0);
  }
}