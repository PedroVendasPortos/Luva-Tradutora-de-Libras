Vamos fazer o projeto inteiramente em C++, sem depender de Python.

A proposta será construir um sistema em que:

- O ESP32 detecta movimentos por meio de um sensor.
- Envia os dados ao computador.
- O computador, executando um programa em C++, interpreta os movimentos.
- O computador escolhe o som correspondente.
- Envia um comando de volta ao ESP32.
- O ESP32 reproduz o áudio em um alto-falante comum.

Vou organizar o projeto como um guia de montagem, desde as compras até os testes.

# 1. Como será o projeto

HC-SR501 with Arduino & Raspberry Pi | DroneBot Workshop

Sensor de movimento

Detecta a presença e o movimento

ESP32 — C++

Lê o sensor e envia dados pela USB

PC — C++

Interpreta os dados e escolhe o áudio

ESP32 — C++

Recebe o comando e gera o áudio

ESP32 Audio Output with I2S DMA and the MAX98357A Class D Amplifier

Amplificador + alto-falante

Reprodução sonora

# 2. Lista de compras

Para a primeira versão, recomendo estes componentes:

GitHub - TronixLab/DOIT\_ESP32\_DevKit-v1\_30P · GitHub


3. Amplificador MAX98357A I2S

ObrigatórioTransforma o áudio digital do ESP32 em sinal amplificado para o alto-falante.

Quantidade: 1 unidade.

Gikfun 4Ohm 40mm Diameter 3W Full Range Audio Speaker Stereo Woofer Lo

4. Alto-falante 4 Ω / 3 W

ObrigatórioÉ a saída física do som.

Quantidade: 1 unidade.

Fonte De Alimentação Tv Box TvBox 5v 2a Bivolt 110 220v Plug P4 - Beta Cabos Cell


Não compre ainda: não precisa de módulo Bluetooth, cartão SD ou módulo MP3. O processamento e a escolha dos arquivos de áudio serão feitos no computador.

# 3. Montagem dos componentes

Vamos dividir em três partes.

## Parte A — Sensor de movimento

Conecte o HC-SR501 ao ESP32:

| Sensor PIR | ESP32 |
| --- | --- |
| VCC | VIN / 5 V |
| GND | GND |
| OUT | GPIO 33 |

O sensor enviará um sinal digital quando detectar movimento.

## Parte B — Sistema de áudio

Conecte o MAX98357A:

| MAX98357A | ESP32 |
| --- | --- |
| VIN | Fonte 5 V |
| GND | GND comum |
| BCLK | GPIO 26 |
| LRC | GPIO 25 |
| DIN | GPIO 27 |

Alto-falante:

- SPK+ no positivo.
- SPK− no negativo.

Não ligue SPK− ao GND. A saída do amplificador é em ponte.

Alimentação: use a fonte de 5 V para o amplificador e una o GND dela ao GND do ESP32. Não injete 5 V em GPIO. O ESP32 pode continuar alimentado pelo USB.

## Parte C — Comunicação com o computador

Conecte o ESP32 ao PC com o cabo USB.

A mesma porta USB será utilizada para duas coisas:

- ESP32 → PC: informações do sensor.
- PC → ESP32: comandos de reprodução.

Não precisa de duas conexões separadas.

# 4. Como será a programação em C++

Teremos dois programas independentes, ambos em C++:

| Programa | Onde executa | Responsabilidade |
| --- | --- | --- |
| Firmware | ESP32 | Sensor, comunicação e saída de áudio |
| Aplicação | PC | Interpretação dos movimentos e escolha dos arquivos |

O fluxo será assim:

1. O sensor detecta movimento.
2. O ESP32 envia `MOVIMENTO:INICIO`.
3. O programa C++ no PC recebe a mensagem.
4. O PC seleciona um arquivo de áudio.
5. O PC envia `TOCAR:ALERTA`.
6. O ESP32 recebe a ordem.
7. O ESP32 reproduz o som correspondente.

Uma diferença importante: o arquivo WAV ou MP3 ficará armazenado no PC. O computador não precisa enviar o arquivo inteiro ao ESP32: pode enviar apenas o comando para tocar um dos sons. O ESP32 gera os tons ou reproduz o áudio que tiver sido previamente disponibilizado a ele. Para reproduzir arquivos WAV/MP3 diretamente, precisaremos implementar também a transferência e decodificação do áudio no ESP32.

# 5. Como o computador escolherá os sons

Na primeira versão, o computador terá regras como estas:

| Movimento detectado | Comando enviado | Resultado |
| --- | --- | --- |
| Movimento começou | `TOM:880:200` | Bip agudo |
| Movimento continuou | `TOM:440:300` | Bip grave |
| Movimento terminou | `TOM:660:500` | Bip médio |

Depois poderemos evoluir para uma biblioteca de arquivos de áudio, com sons personalizados.

# 6. Programas que você precisará instalar

No computador:

1. Visual Studio Community — para desenvolver e compilar o programa C++ do PC.
2. Arduino IDE — para programar o ESP32 em C++.
3. Driver USB da placa, se necessário (CH340 ou CP210x, dependendo do modelo).

Links oficiais:

- [Baixar Visual Studio Community](<https://visualstudio.microsoft.com/vs/community/?utm_source=chatgpt.com>)
- [Baixar Arduino IDE](<https://www.arduino.cc/en/software/?utm_source=chatgpt.com>)
- [Documentação ESP32](<https://docs.espressif.com/projects/arduino-esp32/en/latest/?utm_source=chatgpt.com>)

# 7. Ordem de construção

Eu recomendo seguir esta sequência para evitar problemas:

Checklist de construção

0/8

[ ]Comprar os componentes da lista

[ ]Montar o sensor PIR no ESP32

[ ]Testar a detecção de movimento

[ ]Conectar o ESP32 ao PC pela USB

[ ]Criar o programa C++ do computador

[ ]Fazer o PC enviar comandos ao ESP32

[ ]Conectar o MAX98357A e o alto-falante

[ ]Testar os sons e integrar tudo

