# Arquitetura do fluxo de dados

## Pipeline

```text
5 sensores flexíveis + MPU-6050
          ↓
ESP32 — aquisição e validação
          ↓
Calibração dos sensores flexíveis
          ↓
SensorWindow (buffer circular)
          ↓
Janela cronológica: 40 × 11
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

A ordem e a normalização não podem mudar entre coleta, conjunto de treinamento e inferência. Qualquer mudança exige atualizar firmware, pipeline de treinamento e entrada do modelo em conjunto.

## Dimensões

- Frequência nominal: 20 amostras/s (intervalo de 50 ms).
- Janela: 40 passos temporais.
- Duração nominal: 2 segundos.
- Features por passo: 11.
- Tensor conceitual: `[1, 40, 11]` (lote, tempo, features), sujeito à arquitetura escolhida.

## Validade

Uma linha só é válida quando os cinco sensores flexíveis estão calibrados, suas leituras são válidas e não estão sinalizadas como travadas, e os seis eixos do MPU foram lidos com sucesso.

A janela só fica apta para inferência quando estiver cheia e todas as 40 linhas forem válidas. Leituras não finitas são substituídas por zero para evitar contaminar o buffer, mas também invalidam a linha.

Não substitua silenciosamente sensores ausentes por zeros e classifique a janela como válida. Zeros de preenchimento não representam medições reais.

## Fora do escopo desta etapa

Sensores magnéticos/Hall entre os dedos serão integrados em uma etapa posterior. Quando forem adicionados, a dimensão da janela, o contrato de features e o conjunto de treinamento deverão ser atualizados juntos.
