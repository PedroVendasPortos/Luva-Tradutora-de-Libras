# Luva Tradutora de Libras

Firmware de aquisicao com ESP32 e classificador experimental de gestos estaticos.

## Firmware integrado

Abra `firmware/coleta/coleta.ino` na Arduino IDE. Mantenha `SensorWindow.h` na mesma pasta do sketch.

- 5 sensores flexiveis para curvatura dos dedos.
- MPU-6050 por I2C para aceleracao e giroscopio.
- 3 canais analogicos de abertura entre dedos (um temporariamente desativado por sensor quebrado).
- Amostragem nominal de 20 Hz.
- CSV serial, calibracao persistente e matriz temporal circular de 40 x 14.

Placa-alvo: ESP32 Node32s (`esp32:esp32:node32s`), Arduino-ESP32 3.3.11.

## IA experimental integrada

A primeira classificacao e estatica: P, E, D, R e O. O classificador atual compara a mediana de um bloco de leituras dos cinco flexiveis com perfis experimentais. Ele roda no computador, recebendo o CSV da ESP32 pela USB.

**Isto ainda nao e uma rede neural treinada.** Os perfis sao aproximacoes de bancada e nao comprovam reconhecimento correto de Libras. O pipeline de MLP/TensorFlow e uma etapa posterior, dependente de dados reais.

## Preparar e executar

1. Calibre cada flexivel no Monitor Serial, registrando OPEN e CLOSED para cada dedo.
2. Feche o Monitor Serial.
3. Instale as dependencias: `python -m pip install -r tools/requirements-serial-ai.txt`.
4. Execute: `python tools/serial_ai_test.py COM4`.

O programa envia START, valida as colunas de cada sensor, descarta leituras invalidas/travadas e classifica a mediana de 20 amostras validas. Ctrl+C envia PAUSE.

A matriz 40 x 14 continua sendo registrada no firmware para a futura etapa de gestos dinamicos. O classificador estatico usa apenas os cinco flexiveis, conforme o contrato da IA atual.

## Situacao do hardware

O MPU-6050 ja apresentou leituras variaveis ao inclinar o modulo. As leituras dos flexiveis ainda precisam ser corrigidas/calibradas antes de a classificacao ao vivo produzir resultados. Leituras ausentes nunca sao substituidas por zeros para forcar uma previsao.
