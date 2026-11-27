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
- GESTOS (lista os 7 alvos medidos e a separacao entre eles)
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

### Alvos medidos (logs de 27/11)

| # | Gesto | Amostras | Vetor medido (x, y, z) | Eixo |
|---|---|---|---|---|
| 0 | **OI** | 40 | `(+0.4294, -0.4190, +0.8000)` | +z |
| 1 | **SOU** | 47 | `(+0.9091, -0.0197, +0.4161)` | +x |
| 2 | **F** | 50 | `(+0.2807, -0.6625, +0.6945)` | +z |
| 3 | **L** | 47 | `(+0.2161, -0.5207, +0.8260)` | +z |
| 4 | **A** | 41 | `(+0.3805, -0.3860, +0.8404)` | +z |
| 5 | **V** | 59 | `(+0.1905, -0.6139, +0.7660)` | +z |
| 6 | **I** | 39 | `(+0.3553, -0.7601, +0.5441)` | +z |

Cada vetor e a direcao media da gravidade, normalizada, das amostras com
`0,7 g <= |a| <= 1,3 g` e a menos de 20 graus da media (duas passadas).

### Conflito medido (importante)

Os sete logs têm **praticamente a mesma orientação de mão**:

| Par mais próximo | Ângulo |
|---|---|
| OI × A | **4,1°** |
| L × V | 6,5° |
| F × V | 7,2° |
| F × I | 11,1° |
| OI × L | 13,7° |

Menor separação: **4,1°**. Com folga de 10°, a janela real de aceitação fica
em `(4,1 − 10)/2 = −3°`, ou seja, **nenhuma**. Na prática:

- **SOU** é o único separável (o vizinho mais próximo está a 43°) e é
  reconhecido normalmente;
- os outros seis caem sempre em `NAO_RECONHECIDO`, porque o segundo colocado
  fica a menos de 10° do primeiro.

O firmware **avisa** isso em `GESTOS` e em `DEMO` (`janela efetiva=...`) e
recusa em vez de chutar uma letra.

Isso não é falha de sensor nem de código: `F`, `L`, `A`, `V`, `I` são
**configurações de dedos**, feitas com a mão na mesma orientação — e os
flexíveis estão mortos. Se o caminho for "gesto = orientação da mão", cada
pose precisa **girar o pulso** de forma distinta (como os alvos provisórios
anteriores, que ficavam a 54–90° entre si). Se o caminho for "gesto = dedos",
os flexíveis precisam voltar.

### Procedimento

```
GESTOS   (lista os 7 alvos e a janela efetiva)
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

O `status` só vira `OK` quando o ângulo é <= 30° **e** há 10° de folga sobre o
segundo colocado.

### Como o texto e montado

- Gesto de duas letras ou mais (`OI`, `SOU`) é palavra: separado por espaço.
- Gesto de uma letra (`F`, `L`, `A`, `V`, `I`) cola na letra anterior.
- `OI` + `SOU` + `F` + `L` + `A` + `V` + `I` = **`OI SOU FLAVI`**.

### Como a decisao e tomada

- Amostras com |aceleracao| fora de 0,7..1,3 g sao descartadas (mao em
  movimento). Precisam sobrar 20 amostras boas em ate 6 s.
- Direcao = media das amostras boas, normalizada (so a direcao importa).
- Angulo = `acos` entre essa direcao e cada um dos sete alvos.
- Aceita se o menor angulo for <= 30 graus **e** houver 10 graus de folga
  sobre o segundo colocado.

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
