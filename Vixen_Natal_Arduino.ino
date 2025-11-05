#include <Adafruit_PCF8574.h>
#include <Wire.h>

// ================= CONFIGURAÇÃO =================

#define QTDE_PCF  2       // módulos PCF8574 (endereços 0x20 e 0x21)
#define QTDE_CI   1       // quantidade de 74HC595
#define PORTAS    (QTDE_PCF * 8 + QTDE_CI * 8)

// Pinos 74HC595
#define pinSH_CP 4   // Clock
#define pinST_CP 3   // Latch
#define pinDS    2   // Data

// Polaridade dos módulos (true = active-low / false = active-high)
bool invertPCF[QTDE_PCF] = { false, true };

// =====================================================
Adafruit_PCF8574 pcfs[QTDE_PCF];
int incomingByte[PORTAS];
int lastByte[PORTAS];

// =====================================================
// Função do usuário — 74HC595
// =====================================================
void ciWrite(byte pino, bool estado) {
  static byte ciBuffer[QTDE_CI];
  bitWrite(ciBuffer[pino / 8], pino % 8, estado);

  digitalWrite(pinST_CP, LOW);
  digitalWrite(pinDS, LOW);
  digitalWrite(pinSH_CP, LOW);

  for (int nC = QTDE_CI - 1; nC >= 0; nC--) {
    for (int nB = 7; nB >= 0; nB--) {
      digitalWrite(pinSH_CP, LOW);
      digitalWrite(pinDS, bitRead(ciBuffer[nC], nB));
      digitalWrite(pinSH_CP, HIGH);
      digitalWrite(pinDS, LOW);
    }
  }

  digitalWrite(pinST_CP, HIGH);
}

// =====================================================
// AUTOTESTE DE SAÍDAS
// =====================================================
void autoTest() {
  Serial.println("\n🚦 Iniciando autoteste de saídas...");

  // Liga e desliga cada relé/controlador em sequência
  for (int j = 0; j < PORTAS; j++) {
    Serial.print("Teste canal ");
    Serial.print(j);
    Serial.println(" ON");

    if (j < QTDE_PCF * 8) {
      // Saídas PCF
      int modulo = j / 8;
      int pino = j % 8;
      int levelOn  = invertPCF[modulo] ? LOW : HIGH;
      int levelOff = invertPCF[modulo] ? HIGH : LOW;

      pcfs[modulo].digitalWrite(pino, levelOn);
      delay(150);
      pcfs[modulo].digitalWrite(pino, levelOff);

    } else {
      // Saídas 74HC595
      int pino = j - (QTDE_PCF * 8);
      ciWrite(pino, HIGH);
      delay(150);
      ciWrite(pino, LOW);
    }
  }

  // Finaliza tudo desligado
  for (int j = 0; j < QTDE_PCF * 8; j++) {
    int modulo = j / 8;
    int pino = j % 8;
    int levelOff = invertPCF[modulo] ? HIGH : LOW;
    pcfs[modulo].digitalWrite(pino, levelOff);
  }
  for (int j = 0; j < (QTDE_CI * 8); j++) {
    ciWrite(j, LOW);
  }

  Serial.println("✅ Autoteste concluído.\n");
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(57600);
  while (!Serial) { delay(10); }

  pinMode(pinSH_CP, OUTPUT);
  pinMode(pinST_CP, OUTPUT);
  pinMode(pinDS, OUTPUT);

  digitalWrite(pinSH_CP, LOW);
  digitalWrite(pinST_CP, LOW);
  digitalWrite(pinDS, LOW);

  Serial.println("🚀 Iniciando sistema...");

  // Inicializa PCFs
  for (int i = 0; i < QTDE_PCF; i++) {
    uint8_t endereco = 0x20 + i;
    if (!pcfs[i].begin(endereco, &Wire)) {
      Serial.print("❌ Falha ao inicializar PCF8574 em 0x");
      Serial.println(endereco, HEX);
      while (1) delay(100);
    }

    for (uint8_t p = 0; p < 8; p++) {
      pcfs[i].pinMode(p, OUTPUT);
      if (invertPCF[i]) pcfs[i].digitalWrite(p, HIGH);  // desligado
      else               pcfs[i].digitalWrite(p, LOW);
    }

    Serial.print("✅ PCF8574 em 0x");
    Serial.println(endereco, HEX);
  }

  // Inicializa buffer do CI
  for (int i = 0; i < QTDE_CI * 8; i++) ciWrite(i, LOW);
  for (int i = 0; i < PORTAS; i++) lastByte[i] = -1;

  Serial.print("✅ Sistema pronto. Total de canais: ");
  Serial.println(PORTAS);

  // Executa o autoteste
  autoTest();
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {
  if (Serial.available() >= PORTAS) {
    for (int i = 0; i < PORTAS; i++) {
      incomingByte[i] = Serial.read();
    }

    for (int j = 0; j < PORTAS; j++) {
      if (incomingByte[j] != lastByte[j]) {
        bool logicalOn = (incomingByte[j] != 0);

        if (j < QTDE_PCF * 8) {
          int modulo = j / 8;
          int pino = j % 8;
          int level = invertPCF[modulo]
                        ? (logicalOn ? LOW : HIGH)
                        : (logicalOn ? HIGH : LOW);
          pcfs[modulo].digitalWrite(pino, level);
        } else {
          int pino = j - (QTDE_PCF * 8);
          ciWrite(pino, logicalOn);
        }

        lastByte[j] = incomingByte[j];
      }
    }
  }
}
