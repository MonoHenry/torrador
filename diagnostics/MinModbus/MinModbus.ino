#include <ModbusRTUSlave.h>
ModbusRTUSlave modbus(Serial);
uint16_t ireg[4] = {1111, 2222, 3333, 4444};
void setup() {
  modbus.configureInputRegisters(ireg, 4);
  modbus.begin(1, 9600, SERIAL_8N1);   // 9600 = mais tolerante sobre USB
}
void loop() { modbus.poll(); }
