
Desenvolvimento do Firmware de Sensores para Luva de Libras com ESP32

Implemente exclusivamente a parte de aquisição de dados dos sensores de uma luva experimental para reconhecimento de movimentos associados à Libras, utilizando ESP32.

Não implemente ainda inteligência artificial, treinamento de modelos, classificação de sinais, reconhecimento de palavras ou reprodução de áudio. O objetivo desta etapa é desenvolver e validar uma coleta de dados confiável para uso futuro em um sistema de aprendizado de máquina.

1. Inspeção inicial do projeto

Antes de modificar qualquer arquivo:

Analise a estrutura atual do projeto e identifique os arquivos existentes.
Verifique qual modelo de ESP32 e quais sensores estão especificados no código ou na documentação.
Preserve as implementações existentes que estiverem corretas.
Não recrie arquivos ou pastas sem necessidade.
Utilize os caminhos corretos, sem chaves ou caracteres adicionais nos nomes.
Caso faltem informações sobre os componentes, identifique o que precisa ser confirmado antes de definir os pinos ou as conexões.

A pasta destinada ao firmware dos sensores deverá se chamar exatamente firmware/coleta/, caso essa organização seja compatível com o projeto existente.

2. Leitura dos sensores

Implemente firmware C++ para realizar a aquisição dos dados dos seguintes componentes, caso estejam presentes no hardware:

Sensores flexíveis para medir a flexão dos dedos.
Acelerômetro de três eixos para medir a movimentação da mão.

O firmware deverá:

Ler cada sensor de maneira consistente.
Identificar e sinalizar leituras inválidas.
Aplicar calibração quando necessária.
Registrar os valores brutos e, quando apropriado, os valores calibrados.
Utilizar intervalos de amostragem configuráveis.
Evitar bloqueios longos que provoquem perda de amostras.
Não presumir que os sensores possuem valores ou faixas elétricas universais.
Verificar os limites de tensão e as características das entradas do ESP32.
Permitir configurar os parâmetros sem precisar reescrever toda a lógica de aquisição.

Para o acelerômetro, implemente a comunicação compatível com o modelo efetivamente utilizado, como I²C ou SPI. Não presuma um modelo específico sem verificar o hardware.

3. Calibração

Implemente um procedimento de calibração para os sensores flexíveis, permitindo registrar os valores correspondentes às posições de referência dos dedos.

A calibração deverá:

Permitir registrar os valores de referência de cada sensor.
Verificar se as leituras variam de maneira plausível.
Detectar valores constantes, saturados ou fora da faixa esperada.
Permitir salvar ou configurar os parâmetros de calibração.
Documentar como repetir a calibração caso a luva seja ajustada ou os sensores sejam substituídos.

Não invente valores de calibração nem trate os parâmetros iniciais como valores garantidos.

4. Coleta e transmissão dos dados

Implemente um mecanismo para enviar os dados ao computador por comunicação serial USB.

Cada registro deverá conter:

Marca temporal ou contador de amostras.
Valores de todos os sensores flexíveis instalados.
Valores dos três eixos do acelerômetro, quando disponível.
Indicadores de validade das leituras.
Identificação da sessão de coleta, quando aplicável.

Utilize um formato estruturado e documentado, preferencialmente CSV, com cabeçalho consistente.

Exemplo ilustrativo de formato:

timestamp_ms,flex_1,flex_2,flex_3,flex_4,flex_5,accel_x,accel_y,accel_z
0,1234,1450,1670,1320,1510,0.12,-0.04,0.98
10,1240,1460,1680,1330,1520,0.14,-0.03,0.97


Os nomes e a quantidade de sensores deverão corresponder ao hardware real. Os números apresentados são apenas exemplos de formato, não medições reais.

Evite enviar mensagens de diagnóstico no meio de um registro CSV, para não corromper os dados. Se necessário, utilize um modo de diagnóstico separado.

5. Controle da coleta

Implemente comandos seriais documentados para:

Iniciar a coleta.
Pausar ou finalizar a coleta.
Consultar o estado dos sensores.
Executar a calibração.
Consultar a configuração de amostragem.

A aquisição deverá utilizar uma estratégia não bloqueante sempre que possível. Evite utilizar atrasos longos ou operações de impressão serial que impeçam a leitura dos sensores.

Defina um comportamento seguro caso um sensor falhe ou seja desconectado, sem interromper desnecessariamente a leitura dos demais sensores.

6. Qualidade e integridade dos dados

Inclua mecanismos para:

Detectar leituras fora dos limites esperados.
Identificar falhas de comunicação com o acelerômetro.
Contabilizar amostras perdidas, quando isso puder ser medido.
Informar a frequência de amostragem efetivamente alcançada.
Verificar se o formato transmitido permanece consistente.
Evitar que mensagens de erro sejam interpretadas como dados válidos.

Não declare uma frequência de amostragem garantida antes de medi-la no hardware real. Documente os parâmetros iniciais e explique como verificar o desempenho.

7. Entregáveis

Forneça exclusivamente os arquivos necessários à aquisição dos sensores:

Firmware C++ completo para ESP32.
Arquivos auxiliares de configuração ou drivers, se necessários.
Configuração de compilação compatível com a placa e o ambiente identificados.
Tabela de conexões com pinos, alimentação, interfaces e observações elétricas.
Instruções de instalação das dependências.
Procedimento de calibração.
Procedimento para iniciar e finalizar a coleta.
Exemplo do formato dos dados transmitidos.
Instruções para verificar a frequência de amostragem e diagnosticar falhas.
Orientações para testar o firmware com os sensores conectados.

Não implemente scripts de treinamento, modelos de IA, classificação de gestos, catálogo de palavras ou sistema de áudio nesta etapa.

8. Verificação

Após implementar os arquivos:

Confira os nomes e os caminhos.
Verifique se não foram criados diretórios com nomes incorretos.
Compile o firmware se as ferramentas estiverem disponíveis.
Corrija os erros encontrados.
Informe quais testes foram realmente executados.
Identifique claramente o que depende de testes com o hardware físico.

Comece inspecionando o projeto existente e identificando a placa ESP32 e os modelos dos sensores. Em seguida, implemente apenas a aquisição, a calibração e a transmissão dos dados.
