# Firmware de coleta — ESP32

## Arquivos

- `coleta.ino`: aquisição dos sensores flexíveis, MPU-6050, sensores de distância entre dedos, calibração, comandos seriais, diagnóstico e CSV.
- `SensorWindow.h`: buffer circular de 40 linhas × 15 features.

## Por que SensorWindow.h fica nesta pasta?

A classe `SensorWindow` é um componente privado deste sketch. Mantê-la ao lado de `coleta.ino` permite que a Arduino IDE encontre o cabeçalho pelo include local:

```cpp
#include "SensorWindow.h"
```

Não mova esse arquivo para uma pasta arbitrária chamada `libraries` dentro do repositório: a Arduino IDE não trata automaticamente qualquer subpasta como biblioteca instalada. Se o módulo passar a ser reutilizado por outros sketches, podemos empacotá-lo como uma biblioteca Arduino com `library.properties` e diretório `src/`, instalável no sketchbook.

## Ambiente de compilação

- Placa: Node32s / ESP32 clássico, conforme a configuração já usada.
- FQBN anteriormente utilizado: `esp32:esp32:node32s`.
- Core Arduino-ESP32 observado no ambiente de desenvolvimento: 3.3.11.
- Bibliotecas: `Wire` e `Preferences` (incluídas no core).

Abra `coleta.ino` na Arduino IDE. Não abra o arquivo histórico de `TESTS/` como firmware principal.

## Janela para o futuro modelo

- Frequência configurada inicialmente: 20 Hz (50 ms).
- Tamanho: 40 amostras.
- Duração aproximada: 2 segundos.
- Dimensão: 15 features por amostra.
- Ordem exata das features está documentada em `SensorWindow.h` e em `docs/arquitetura-dados.md`.

O firmware só marca uma janela como válida quando todas as features de todas as linhas foram consideradas válidas. Uma janela incompleta ou inválida não deve ser enviada ao modelo.

## Comandos seriais

Envie os comandos pelo Monitor Serial conforme as mensagens `HELP` do firmware. Os comandos implementados incluem coleta, pausa, estado, diagnóstico, configuração, calibração, leitura da janela e varredura I²C.

## Observação sobre os sensores de distância

O código atual contém quatro entradas analógicas adicionais para sensores entre dedos (GPIO 36, 39, 27 e 26). Confirme que esses quatro sensores realmente fazem parte do hardware montado. Se não fizerem, devemos desativar esse bloco e redefinir a dimensão do modelo para 11 features (5 flex + 3 acelerômetro + 3 giroscópio), em vez de treinar um modelo com entradas inexistentes.

## Verificações pendentes

1. Compilar com a placa e a versão do core registradas acima.
2. Confirmar a pinagem e a compatibilidade elétrica de cada sensor.
3. Calibrar cada sensor físico.
4. Verificar a frequência real e a integridade das linhas CSV.
5. Confirmar que a janela atinge 40/40 amostras válidas.
6. Só então exportar e integrar o modelo TensorFlow Lite Micro.
