# Arquitetura inicial da IA

## Objetivo

Classificar configurações estáticas da mão correspondentes às letras P, E, D, R e O. O primeiro experimento utiliza somente os cinco sensores flexíveis.

## Entrada

Um exemplo representa uma execução de um gesto. Cada exemplo contém cinco features, sempre nesta ordem:

1. `flex_thumb` — polegar
2. `flex_index` — indicador
3. `flex_middle` — médio
4. `flex_ring` — anelar
5. `flex_pinky` — mínimo

Os valores devem ser calibrados e convertidos para o intervalo [0, 1]. O firmware atualmente calcula uma referência calibrada de 0 a 1000; a conversão para o dataset é dividir por 1000.

A primeira versão deve usar uma agregação estável de várias leituras durante a posição mantida (por exemplo, mediana). Não usar as 40 linhas temporais como 40 exemplos independentes: isso causaria pseudorreplicação e vazamento entre treino e teste.

## Modelo de referência

MLP configurável:

- entrada: 5 valores;
- camada densa: 16 neurônios, ReLU;
- camada densa: 8 neurônios, ReLU;
- saída: número de classes, Softmax;
- loss: sparse categorical cross-entropy;
- otimizador: Adam.

Essa é uma linha de base experimental, não uma arquitetura final comprovada. Regularização, número de neurônios e limiar de confiança serão definidos com dados reais.

## Limitações conhecidas

Sensores flexíveis medem curvatura, não orientação nem contato entre dedos. Letras com configurações de curvatura semelhantes podem ser ambíguas. Se os resultados mostrarem confusões sistemáticas, considerar adicionar informação do MPU-6050 ou sensores magnéticos em uma versão posterior.

## Avaliação

- Separar execuções de treino, validação e teste por sessão e, quando possível, por participante.
- Nunca dividir aleatoriamente linhas de uma mesma execução entre conjuntos.
- Relatar acurácia, precision, recall, F1 e matriz de confusão.
- Manter conjunto de teste separado até a avaliação final.
- Incluir rejeição de baixa confiança; não forçar uma letra quando o modelo estiver incerto.

## Evolução para gestos dinâmicos

O firmware possui contrato temporal 40 × 11 (5 flex + 6 eixos do MPU). Ele permanece fora da entrada desta MLP estática. Uma futura arquitetura temporal deverá ter pipeline, dataset e avaliação próprios.
