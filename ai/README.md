# Classificador experimental da luva

## Entrada

Cinco valores calibrados entre 0 e 1, na ordem polegar, indicador, medio, anelar e minimo. O firmware produz valores calibrados entre 0 e 1000; a ponte serial faz a normalizacao.

## Protótipo atual

O classificador compara a distancia RMSE entre a leitura e perfis provisórios das letras P, E, D, R e O. Leituras muito distantes ou ambiguas sao rejeitadas.

Este modulo e um prototipo por regras, nao uma rede neural treinada. Os perfis nao representam todas as caracteristicas da Libras: por exemplo, cruzamento dos dedos e orientacao da mao nao sao medidos pelos cinco flexiveis.

## Teste com hardware

Na raiz do repositorio:

```powershell
python -m pip install -r tools/requirements-serial-ai.txt
python tools/serial_ai_test.py COM4
```

Calibre previamente todos os dedos com OPEN e CLOSED pelo Monitor Serial da Arduino IDE e feche o Monitor antes de iniciar a ponte.

O programa acumula 20 amostras consecutivas validas, calcula a mediana de cada dedo e classifica o vetor. Qualquer leitura invalida ou travada reinicia o bloco.

## Testes matematicos

```powershell
python -m pip install numpy pytest
$env:PYTHONPATH = "ai/src"
pytest ai/tests/test_rule_classifier.py -q
```

Os testes automatizados verificam o comportamento do classificador com perfis artificiais; nao validam sensores fisicos nem desempenho real.
