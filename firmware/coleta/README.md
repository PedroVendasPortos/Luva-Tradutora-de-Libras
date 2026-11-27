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
- DEMO (liga a demo PEDRO: orientacao da mao -> letra)
- GO (na demo: captura 20 amostras e imprime a letra)
- ALVO (ao vivo: angulo de cada letra, para acertar a pose)
- GESTOS (lista os 5 alvos fixos e a separacao em graus)
- ANELAR (mede o curso real do flexivel do anelar por 10 s)

Nomes dos dedos: polegar, indicador, medio, anelar, minimo.

Nomes dos gaps: indicador_medio, medio_anelar, anelar_minimo. O gap medio_anelar fica desativado enquanto o sensor no GPIO32 estiver quebrado.

Os comandos **nao diferenciam maiusculas de minusculas** (`START`, `start`,
`Start`). Durante a coleta, apenas `PAUSE` e `STOP` sao aceitos; os demais
comandos respondem com um aviso explicito em vez de serem ignorados em
silencio.

## Demo PEDRO (alvos fixos, sem ensinar)

Demonstracao, **nao e Libras**: cinco poses de mao devolvem as letras de
PEDRO. **Nao ha nada para ensinar nem gravar** - os cinco alvos estao no
codigo. A decisao usa somente a direcao da gravidade medida pelo MPU-6050.

Por que nao usar o anelar nem os magnetos: no log de 25/11 os quatro
flexiveis estao mortos, o anelar tem curso irregular e os dois magnetos
estao flutuando. O MPU e o unico canal com sinal pleno (100% valido). O
anelar continua sendo medido e impresso em cada captura, como diagnostico;
`ANELAR` diz se o curso dele serve para classificar.

### Os cinco alvos

| Letra | Alvo (eixo do MPU apontando para o chao) | Pose |
|---|---|---|
| **P** | `az = +1` | mao de costas para cima, palma para baixo |
| **E** | `ay = +1` | dedos apontando para baixo |
| **D** | `ay = -1` | dedos apontando para cima |
| **R** | `ax = +1` | mao de lado, um polegar para baixo |
| **O** | `ax = -1` | mao de lado, o outro polegar para baixo |

Aceita ate **35 graus** de desvio do alvo (absorve a inclinacao natural da
mao). Com alvos a 90 graus um do outro, sobram 20 graus de zona morta entre
eles.

> Se a placa do MPU estiver montada girada na luva, a letra aparece em outra
> pose fisica. Nao e problema: `ALVO` mostra qual letra esta mais perto
> enquanto voce gira a mao, e os cinco alvos continuam alcancaveis.

### Procedimento (Monitor Serial a 115200)

```
GESTOS   (ve os alvos e a separacao em graus)
ALVO     (ao vivo: gire a mao e veja o angulo de cada letra cair)
DEMO     (liga)
GO       (mantenha a pose: 20 amostras, ~1 s, e a letra)
GO       ... repita para P, E, D, R, O
stop     (encerra e imprime LETRAS=PEDRO)
```

`ALVO` imprime cinco linhas por segundo:

```
t_ms,P,E,D,R,O,gx,gy,gz,melhor,status
2450,   4*,  86,  94,  90,  92,0.02,-0.03,1.00,P,OK
```

Cada numero e o angulo, em graus, ate a pose daquela letra. Escolha a coluna,
gire a mao ate o numero cair; quando aparecer `OK` (e o `*` estiver nela),
envie `GO`. Qualquer comando encerra o `ALVO`.

Saida de uma captura:

```
g=(0.02 -0.03 1.00) anelar=1810
LETRA=P angulo=4 segundo=83
LETRAS=P
```

Pose fora dos alvos (mao torta ou em movimento):

```
NAO_RECONHECIDO angulo=41 segundo=44 limite=35 folga=3
```

Nesse caso **nenhuma letra e emitida** - o firmware prefere calar a inventar.

### Como a decisao e tomada

- Amostras com |aceleracao| fora de 0,7..1,3 g sao descartadas (mao em
  movimento). Precisam sobrar 20 amostras boas em ate 6 s.
- Direcao = media das amostras boas, normalizada (so a direcao importa,
  nao a intensidade).
- Angulo = `acos` entre essa direcao e cada um dos cinco alvos.
- Aceita se o menor angulo for <= 35 graus **e** houver 10 graus de folga
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