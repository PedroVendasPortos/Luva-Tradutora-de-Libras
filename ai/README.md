# Inteligência Artificial — Luva Tradutora de Libras

Primeira etapa: reconhecimento supervisionado de **configurações estáticas da mão**, começando pelas letras **P, E, D, R e O**.

## Escopo

- Entrada: cinco sensores flexíveis, na ordem polegar, indicador, médio, anelar e mínimo.
- Um exemplo: um vetor de cinco valores calibrados em [0, 1], resumido a partir de uma execução estável.
- Modelo inicial: MLP com camadas densas 16 e 8 (ReLU) e saída Softmax de cinco classes.
- Aprendizado: supervisionado.
- Inferência: rejeição provisória de previsões abaixo de 0,70 de confiança.

A janela temporal 40 × 11 do firmware não é usada nesta primeira MLP estática. Ela poderá ser aproveitada em uma etapa posterior para gestos dinâmicos.

## Preparar o ambiente

Recomendado: Python 3.10 ou 3.11 de 64 bits. O TensorFlow configurado é CPU; não depende de CUDA.

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
pip install -e .
pytest
```

## Dataset

Siga [docs/data-schema.md](docs/data-schema.md) e [docs/data-collection.md](docs/data-collection.md). Cada linha deve representar uma execução independente, não uma leitura consecutiva. Não há dados reais incluídos e os exemplos documentais não servem para treinamento.

## Treinar quando houver dados

```powershell
python -m libras_ai.training data/processed/gestos.csv --output models/experimento_001
```

O pipeline valida o CSV, divide os dados por participante+sessão (sem vazamento entre partições), treina com early stopping, avalia no conjunto de teste e salva `static_mlp.keras` e `metrics.json`. Consulte [docs/training.md](docs/training.md).

Se não houver sessões independentes suficientes ou se alguma partição ficar sem uma das cinco classes, o treinamento é interrompido com uma mensagem clara. Não são geradas métricas artificiais.

## Protótipo experimental por regras

Antes do treinamento da MLP, há um classificador provisório por distância aos perfis P/E/D/R/O para testar o caminho de cinco leituras calibradas até uma letra. Ele não é uma IA treinada e os perfis são hipóteses ajustáveis, não definições completas de Libras. Consulte [docs/rule-prototype.md](docs/rule-prototype.md) para os comandos e limitações.

```powershell
python -m libras_ai.experimental.classifier 0 1 0 0 0
```

## Estado atual

A estrutura e o pipeline de treinamento estão preparados, mas **não existe modelo treinado nem resultado de desempenho** até que sejam coletados dados reais. Exportação para TensorFlow Lite Micro fica para depois da validação do modelo em computador.
