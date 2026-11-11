# Treinamento e avaliação

O pipeline não treina automaticamente ao instalar o projeto. É necessário fornecer um CSV com medições reais, conforme [data-schema.md](data-schema.md).

## Executar

Na pasta `ai/`, com o ambiente virtual ativado e o pacote instalado:

```powershell
python -m libras_ai.training data/processed/gestos.csv --output models/experimento_001
```

Opções disponíveis: `--epochs`, `--batch-size`, `--seed`, `--test-size` e `--validation-size`.

## Divisão dos dados

A separação é feita por grupo `participant_id + session_id`, nunca por linhas aleatórias. O programa tenta encontrar uma divisão em que as cinco classes apareçam em treino, validação e teste. Se não houver sessões suficientes ou uma distribuição adequada, interrompe com uma mensagem explicativa em vez de produzir métricas enganosas.

Como consequência, cada sessão usada precisa conter execuções das cinco letras. Para avaliar generalização entre pessoas, o protocolo deve evoluir para reservar participantes inteiros para teste.

## Saídas

- `static_mlp.keras`: modelo treinado.
- `metrics.json`: métricas do conjunto de teste, matriz de confusão, contagens por classe e grupos de cada partição.

O limiar de confiança de 0,70 continua marcado como provisório. A acurácia e as métricas de um único conjunto não garantem desempenho fora das condições de coleta.

**Não use dados sintéticos ou linhas ilustrativas da documentação como dataset de treinamento.**
