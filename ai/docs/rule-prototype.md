# Protótipo experimental por regras — P, E, D, R e O

Este módulo é uma etapa de bancada para verificar o fluxo de cinco leituras calibradas até uma letra. **Não é uma IA treinada e não valida, por si só, o reconhecimento de Libras.**

## Entrada

Um vetor com cinco valores em `[0, 1]`, na ordem `polegar, indicador, médio, anelar, mínimo`. Neste protótipo, `0` representa dedo estendido e `1` dedo dobrado. Os valores reais devem vir da calibração dos sensores da luva; não use os exemplos abaixo como medições reais.

## Perfis provisórios

Os perfis estão em `src/libras_ai/experimental/gestures.py` e podem ser ajustados sem alterar o classificador:

| Letra | Polegar | Indicador | Médio | Anelar | Mínimo |
|---|---:|---:|---:|---:|---:|
| P | 0 | 1 | 0 | 0 | 0 |
| E | 1 | 1 | 1 | 1 | 1 |
| D | 1 | 0 | 1 | 1 | 1 |
| R | 0 | 0 | 0 | 0 | 0 |
| O | 1 | 0,5 | 0,5 | 0,5 | 0,5 |

São hipóteses simplificadas para teste de software, não definições anatômicas completas. Em particular, cruzamento dos dedos em R, orientação da mão e contato entre dedos não são observados pelos cinco sensores flexíveis. Se dois gestos produzirem padrões de curvatura semelhantes, será necessário adicionar sensores ou rever o escopo.

## Como executar

Na pasta `ai/`, com o ambiente virtual ativado e o pacote instalado (`pip install -e .`):

```powershell
python -m libras_ai.experimental.classifier 0 1 0 0 0
python -m libras_ai.experimental.classifier 1 1 1 1 1
```

O resultado apresenta a letra candidata, a distância ao perfil mais próximo e a segunda menor distância. O classificador rejeita leituras muito distantes de todos os perfis ou com pouca separação entre as duas melhores opções. Os limites `MAX_DISTANCE` e `MIN_MARGIN` são provisórios e devem ser ajustados após observar leituras reais.

## Testes automatizados

```powershell
pytest tests/test_rule_classifier.py -q
```

Os testes usam apenas vetores de referência definidos no código para verificar o comportamento matemático do classificador. Eles **não** são dados de treinamento, não simulam desempenho real e não substituem a validação com a luva física.

## Teste com a luva física

1. Calibre os cinco sensores usando o procedimento do firmware.
2. Faça uma pose de cada vez, mantendo a mão direita estável.
3. Leia os cinco valores calibrados na ordem oficial.
4. Execute o comando com os valores medidos, sem arredondar além do necessário.
5. Registre pose pretendida, vetor medido, resultado e se a leitura foi estável.
6. Repita em execuções separadas. Não conclua que uma letra funciona por causa de uma única leitura.

Quando o firmware disponibilizar uma linha serial estável com os cinco valores calibrados, podemos criar uma ponte serial para classificar diretamente o fluxo, sem digitar os valores manualmente.
