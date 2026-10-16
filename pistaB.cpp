#include <Servo.h>

Servo servo;
const int servoPin = 6;

const int echo1 = 31, trig1 = 30; // Frontal
const int echo2 = 33, trig2 = 32; // Izquierda
const int echo3 = 35, trig3 = 34; // Derecha
const int echo4 = 43, trig4 = 44; // pelota

// Lado izquierdo
const int L_IN1 = 26, L_IN2 = 27, L_ENA = 4;
const int L_IN3 = 28, L_IN4 = 29, L_ENB = 5;

// Lado derecho 
const int R_IN1 = 22, R_IN2 = 23, R_ENA = 2;
const int R_IN3 = 24, R_IN4 = 25, R_ENB = 3;

const int VELOCIDAD = 100;
const int VEL_LENTA = 80;          // para entrar y salir de la celda

const int DIST_OBSTACULO = 15;
const int DIST_PELOTA = 6;         // distancia para cerrar el servo
const int LECTURAS_PELOTA = 3;     // lecturas seguidas para confirmar la pelota

const int LADO_CELDA = 12;         // distancia lateral que indica que está dentro de la celda
const int DIST_ENTRAR = 30;

const int ANGULO_ABIERTO = 0;
const int ANGULO_CERRADO = 90;

const unsigned long TIMEOUT_US = 12000;
const unsigned long ATORADO_MS = 3000;        // tiempo sin cambios para considerar atorado
const int TOL_ATORADO = 2;                    // cm
const unsigned long ENTRAR_MAX_MS = 4000;     // máximo para atrapar la pelota
const unsigned long SALIR_EXTRA_MS = 400;     // retroceso extra al salir
const unsigned long SALIR_MAX_MS = 3500;      // máximo tiempo retrocediendo

const bool DEBUG = true;                      // imprime lecturas para calibrar

// Intento de máquina de estados
enum Estado { BUSCAR_PELOTA, ENTRAR_CELDA, CAPTURAR_PELOTA, SALIR_DE_LA_CELDA, ATORADO };
Estado estado = BUSCAR_PELOTA;
unsigned long tEstado = 0;
bool tienePelota = false;
int contadorPelota = 0;

// para detectar atoramiento
long sumaAnterior = 0;
unsigned long tUltimoCambio = 0;

// para salir de la celda
bool despejado = false;
unsigned long tDespejado = 0;

unsigned long tDebug = 0;

void ponerMotor(int in1, int in2, int en, bool adelante, int vel) {
  if (adelante) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }
  analogWrite(en, vel);
}

void avanzar(int vel) {
  ponerMotor(L_IN1, L_IN2, L_ENA, true, vel);
  ponerMotor(L_IN3, L_IN4, L_ENB, true, vel);
  ponerMotor(R_IN1, R_IN2, R_ENA, true, vel);
  ponerMotor(R_IN3, R_IN4, R_ENB, true, vel);
}

void retroceder(int vel) {
  ponerMotor(L_IN1, L_IN2, L_ENA, false, vel);
  ponerMotor(L_IN3, L_IN4, L_ENB, false, vel);
  ponerMotor(R_IN1, R_IN2, R_ENA, false, vel);
  ponerMotor(R_IN3, R_IN4, R_ENB, false, vel);
}

void girarIzquierda(int vel) {
  ponerMotor(L_IN1, L_IN2, L_ENA, false, vel);
  ponerMotor(L_IN3, L_IN4, L_ENB, false, vel);
  ponerMotor(R_IN1, R_IN2, R_ENA, true, vel);
  ponerMotor(R_IN3, R_IN4, R_ENB, true, vel);
}

void girarDerecha(int vel) {
  ponerMotor(L_IN1, L_IN2, L_ENA, true, vel);
  ponerMotor(L_IN3, L_IN4, L_ENB, true, vel);
  ponerMotor(R_IN1, R_IN2, R_ENA, false, vel);
  ponerMotor(R_IN3, R_IN4, R_ENB, false, vel);
}

void parar() {
  analogWrite(L_ENA, 0); analogWrite(L_ENB, 0);
  analogWrite(R_ENA, 0); analogWrite(R_ENB, 0);
  digitalWrite(L_IN1, LOW); digitalWrite(L_IN2, LOW);
  digitalWrite(L_IN3, LOW); digitalWrite(L_IN4, LOW);
  digitalWrite(R_IN1, LOW); digitalWrite(R_IN2, LOW);
  digitalWrite(R_IN3, LOW); digitalWrite(R_IN4, LOW);
}

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

// true cuando el sensor 4 ve la pelota varias lecturas seguidas
bool pelotaDentro(long d4) {
  if (d4 < DIST_PELOTA) contadorPelota++;
  else contadorPelota = 0;
  return contadorPelota >= LECTURAS_PELOTA;
}

void cambiarEstado(Estado nuevo) {
  estado = nuevo;
  tEstado = millis();
  despejado = false;
  contadorPelota = 0;
  tUltimoCambio = millis();
}

void buscar(long d1, long d2, long d3) {
  // celda
  if (!tienePelota && d1 < DIST_ENTRAR && d2 < LADO_CELDA && d3 < LADO_CELDA) {
    cambiarEstado(ENTRAR_CELDA);
    return;
  }

  // atorado
  long suma = d1 + d2 + d3;
  if (abs(suma - sumaAnterior) > TOL_ATORADO) {
    sumaAnterior = suma;
    tUltimoCambio = millis();
  } else if (millis() - tUltimoCambio > ATORADO_MS) {
    cambiarEstado(ATORADO);
    return;
  }

  if (d1 < DIST_OBSTACULO) {
    if (d3 > d2) girarDerecha(VELOCIDAD);
    else girarIzquierda(VELOCIDAD);
  } else {
    avanzar(VELOCIDAD);
  }
}

void setup() {
  Serial.begin(9600);
  int pinesMotores[] = { L_IN1, L_IN2, L_ENA, L_IN3, L_IN4, L_ENB, R_IN1, R_IN2, R_ENA, R_IN3, R_IN4, R_ENB };
  for (int i = 0; i < 12; i++) pinMode(pinesMotores[i], OUTPUT);
  parar();
  pinMode(trig1, OUTPUT); pinMode(echo1, INPUT);
  pinMode(trig2, OUTPUT); pinMode(echo2, INPUT);
  pinMode(trig3, OUTPUT); pinMode(echo3, INPUT);
  pinMode(trig4, OUTPUT); pinMode(echo4, INPUT);
  servo.attach(servoPin);
  servo.write(ANGULO_ABIERTO);
  delay(2000);
  cambiarEstado(BUSCAR_PELOTA);
}

void loop() {
  long d1 = medirDistancia(trig1, echo1); // Delante
  long d2 = medirDistancia(trig2, echo2); // Izquierda
  long d3 = medirDistancia(trig3, echo3); // Derecha
  long d4 = medirDistancia(trig4, echo4); // Pelota

  bool pelota = pelotaDentro(d4);

  switch (estado) {
    case BUSCAR_PELOTA:
      buscar(d1, d2, d3);
      break;

    case ENTRAR_CELDA:
      if (pelota) {
        cambiarEstado(CAPTURAR_PELOTA);
      } else if (millis() - tEstado > ENTRAR_MAX_MS) {
        cambiarEstado(ATORADO);               // reintentar
      } else if (d2 < LADO_CELDA && d3 < LADO_CELDA) {
        avanzar(VEL_LENTA);                   // sigue dentro de la celda
      } else {
        cambiarEstado(BUSCAR_PELOTA);         // buscar la celda
      }
      break;

    case CAPTURAR_PELOTA:
      parar();
      servo.write(ANGULO_CERRADO);
      delay(500);                             // tiempo del servo
      tienePelota = true;
      cambiarEstado(SALIR_DE_LA_CELDA);
      break;

    case SALIR_DE_LA_CELDA:
      retroceder(VEL_LENTA);
      if (d2 > LADO_CELDA && d3 > LADO_CELDA) {
        if (!despejado) {
          despejado = true;
          tDespejado = millis();
        }
        if (millis() - tDespejado > SALIR_EXTRA_MS) {
          parar();
          cambiarEstado(BUSCAR_PELOTA);
        }
      } else {
        despejado = false;
      }
      if (millis() - tEstado > SALIR_MAX_MS) {
        parar();
        cambiarEstado(BUSCAR_PELOTA);
      }
      break;

    case ATORADO: {
      unsigned long t = millis() - tEstado;
      if (t < 600) retroceder(VELOCIDAD);
      else if (t < 1100) girarDerecha(VELOCIDAD);
      else cambiarEstado(BUSCAR_PELOTA);
      break;
    }
  }

  if (DEBUG && millis() - tDebug > 250) {
    tDebug = millis();
    Serial.print("estado="); Serial.print(estado);
    Serial.print(" d1="); Serial.print(d1);
    Serial.print(" d2="); Serial.print(d2);
    Serial.print(" d3="); Serial.print(d3);
    Serial.print(" d4="); Serial.println(d4);
  }
}