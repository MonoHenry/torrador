# Perfil de máquina no Artisan — Torrador Modbus RTU

Guia para configurar o Artisan como **mestre Modbus RTU** falando com o Arduino
(escravo, `RoasterModbus.ino`). Testado com a estrutura de menus do Artisan 2.x.

> Dica: ao terminar, salve tudo em **Help ▸ Save Settings…** (`.aset`). Esse arquivo
> É o "perfil de máquina" reutilizável — carregue com **Help ▸ Load Settings…** em
> qualquer instalação.

---

## 1. Porta serial / Modbus

**Config ▸ Port…** → aba **Modbus**:

| Campo        | Valor                                   |
|--------------|-----------------------------------------|
| Comm Port    | a porta USB do Arduino (ex.: `/dev/ttyUSB0`, `/dev/ttyACM0`, `COM5`) |
| Baud Rate    | `19200`                                 |
| Byte Size    | `8`                                     |
| Parity       | `N`                                     |
| Stopbits     | `1`                                     |
| Timeout      | `1.0` s                                 |
| Protocol     | `Serial (RTU)`                          |

## 2. Dispositivo principal (BT/ET)

**Config ▸ Device…** → aba **Meter** → selecione **Modbus**.

Ainda na aba **Modbus** do diálogo Device (ou em Config ▸ Port ▸ Modbus, conforme
a versão), configure os *inputs*. Cada input tem: **slave**, **register**, **função**
(3=holding / 4=input), **float** (desligado = inteiro) e **divisor**.

| Input | Slave | Register | Func | Tipo         | Divisor | → Canal Artisan |
|:-----:|:-----:|:--------:|:----:|--------------|:-------:|-----------------|
| 1     | 1     | 0        | 4    | int c/ sinal | 10      | **BT**          |
| 2     | 1     | 1        | 4    | int c/ sinal | 10      | **ET**          |
| 3     | 1     | 2        | 4    | int c/ sinal | 10      | Inlet           |
| 4     | 1     | 3        | 4    | int c/ sinal | 10      | Exhaust         |

## 3. Canais extra (demais sensores)

**Config ▸ Device…** → aba **Extra Devices** → **Add** para cada par.
Escolha o tipo **+MODBUS_5** … (os canais extra do Modbus). Cada dispositivo extra
adiciona 2 canais; mapeie os registradores restantes:

| Register | Func | Div | Canal sugerido      |
|:--------:|:----:|:---:|---------------------|
| 4        | 4    | 10  | Ambient Temp        |
| 8        | 4    | 1   | Heat % (real)       |
| 9        | 4    | 1   | Fan % (real)        |
| 11       | 4    | 1   | Drum RPM            |
| 12       | 4    | 10  | Airflow             |
| 15       | 4    | 1   | Bean weight (g)     |

> Cor RGB (regs 5/6/7) e estado das portas (13/14) podem ser adicionados como
> canais extra também, mas normalmente não precisam virar curva — deixe de fora
> se quiser um gráfico limpo.

Marque em **Curves/LCDs** quais canais aparecem no gráfico e nos mostradores.

---

## 4. Atuadores — Sliders (controle contínuo)

**Config ▸ Events…** → aba **Sliders**. Habilite um slider por atuador (0–100 ou
0–RPM) e no campo de ação escolha **Modbus** com o comando de escrita.

Sintaxe Modbus do Artisan para *holding register*: `write(slave,register,value)`.
Use `{}` para inserir o valor atual do slider.

| Slider | Faixa   | Comando (ação = Modbus)      | Escreve      |
|--------|---------|------------------------------|--------------|
| Power  | 0–100   | `write(1,1,{})`              | HR1 power    |
| Fan    | 0–100   | `write(1,3,{})`              | HR3 exhaust  |
| Drum   | 0–60    | `write(1,0,{})`              | HR0 rpm      |
| Cooling| 0–100   | `write(1,4,{})`              | HR4 agitador |

> Para usar **temperatura-alvo** em vez de potência, aponte um slider para
> `write(1,2,{})` (HR2) — lembrando que aí quem faz o PID é o Arduino.

## 5. Atuadores — Botões (on/off, coils)

**Config ▸ Events…** → aba **Buttons**. Crie um botão por coil. Ação = **Modbus**,
comando de escrita de coil: `writeCoil(slave,coil,value)` (value 1=liga, 0=desliga).
Use um botão para cada estado, ou um botão "toggle".

| Botão            | Liga                 | Desliga              |
|------------------|----------------------|----------------------|
| Aquecimento      | `writeCoil(1,5,1)`   | `writeCoil(1,5,0)`   |
| Tambor           | `writeCoil(1,4,1)`   | `writeCoil(1,4,0)`   |
| Vent. resfriam.  | `writeCoil(1,2,1)`   | `writeCoil(1,2,0)`   |
| Agitador bandeja | `writeCoil(1,3,1)`   | `writeCoil(1,3,0)`   |
| Porta de carga   | `writeCoil(1,0,1)`   | `writeCoil(1,0,0)`   |
| Porta descarga   | `writeCoil(1,1,1)`   | `writeCoil(1,1,0)`   |

> A sintaxe exata (`write` / `writeSingleRegister` / `writeCoil`) pode variar
> conforme a versão do Artisan. Confira em **Help ▸ Documentation ▸ Modbus** os
> comandos suportados pela sua build e ajuste `slave`/`register` conforme a tabela.

## 6. PID (opcional)

Se quiser que o Artisan controle a temperatura automaticamente:
**Config ▸ PID…** → Control = **Modbus**, saída escrevendo em HR1 (power), entrada
lendo BT (input reg 0). Assim o Arduino só executa a potência que o Artisan mandar.

---

## 7. Ordem de testes recomendada

1. Faça upload do `RoasterModbus.ino` (instale a lib **ModbusRTUSlave** antes).
2. Feche o Serial Monitor da IDE — a porta é do Modbus, não pode estar ocupada.
3. No Artisan, configure a porta (passo 1) e clique **ON**: BT/ET devem aparecer.
4. Teste um slider (ex.: Fan) e confirme o PWM no pino `PIN_EXHAUST` (5).
5. Teste um botão de coil e confirme o rele correspondente.
6. Calibre a célula de carga (use `sketch_jun20a`) e ajuste `calibration_factor`.
