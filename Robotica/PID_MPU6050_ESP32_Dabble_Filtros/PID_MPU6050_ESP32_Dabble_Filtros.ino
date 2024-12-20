/*
 * Projeto: Controle de Carro Robô 2WD com ESP32 e MPU6050
 * 
 * Descrição:
 * Este código controla um carro robô de duas rodas (2WD) utilizando um microcontrolador ESP32
 * e um sensor MPU6050. O robô pode ser comandado através do aplicativo Dabble via Bluetooth.
 * O controle de trajetória é realizado utilizando um algoritmo PID para corrigir a direção.
 * 
 * Versão: 1.3
 * Autor: Prof. Dr. Robson Marinho
 * Data: 18/10/2024
 */

#include <Wire.h>
#include <MPU6050.h>
#include <DabbleESP32.h>

// Configurações do MPU6050
MPU6050 mpu;
int16_t ax, ay, az; // Aceleração
int16_t gx, gy, gz; // Giroscópio
float currentYaw = 0;
float targetYaw = 0;

// Filtro passa-baixa para suavizar a leitura do giroscópio
float alpha = 0.8;  // Constante do filtro passa-baixa
float filteredYaw = 0;

// Defina os pinos do TB6612 para os motores
const int motorRightPWM = 13;  
const int rightMotorPin1 = 25;
const int rightMotorPin2 = 33;

const int motorLeftPWM = 32;
const int leftMotorPin1 = 26;
const int leftMotorPin2 = 27;

// Velocidades base ajustadas para as rodas
int baseMotorSpeedLeft = 50;  
int baseMotorSpeedRight = 100;  //no robô 4 a roda direita está mais lenta
int rotationSpeed = 20;        

// Constantes PID ajustadas para a roda esquerda
float KpLeft = 3.0, KiLeft = 0.1, KdLeft = 1.5;
float prevErrorYawLeft = 0;
float integralYawLeft = 0;

// Constantes PID ajustadas para a roda direita
float KpRight = 12.0, KiRight = 0.1, KdRight = 3.0; //no robô 4 a roda direita está mais lenta
float prevErrorYawRight = 0;
float integralYawRight = 0;

// Limites para o windup do PID (para evitar saturação do termo integral)
const float windupLimit = 50.0;  // Ajuste conforme necessário

unsigned long lastTime = 0;
float dt = 0.1;

void setup() {
  Serial.begin(115200);

 // Dabble.begin("ESP32-Robo2");  
 Dabble.begin("ESP32-Robo4");  
  Wire.begin();
  mpu.initialize();
  
  if (!mpu.testConnection()) {
    Serial.println("Falha ao conectar ao MPU6050!");
    while (1);
  } else {
    Serial.println("MPU6050 conectado!");
  }

  pinMode(rightMotorPin1, OUTPUT);
  pinMode(rightMotorPin2, OUTPUT);
  pinMode(motorRightPWM, OUTPUT);
  
  pinMode(leftMotorPin1, OUTPUT);
  pinMode(leftMotorPin2, OUTPUT);
  pinMode(motorLeftPWM, OUTPUT);
}

void loop() {
  Dabble.processInput();  // Processa a entrada do app Dabble
  
  // Leitura dos dados do MPU6050
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Atualiza e filtra a leitura de yaw (giroscópio)
  float rawYaw = (gz / 131.0) * dt;
  filteredYaw = alpha * filteredYaw + (1 - alpha) * rawYaw;
  currentYaw += filteredYaw;

  // Verifica os comandos do controle e aplica a ação correspondente
  if (GamePad.isUpPressed()) {
    moveForward();
    targetYaw = currentYaw;  
    correctTrajectoryPID();  
  } else if (GamePad.isDownPressed()) {
    moveBackward();
    targetYaw = currentYaw;
    correctTrajectoryPID();  
  } else if (GamePad.isLeftPressed()) {
    rotateLeft();
    targetYaw -= 5;  
    correctRotationPID();  
  } else if (GamePad.isRightPressed()) {
    rotateRight();
    targetYaw += 5;  
    correctRotationPID();  
  } else {
    stopMotors();
  }
}

// Função para mover o robô para frente
void moveForward() {
  digitalWrite(leftMotorPin1, HIGH);
  digitalWrite(leftMotorPin2, LOW);
  analogWrite(motorLeftPWM, baseMotorSpeedLeft);

  digitalWrite(rightMotorPin1, HIGH);
  digitalWrite(rightMotorPin2, LOW);
  analogWrite(motorRightPWM, baseMotorSpeedRight);
}

// Função para mover o robô para trás
void moveBackward() {
  digitalWrite(leftMotorPin1, LOW);
  digitalWrite(leftMotorPin2, HIGH);
  analogWrite(motorLeftPWM, baseMotorSpeedLeft);

  digitalWrite(rightMotorPin1, LOW);
  digitalWrite(rightMotorPin2, HIGH);
  analogWrite(motorRightPWM, baseMotorSpeedRight);
}

// Função para parar os motores
void stopMotors() {
  analogWrite(motorLeftPWM, 0);
  analogWrite(motorRightPWM, 0);
}

// Função para rotacionar o robô para a direita
void rotateRight() {
  digitalWrite(leftMotorPin1, LOW);  
  digitalWrite(leftMotorPin2, HIGH);
  analogWrite(motorLeftPWM, rotationSpeed);

  digitalWrite(rightMotorPin1, HIGH);  
  digitalWrite(rightMotorPin2, LOW);
  analogWrite(motorRightPWM, rotationSpeed);
}

// Função para rotacionar o robô para a esquerda
void rotateLeft() {
  digitalWrite(leftMotorPin1, HIGH);  
  digitalWrite(leftMotorPin2, LOW);
  analogWrite(motorLeftPWM, rotationSpeed);

  digitalWrite(rightMotorPin1, LOW);  
  digitalWrite(rightMotorPin2, HIGH);
  analogWrite(motorRightPWM, rotationSpeed);
}

// Função para corrigir o windup do PID
void limitIntegral(float &integralYaw) {
  integralYaw = constrain(integralYaw, -windupLimit, windupLimit);
}

// Função para corrigir a trajetória usando PID com filtro passa-baixa
void correctTrajectoryPID() {
  unsigned long currentTime = millis();
  dt = (currentTime - lastTime) / 1000.0;
  lastTime = currentTime;

  float errorYaw = targetYaw - currentYaw;

  // Reduz os ganhos do PID para movimentos em linha reta
  float KpAdjustLeft = KpLeft * 0.5;  // Ajuste proporcional para roda esquerda
  float KpAdjustRight = KpRight * 0.5;  // Ajuste proporcional para roda direita

  // Cálculo dos termos PID para a roda esquerda
  float proportionalYawLeft = KpAdjustLeft * errorYaw;
  integralYawLeft += errorYaw * dt;
  limitIntegral(integralYawLeft);  
  float integralTermYawLeft = KiLeft * integralYawLeft;
  float derivativeYawLeft = (errorYaw - prevErrorYawLeft) / dt;
  float derivativeTermYawLeft = KdLeft * derivativeYawLeft;

  // Cálculo dos termos PID para a roda direita
  float proportionalYawRight = KpAdjustRight * errorYaw;
  integralYawRight += errorYaw * dt;
  limitIntegral(integralYawRight);  
  float integralTermYawRight = KiRight * integralYawRight;
  float derivativeYawRight = (errorYaw - prevErrorYawRight) / dt;
  float derivativeTermYawRight = KdRight * derivativeYawRight;

  // Correção total para cada roda
  float correctionYawLeft = proportionalYawLeft + integralTermYawLeft + derivativeTermYawLeft;
  float correctionYawRight = proportionalYawRight + integralTermYawRight + derivativeTermYawRight;

  // Ajuste das velocidades das rodas com base na correção do PID
  int leftSpeed = baseMotorSpeedLeft - correctionYawLeft;
  int rightSpeed = baseMotorSpeedRight + correctionYawRight;

  // Limite das velocidades das rodas, com windup aplicado
  leftSpeed = constrain(leftSpeed, -windupLimit, windupLimit);
  rightSpeed = constrain(rightSpeed, -windupLimit, windupLimit);

  // Aplicação das velocidades aos motores
  analogWrite(motorLeftPWM, leftSpeed);
  analogWrite(motorRightPWM, rightSpeed);

  prevErrorYawLeft = errorYaw;
  prevErrorYawRight = errorYaw;
}


// Função para corrigir a rotação usando PID
void correctRotationPID() {
  unsigned long currentTime = millis();
  dt = (currentTime - lastTime) / 1000.0;
  lastTime = currentTime;

  float errorYaw = targetYaw - currentYaw;

  // Cálculos do PID para correção durante a rotação
  float proportionalYawLeft = KpLeft * errorYaw;
  integralYawLeft += errorYaw * dt;
  limitIntegral(integralYawLeft);
  float integralTermYawLeft = KiLeft * integralYawLeft;
  float derivativeYawLeft = (errorYaw - prevErrorYawLeft) / dt;
  float derivativeTermYawLeft = KdLeft * derivativeYawLeft;

  float proportionalYawRight = KpRight * errorYaw;
  integralYawRight += errorYaw * dt;
  limitIntegral(integralYawRight);
  float integralTermYawRight = KiRight * integralYawRight;
  float derivativeYawRight = (errorYaw - prevErrorYawRight) / dt;
  float derivativeTermYawRight = KdRight * derivativeYawRight;

  float correctionYawLeft = proportionalYawLeft + integralTermYawLeft + derivativeTermYawLeft;
  float correctionYawRight = proportionalYawRight + integralTermYawRight + derivativeTermYawRight;

  int leftSpeed = baseMotorSpeedLeft - correctionYawLeft;
  int rightSpeed = baseMotorSpeedRight + correctionYawRight;

  leftSpeed = constrain(leftSpeed, -windupLimit, windupLimit);
  rightSpeed = constrain(rightSpeed, -windupLimit, windupLimit);

  analogWrite(motorLeftPWM, leftSpeed);
  analogWrite(motorRightPWM, rightSpeed);

  prevErrorYawLeft = errorYaw;
  prevErrorYawRight = errorYaw;
}
