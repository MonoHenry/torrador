# Mapa de registradores Modbus — Torrador

Escravo Modbus RTU. **Slave ID = 1**, **19200 8N1** (ajustável no topo do `.ino`).

Convenção de endereço: os números abaixo são **offsets de dados** (o registrador
"0" do firmware). No Artisan os endereços de Input/Holding Register normalmente são
digitados **iguais** a estes offsets (base 0). Se o seu Artisan usar base 1, some 1.

## Input Registers — sensores (Artisan LÊ, função 04)

| Addr | Nome            | Unidade / escala      | Divisor no Artisan |
|-----:|-----------------|-----------------------|:------------------:|
| 0    | Bean Temp (BT)  | 0.1 °C (2103=210.3)   | 10                 |
| 1    | Env Temp (ET)   | 0.1 °C                | 10                 |
| 2    | Inlet Temp      | 0.1 °C                | 10                 |
| 3    | Exhaust Temp    | 0.1 °C                | 10                 |
| 4    | Ambient Temp    | 0.1 °C                | 10                 |
| 5    | Bean color R    | 0–255                 | 1                  |
| 6    | Bean color G    | 0–255                 | 1                  |
| 7    | Bean color B    | 0–255                 | 1                  |
| 8    | Heat source real| 0–100 %               | 1                  |
| 9    | Exhaust fan real| 0–100 %               | 1                  |
| 10   | Cooling fan     | 0/1                   | 1                  |
| 11   | Drum speed real | RPM                   | 1                  |
| 12   | Airflow/pressão | 0.1 unidade           | 10                 |
| 13   | Porta de carga  | 0=aberta 1=fechada    | 1                  |
| 14   | Porta descarga  | 0=aberta 1=fechada    | 1                  |
| 15   | Bean weight     | gramas                | 1                  |

## Holding Registers — setpoints (Artisan ESCREVE, função 06/16)

| Addr | Nome            | Unidade / escala |
|-----:|-----------------|------------------|
| 0    | Drum speed alvo | RPM              |
| 1    | Target power    | 0–100 %          |
| 2    | Target temp     | 0.1 °C           |
| 3    | Exhaust fan alvo| 0–100 %          |
| 4    | Agitador/vento resfr. | 0–100 %    |

## Coils — comandos on/off (Artisan ESCREVE, função 05)

| Addr | Nome                    | 1 = |
|-----:|-------------------------|-----|
| 0    | Load Chute Actuator     | abre porta de carga |
| 1    | Drop Door Actuator      | abre porta de descarga |
| 2    | Cooling fan             | liga |
| 3    | Cooling Tray Agitator   | liga |
| 4    | Drum enable             | liga motor do tambor |
| 5    | Heat enable             | libera aquecimento |

## Notas de projeto

- **Cor do grão (RGB):** o Artisan não plota "cor" nativamente como curva. As três
  componentes ficam expostas como canais extra; use um sensor tipo TCS34725. Se não
  precisar da cor durante a torra, pode ignorar esses registradores.
- **Drum speed:** por padrão muitos torradores só têm on/off. Aqui o RPM alvo
  (HR0) vira PWM no motor (`PIN_DRUM`), e o `CO_DRUM_ENABLE` liga/desliga o driver.
  Para leitura real de RPM adicione um sensor hall/encoder por interrupção.
- **Target power vs Target temperature:** exponho os dois (HR1 e HR2). Escolha um
  como principal na UI. Se usar temperatura-alvo, implemente um PID no Arduino (ou
  deixe o Artisan fazer o PID escrevendo em HR1 = power).
- Temperaturas usam `int16`, então negativos são suportados; no Artisan marque o
  canal como inteiro **com sinal** e divisor 10.
