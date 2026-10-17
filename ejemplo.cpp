/* SI FUNCIONA
  Carrito - Arduino Mega
  1) Al encender espera 3 segundos (cuenta regresiva en la OLED)
  2) Avanza 10 segundos (cuenta regresiva en la OLED)
  3) Se detiene y se queda parado (para repetir, presiona RESET)

  Pines tomados del esquematico (Esquematico_2.pdf):
    OLED 0.91" I2C por software: SDA = 41, SCL = 42
    L298N U3 (lado izquierdo, M2 y M4): IN1=26 IN2=27 ENA=4  IN3=28 IN4=29 ENB=5
    L298N U2 (lado derecho,   M1 y M3): IN1=22 IN2=23 ENA=2  IN3=24 IN4=25 ENB=3

  Libreria necesaria: U8g2
*/

#include <U8g2lib.h>

// ---------- OLED 128x32 (0.91") ----------
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C u8g2(U8G2_R0, /*clock=*/ 42, /*data=*/ 41, /*reset=*/ U8X8_PIN_NONE);

// ---------- Motores ----------
// Lado izquierdo (U3)
const int L_IN1 = 26, L_IN2 = 27, L_ENA = 4;
const int L_IN3 = 28, L_IN4 = 29, L_ENB = 5;
// Lado derecho (U2)
const int R_IN1 = 22, R_IN2 = 23, R_ENA = 2;
const int R_IN3 = 24, R_IN4 = 25, R_ENB = 3;

// Velocidad de los motores (0 a 255). Bajala si va muy rapido.
const int VELOCIDAD = 150;

// ---------- Tiempos (milisegundos) ----------
const unsigned long TIEMPO_ESPERA = 3000;
const unsigned long TIEMPO_AVANCE = 10000;

enum Estado { ESPERA, AVANZA, PARADO };
Estado estado = ESPERA;
unsigned long tEstado = 0;      // momento en que empezo el estado actual
unsigned long tPantalla = 0;    // ultima actualizacion de la OLED

// ---------- Control de motores ----------
void ponerMotor(int in1, int in2, int en, bool adelante, int vel) {
  digitalWrite(in1, adelante ? HIGH : LOW);
  digitalWrite(in2, adelante ? LOW : HIGH);
  analogWrite(en, vel);
}

void avanzar() {
  // Si algun lado va hacia atras, cambia true por false en ese lado
  ponerMotor(L_IN1, L_IN2, L_ENA, true, VELOCIDAD);  // M2
  ponerMotor(L_IN3, L_IN4, L_ENB, true, VELOCIDAD);  // M4
  ponerMotor(R_IN1, R_IN2, R_ENA, true, VELOCIDAD);  // M3
  ponerMotor(R_IN3, R_IN4, R_ENB, true, VELOCIDAD);  // M1
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

// ---------- Pantalla ----------
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

// ---------- Programa ----------
void setup() {
  int pines[] = { L_IN1, L_IN2, L_ENA, L_IN3, L_IN4, L_ENB,
                  R_IN1, R_IN2, R_ENA, R_IN3, R_IN4, R_ENB };
  for (int i = 0; i < 12; i++) pinMode(pines[i], OUTPUT);
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
        transcurrido = 0;
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

  // Refresca la OLED cada 100 ms
  if (millis() - tPantalla >= 100) {
    tPantalla = millis();
    if (estado == ESPERA)      mostrar("ESPERANDO...", restante);
    else if (estado == AVANZA) mostrar("AVANZANDO", restante);
    else                       mostrar("PARADO", 0);
  }
}
