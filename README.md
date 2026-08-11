# Torrador de Café — Controle via Modbus RTU + Artisan

Firmware para **Arduino UNO** que atua como escravo **Modbus RTU**, integrando
os sensores e atuadores de um torrador de café ao software de torra
[Artisan](https://artisan-scope.org/). O Artisan lê os sensores (temperaturas,
peso, etc.) e escreve os comandos (potência, exaustor, tambor, portas).

## Destaque técnico

A biblioteca `ModbusRTUSlave` (CMB27) **não respondia** neste UNO com chip
USB-serial **CH340**: o framing dela usa timeout entre bytes muito apertado
(~1 ms a 19200), e o CH340 entrega os bytes da USB em rajadas, quebrando os
frames. Comprovado em teste lado a lado (biblioteca = 0 % de resposta; versão
própria = 100 %).

A solução foi **implementar o Modbus RTU do zero**, detectando o fim de frame
por uma **pausa de 4 ms** (gap) em vez de timeout por byte — bem mais tolerante
ao comportamento do USB. Ver comentários no topo de
[`RoasterModbus/RoasterModbus.ino`](RoasterModbus/RoasterModbus.ino).

Funções Modbus suportadas: `0x01` `0x03` `0x04` `0x05` `0x06` `0x0F` `0x10`.
Parâmetros: escravo **ID 1**, **9600 8N1**.

## Estrutura

| Caminho | Descrição |
|---|---|
| `RoasterModbus/RoasterModbus.ino` | Firmware principal (Modbus RTU sem biblioteca) |
| `RoasterModbus/REGISTER_MAP.md`   | Mapa dos registradores (contrato Arduino ↔ Artisan) |
| `RoasterModbus/ARTISAN_SETUP.md`  | Guia de configuração do Artisan |
| `mbtest.py`                       | Script de teste headless (pymodbus) |
| `diagnostics/`                    | Sketches usados no diagnóstico do problema da biblioteca |

## Hardware de bancada

- **MLX90614** (I²C, A4/A5) → Bean Temp + temperatura ambiente
- **HX711** (D8/D9) + célula de carga → peso dos grãos
- Demais canais (ET, Inlet, Exhaust, cor, pressão, RPM, portas) são *stubs*
  marcados com `TODO` no código, prontos pra receber o hardware real.

## Como testar

1. Subir `RoasterModbus/RoasterModbus.ino` no Arduino (IDE do Arduino).
2. Sem o Artisan aberto, rodar `python mbtest.py` para ler/escrever registradores.
3. No Artisan, configurar Modbus conforme `RoasterModbus/ARTISAN_SETUP.md`.
