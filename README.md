# Luva Tradutora de Libras

Projeto experimental de aquisição de movimentos da mão com ESP32 e preparação de dados para reconhecimento de gestos.

## Estrutura

```text
firmware/coleta/
  coleta.ino             Firmware principal do ESP32
  SensorWindow.h         Buffer circular 40 × 11
  README.md              Pinagem, comandos e validação

docs/
  arquitetura-dados.md   Contrato de features e fluxo de dados
  prompts/               Especificações de desenvolvimento
  ideas/                 Rascunhos de etapas futuras

tests/
  test_sensor_window.cpp Testes automatizados do buffer

archive/legacy-tests/    Versões antigas preservadas para consulta
tools/                   Pequenos utilitários de desenvolvimento
```

## Firmware oficial

Abra `firmware/coleta/coleta.ino` na Arduino IDE. O cabeçalho `SensorWindow.h` deve permanecer na mesma pasta do sketch.

Esta etapa trabalha com **5 sensores flexíveis + MPU-6050 (acelerômetro e giroscópio)**, totalizando 11 features por amostra. Os sensores magnéticos/Hall entre os dedos ficam fora desta versão-base e serão integrados depois.

## Testes

Os testes automatizados do buffer podem ser executados localmente:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Itests/stubs tests/test_sensor_window.cpp -o test_sensor_window
./test_sensor_window
```

A compilação do firmware para `esp32:esp32:node32s` é verificada no GitHub Actions. Isso verifica a compilação, não substitui testes elétricos com a placa e os sensores físicos.

Consulte [firmware/coleta/README.md](firmware/coleta/README.md) e [docs/arquitetura-dados.md](docs/arquitetura-dados.md).
