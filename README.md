# Luva Tradutora de Libras

Projeto experimental de aquisição de movimentos da mão com ESP32 e preparação de dados para reconhecimento de gestos.

## Estrutura

```text
firmware/coleta/
  coleta.ino             Firmware principal do ESP32
  SensorWindow.h         Buffer circular 40 × 11
  README.md              Pinagem, comandos e validação

ai/
  src/libras_ai/          Pipeline Python para reconhecimento de gestos estáticos
  data/                   Dados brutos e processados (não versionados)
  tests/                  Testes automatizados da IA
  docs/                   Arquitetura, esquema e protocolo de coleta

docs/
  arquitetura-dados.md   Contrato temporal de features do firmware
  prompts/               Especificações de desenvolvimento
  ideas/                 Rascunhos de etapas futuras

tests/
  test_sensor_window.cpp Testes automatizados do buffer

archive/legacy-tests/    Versões antigas preservadas para consulta
tools/                   Pequenos utilitários de desenvolvimento
.github/workflows/       CI do firmware e da IA
```

## Firmware

Abra `firmware/coleta/coleta.ino` na Arduino IDE. O cabeçalho `SensorWindow.h` deve permanecer na mesma pasta do sketch.

A versão atual trabalha com **5 sensores flexíveis + MPU-6050**, totalizando 11 features por amostra e uma janela temporal 40 × 11. Os sensores magnéticos/Hall entre os dedos ficam para uma etapa posterior.

## Inteligência artificial

A primeira etapa da IA reconhece **gestos estáticos** correspondentes às letras P, E, D, R e O, usando inicialmente apenas os cinco sensores flexíveis e uma MLP em Python/TensorFlow.

A entrada da primeira MLP é um vetor de cinco valores calibrados (0 a 1) por execução de gesto. A janela temporal 40 × 11 do firmware fica reservada para uma futura etapa de gestos dinâmicos.

Consulte [ai/README.md](ai/README.md) para configurar o ambiente, entender o dataset e executar os testes.

## Testes do firmware

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Itests/stubs -Ifirmware/coleta tests/test_sensor_window.cpp -o test_sensor_window
./test_sensor_window
```

A compilação do firmware para `esp32:esp32:node32s` é verificada no GitHub Actions. Isso não substitui testes elétricos com a placa e os sensores físicos.
