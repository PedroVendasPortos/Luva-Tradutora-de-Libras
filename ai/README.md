# Inteligência Artificial — Luva Tradutora de Libras

Primeira etapa: reconhecimento supervisionado de **configurações estáticas da mão**, começando pelas letras do nome **P, E, D, R, O**.

## Escopo da versão inicial

- Entrada: cinco sensores flexíveis, na ordem polegar, indicador, médio, anelar e mínimo.
- Representação: um vetor de cinco valores por execução de gesto.
- Escala esperada: valores calibrados entre 0.0 e 1.0.
- Modelo inicial: MLP (Keras), com duas camadas densas ocultas.
- Saída: cinco classes (P, E, D, R, O).
- Aprendizado: supervisionado.

O firmware atual também mantém uma janela temporal 40 × 11 com os cinco sensores flexíveis e o MPU-6050. **Essa janela não será a entrada da primeira MLP estática.** O contrato temporal permanece documentado no firmware para uma etapa futura de gestos dinâmicos.

## Estrutura

```text
ai/
├── config/classes.json
├── data/
│   ├── raw/
│   └── processed/
├── docs/
├── models/                 # artefatos treinados (não versionar)
├── notebooks/              # experimentos exploratórios
├── src/libras_ai/
│   ├── schema.py
│   ├── preprocessing.py
│   ├── model.py
│   ├── training.py
│   └── inference.py
├── tests/
├── requirements.txt
└── pyproject.toml
```

## Ambiente

Recomendado: Python 3.10 ou 3.11 de 64 bits. O ambiente inicial usa TensorFlow CPU; não depende de CUDA ou da GPU NVIDIA.

No Windows, a partir desta pasta:

```powershell
py -3.11 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
pip install -e .
pytest
```

## Dataset

O esquema CSV está em [docs/data-schema.md](docs/data-schema.md). Não há amostras reais incluídas ainda. Não treine nem avalie o modelo com dados sintéticos como se fossem medições da luva.

## Próximas implementações

1. Implementar a coleta/ingestão de execuções rotuladas.
2. Implementar divisão de treino/validação/teste sem vazamento entre sessões.
3. Treinar a MLP e estabelecer uma linha de base.
4. Avaliar matriz de confusão e rejeição de baixa confiança.
5. Só depois estudar quantização e exportação para TensorFlow Lite Micro.

Consulte [docs/architecture.md](docs/architecture.md) e [docs/data-collection.md](docs/data-collection.md).
