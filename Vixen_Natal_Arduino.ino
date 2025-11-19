#include <Adafruit_PCF8574.h>
#include <Wire.h>
#include <FastLED.h>

#define QTDE_PCF  2
#define QTDE_CI   1
#define PORTAS_RELE (QTDE_PCF * 8 + QTDE_CI * 8)

#define LED_PIN1 5
#define LED_PIN2 6
#define NUM_LEDS1 50
#define NUM_LEDS2 50

#define pinSH_CP 4
#define pinST_CP 3
#define pinDS    2
#define LED_STATUS 13

#define ONMIN 235

bool invertPCF[QTDE_PCF] = { true, true };

Adafruit_PCF8574 pcfs[QTDE_PCF];
CRGB leds1[NUM_LEDS1];
CRGB leds2[NUM_LEDS2];

// Buffer para relés
uint8_t bufferRele[(PORTAS_RELE + 7) / 8] = {0};

unsigned long ultimaRecepcao = 0;
const int TOTAL_CANAIS = PORTAS_RELE + (NUM_LEDS1 * 3) + (NUM_LEDS2 * 3);
bool recebendoFrame = false;
int canalAtual = 0;

void ciWrite(byte pino, bool estado) {
  bitWrite(bufferRele[QTDE_PCF + pino / 8], pino % 8, estado ? 1 : 0);
  digitalWrite(pinST_CP, LOW);
  for (int nC = QTDE_CI - 1; nC >= 0; nC--) {
    for (int nB = 7; nB >= 0; nB--) {
      digitalWrite(pinSH_CP, LOW);
      digitalWrite(pinDS, bitRead(bufferRele[QTDE_PCF + nC], nB));
      digitalWrite(pinSH_CP, HIGH);
    }
  }
  digitalWrite(pinST_CP, HIGH);
}

void setRele(int canal, bool estado) {
  int byteIdx = canal / 8;
  int bitIdx  = canal % 8;
  if (estado) bufferRele[byteIdx] |= (1 << bitIdx);
  else        bufferRele[byteIdx] &= ~(1 << bitIdx);
}

void atualizarRele() {
  for (int j = 0; j < QTDE_PCF * 8; j++) {
    int modulo = j / 8;
    int pino   = j % 8;
    bool estado = bufferRele[modulo] & (1 << pino);
    pcfs[modulo].digitalWrite(pino, invertPCF[modulo] ? !estado : estado);
  }

  for (int j = 0; j < QTDE_CI * 8; j++) {
    bool estado = bufferRele[QTDE_PCF + j / 8] & (1 << (j % 8));
    ciWrite(j, estado);
  }
}

void setup() {
  Serial.begin(57600);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(pinSH_CP, OUTPUT);
  pinMode(pinST_CP, OUTPUT);
  pinMode(pinDS, OUTPUT);

  ultimaRecepcao = millis();   // *** impede standby no início ***

  Serial.println("🚀 Sistema iniciado. Aguardando '$'...");

  for (int i = 0; i < QTDE_PCF; i++) {
    uint8_t endereco = 0x20 + i;
    pcfs[i].begin(endereco, &Wire);
    for (uint8_t p = 0; p < 8; p++) {
      pcfs[i].pinMode(p, OUTPUT);
      pcfs[i].digitalWrite(p, invertPCF[i] ? HIGH : LOW);
    }
  }

  for (int i = 0; i < 8; i++) ciWrite(i, LOW);

  FastLED.addLeds<WS2811, LED_PIN1, BRG>(leds1, NUM_LEDS1);
  FastLED.addLeds<WS2811, LED_PIN2, BRG>(leds2, NUM_LEDS2);
  FastLED.setBrightness(180);
  fill_solid(leds1, NUM_LEDS1, CRGB::Black);
  fill_solid(leds2, NUM_LEDS2, CRGB::Black);
  FastLED.show();
}

void loop() {
  static bool standbyAtivo = false;

  while (Serial.available()) {
    byte valor = Serial.read();

    if (valor == '$') {
      recebendoFrame = true;
      canalAtual = 0;
      ultimaRecepcao = millis();
      continue;
    }

    if (!recebendoFrame) continue;

    if (canalAtual < PORTAS_RELE) {
      setRele(canalAtual, valor > ONMIN);
    }
    else if (canalAtual < PORTAS_RELE + NUM_LEDS1 * 3) {
      int idx = canalAtual - PORTAS_RELE;
      int led = idx / 3;
      int comp = idx % 3;
      if (comp == 0) leds1[led].r = valor;
      else if (comp == 1) leds1[led].g = valor;
      else leds1[led].b = valor;
    }
    else if (canalAtual < PORTAS_RELE + (NUM_LEDS1 + NUM_LEDS2) * 3) {
      int idx = canalAtual - PORTAS_RELE - NUM_LEDS1 * 3;
      int led = idx / 3;
      int comp = idx % 3;
      if (comp == 0) leds2[led].r = valor;
      else if (comp == 1) leds2[led].g = valor;
      else leds2[led].b = valor;
    }

    canalAtual++;
    ultimaRecepcao = millis();

    if (canalAtual >= TOTAL_CANAIS) {
      recebendoFrame = false;
      canalAtual = 0;
      break;
    }
  }

  if (recebendoFrame && millis() - ultimaRecepcao > 100) {
    recebendoFrame = false;
    canalAtual = 0;
  }

  // ================================
  // ENTRADA EM STANDBY
  // ================================
  if (millis() - ultimaRecepcao > 2000) {

    if (!standbyAtivo) {
      standbyAtivo = true;

      // Liga todos os reles
      for (int j = 0; j < QTDE_PCF * 8; j++) {
        int modulo = j / 8;
        int pino = j % 8;
        int levelOn = invertPCF[modulo] ? LOW : HIGH;
        pcfs[modulo].digitalWrite(pino, levelOn);
      }
      for (int j = 0; j < QTDE_CI * 8; j++) {
        ciWrite(j, HIGH);
      }
    }

    // Efeito RGB
    static uint8_t hue = 0;
    static unsigned long lastStandby = 0;
    if (millis() - lastStandby > 30) {
      hue++;
      fill_solid(leds1, NUM_LEDS1, CHSV(hue, 255, 200));
      fill_solid(leds2, NUM_LEDS2, CHSV(hue + 64, 255, 200));
      FastLED.show();
      lastStandby = millis();
    }

    return; // impede atualização normal
  }

  // ================================
  // SAIU DO STANDBY → ATUALIZA NORMAL
  // ================================
  if (standbyAtivo) {
    standbyAtivo = false;
  }

  static unsigned long lastUpdate = 0;
  if (!recebendoFrame && millis() - lastUpdate > 0) {
    atualizarRele();
    FastLED.show();
    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    lastUpdate = millis();
  }
}
