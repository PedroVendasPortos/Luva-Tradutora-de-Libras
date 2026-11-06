# Firmware de coleta — ESP32

## Arquivos

- `coleta.ino`: aquisição dos cinco sensores flexíveis e do MPU-6050, calibração, comandos seriais, diagnóstico e CSV.
- `SensorWindow.h`: buffer circular de 40 linhas × 11 features.

## Organização

`SensorWindow.h` fica ao lado de `coleta.ino` porque é um módulo interno deste sketch, incluído por caminho local. Não é necessário instalar uma biblioteca Arduino externa.

## Hardware-base desta etapa

| Componente | ESP32 | Observação |
|---|---|---|
| Flexível — polegar | GPIO 34 | Entrada ADC |
| Flexível — indicador | GPIO 35 | Entrada ADC |
| Flexível — médio | GPIO 32 | Entrada ADC |
| Flexível — anelar | GPIO 33 | Entrada ADC |
| Flexível — mínimo | GPIO 25 | Entrada ADC |
| MPU-6050 SDA | GPIO 21 | I²C |
| MPU-6050 SCL | GPIO 22 | I²C |

Confirme a placa exata, alimentação, circuito divisor dos sensores flexíveis e tensão de saída antes de conectar. Os GPIOs do ESP32 não são tolerantes a 5 V.

## Ambiente de compilação

- Placa: Node32s / ESP32 clássico.
- FQBN: `esp32:esp32:node32s`.
- Core Arduino-ESP32: 3.3.11.
- Dependências: `Wire` e `Preferences`, fornecidas pelo core.

Abra `coleta.ino` na Arduino IDE. Não compile os arquivos históricos de `archive/legacy-tests/`.

## Janela para o futuro modelo

- Frequência configurada: 20 Hz (50 ms).
- Tamanho: 40 amostras.
- Duração nominal: 2 segundos.
- Dimensão: 11 features por amostra.
- Ordem: 5 flex, aceleração X/Y/Z, giroscópio X/Y/Z.
- Tensor conceitual: `[1, 40, 11]`.

Uma janela só é válida quando estiver cheia e todas as linhas forem válidas. Leituras inválidas não devem ser usadas para inferência.

## Comandos seriais

Monitor Serial em 115200 baud:

- `HELP`: lista comandos.
- `START`: inicia uma sessão CSV e zera a janela.
- `PAUSE` ou `STOP`: encerra a transmissão.
- `STATUS` ou `DIAG`: estado e diagnóstico.
- `CONFIG`: configuração atual.
- `CONFIG SAMPLE_MS <1..1000>`: altera o intervalo.
- `CALIBRATE <dedo> OPEN` / `CLOSED`: registra referências.
- `I2C`: varre o barramento.
- `MPU`: tenta inicializar o MPU.
- `WINDOW`: imprime a janela; pause a coleta antes.

A calibração deve ser feita com a luva e os sensores montados. Não reutilize números de calibração de outro sensor ou outra montagem.

## Validações pendentes no hardware

1. Compilar para a placa configurada.
2. Confirmar as leituras brutas dos cinco ADCs.
3. Verificar a comunicação I²C e os seis eixos do MPU.
4. Calibrar cada sensor flexível.
5. Medir frequência real, atrasos e integridade do CSV.
6. Confirmar janela 40/40 válida e ordem cronológica.
7. Só então integrar o modelo TensorFlow Lite Micro.
