![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)
![Status](https://img.shields.io/badge/Project-SmartStop%20Elevator-blue)
![Platform](https://img.shields.io/badge/Hardware-Raspberry%20Pi%20Pico%20W-orange)

# SmartStop Elevator Simulator  
### Simulador Realista de Controle de Elevadores usando Raspberry Pi Pico W (BitDogLab)



# SmartStop Elevator Simulator
### Simulador Realista de Controle de Elevadores usando Raspberry Pi Pico W (BitDogLab)

---

##  Visão Geral

O **SmartStop Elevator Simulator** é um simulador avançado de controle de elevadores desenvolvido em **C** utilizando o **Raspberry Pi Pico W (BitDogLab)**.  
O objetivo é reproduzir a lógica real de despacho de elevadores modernos, priorizando **eficiência, tempo de espera e fluxo de passageiros**.

Este projeto implementa um algoritmo inspirado em sistemas reais de controle, incluindo:

- Atender quem espera *há mais tempo*  
- Reduzir paradas desnecessárias  
- Priorizar chamadas internas e botões físicos  
- Desviar para emergências  
- Evitar rotas ineficientes  
- Parar apenas quando realmente vale a pena (**SmartStop**)  

O sistema gera logs detalhados no terminal, simulando o comportamento operacional do elevador em tempo real.

---

##  Funcionalidades Principais

### 🔵 Chamadas Internas (Botão A)
- Passageiros dentro do elevador solicitam um andar.
- Lógica garante prioridade e desembarque obrigatório.

### 🟢 Chamadas Externas – Hall Calls (Botão B)
- Criadas por pessoas fora do elevador.
- Controlador estima quantidade de passageiros e tempo de espera.

### 🚨 Emergência por tempo de espera
- Se um andar espera muitos ciclos, vira prioridade absoluta.
- Simula frustração de usuários e SLA de elevadores reais.

### 🎯 Prioridades Inteligentes

A decisão segue a seguinte ordem:

| Prioridade | Regra |
|-----------|--------|
| **0** | Atender emergências (wait_time ≥ limite) |
| **1** | Atender chamadas criadas manualmente (botões A/B) |
| **2** | Atender chamadas internas (passageiros já embarcados) |
| **3** | Desembarque forçado quando lotado |
| **4** | Paradas próximas na direção atual |
| **5** | SmartStop (parada eficiente) |
| **6** | Quando muito cheio, buscar locais para desembarque |
| **7** | Fallback (ocupação baixa ≤ 2 passageiros) |

---

##  Hardware Utilizado (BitDogLab + Pico W)

- Raspberry Pi Pico W  
- LED RGB (GPIO 11, 12, 13)  
- Botão A (GPIO 5) — chamadas internas  
- Botão B (GPIO 6) — hall calls  
- Buzzer (opcional)  
- Terminal USB (UART) para logs  

---

##  Lógica de Simulação

###  Embarque e Desembarque

- Desembarque probabilístico realista  
- Andares extremos têm probabilidade aumentada de desembarque  
- Não desembarca mais passageiros do que a ocupação atual  
- Embarque limitado pela capacidade (default: 8 pessoas)

### 📊 Estatísticas geradas automaticamente

Ao final de cada ciclo, são exibidos:

- Ciclos simulados  
- Paradas realizadas  
- Paradas ignoradas (SmartStop)  
- Passageiros embarcados  
- Taxa de eficiência  

Exemplo de log:

```text
 DECISÃO: Parar no andar 5
  >> EMBARQUE: Passageiros entraram no elevador
 Ocupação atual: 6/8

Paradas realizadas: 8
Paradas ignoradas: 9
Eficiência: 52.9%
```

---

## 📂 Estrutura do Projeto

```text
smartstop-elevator-simulator/
│
├── src/
│   ├── main.c
│   ├── smartstop.c
│   └── smartstop.h
│
├── CMakeLists.txt
├── README.md
└── .gitignore
```

---

##  Como Compilar (Pico SDK + CMake)

Dentro da pasta principal:

```bash
mkdir build
cd build
cmake ..
make
```

Será gerado um arquivo `.uf2`.  

Basta conectar o Pico em modo BOOTSEL e copiar o `.uf2` para a unidade USB que aparecer.

---

##  Como Rodar

1. Compile usando o comando acima  
2. Copie o `.uf2` para o Raspberry Pi Pico W  
3. Abra o **Serial Monitor** (115200 baud) no VS Code ou outra IDE  
4. Observe o comportamento do elevador em tempo real no terminal  

---

##  Exemplo de Saída

```text
┌──────────────────────────────────────────────┐
│ Ciclo:  15 | Andar:  6 | Dir: Descendo       │
│ Ocupação: 3/8                                │
└──────────────────────────────────────────────┘
Chamadas ativas:
  • Andar 4: 3 pessoa(s) | Espera: 4 ciclos
[PROXIMIDADE] Chamada próxima detectada no andar 4

🎯 DECISÃO: Parar no andar 4
  ├─ Deslocando: andar 6 → 5
  ├─ Deslocando: andar 5 → 4
  └─ 🚪 PARADA no andar 4
```

---

##  Próximas Evoluções

- Aprimorar SmartStop usando heurísticas ou ML  
- Criar simulação gráfica (SDL/Python GUI)  
- Gerar métricas de SLA de elevadores  
- Comparar diferentes estratégias de controle  
- Suporte a múltiplos elevadores em grupo  

---
##  Análise de logs e Geração de Relatórios (Ferramenta de Qualidade)

O SmartStop agora conta com uma ferramenta opcional para análise de logs da simulação, permitindo transformar os dados brutos do terminal em tabelas, relatórios e métricas de operação.

A ferramenta está localizada em:
tools/analisar_smartstop.py

Como funciona

1- Execute a simulação normalmente pelo Serial (VS Code, PuTTY ou outro).

2- Ative o recurso Logging da ferramenta escolhida e salve o log como, por exemplo:
smartstop_log.txt

3- em seu computador, execute:
python tools/analisar_smartstop.py

Resultados gerados automaticamente

Após processar os logs, o script cria:

             Arquivo                                           Conteúdo                                   
  ------------------------------------          ------------------------------------------ 
  `*_data.csv`                                  Dados estruturados, prontos para Excel     
  `*_data.xlsx`                                 Planilha com tabelas organizadas           
  `*_summary.txt`                               Resumo com métricas de qualidade           
  `smartstop_consolidated_summary.txt`          Consolidação de múltiplos logs (se houver) 
  
  
Métricas extraídas

1. Ocupação média por ciclo

2. Tempo de espera por andar

3. Ciclos críticos (emergência / alta espera)

4. Andares mais movimentados

5. Eficiência de cada decisão

6. Número de paradas ignoradas / justificativas

7.Análise de SmartStop (quando evitou ou quando não deveria evitar)

Objetivo

Esta ferramenta permite avaliar:

Qualidade operacional, Riscos e comportamentos críticos, Eficiência da lógica SmartStop, Padrões de atendimento e, Comparação entre diferentes simulações

É um recurso essencial para estudo de qualidade, segurança e análise de riscoem em transporte vertical.
---
##  Autor

**Joacy Raimundo França**  
Desenvolvedor | Sistemas Embarcados | Automação | Simulações  

---

##  Contribuições

Contribuições são bem-vindas!  
Sinta-se à vontade para abrir *Issues* e *Pull Requests* com sugestões, melhorias e correções.

---

##  Licença

Licenciado sob MIT License.
