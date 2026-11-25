# Firmware de coleta — ESP32

## Hardware e pinos

| Componente | GPIO |
|---|---:|
| Flexivel — polegar | 33 |
| Flexivel — indicador | 34 |
| Flexivel — medio | 14 |
| Flexivel — anelar | 12 |
| Flexivel — minimo | 25 |
| MPU-6050 SDA | 21 |
| MPU-6050 SCL | 22 |
| Gap indicador-medio | 35 |
| Gap medio-anelar | 32 (desativado: sensor quebrado) |
| Gap anelar-minimo | 13 |

Os GPIOs 12, 13, 14 e 25 pertencem ao ADC2 no ESP32 classico; nao use Wi-Fi durante leituras analogicas. GPIO12 e pino de strapping e pode impedir o boot se o circuito o mantiver em nivel alto durante a inicializacao. GPIOs nao toleram 5 V.

## Configuracao

- Placa: Node32s / ESP32 classico.
- FQBN: esp32:esp32:node32s.
- Core Arduino-ESP32: 3.3.11.
- Serial: 115200 baud.
- Frequencia nominal: 20 Hz (50 ms).
- MPU-6050: acelerometro +/-2 g e giroscopio +/-250 graus/s.
- Janela: 40 x 14, aproximadamente 2 segundos.

## Comandos seriais

- HELP
- START / PAUSE / STOP
- STATUS ou DIAG
- CONFIG
- I2C
- MPU
- CALIBRATE_GYRO
- CALIBRATE <dedo> OPEN
- CALIBRATE <dedo> CLOSED
- CALIBRATE_GAP <gap> TOGETHER ou APART
- GAPS
- WINDOW

Nomes dos dedos: polegar, indicador, medio, anelar, minimo.

Nomes dos gaps: indicador_medio, medio_anelar, anelar_minimo. O gap medio_anelar fica desativado enquanto o sensor no GPIO32 estiver quebrado.

## Integracao com o classificador

A IA estatica utiliza somente os cinco valores flexiveis calibrados, na ordem polegar, indicador, medio, anelar, minimo. O firmware envia cada valor na escala 0..1000; o programa Python divide por 1000 para obter [0,1].

Antes de iniciar o programa Python, calibre OPEN e CLOSED de cada dedo pelo Monitor Serial e feche o Monitor. O programa serial envia START e PAUSE automaticamente. Uma amostra so e aceita quando os cinco campos valid estao em 1, os campos stuck estao em 0 e os valores calibrados existem.

O MPU e os tres canais gap continuam no CSV e na matriz 40 x 14, mas nao entram no classificador estatico atual. Essa separacao evita alimentar um modelo de cinco entradas com um vetor de quatorze features.

## Diagnostico

O ultimo teste fisico confirmou leituras variaveis do MPU-6050 ao inclinar o modulo. Os sensores flexiveis ainda precisam fornecer leituras validas e passar pela calibracao. Enquanto isso nao ocorrer, o classificador deve aguardar e nao emitir letras.
