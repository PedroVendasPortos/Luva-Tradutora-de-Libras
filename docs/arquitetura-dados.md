# Arquitetura do fluxo de dados

## Pipeline

```text
Sensores físicos
    ↓
ESP32 — leitura e validação
    ↓
Calibração e normalização
    ↓
SensorWindow (buffer circular)
    ↓
Janela cronológica: 40 × 15
    ↓
Futuro modelo TensorFlow Lite Micro
    ↓
Classe/gesto reconhecido (etapa futura)
```

## Contrato de features

| Índices | Conteúdo | Unidade/escala |
|---|---|---|
| 0–4 | Flexão: polegar, indicador, médio, anelar, mínimo | 0 a 1 após calibração |
| 5–7 | Acelerômetro X, Y, Z | g / 2 |
| 8–10 | Giroscópio X, Y, Z | graus/s / 250 |
| 11–14 | Abertura entre dedos adjacentes | 0 a 1 após calibração |

A ordem e a normalização não podem mudar entre coleta, conjunto de treinamento e inferência. Qualquer mudança exige atualizar o firmware, o pipeline de treinamento e a forma de entrada do modelo em conjunto.

## Dimensões

- Frequência nominal: 20 amostras/s.
- Janela: 40 passos temporais.
- Duração nominal: 2 segundos.
- Features por passo: 15.
- Tensor conceitual: `[1, 40, 15]` (lote, tempo, features), sujeito à arquitetura escolhida para o modelo.

## Validade

Uma linha é inválida se faltar calibração ou leitura válida de qualquer sensor exigido, se um flex sensor estiver sinalizado como travado ou se a leitura do MPU falhar. A janela só fica apta para inferência quando estiver cheia e todas as linhas forem válidas.

Não substitua silenciosamente sensores ausentes por zeros e classifique a janela como válida. Zeros usados como preenchimento não representam uma medição real.
