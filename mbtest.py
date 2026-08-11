#!/usr/bin/env python3
"""
Testa o Arduino escravo Modbus RTU (RoasterModbus.ino) sem o Artisan.

Uso:
    .mbvenv/bin/python mbtest.py            # uma leitura
    .mbvenv/bin/python mbtest.py --loop     # atualiza a cada 0.5s (Ctrl+C sai)

Feche o Artisan e o Serial Monitor antes: a porta so aceita um dono.
"""
import sys, time
from pymodbus.client import ModbusSerialClient

PORT = "/dev/ttyUSB0"
BAUD = 19200
SLAVE = 1

# (nome, escala) -> valor mostrado = registrador / escala
INPUTS = [
    ("BT (Bean Temp)",   10),
    ("ET (Env Temp)",    10),
    ("Inlet Temp",       10),
    ("Exhaust Temp",     10),
    ("Ambient Temp",     10),
    ("Cor R",             1),
    ("Cor G",             1),
    ("Cor B",             1),
    ("Heat % real",       1),
    ("Fan % real",        1),
    ("Cooling fan",       1),
    ("Drum RPM",          1),
    ("Airflow",          10),
    ("Porta carga",       1),
    ("Porta descarga",    1),
    ("Peso (g)",          1),
]

def to_signed(v):
    return v - 0x10000 if v >= 0x8000 else v

def read_and_print(client):
    rr = client.read_input_registers(0, count=len(INPUTS), device_id=SLAVE)
    if rr.isError():
        print("ERRO ao ler:", rr)
        return False
    regs = rr.registers
    print("\n=== INPUT REGISTERS (func 04) ===")
    for i, (name, scale) in enumerate(INPUTS):
        raw = regs[i]
        val = to_signed(raw) / scale if scale != 1 else raw
        print(f"  reg {i:>2}  {name:<18} raw={raw:<6} -> {val}")
    return True

def main():
    loop = "--loop" in sys.argv
    client = ModbusSerialClient(PORT, baudrate=BAUD, parity="N",
                                stopbits=1, bytesize=8, timeout=1)
    if not client.connect():
        print(f"Nao consegui abrir {PORT}. Artisan/Serial Monitor aberto? Cabo?")
        sys.exit(1)
    print(f"Conectado em {PORT} @ {BAUD}, slave {SLAVE}")
    print("Aguardando o Arduino bootar (reset ao abrir a porta)...")
    time.sleep(2.5)
    try:
        if loop:
            while True:
                if not read_and_print(client):
                    break
                time.sleep(0.5)
        else:
            read_and_print(client)
    except KeyboardInterrupt:
        pass
    finally:
        client.close()

if __name__ == "__main__":
    main()
