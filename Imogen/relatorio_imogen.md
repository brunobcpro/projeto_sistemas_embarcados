# ESCOLA POLITÉCNICA DA UNIVERSIDADE DE PERNAMBUCO (UPE-POLI)
## ENGENHARIA DA COMPUTAÇÃO - SISTEMAS EMBARCADOS

<br>

**Equipe:** Bruno Protásio, Daniel Guilherme, Paulo Ferraz e Marcus Vinicius.

<br><br><br><br>

# PROJETO IMOGEN:
## Sistema Embarcado de Alta Confiabilidade para Monitoramento Portátil de Temperatura e CO₂/CO em Transporte de Material Biológico Sensível

<br><br><br><br>

**Recife, Pernambuco**
**2026**

---

## RESUMO

O transporte de material biológico sensível, como oócitos e embriões para fertilização *in vitro* (FIV), impõe restrições severas de controle ambiental, onde variações térmicas maiores que ±0.5 °C ou alterações no pH do meio causadas por gases difusos (ex: Monóxido de Carbono) podem ser letais para as células. Este relatório técnico descreve o projeto e desenvolvimento do **IMOGEN**, um sistema embarcado de alta confiabilidade concebido para monitoramento ativo destas maletas de transporte. Fundamentado na arquitetura ARM Cortex-M4 (STM32L476RG), o projeto propõe um firmware estritamente assíncrono e orientado a interrupções de temporizadores de hardware, abolindo atrasos de software (*blocking delays*) no laço principal para garantir imunidade a travamentos e alta disponibilidade. O hardware integra sensores I²C (LM75) e analógicos (MQ-7) tratados por filtros digitais de média móvel, exibindo os dados em tempo real via display LCD e transmitindo pacotes de telemetria serial via UART. O sistema foi validado experimentalmente, demonstrando processamento inferior a ppm00.5% de carga de CPU, alta supressão de ruídos em conversores ADC e integridade na persistência de dados via backend em Python com filtragem por expressões regulares (Regex).

**Palavras-chave:** Sistemas Embarcados, STM32, Confiabilidade, Monitoramento Biológico, Telemetria Assíncrona.

---

## 1. INTRODUÇÃO

No âmbito da engenharia biomédica e da reprodução humana assistida, a logística de transporte de material biológico vivo — notadamente oócitos e embriões — é uma operação crítica. Tais células exigem a manutenção de um ambiente térmico altamente restrito, tipicamente ancorado na zona de ~37 °C, com tolerâncias marginais de desvio. Adicionalmente, a exposição a gases poluentes comuns em ambientes urbanos e rodovias, como o Monóxido de Carbono (CO), apresenta o risco de dissolução gasosa no meio de cultura, induzindo o deslocamento do pH para faixas ácidas letais às células.

As soluções convencionais de transporte frequentemente baseiam-se em caixas passivas de poliestireno extrudado que, embora possuam inércia térmica, comportam-se como "caixas-pretas". Tais recipientes não oferecem instrumentação ativa, carecendo de *feedback* visual e de registros de telemetria auditáveis que comprovem a integridade do trajeto. 

Diante desta problemática, o **Projeto IMOGEN** propõe a implementação de um escudo de monitoramento ativo. Trata-se de um sistema embarcado avançado que realiza a aquisição temporal de variáveis ambientais, executando o tratamento digital de sinais a bordo (Edge Computing) e fornecendo tanto a visualização *in situ* quanto a transmissão de dados persistente. O foco central desta pesquisa reside na aplicação de boas práticas de engenharia de *software* embarcado para garantir tolerância a falhas, concorrência segura em barramentos e previsibilidade determinística na execução do *firmware*.

---

## 2. FUNDAMENTAÇÃO TEÓRICA

O projeto arquitetônico do IMOGEN apoia-se em conceitos fundamentais de microcontroladores modernos e comunicação de periféricos:

*   **Inter-Integrated Circuit (I²C):** Barramento de comunicação serial síncrona *half-duplex* que opera através de duas vias (SDA e SCL). Utilizado no projeto para endereçamento do sensor de temperatura LM75 e da interface do Display LCD (via expansor PCF8574).
*   **Analog-to-Digital Converter (ADC):** Periférico responsável pela discretização e quantização de grandezas contínuas. A resolução de 12 bits do ADC do STM32 fornece 4096 níveis de quantização, essenciais para a detecção milivolt-sensível da condutividade do sensor MQ-7.
*   **Universal Asynchronous Receiver-Transmitter (UART):** Protocolo de comunicação assíncrono empregado para a telemetria ponta a ponta entre a placa núcleo e o *host* computacional.
*   **Temporizadores de Hardware (Timers):** Módulos contadores independentes do núcleo da CPU. O acionamento de eventos via *Timer Interrupts* garante bases de tempo precisas, isentas de *drift* de processamento causado por instruções de atraso (`HAL_Delay`).

---

## 3. ARQUITETURA DO SISTEMA E METODOLOGIA

### 3.1 Hardware e Mapeamento de Pinos

O cérebro do sistema baseia-se no microcontrolador STMicroelectronics **STM32L476RG** (pacote LQFP64), configurado para operar em sua frequência nominal superior através de um *Phase-Locked Loop* (PLL) a 80 MHz. 

A disposição do circuito elétrico na bancada e o roteamento de *software* foram projetados com isolamento de barramentos para mitigação de falhas em cascata:
*   **Sensor de Temperatura LM75:** Mapeado no barramento **I²C2** (Pinos PB10/SCL e PB11/SDA).
*   **Interface Homem-Máquina (Display LCD 16x2):** Mapeada no barramento **I²C1** (Pinos PB6/SCL e PB7/SDA). A isolação lógica entre o I²C1 e o I²C2 garante que um curto-circuito físico no cabo do LCD não paralise a leitura vital de temperatura.
*   **Sensor de Gás MQ-7:** Conectado ao canal analógico **ADC1_IN1**, mapeado no pino **PC0**.
*   **Telemetria (USART2):** Roteada via pinos PA2 (TX) e PA3 (RX) diretamente pelo terminal USB da placa Nucleo.
*   **Sinalização e Diagnóstico:** LED de *status* configurado em modo *Push-Pull* (PA5) para alta capacidade de corrente de dreno/fonte, evitando acionamentos fantasmas, e um Botão de interrupção externa no pino PC13.

As montagens em bancada de testes comprovaram o arranjo físico dos terminais de expansão conectados individualmente aos módulos sensores, demonstrando a eficácia do isolamento entre o Módulo I2C e a barreira analógica.

### 3.2 Arquitetura de Firmware e Loop Assíncrono

A filosofia norteadora do *firmware* é a aversão total ao bloqueio de *thread*. Em microcontroladores comerciais simples, o uso da função de atraso (ex: `HAL_Delay()`) gasta ciclos de máquina e monopoliza a CPU. No IMOGEN, o laço infinito principal (`while(1)`) entra em repouso dinâmico e o fluxo é inteiramente governado por eventos disparados por um **Timer de Hardware (TIM1)** configurado para 1 Hz.

A cada 1 segundo (interrupção do TIM1), uma *flag* de liberação de tempo (`time_to_read_sensors`) é setada, orientando o laço principal a:
1. Requisitar dados de forma assíncrona.
2. Atualizar o *buffer* do filtro digital.
3. Atualizar o visor em malha I2C1 isolada.
4. Transmitir o *frame* pela serial via interrupção de UART (`HAL_UART_Transmit_IT`).

Isso configura um paradigma assíncrono que libera mais de 99% da capacidade dos 80 milhões de ciclos por segundo do Cortex-M4 para tratar eventos imprevistos, maximizando a robustez temporal.

### 3.3 Tratamento de Sinais Digitais e Equacionamento de Dados

Para processar o sinal altamente ruidoso provindo da malha analógica do sensor de gases MQ-7, induzido por interferência eletromagnética em operação não blindada, implementou-se um **Filtro Circular de Média Móvel**. O ruído espúrio é anulado computando-se a média das últimas ppmNN=20 leituras em tempo de execução.

**Equacionamento da Calibração ADC e Conversão PPM:**
O valor bruto do ADC (ppmADC_ADC_raw, abrangendo [0, 4095]) é primeiramente convertido em tensão de saída em milivolts (ppmV_V_out) utilizando a relação de 12 bits para a referência de 3.3V (3300 mV):

ppmppm V_{out}(mV) = \frac{ADC_{raw} \times 3300}{4095} ppmppm

Em seguida, calcula-se a taxa da resistência elétrica do sensor no ar ambiente (ppmR_s/R_0ppm), partindo de um divisor resistivo (com resistência de carga de ppm10\ k\Omegappm sob alimentação teórica de ppm5Vppm). A formulação matemática aplicada à sensibilidade do MQ-7 segue o decaimento em potência logarítmica para obter a concentração final em partes por milhão (ppmppmppm):

ppmppm ppm = 100 \times \left(\frac{R_s}{R_0}\right)^{-1.53} ppmppm

A formatação dessa conversão ocorre diretamente no microcontrolador para exibição no display LCD. Para contornar a ineficiência e o custo de processamento das rotinas pesadas de formatação de ponto flutuante como `printf("%f")`, o valor é decomposto matematicamente. O sistema utiliza a parte inteira (quociente) e a parte fracionária (resto multiplicado por 1000) para exibir nativamente três casas decimais. Dessa forma, o valor convertido de CO aparece claramente no LCD como, por exemplo: `CO:1.234ppm`.

### 3.4 Integração com Backend e Persistência de Dados

O nó de processamento primário transmite, a 1 Hz, *strings* telemetradas num formato auditável que já encapsula a conversão, exibindo o valor tanto em milivolts brutos quanto no valor final convertido em PPM, na forma de:
`Temp:25.5C CO:1500mV(15.20ppm)`

Um *script* em Python (`guardar_dados.py`), atuando como controlador de *backend* via porta Serial, escuta ativamente a linha de telecomunicação. Para garantir que dados corrompidos não contaminem a base histórica, aplica-se validação cruzada através de uma Expressão Regular (Regex):

```python
DATA_PATTERN = re.compile(r"Temp:(-?\d+(?:\.\d+)?)C\s+CO:(\d+)mV\(([\d\.]+)(%|ppm)\)")
```

Uma vez validada a integridade do pacote telemetrado, o backend extrai os grupos lexicais. Como o *script* é híbrido e oferece suporte a versões antigas que emitiam o percentual bruto (`%`), ele possui em sua lógica a própria função conversora `mv_to_ppm(mv)`. Porém, na execução ideal, ao ler a tag `ppm` transmitida, o Python descarta a necessidade de recalcular e apenas capta a variável exata extraída e a consolida diretamente como `CO_PPM` num _Data Logger_ (arquivo persistente `dados_sensores.csv`), com aposição de marca d'água de tempo real (*timestamping*).

### 3.5 Desafios Técnicos e Implementação de Resiliência

Durante o desenvolvimento arquitetônico, observou-se que a ocorrência de desconexões físicas (terminais frouxos) no barramento I²C induziam o evento crítico de _deadlock_ na máquina de estados I²C da biblioteca HAL, enclausurando o microcontrolador em um _loop_ infinito de *timeout* à espera do bit `ACK` da camada de transporte.

**Solução Aplicada:** Introdução pragmática de *Callbacks* de Erro Defensivo na pilha HAL (`HAL_I2C_ErrorCallback`). Ao detectar a falha condicional `HAL_I2C_ERROR_AF` (Acknowledge Failure), a interrupção destrava arbitrariamente os semáforos condicionais (as flags globais de `tx_done` e `rx_done`). Esse comportamento evasivo impede que a CPU trave. Mesmo mediante rompimento mecânico das linhas de display ou sensores sob intenso estresse vibratório, o firmware circunda o gargalo silenciosamente, assegurando a continuidade vital da telemetria externa.

---

## 6. RESULTADOS OBTIDOS E VALIDAÇÃO

A arquitetura orientada a interrupções foi validada com sucesso, sendo seu escopo de telemetria confrontado contra ensaios reais (bancada e simulação de atmosfera). O _profiling_ de ciclos confirmou consumo abaixo de ppm00.5% de CPU.

O rastreamento persistente dos ensaios em banco de dados comprovou numericamente a estabilidade do conjunto:
*   **Estabilidade Basal:** Em repouso (fase de não-exposição), o sistema exibiu leitura consistente entre ppm28.0^{\circ}Cppm e ppm29.5^{\circ}Cppm, com traços ínfimos de gás mensurados na margem base de ppmCO \approx 0.01\ ppmppm.
*   **Resposta Dinâmica a Transientes (Spike Test):** O CSV revelou, mediante estímulo gasoso artificial no estagiamento de testes (*Timestamp: 2026-06-14 20:42:51*), uma elevação quase instantânea da tensão MQ-7 para a ordem de ppm1178\ mVppm, traduzida matematicamente como ppm0.89\ ppmppm. Na sequência, o filtro móvel de 20 posições se encarregou da dissipação logarítmica da anomalia ao longo dos 90 segundos seguintes, conduzindo perfeitamente a leitura para ppm0.01\ ppmppm em ppm20:44:04ppm, indicando excelente imunidade ao ruído de relaxamento e total ausência de falha permanente de histerese.

As imagens do circuito experimental acoplado (Display indicando conformidade de ppm0.010\ ppmppm) endossam o paralelismo funcional entre medição térmica simultânea e supressão de alarmes falsos, validando que os filtros embarcados não distorcem os dados reais nem causam estrangulamento da via I²C1.

---

## 7. CONCLUSÃO E TRABALHOS FUTUROS

O projeto IMOGEN atingiu plenamente a meta de instrumentação avançada demandada para a custódia biológica no processo de fertilização. A decomposição de canais (Display e Termômetro em I²C separados), a rejeição absoluta a _delays_ bloqueantes e a blindagem contra falhas intrínsecas de protocolo (_deadlocks_ em falha de reconhecimento) culminaram em um sistema com tolerância a falhas exponencialmente superior à das abordagens de software síncronas. O processamento por matriz inteira blindou a carga temporal do Cortex-M4 e conferiu telemetria serial determinística, fundamental para a cadeia probatória em clínicas de FIV.

**Trabalhos Futuros (Roadmap de Engenharia):**
A conversão evolutiva do IMOGEN exigirá transcender a barreira do simples monitoramento passivo. A meta prospectiva consiste em implementar um Sistema Térmico de Controle em Malha Fechada (Controle PID). Isso abarcará a adição de pastilhas de Peltier sobutando a modulação PWM dinâmica de alta frequência proveniente do microcontrolador. Este esforço garantirá a manutenção algorítmica exata em ppm37.0^{\circ}Cppm, reagindo a choques térmicos com latência da ordem de milissegundos. Recomenda-se ainda o design físico para placa de circuito impresso (PCB) multi-layer, incorporando plano de malha terra (Ground Plane) robusto visando a certificação de classe médica no tocante a imunidade à Interferência Eletromagnética (EMI).
