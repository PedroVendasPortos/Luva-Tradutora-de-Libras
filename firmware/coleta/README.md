# Firmware de coleta — ESP32

## Hardware e pinos

| Componente | GPIO |
|---|---:|
| Flexivel — polegar | 33 |
| Flexivel — indicador | 34 |
| Flexivel — medio | 14 |
| Flexivel — anelar | 12 |
| Flexivel — minimo | 25 |
| MPU-6050 SDA | 21 |
| MPU-6050 SCL | 22 |
| Gap indicador-medio | 35 |
| Gap medio-anelar | 32 (desativado: sensor quebrado) |
| Gap anelar-minimo | 13 |

Os GPIOs 12, 13, 14 e 25 pertencem ao ADC2 no ESP32 classico; nao use Wi-Fi durante leituras analogicas. GPIO12 e pino de strapping e pode impedir o boot se o circuito o mantiver em nivel alto durante a inicializacao. GPIOs nao toleram 5 V.

## Configuracao

- Placa: Node32s / ESP32 classico.
- FQBN: esp32:esp32:node32s.
- Core Arduino-ESP32: 3.3.11.
- Serial: 115200 baud.
- Frequencia nominal: 20 Hz (50 ms).
- MPU-6050: acelerometro +/-2 g e giroscopio +/-250 graus/s.
- Janela: 40 x 14, aproximadamente 2 segundos.

## Comandos seriais

- HELP
- START / PAUSE / STOP
- STATUS ou DIAG
- CONFIG
- I2C
- MPU
- CALIBRATE_GYRO
- CALIBRATE <dedo> OPEN
- CALIBRATE <dedo> CLOSED
- CALIBRATE_GAP <gap> TOGETHER ou APART
- GAPS
- WINDOW
- SCAN (diagnostico de fiacao: le todos os GPIOs analogicos candidatos)
- SELFTEST (min/max/ruido dos 8 canais; separa codigo de hardware)
- DEMO (liga a demo: 7 gestos -> "OI SOU FLAVI")
- GO (na demo: captura 20 amostras e imprime o gesto)
- ALVO (ao vivo: angulo de cada gesto, para acertar a pose)
- GESTOS (lista os 7 alvos medidos e o conflito entre eles)
- ANELAR (mede o curso real do flexivel do anelar por 10 s)

Nomes dos dedos: polegar, indicador, medio, anelar, minimo.

Nomes dos gaps: indicador_medio, medio_anelar, anelar_minimo. O gap medio_anelar fica desativado enquanto o sensor no GPIO32 estiver quebrado.

Os comandos **nao diferenciam maiusculas de minusculas** (`START`, `start`,
`Start`). Durante a coleta, apenas `PAUSE` e `STOP` sao aceitos; os demais
comandos respondem com um aviso explicito em vez de serem ignorados em
silencio.

## Demo: 7 gestos -> "OI SOU FLAVI"

Sete gestos ficticios devolvem pedacos de texto que, em sequencia, formam
**OI SOU FLAVI**. A decisao usa **somente a direcao da gravidade** do MPU;
os flexiveis e os magnetos nao participam.

### Alvos medidos (2ª coleta, logs de 27/11)

| # | Gesto | Amostras | Vetor medido (x, y, z) |
|---|---|---|---|
| 0 | **OI** | 46 | `(+0.5890, -0.6358, +0.4989)` |
| 1 | **SOU** | 43 | `(+0.5485, -0.1388, +0.8246)` |
| 2 | **F** | 36 | `(+0.3736, -0.6168, +0.6927)` |
| 3 | **L** | 35 + 43 | `(+0.3452, -0.6540, +0.6731)` |
| 4 | **A** | 39 | `(+0.3264, -0.4491, +0.8317)` |
| 5 | **V** | 34 | `(+0.0093, -0.6812, +0.7321)` |
| 6 | **I** | 37 | `(+0.1822, -0.7801, +0.5986)` |

Cada vetor é a direção média da gravidade, normalizada, das amostras com
`0,7 g ≤ |a| ≤ 1,3 g` e a menos de 20° da média (duas passadas).

O **L** é a média ponderada de duas coletas (35 + 43 amostras). Elas ficaram a
**9,0°** uma da outra — essa é a repetibilidade do próprio gesto: repetir o L
varia ~9°. Vale ter isso em mente ao ler a tabela abaixo.

### Conflito medido

| Gesto | Vizinho mais próximo | Separação | Janela real |
|---|---|---|---|
| OI | F | 16,7° | 3,4° |
| SOU | A | 22,0° | 6,0° |
| **F** | **L** | **3,0°** | **−3,5°** |
| **L** | **F** | **3,0°** | **−3,5°** |
| A | F | 12,8° | 1,4° |
| V | I | 13,8° | 1,9° |
| I | L | 12,6° | 1,3° |

Janela = `(separação − 10)/2`. **Nenhum gesto tem janela confortável**: F e L
ficam a 3,0° e os demais abaixo de 2°, então `GO` vai recusar quase tudo. O
melhor caso é SOU, com 6° — possível, mas apertado.

O dado decisivo: **F e L estão a 3,0°, mas repetir o mesmo L varia 9,0°**. A
distância entre os dois gestos é **3× menor** que o erro de repetir um deles —
nenhum limiar separa isso sem chutar.

Motivo: os sete vetores ficaram na **mesma região** da esfera (todos com
`z` entre +0,50 e +0,83 e `y` negativo). A mão não girou entre os gestos —
provavelmente porque os gestos foram feitos com os **dedos**, mantendo o
punho na mesma posição. A orientação da mão é o único sinal que sobrou nesta
luva, e ela quase não mudou.

O firmware **avisa** em `GESTOS` e em `DEMO` (pior par, janela efetiva e
vizinho de cada gesto) e recusa em vez de chutar letra.

### O que fazer para os 7 funcionarem

Cada gesto precisa **girar o punho** de forma distinta — como as poses
provisórias antigas (palma para baixo/cima, dedos para baixo/cima, os dois
lados, diagonal), que ficavam a 54–90° entre si e que você já alcançou com o
`ALVO`. Com elas a janela volta a ~22° e os sete passam.

Regra prática: entre dois gestos, o `ALVO` tem que mostrar **pelo menos 50°
de diferença**. Abaixo de ~30° não há como separar com folga.

### Procedimento

```
GESTOS   (lista os alvos, o pior par e o vizinho de cada gesto)
ALVO     (ao vivo: gire a mao e veja o angulo de cada gesto cair)
DEMO     (liga)
GO       (mantenha a pose: 20 amostras, ~1 s -> GESTO=...)
stop     (encerra e imprime SEQUENCIA=...)
```

`ALVO` imprime cinco linhas por segundo:

```
t_ms,OI,SOU,F,L,A,V,I,gx,gy,gz,melhor,status
2450,   4*,  92,  89,  95,  91,  88,  49,0.02,-0.03,1.00,OI,-
```

O `status` só vira `OK` quando o ângulo é ≤ 30° **e** há 10° de folga sobre o
segundo colocado.

### Como o texto e montado

- Gesto de duas letras ou mais (`OI`, `SOU`) é palavra: separado por espaço.
- Gesto de uma letra (`F`, `L`, `A`, `V`, `I`) cola na letra anterior.
- `OI` + `SOU` + `F` + `L` + `A` + `V` + `I` = **`OI SOU FLAVI`**.

### Como a decisao e tomada

- Amostras com |aceleracao| fora de 0,7..1,3 g são descartadas (mão em
  movimento). Precisam sobrar 20 amostras boas em até 6 s.
- Direção = média das amostras boas, normalizada (só a direção importa).
- Ângulo = `acos` entre essa direção e cada um dos sete alvos.
- Aceita se o menor ângulo for ≤ 30° **e** houver 10° de folga sobre o
  segundo colocado.

## Quando as leituras flex nao fazem sentido

O procedimento completo, com criterio de decisao entre **erro de codigo** e
**erro de hardware**, esta em [`DIAGNOSTICO.md`](DIAGNOSTICO.md). Resumo:

1. `SCAN` — dobre um dedo de cada vez e veja em **qual GPIO** o valor muda.
   Se mudar em outro GPIO, o mapa `FLEX_PINS` esta errado (codigo).
2. `SELFTEST` — rode com os dedos estendidos e depois dobrados. Se `min/max`
   nao mudarem, o sensor nao chega naquele GPIO (hardware).
3. `STATUS` — cada dedo mostra `valid`, `stuck`, `sat`, `ruido`,
   `cal=OPEN..CLOSED` e um `estado` que explica a recusa.
4. `CALIBRATE ...` — a mensagem de erro agora diz se o canal esta colado no
   trilho, instavel ou indisponivel.

## Integracao com o classificador

A IA estatica utiliza somente os cinco valores flexiveis calibrados, na ordem polegar, indicador, medio, anelar, minimo. O firmware envia cada valor na escala 0..1000; o programa Python divide por 1000 para obter [0,1].

Antes de iniciar o programa Python, calibre OPEN e CLOSED de cada dedo pelo Monitor Serial e feche o Monitor. O programa serial envia START e PAUSE automaticamente. Uma amostra so e aceita quando os cinco campos valid estao em 1, os campos stuck estao em 0 e os valores calibrados existem.

O MPU e os tres canais gap continuam no CSV e na matriz 40 x 14, mas nao entram no classificador estatico atual. Essa separacao evita alimentar um modelo de cinco entradas com um vetor de quatorze features.

## Diagnostico

O ultimo teste fisico confirmou leituras variaveis do MPU-6050 ao inclinar o modulo. As leituras dos flexiveis ainda nao foram validadas: os comandos `SCAN` e `SELFTEST` existem para isso, e o [`DIAGNOSTICO.md`](DIAGNOSTICO.md) descreve como interpretar o resultado.

Enquanto os cinco flexiveis nao estiverem calibrados e com `valid=1`, o classificador deve aguardar e nao emitir letras. Leituras ausentes nunca sao substituidas por zeros para forcar uma previsao.

A aquisicao dos 7 canais custa cerca de 10-12 ms por amostra (`acquisition_us`
no `STATUS`), portanto 20 Hz (50 ms) e folgado e valores abaixo de ~12 ms em
`CONFIG SAMPLE_MS` nao sao cumpriveis - o firmware avisa.
