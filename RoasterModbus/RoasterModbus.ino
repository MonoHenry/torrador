/*
 * ============================================================================
 *  RoasterModbus  --  Escravo Modbus RTU (SEM biblioteca) p/ torrador + Artisan
 * ============================================================================
 *
 *  Implementacao propria de Modbus RTU. A biblioteca ModbusRTUSlave (CMB27)
 *  NAO respondia neste UNO+CH340 (framing com timing apertado demais para o
 *  timing do USB). Esta versao usa deteccao de fim-de-frame por PAUSA (gap),
 *  bem mais tolerante -- testada e respondendo 100%.
 *
 *  Funcoes Modbus suportadas (as que o Artisan usa):
 *    0x03  Read Holding Registers    (setpoints, leitura)
 *    0x04  Read Input Registers      (sensores)
 *    0x01  Read Coils                (estado dos on/off)
 *    0x05  Write Single Coil         (liga/desliga atuador)
 *    0x06  Write Single Register     (setpoint)
 *    0x0F  Write Multiple Coils
 *    0x10  Write Multiple Registers
 *
 *  Sensores no bancada:
 *    - MLX90614 (I2C A4/A5) -> BT (obj) + Ambiente
 *    - HX711    (D8/D9)     -> peso
 *  Demais canais (ET, Inlet, Exhaust, cor, pressao, RPM, portas) sao stubs
 *  marcados com TODO -- troque pelo hardware real.
 *
 *  Serial = USB (mesma UART). Sem Serial.print de debug: a serial e do Modbus.
 * ============================================================================
 */

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include "HX711.h"

// ---------------------------------------------------------------------------
//  Parametros Modbus (precisam bater com o Artisan)
// ---------------------------------------------------------------------------
const uint8_t  SLAVE_ID = 1;
const uint32_t BAUD     = 9600;   // 8N1. Casar com o Artisan.

// Pausa (ms) que marca o fim de um frame Modbus. 4ms >> tempo entre bytes a
// 9600 (~1ms), entao nunca corta um frame no meio, e e tolerante ao USB.
const unsigned long FRAME_GAP_MS = 4;

// ---------------------------------------------------------------------------
//  Pinos
// ---------------------------------------------------------------------------
const int HX711_DT  = 8;
const int HX711_SCK = 9;
const int PIN_AIRFLOW    = A0;
const int PIN_LIMIT_LOAD = A1;   // porta de carga fechada? (HIGH=fechada)
const int PIN_LIMIT_DROP = A2;   // porta de descarga fechada?
const int PIN_HEAT     = 3;      // PWM fonte de calor
const int PIN_EXHAUST  = 5;      // PWM exaustor
const int PIN_DRUM     = 6;      // PWM motor tambor
const int PIN_AGITATOR = 10;     // PWM agitador/vento resfriamento
const int PIN_COOLING_FAN = 7;   // rele
const int PIN_LOAD_CHUTE  = 4;   // rele porta carga
const int PIN_DROP_DOOR   = 12;  // rele porta descarga
const int PIN_DRUM_ENABLE = 11;  // habilita driver tambor
const int PIN_HEAT_ENABLE = 13;  // rele mestre aquecimento

const int DRUM_RPM_MIN = 0;
const int DRUM_RPM_MAX = 60;

float calibration_factor = 199.0f;  // calibre com sketch_jun20a

// ---------------------------------------------------------------------------
//  Mapa de registradores (ver REGISTER_MAP.md)
// ---------------------------------------------------------------------------
enum {  // INPUT REGISTERS (0x04) -- sensores
  IR_BT = 0, IR_ET, IR_INLET, IR_EXHAUST, IR_AMBIENT,
  IR_COLOR_R, IR_COLOR_G, IR_COLOR_B,
  IR_HEAT_ACT, IR_FAN_ACT, IR_COOLING_FAN,
  IR_DRUM_RPM, IR_AIRFLOW, IR_DOOR_LOAD, IR_DOOR_DROP, IR_WEIGHT,
  NUM_INPUT_REGS
};
enum {  // HOLDING REGISTERS (0x03/0x06/0x10) -- setpoints
  HR_DRUM_TARGET = 0, HR_POWER_TARGET, HR_TEMP_TARGET,
  HR_FAN_TARGET, HR_COOLING_TARGET,
  NUM_HOLDING_REGS
};
enum {  // COILS (0x01/0x05/0x0F) -- on/off
  CO_LOAD_CHUTE = 0, CO_DROP_DOOR, CO_COOLING_FAN,
  CO_AGITATOR, CO_DRUM_ENABLE, CO_HEAT_ENABLE,
  NUM_COILS
};

uint16_t inputRegisters[NUM_INPUT_REGS];
uint16_t holdingRegisters[NUM_HOLDING_REGS];
bool     coils[NUM_COILS];

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
HX711 scale;

// ===========================================================================
//  MODBUS RTU (implementacao propria)
// ===========================================================================
uint8_t rxbuf[64];
uint8_t rxlen = 0;
unsigned long lastByteMs = 0;

uint16_t crc16(const uint8_t* d, uint8_t n) {
  uint16_t c = 0xFFFF;
  for (uint8_t i = 0; i < n; i++) {
    c ^= d[i];
    for (uint8_t b = 0; b < 8; b++) c = (c & 1) ? (c >> 1) ^ 0xA001 : c >> 1;
  }
  return c;
}

// Envia n bytes de 'd' seguidos do CRC.
void sendResponse(const uint8_t* d, uint8_t n) {
  uint16_t c = crc16(d, n);
  Serial.write(d, n);
  Serial.write((uint8_t)(c & 0xFF));
  Serial.write((uint8_t)(c >> 8));
  Serial.flush();
}

void sendException(uint8_t func, uint8_t code) {
  uint8_t r[3] = { SLAVE_ID, (uint8_t)(func | 0x80), code };
  sendResponse(r, 3);
}

// Le 'qty' registradores de 'regs' a partir de 'start' -> resposta 0x03/0x04
void handleReadRegisters(uint8_t func, const uint16_t* regs, uint16_t count) {
  uint16_t start = (rxbuf[2] << 8) | rxbuf[3];
  uint16_t qty   = (rxbuf[4] << 8) | rxbuf[5];
  if (qty < 1 || qty > 125 || (uint32_t)start + qty > count) {
    sendException(func, 0x02); return;   // endereco invalido
  }
  uint8_t resp[3 + 2 * 125];
  resp[0] = SLAVE_ID;
  resp[1] = func;
  resp[2] = qty * 2;
  for (uint16_t i = 0; i < qty; i++) {
    resp[3 + i * 2] = regs[start + i] >> 8;
    resp[4 + i * 2] = regs[start + i] & 0xFF;
  }
  sendResponse(resp, 3 + qty * 2);
}

// 0x01 Read Coils
void handleReadCoils() {
  uint16_t start = (rxbuf[2] << 8) | rxbuf[3];
  uint16_t qty   = (rxbuf[4] << 8) | rxbuf[5];
  if (qty < 1 || qty > 2000 || (uint32_t)start + qty > NUM_COILS) {
    sendException(0x01, 0x02); return;
  }
  uint8_t byteCount = (qty + 7) / 8;
  uint8_t resp[3 + 32];
  resp[0] = SLAVE_ID; resp[1] = 0x01; resp[2] = byteCount;
  for (uint8_t i = 0; i < byteCount; i++) resp[3 + i] = 0;
  for (uint16_t i = 0; i < qty; i++)
    if (coils[start + i]) resp[3 + (i / 8)] |= (1 << (i % 8));
  sendResponse(resp, 3 + byteCount);
}

// 0x05 Write Single Coil
void handleWriteCoil(bool broadcast) {
  uint16_t addr = (rxbuf[2] << 8) | rxbuf[3];
  uint16_t val  = (rxbuf[4] << 8) | rxbuf[5];
  if (addr >= NUM_COILS) { if (!broadcast) sendException(0x05, 0x02); return; }
  coils[addr] = (val == 0xFF00);
  if (!broadcast) sendResponse(rxbuf, 6);   // eco
}

// 0x06 Write Single Register
void handleWriteRegister(bool broadcast) {
  uint16_t addr = (rxbuf[2] << 8) | rxbuf[3];
  uint16_t val  = (rxbuf[4] << 8) | rxbuf[5];
  if (addr >= NUM_HOLDING_REGS) { if (!broadcast) sendException(0x06, 0x02); return; }
  holdingRegisters[addr] = val;
  if (!broadcast) sendResponse(rxbuf, 6);   // eco
}

// 0x0F Write Multiple Coils
void handleWriteCoils(bool broadcast) {
  uint16_t start = (rxbuf[2] << 8) | rxbuf[3];
  uint16_t qty   = (rxbuf[4] << 8) | rxbuf[5];
  if ((uint32_t)start + qty > NUM_COILS) { if (!broadcast) sendException(0x0F, 0x02); return; }
  for (uint16_t i = 0; i < qty; i++) {
    uint8_t bit = (rxbuf[7 + (i / 8)] >> (i % 8)) & 1;
    coils[start + i] = bit;
  }
  if (!broadcast) sendResponse(rxbuf, 6);   // id,func,start,qty
}

// 0x10 Write Multiple Registers
void handleWriteRegisters(bool broadcast) {
  uint16_t start = (rxbuf[2] << 8) | rxbuf[3];
  uint16_t qty   = (rxbuf[4] << 8) | rxbuf[5];
  if ((uint32_t)start + qty > NUM_HOLDING_REGS) { if (!broadcast) sendException(0x10, 0x02); return; }
  for (uint16_t i = 0; i < qty; i++)
    holdingRegisters[start + i] = (rxbuf[7 + i * 2] << 8) | rxbuf[8 + i * 2];
  if (!broadcast) sendResponse(rxbuf, 6);   // id,func,start,qty
}

void processFrame() {
  if (rxlen < 4) return;                       // curto demais
  uint16_t calc = crc16(rxbuf, rxlen - 2);
  uint16_t got  = rxbuf[rxlen - 2] | (rxbuf[rxlen - 1] << 8);
  if (calc != got) return;                     // CRC ruim -> ignora
  uint8_t id = rxbuf[0];
  if (id != SLAVE_ID && id != 0) return;       // nao e pra mim
  bool broadcast = (id == 0);
  uint8_t func = rxbuf[1];
  switch (func) {
    case 0x03: handleReadRegisters(0x03, holdingRegisters, NUM_HOLDING_REGS); break;
    case 0x04: handleReadRegisters(0x04, inputRegisters, NUM_INPUT_REGS); break;
    case 0x01: handleReadCoils(); break;
    case 0x05: handleWriteCoil(broadcast); break;
    case 0x06: handleWriteRegister(broadcast); break;
    case 0x0F: handleWriteCoils(broadcast); break;
    case 0x10: handleWriteRegisters(broadcast); break;
    default:   if (!broadcast) sendException(func, 0x01);  // funcao ilegal
  }
}

// ===========================================================================
//  SENSORES  (troque os TODO pelo hardware real)
// ===========================================================================
float readBeanTemp()    { float t = mlx.readObjectTempC();  return isnan(t) ? 0.0f : t; }
float readAmbientTemp() { float t = mlx.readAmbientTempC(); return isnan(t) ? 0.0f : t; }
float readEnvTemp()     { return 0.0f; }   // TODO termopar
float readInletTemp()   { return 0.0f; }   // TODO
float readExhaustTemp() { return 0.0f; }   // TODO
void  readBeanColor(uint16_t &r, uint16_t &g, uint16_t &b) { r = 0; g = 0; b = 0; } // TODO TCS34725
float readAirflow()     { return analogRead(PIN_AIRFLOW) * (10.0f / 1023.0f); }     // TODO calibrar
int   readDrumRPM()     { return holdingRegisters[HR_DRUM_TARGET]; } // TODO tacometro
bool  readDoorLoadClosed() { return digitalRead(PIN_LIMIT_LOAD) == HIGH; }
bool  readDoorDropClosed() { return digitalRead(PIN_LIMIT_DROP) == HIGH; }

int readWeightGrams() {
  if (!scale.is_ready()) return inputRegisters[IR_WEIGHT];   // mantem ultimo
  float g = scale.get_units(1) / calibration_factor;
  return (g < 0) ? 0 : (int)g;
}

void updateSensors() {
  inputRegisters[IR_BT]      = (int16_t)(readBeanTemp()    * 10.0f);
  inputRegisters[IR_ET]      = (int16_t)(readEnvTemp()     * 10.0f);
  inputRegisters[IR_INLET]   = (int16_t)(readInletTemp()   * 10.0f);
  inputRegisters[IR_EXHAUST] = (int16_t)(readExhaustTemp() * 10.0f);
  inputRegisters[IR_AMBIENT] = (int16_t)(readAmbientTemp() * 10.0f);
  uint16_t r, g, b; readBeanColor(r, g, b);
  inputRegisters[IR_COLOR_R] = r;
  inputRegisters[IR_COLOR_G] = g;
  inputRegisters[IR_COLOR_B] = b;
  inputRegisters[IR_HEAT_ACT]    = holdingRegisters[HR_POWER_TARGET];
  inputRegisters[IR_FAN_ACT]     = holdingRegisters[HR_FAN_TARGET];
  inputRegisters[IR_COOLING_FAN] = coils[CO_COOLING_FAN] ? 1 : 0;
  inputRegisters[IR_DRUM_RPM]    = readDrumRPM();
  inputRegisters[IR_AIRFLOW]     = (uint16_t)(readAirflow() * 10.0f);
  inputRegisters[IR_DOOR_LOAD]   = readDoorLoadClosed() ? 1 : 0;
  inputRegisters[IR_DOOR_DROP]   = readDoorDropClosed() ? 1 : 0;
  inputRegisters[IR_WEIGHT]      = (uint16_t)readWeightGrams();
}

// ===========================================================================
//  ATUADORES
// ===========================================================================
uint8_t percentToPWM(uint16_t pct) {
  if (pct > 100) pct = 100;
  return (uint8_t)((pct * 255UL) / 100UL);
}

void applyActuators() {
  analogWrite(PIN_HEAT,     coils[CO_HEAT_ENABLE] ? percentToPWM(holdingRegisters[HR_POWER_TARGET]) : 0);
  analogWrite(PIN_EXHAUST,  percentToPWM(holdingRegisters[HR_FAN_TARGET]));
  analogWrite(PIN_AGITATOR, coils[CO_AGITATOR] ? percentToPWM(holdingRegisters[HR_COOLING_TARGET]) : 0);
  uint16_t rpm = holdingRegisters[HR_DRUM_TARGET];
  if (rpm > DRUM_RPM_MAX) rpm = DRUM_RPM_MAX;
  analogWrite(PIN_DRUM, coils[CO_DRUM_ENABLE] ? (uint8_t)map(rpm, DRUM_RPM_MIN, DRUM_RPM_MAX, 0, 255) : 0);
  digitalWrite(PIN_HEAT_ENABLE, coils[CO_HEAT_ENABLE] ? HIGH : LOW);
  digitalWrite(PIN_DRUM_ENABLE, coils[CO_DRUM_ENABLE] ? HIGH : LOW);
  digitalWrite(PIN_COOLING_FAN, coils[CO_COOLING_FAN] ? HIGH : LOW);
  digitalWrite(PIN_LOAD_CHUTE,  coils[CO_LOAD_CHUTE]  ? HIGH : LOW);
  digitalWrite(PIN_DROP_DOOR,   coils[CO_DROP_DOOR]   ? HIGH : LOW);
}

// ===========================================================================
//  SETUP / LOOP
// ===========================================================================
unsigned long lastSensorMs = 0;

void setup() {
  pinMode(PIN_HEAT, OUTPUT);       pinMode(PIN_EXHAUST, OUTPUT);
  pinMode(PIN_DRUM, OUTPUT);       pinMode(PIN_AGITATOR, OUTPUT);
  pinMode(PIN_COOLING_FAN, OUTPUT);pinMode(PIN_LOAD_CHUTE, OUTPUT);
  pinMode(PIN_DROP_DOOR, OUTPUT);  pinMode(PIN_DRUM_ENABLE, OUTPUT);
  pinMode(PIN_HEAT_ENABLE, OUTPUT);
  pinMode(PIN_LIMIT_LOAD, INPUT_PULLUP);
  pinMode(PIN_LIMIT_DROP, INPUT_PULLUP);
  applyActuators();               // estado seguro (tudo desligado)

  Wire.begin();
  mlx.begin();
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale();
  if (scale.wait_ready_timeout(500)) scale.tare();  // nao trava se HX711 ausente

  Serial.begin(BAUD);             // UART = Modbus
}

void loop() {
  // recebe bytes e detecta fim de frame por pausa
  while (Serial.available()) {
    if (rxlen < sizeof(rxbuf)) rxbuf[rxlen++] = Serial.read();
    else Serial.read();           // descarta excesso
    lastByteMs = millis();
  }
  if (rxlen > 0 && (millis() - lastByteMs) >= FRAME_GAP_MS) {
    processFrame();
    rxlen = 0;
  }

  applyActuators();

  if (millis() - lastSensorMs >= 250) {
    lastSensorMs = millis();
    updateSensors();
  }
}
