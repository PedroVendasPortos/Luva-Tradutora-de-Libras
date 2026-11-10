# Firmware consolidado — Luva tradutora de Libras

## Estrutura
- `Luva_ESP32.ino`: aquisição dos sensores, calibração, comandos e ciclo de coleta.
- `SensorWindow.h`: janela fixa de 40 amostras × 15 características.
- `ModelRunner.h`: contrato de inferência; atualmente desativado até existir um modelo treinado/exportado.

## Hardware e canais
- Flex: GPIO 34, 35, 32, 33, 25 (polegar → mínimo).
- MPU-6050: SDA 21, SCL 22; endereços 0x68/0x69.
- Sensores magnéticos analógicos: GPIO 36, 39, 27, 26 (entre dedos adjacentes).

Atenção: GPIO 27 e 26 pertencem ao ADC2 no ESP32 clássico. Eles podem ser usados para leitura analógica quando Wi-Fi/Bluetooth não estiver usando o ADC2. Se o projeto precisar de rádio simultaneamente, mova esses sensores para canais ADC1 disponíveis e revise a pinagem.

## Janela do modelo
- Amostragem padrão: 20 Hz (50 ms).
- Janela: 40 amostras consecutivas (2 segundos), sem sobreposição.
- Ordem das 15 características: 5 flex, 3 aceleração, 3 giroscópio, 4 distâncias magnéticas.
- Escala: flex e gaps em 0..1; aceleração em g/2; giroscópio em dps/250.
- Cada linha precisa ser válida. Se uma calibração/leitura estiver ausente, a janela é rejeitada e limpa.
- Ao completar 40 linhas, a janela é copiada em ordem temporal, enviada à interface de inferência e limpa para a próxima janela.

## Estado da IA
O repositório ainda não contém modelo treinado nem arquivo `.tflite`/header de modelo. Portanto, o firmware compila com o adaptador desativado e reporta `MODEL_NOT_READY`; ele não inventa classes ou previsões. Para ativar inferência real, precisamos definir as classes de gestos, treinar/exportar o modelo com entrada compatível [1, 40, 15] e preencher `ModelRunner.h` com TensorFlow Lite Micro e os operadores necessários.

## Calibração
1. Para cada flex: `CALIBRATE <dedo> OPEN` e `CALIBRATE <dedo> CLOSED`.
2. Para cada gap: `CALIBRATE_GAP <nome> TOGETHER` e `CALIBRATE_GAP <nome> APART`.
3. Verifique `STATUS`, `DIAG` e `GAPS`.
4. Inicie com `START`. Cada janela completa gera `WINDOW_COMPLETE`; sem modelo, também gera `MODEL_NOT_READY`.

## Limitações de validação
O código foi organizado a partir do firmware atual, mas ainda precisa ser compilado no Arduino IDE e testado com a luva física. A leitura dos sensores magnéticos depende de os módulos entregarem saída analógica compatível com 0–3,3 V.
