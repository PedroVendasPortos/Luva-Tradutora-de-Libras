# Diagnóstico: o erro está no código ou na luva?

Este documento é o procedimento para responder isso com evidência, não por
suposição. Ele assume o firmware desta pasta (`coleta.ino`).

---

## 0. Veredito do log de 25/11 (3219 amostras, 50 Hz)

O log `145833.310 - session_id,...txt` **foi gravado com esta versão do
firmware** (é possível provar: canais presos no trilho aparecem como
`valid=1` exatamente nas 19 primeiras amostras e caem em `stuck=1` depois —
é o `CHANNEL_FAULT_SAMPLES = 20`; o firmware antigo nunca teria `stuck=1`).

| Canal | Evidência no log | Veredito |
|---|---|---|
| polegar | `raw = 0` em 100% das amostras, `stuck=97%` | **hardware**: entrada no GND / flex aberto |
| indicador | `raw = 0` em 100%, `stuck=97%` | **hardware**: idem |
| mínimo | `raw` 0..197, `stuck=93%` | **hardware**: praticamente morto |
| médio | `raw` 0..139, validade 4,8% | **hardware**: sinal residual, sem uso |
| **anelar** | `raw` 308..2928, validade 45,8%, `stuck=6,5%` | **responde, mas com curso irregular** — medível com `ANELAR` |
| magneto indicador-medio | 100% das leituras no trilho (0 ou 4095), validade 0,6% | **hardware**: pino flutuante |
| magneto anelar-mínimo | validade 17,5%, oscila 3850 → 483 | **hardware**: pino flutuante |
| MPU-6050 | `motion_valid` 100%, accel ±2 g coerente | **funciona** |

**Conclusão:** com este log, a falha dos flexíveis é **de hardware**, não de
código. A prova é o anelar: ele passa pelo mesmo `readStableADC`, pelo mesmo
filtro e pela mesma regra de validade e funciona. Se a regra de validade
estivesse bloqueando leituras boas, ela bloquearia o anelar também.

O que o log **não** permite descartar é o mapa de pinos (um sensor pode estar
ligado no GPIO errado e o firmware lê outro). Para isso, `SCAN`.

> Correção que o log provocou: a saturação passou a ser medida pelo **valor
> médio** das 8 conversões (não por "qualquer conversão tocou o trilho") e o
> `spread` passou a descartar um extremo de cada lado. Antes, um único pico
> de ruído do ADC2 marcava um canal bom como saturado/instável — foi o que
> deixou o anelar inválido em 54% das amostras.

### O curso do anelar serve para classificar?

O log mostra que ele varia 308..2928, mas com trechos travados e sem
acompanhar o dobramento de forma confiável ("não reduz como deveria").
O comando `ANELAR` mede isso em números: 10 s de ADC cru, dobre e estique o
dedo, e o firmware dá o veredito.

| `curso` medido | Veredito |
|---|---|
| saturado na metade das amostras | canal preso no trilho → `SCAN`/`SELFTEST` |
| < 100 contagens | sem curso útil (flex rompido, solda, GPIO errado) |
| 100..400 | curso curto: serve no máximo como dimensão extra |
| > 400 | curso bom: dá para usar na classificação |

Enquanto o curso for curto, a demo dos 7 gestos usa **apenas a orientação da
mão** (MPU), que é a única coisa com sinal pleno neste hardware.

### Veredito dos 7 logs de 27/11 (OI, SOU, F, L, A, V, I)

As sete direções medidas ficaram entre si a **4,1° e 59,7°**, com a maioria
abaixo de 20°:

| Par | Ângulo |
|---|---|
| OI × A | 4,1° |
| L × V | 6,5° |
| F × V | 7,2° |
| F × I | 11,1° |
| SOU × OI | 43,0° (o maior vizinho mais próximo) |

Só **SOU** é separável. Os outros seis têm o segundo colocado a menos de 10°,
então a regra de folga os rejeita sempre.

Causa: `F`, `L`, `A`, `V`, `I` foram feitos com a mão na mesma orientação
(são configurações de dedos, não de punho). A orientação da mão é o único
sinal que sobrou, e ela é praticamente a mesma nos cinco. Não é falha de
sensor nem de firmware — é o tipo de gesto que este hardware não consegue
medir.


---

## 1. O que já foi verificado no código

| Verificação | Resultado |
|---|---|
| Compila? | Sim. `arduino-cli compile --fqbn esp32:esp32:node32s` com o core **4.0.0** instalado nesta máquina e `--warnings all`: **0 erros, 0 warnings**. Flash 353 KB (26%), RAM 27 KB (8%). O README fixa o core 3.3.11; vale alinhar os dois (nada aqui depende de API exclusiva de uma versão). |
| É erro de compilação/versão da placa? | Não. |
| O CSV e o cabeçalho batem? | Sim: 39 colunas no cabeçalho e 39 no registro. |
| O MPU-6050 funciona? | Pelo relato do projeto, sim (leituras variam ao inclinar). |

Portanto **o erro não é de compilação**. O que sobrou é:

1. defeitos de firmware que **transformam hardware bom em "leitura inválida"**;
2. defeitos de **mapeamento de pinos** (código aponta para um GPIO onde não há sensor);
3. defeitos de **hardware** (sensor, divisor, solda, alimentação);
4. qualquer combinação dos três.

Os itens 1 e 2 foram corrigidos/instrumentados nesta versão. O item 3 é o que
os comandos novos medem.

### Defeitos de firmware corrigidos (podiam causar o sintoma com hardware bom)

| # | Antes | Consequência | Agora |
|---|---|---|---|
| 1 | `valid = !clipped && raw > 0 && raw < 4095` | **Uma única** conversão encostando em 0 ou 4095 invalidava a amostra inteira. Um dedo no fim de curso (ou ruído sobre o trilho) deixava o canal permanentemente inválido. | Trilho é medida legítima; só invalida se o canal ficar **preso** no trilho por 1 s (`CHANNEL_FAULT_SAMPLES`). |
| 2 | Calibração rejeitava se **>5 de 100** amostras estivessem saturadas | `CALIBRATE` respondia "capture invalid" mesmo com o dedo parado, sempre que o fim de curso tocava o trilho. | Saturação parcial é aceita, com aviso. Só rejeita se **100/100** estiverem no trilho (aí o canal não mede nada). |
| 3 | `CALIBRATE`, `CONFIG SAMPLE_MS` comparavam com `startsWith` (sensível a maiúsculas) | `calibrate polegar open` → `ERROR: unknown command`. | Comando inteiro é normalizado; aceita qualquer caixa. |
| 4 | Durante a coleta, qualquer comando ≠ PAUSE/STOP era descartado **em silêncio** | Você digitava `STATUS`/`CALIBRATE` e não acontecia nada, sem mensagem. | Responde `# coleta ativa: comando '...' ignorado. Envie PAUSE...`. |
| 5 | Estado de `stuck`/filtro sobrevivia ao `START` | Um canal marcado travado voltava travado na sessão seguinte. | `resetFilters()` zera também os detectores. |
| 6 | Mensagem de erro única para todos os casos | "finger moved, noisy or saturated" não dizia **o que** verificar. | Erro específico: canal colado, leitura instável ou canal indisponível. |
| 7 | Não havia como ver o ADC cru | Impossível distinguir pino errado de sensor quebrado. | Comandos `SCAN` e `SELFTEST`. |

---

## 2. Passo 1 — conferir o mapa de pinos (suspeita mais forte)

O firmware lê **estes** GPIOs:

| Canal | GPIO | ADC |
|---|---|---|
| polegar | 33 | ADC1 |
| indicador | 34 | ADC1 |
| médio | 14 | ADC2 |
| anelar | 12 | ADC2 (strapping) |
| mínimo | 25 | ADC2 |
| gap indicador-medio | 35 | ADC1 |
| gap medio-anelar | 32 | ADC1 — **desativado no código** |
| gap anelar-mínimo | 13 | ADC2 |

Se a luva foi montada seguindo `PROMPTS/Prompt.2` (polegar 34, indicador 35,
médio 32, anelar 33, mínimo 25), então **três canais estão no pino errado**:

- o firmware lê 14 e 12, onde não há sensor → pino flutuante → inválido;
- o sensor do dedo médio está em **GPIO32**, que o firmware declara como
  "gap quebrado" e ignora;
- polegar e indicador aparecem trocados.

O nome "sensor quebrado no GPIO32" é compatível com isso: o sensor pode estar
**bom**, apenas ligado no pino que o firmware passou a tratar como gap.

**Como confirmar em 60 segundos:** rode `SCAN` e dobre um dedo de cada vez.

---

## 3. Passo 2 — `SCAN`: em qual GPIO cada sensor realmente está?

No Monitor Serial (115200), com a coleta parada:

```
SCAN
```

O firmware passa a imprimir uma linha a cada 200 ms com o valor cru de 12
GPIOs candidatos (32, 33, 34, 35, 36, 39, 25, 26, 27, 14, 12, 13):

```
# gpio,32,33,34,35,36,39,25,26,27,14,12,13
15230,2047,3105,120,3880,17,22,2601,4095,4095,4095,4095,4095
...
```

Dobre **um** dedo e observe **qual coluna** muda (e se volta ao valor anterior).

| Observação | Conclusão | Ação |
|---|---|---|
| A coluna muda e volta | Sensor bom, no GPIO esperado | Siga para a calibração |
| A coluna muda, mas é **outro** GPIO | **Código**: mapa de pinos errado | Corrigir `FLEX_PINS`/`GAP_PINS` |
| Nenhuma coluna muda em nenhum dedo | Sensor, fiação ou alimentação | Seção 6 |
| Coluna oscila sozinha (sem tocar) | Pino **sem sensor** (flutuante) | Sensor não chega nesse GPIO |
| Coluna fixa em **4095** | Divisor em **5 V** (VIN) ou saída > 3,1 V | Alimentar o divisor em **3V3** |
| Coluna fixa em **0** | Falta GND, resistor aberto ou sensor em curto | Conferir GND e o resistor fixo |

Qualquer comando encerra o `SCAN` e imprime o resumo `min/max/delta` por GPIO
(`delta` alto = aquele pino tem sensor que se moveu).

---

## 4. Passo 3 — `SELFTEST`: o canal está medindo algo?

```
SELFTEST        # ~1,5 s, não mova a luva
```

Saída (CSV):

```
canal,gpio,min,max,delta,media,ruido_max,total0,total4095,estado
polegar,33,3100,3112,12,3105,9,0,0,ESTAVEL
indicador,34,0,4095,4095,2010,3201,11,12,OSCILA_MUITO
medio,14,14,18,4,16,3,0,0,ESTAVEL
...
```

| Estado | Significado | Culpa |
|---|---|---|
| `ESTAVEL` | sinal estável, divisor presente | hardware ok (para essa posição) |
| `OSCILA_POUCO` | variou ≤ 300 contagens | dedo/luva se moveu — ou ruído alto |
| `OSCILA_MUITO` | variou > 300 contagens | **pino sem sensor** (flutuante) ou fiação solta |
| `TRAVADO` | nenhuma conversão mudou | fio rompido, GPIO errado, canal morto |
| `PRESO_EM_4095` | ≥ 90% das leituras no topo | divisor em 5 V / saída acima de ~3,1 V |
| `PRESO_EM_0` | ≥ 90% das leituras em zero | falta GND, resistor aberto, curto |

**O teste decisivo:** rode `SELFTEST` com os dedos **estendidos** e anote
`min/max`; rode de novo com o dedo **dobrado**.

- `min/max` mudou de forma coerente → sensor e fiação **ok**; se a classificação
  falha, o problema é de calibração/perfil, não de hardware.
- `min/max` **idêntico** nas duas posições → o sensor **não chega** naquele GPIO
  (fiação) ou está morto. **Não é o código.**
- `delta` grande nos dois testes, com a mão parada → pino flutuante.

---

## 5. Passo 4 — calibrar e ler o erro específico

```
CALIBRATE polegar OPEN
CALIBRATE polegar CLOSED
```

Agora a resposta diz o que falhou:

| Resposta | Significado |
|---|---|
| `OK: polegar OPEN reference=3105 calibrated=1` | referência gravada |
| `# aviso: referencia ... amostras no trilho` | referência válida, mas o canal satura: resolução ruim |
| `ERRO: ... canal colado em 0/4095 nas 100 amostras` | canal não mede nada → `SCAN`/`SELFTEST` |
| `ERRO: ... leitura instavel` | dedo se moveu, fio solto ou ruído alto |
| `# aviso: ... ainda nao tem OPEN e CLOSED com diferenca minima de 20 contagens` | as duas posições leem quase igual → sensor não responde |

Depois: `STATUS` mostra, por dedo, `raw`, `valid`, `stuck`, `sat`, `ruido`,
`cal=OPEN..CLOSED` e `estado` (`ok`, `sem_calibracao`, `travado`,
`preso_em_0`, `preso_em_4095`, `pino_flutuante`, `invalido`).

---

## 6. Se o veredito for hardware (checklist)

1. **Alimentação do divisor**: 3V3, nunca 5 V/VIN. Em 5 V o ADC satura em 4095
   e nenhuma calibração funciona.
2. **GND comum** entre sensores, MPU e ESP32.
3. **Resistor fixo** presente (10 kΩ típico) e soldado (puxe levemente o fio).
4. **Sensor flex no multímetro**: resistência muda de forma monotônica ao
   dobrar. Se fica aberto (∞) ou fixo, o sensor está rompido.
5. **GPIO12**: é pino de *strapping* (MTDI). Se o divisor puxar nível alto no
   boot, a placa pode não iniciar. Se puder, mova o canal para um ADC1 livre
   (36/39) e ligue o divisor em 3V3.
6. **ADC2 (12, 13, 14, 25)**: só funciona com Wi-Fi/BT desligado — este
   firmware não usa rádio, então está ok, mas é o ADC mais ruidoso do ESP32.
7. **Fios longos** entre luva e placa: aumentam ruído; `ruido_max` no
   `SELFTEST` mostra isso. Acima de ~250 contagens o canal é considerado
   flutuante pelo firmware.

---

## 7. Constantes que você pode ajustar

Todas no topo do `coleta.ino`:

| Constante | Padrão | Quando mexer |
|---|---|---|
| `FLOAT_SPREAD_COUNTS` | 250 | Se sensores com fio longo forem marcados como `pino_flutuante` sem estarem soltos, aumente. |
| `CHANNEL_FAULT_SAMPLES` | 20 (1 s a 20 Hz) | Tempo de persistência antes de invalidar um canal. |
| `CALIBRATION_MAX_SPREAD` | 60 contagens | Aumente se o ADC2 for ruidoso demais para calibrar. |
| `ADC_11db` | atenuação ~3,1 V | Fixo; o divisor precisa caber nessa faixa. |
| `ADC_SAMPLES` | 8 | Mais amostras = menos ruído e mais tempo por canal. |

`STATUS` informa `acquisition_us`: é o custo real dos 7 canais por amostra
(~10-12 ms). Intervalos menores que isso (`CONFIG SAMPLE_MS`) não são
cumpríveis — o firmware agora avisa.
