//
//

#include <U8g2lib.h>

U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /*clock=*/ 42, /*data=*/ 41, /*reset=*/ U8X8_PIN_NONE);

// motores
const int L_IN1 = 26, L_IN2 = 27, L_ENA = 4;
const int L_IN3 = 28, L_IN4 = 29, L_ENB = 5;
const int R_IN1 = 22, R_IN2 = 23, R_ENA = 2;
const int R_IN3 = 24, R_IN4 = 25, R_ENB = 3;

const int VELOCIDAD = 150;        // avance
const int VELOCIDAD_GIRO = 170;   // giro

// ultras
const int TRIG_F = 30, ECHO_F = 31;   // frente
const int TRIG_I = 32, ECHO_I = 33;   // izquierda
const int TRIG_D = 34, ECHO_D = 35;   // derecha

const unsigned long INTERVALO_SENSORES = 100;  // cada cuanto mide (ms)
const int DIST_OBSTACULO = 30;                 // cm, solo evalua giro si el frente esta a menos de esto
const unsigned long TIEMPO_GIRO = 500;         // 

// tiempo
const unsigned long TIEMPO_ESPERA = 3000;
const unsigned long TIEMPO_AVANCE = 10000;

enum Estado { ESPERA, AVANZA, GIRA, PARADO };
Estado estado = ESPERA;
unsigned long tEstado = 0;
unsigned long tPantalla = 0;
unsigned long tSensores = 0;
unsigned long tGiro = 0;

// motores
void ponerMotor(int in1, int in2, int en, bool adelante, int vel) {
  digitalWrite(in1, adelante ? HIGH : LOW);
  digitalWrite(in2, adelante ? LOW : HIGH);
  analogWrite(en, vel);
}

void avanzar() {
  ponerMotor(L_IN1, L_IN2, L_ENA, true, VELOCIDAD);  // M2
  ponerMotor(L_IN3, L_IN4, L_ENB, true, VELOCIDAD);  // M4
  ponerMotor(R_IN1, R_IN2, R_ENA, true, VELOCIDAD);  // M3
  ponerMotor(R_IN3, R_IN4, R_ENB, true, VELOCIDAD);  // M1
}

// Giro a la derecha: izquierda adelante, derecha atras
void girarDerecha() {
  ponerMotor(L_IN1, L_IN2, L_ENA, true,  VELOCIDAD_GIRO);
  ponerMotor(L_IN3, L_IN4, L_ENB, true,  VELOCIDAD_GIRO);
  ponerMotor(R_IN1, R_IN2, R_ENA, false, VELOCIDAD_GIRO);
  ponerMotor(R_IN3, R_IN4, R_ENB, false, VELOCIDAD_GIRO);
}

void parar() {
  analogWrite(L_ENA, 0);
  analogWrite(L_ENB, 0);
  analogWrite(R_ENA, 0);
  analogWrite(R_ENB, 0);
  digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, LOW); digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
}

// ultra
// Devuelve distancia en cm (si no hay eco, regresa 400 = muy lejos)
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

// pantalla
void mostrar(const char* titulo, unsigned long restanteMs) {
  char num[12];
  snprintf(num, sizeof(num), "%lu.%lu s", restanteMs / 1000, (restanteMs % 1000) / 100);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(0, 11, titulo);
  u8g2.setFont(u8g2_font_logisoso16_tr);
  u8g2.drawStr(0, 31, num);
  u8g2.sendBuffer();
}

void setup() {
  int pines[] = { L_IN1, L_IN2, L_ENA, L_IN3, L_IN4, L_ENB,
                  R_IN1, R_IN2, R_ENA, R_IN3, R_IN4, R_ENB };
  for (int i = 0; i < 12; i++) pinMode(pines[i], OUTPUT);

  pinMode(TRIG_F, OUTPUT); pinMode(ECHO_F, INPUT);
  pinMode(TRIG_I, OUTPUT); pinMode(ECHO_I, INPUT);
  pinMode(TRIG_D, OUTPUT); pinMode(ECHO_D, INPUT);

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
        avanzar();
        estado = AVANZA;
        tEstado = millis();
        tSensores = millis();
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

        // Revisa sensores 
        if (millis() - tSensores >= INTERVALO_SENSORES) {
          tSensores = millis();
          int dF = medirCm(TRIG_F, ECHO_F);
          int dI = medirCm(TRIG_I, ECHO_I);
          int dD = medirCm(TRIG_D, ECHO_D);

          // Derecha con mas distancia que frente e izquierda -> gira a la derecha
          if (dF < DIST_OBSTACULO && dD > dI && dD > dF) {
            girarDerecha();
            estado = GIRA;
            tGiro = millis();
          }
        }
      }
      break;

    case GIRA:
      restante = TIEMPO_AVANCE - transcurrido;
      if (millis() - tGiro >= TIEMPO_GIRO) {
        tEstado += TIEMPO_GIRO;     // el giro no cuenta como tiempo de avance
        avanzar();
        estado = AVANZA;
        tSensores = millis();
      }
      break;

    case PARADO:
      restante = 0;
      break;
  }

  // Refresca la pantalla cada 100 ms
  if (millis() - tPantalla >= 100) {
    tPantalla = millis();
    if (estado == ESPERA)      mostrar("ESPERANDO...", restante);
    else if (estado == AVANZA) mostrar("AVANZANDO", restante);
    else if (estado == GIRA)   mostrar("GIRO DERECHA", restante);
    else                       mostrar("PARADO", 0);
  }
}