# Esquema do dataset

## Granularidade

Uma linha CSV = uma execução rotulada de um gesto estático, resumida a partir de várias leituras estáveis dos cinco sensores flexíveis. Não registrar uma linha por amostra de 20 Hz como se cada linha fosse uma execução independente.

## Colunas obrigatórias

| Coluna | Tipo | Descrição |
|---|---|---|
| flex_thumb | float | Polegar calibrado, [0, 1] |
| flex_index | float | Indicador calibrado, [0, 1] |
| flex_middle | float | Médio calibrado, [0, 1] |
| flex_ring | float | Anelar calibrado, [0, 1] |
| flex_pinky | float | Mínimo calibrado, [0, 1] |
| label | string | Uma das classes P, E, D, R, O |
| participant_id | string | Identificador pseudônimo do participante |
| session_id | string | Identificador da sessão de coleta |
| trial_id | string | Identificador único da execução |

A ordem das cinco features é fixa. Não incluir timestamp, IDs ou rótulos como entrada do modelo.

## Exemplo ilustrativo

Os valores abaixo são apenas exemplos de formato, **não são medições e não devem ser usados para treinar**:

```csv
flex_thumb,flex_index,flex_middle,flex_ring,flex_pinky,label,participant_id,session_id,trial_id
0.20,0.85,0.82,0.80,0.78,P,participant_01,session_01,trial_0001
0.18,0.22,0.25,0.24,0.21,E,participant_01,session_01,trial_0002
```

## Regras de qualidade

- Todos os cinco valores devem ser numéricos, finitos e estar em [0, 1].
- Rejeitar linhas com valores ausentes ou fora da escala; não preencher sensor ausente com zero.
- O rótulo deve existir em `config/classes.json`.
- `trial_id` deve identificar uma execução única.
- Preservar participante e sessão para permitir divisão de dados sem vazamento.
- Guardar arquivos brutos separados dos dados processados.
