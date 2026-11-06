# Luva Tradutora de Libras

Projeto experimental de aquisição de movimentos da mão com ESP32 e preparação de dados para reconhecimento de gestos.

## Estrutura atual

```text
firmware/
└── coleta/
    ├── coleta.ino       # Firmware principal: sensores, calibração, serial e diagnóstico
    ├── SensorWindow.h   # Buffer circular das últimas 40 amostras (15 features)
    └── README.md        # Como abrir, compilar e testar

docs/
└── arquitetura-dados.md # Contrato de dados entre sensores, firmware e futuro modelo

PROMPTS/                 # Instruções e especificações de desenvolvimento
TESTS/                   # Códigos históricos de teste; não são o firmware oficial
SensoresWindow/          # Arquivos antigos mantidos durante a migração
SOM                      # Rascunho separado sobre saída de áudio
```

A implementação oficial do firmware passa a ser `firmware/coleta/coleta.ino`. Os arquivos em `TESTS/` e `SensoresWindow/` são históricos e não devem ser usados como fonte de compilação.

## Firmware de coleta

Abra diretamente `firmware/coleta/coleta.ino` na Arduino IDE. O arquivo `SensorWindow.h` fica na mesma pasta do sketch e é incluído com:

```cpp
#include "SensorWindow.h"
```

Essa escolha é intencional: `SensorWindow` é um módulo interno deste firmware, não uma biblioteca externa que precise ser instalada no diretório global de bibliotecas da Arduino IDE.

Placa-alvo usada no desenvolvimento: ESP32 Dev Module / Node32s (FQBN anteriormente utilizado: `esp32:esp32:node32s`). Dependências `Wire` e `Preferences` fazem parte do core Arduino-ESP32.

Consulte [firmware/coleta/README.md](firmware/coleta/README.md) antes de compilar ou conectar o hardware.

## Estado de validação

A organização e a consistência entre o firmware e o cabeçalho da janela foram revisadas estaticamente. Ainda é necessário compilar no ambiente local e testar com os sensores físicos. Nenhuma medição de hardware é presumida.
