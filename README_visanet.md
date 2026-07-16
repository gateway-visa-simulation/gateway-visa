# Desafio: Simulador do Switch VISA (Simulation) para Homologação de Adquirência

## 🎯 Objetivo do Projeto

Construir um "mini payment switch" em C++17, inspirado na arquitetura da VisaNet, capaz de simular o roteamento e processamento de transações ISO 8583. Este simulador visa facilitar a homologação de adquirentes, permitindo que testem suas integrações de forma eficiente e controlada.

## 🚀 Funcionalidades Atuais

O projeto implementa os seguintes componentes e funcionalidades:

### 1. ISO 8583 Engine (Core Crítico)
-   **Parsing e Montagem:** Motor completo para mensagens ISO 8583.
-   **MTI Suportados:** 0100 (Autorização), 0110 (Resposta de Autorização), 0200 (Transação Financeira), 0210 (Resposta de Transação Financeira).
-   **Bitmap:** Suporte a bitmap primário (64 bits) e secundário (128 bits).
-   **Campos Variáveis:** Suporte a formatos `FIXED`, `LLVAR` e `LLLVAR`.
-   **Encoding:** Suporte a `ASCII` e `BCD` (Binary Coded Decimal).
-   **Campos Mínimos:** DE2 (PAN), DE3 (Processing Code), DE4 (Amount), DE7 (Transmission Date & Time), DE11 (STAN), DE39 (Response Code), DE41 (Terminal ID), DE42 (Merchant ID), DE49 (Currency Code).

### 2. TCP Gateway (Entrada/Saída)
-   **Servidor Assíncrono:** Implementado com Asio (standalone), suportando múltiplas conexões simultâneas.
-   **Protocolo:** Utiliza um cabeçalho de 2 bytes (Big Endian) para indicar o tamanho do payload binário ISO 8583.
-   **Reuso de Endereço:** Configurado com `SO_REUSEADDR` para permitir reinícios rápidos do servidor.

### 3. Switch Core (Roteamento em Memória)
-   **Roteamento por BIN:** Identifica o BIN (primeiros 6 dígitos do PAN) e roteia a transação para um emissor simulado.
-   **Tabela de Roteamento:** Atualmente configurada em memória (`main.cpp`) para BINs específicos (ex: `123456`, `654321`, `435087`).

### 4. Issuer Simulator (Banco Emissor Mock)
-   **Serviço Simulado:** Mantém contas com saldo e status de cartão em memória.
-   **Processamento:** Simula autorizações (0100 -> 0110).
-   **Regras:**
    -   Saldo suficiente -> `00` (Aprovado)
    -   Saldo insuficiente -> `51`
    -   Cartão inválido -> `14`

### 5. Segurança (Nível Simulado)
-   **Mask PAN:** Mascara o PAN em logs (ex: `123456******3456`).
-   **HSM Simulado:** Stub para criptografia/decriptografia de dados sensíveis (usando XOR simples) e tokenização de PAN.

### 6. Observabilidade
-   **Logs Estruturados:** Utiliza `spdlog` para logs informativos, de debug e de erro.

## 🧱 Estrutura do Projeto

```
/gateway-visa
├── src/
│   ├── common/
│   │   ├── logger.h
│   │   └── logger.cpp
│   ├── iso8583/
│   │   ├── bitmap.h
│   │   ├── bitmap.cpp
│   │   ├── field_def.h
│   │   ├── field_def.cpp
│   │   ├── iso_message.h
│   │   ├── iso_message.cpp
│   │   ├── iso_parser.h
│   │   ├── iso_parser.cpp
│   │   ├── iso_builder.h
│   │   └── iso_builder.cpp
│   ├── issuer/
│   │   ├── issuer_service.h
│   │   └── issuer_service.cpp
│   ├── network/
│   │   ├── session.h
│   │   ├── session.cpp
│   │   ├── tcp_server.h
│   │   └── tcp_server.cpp
│   ├── switch/
│   │   ├── router.h
│   │   └── router.cpp
│   └── main.cpp
├── CMakeLists.txt
└── client_test.py
```

## ⚙️ Requisitos Técnicos

-   **Linguagem:** C++17+
-   **Compilador:** GCC/Clang (Linux/WSL) ou MSVC (Windows)
-   **Sistema de Build:** CMake 3.17+
-   **Bibliotecas (gerenciadas via FetchContent):**
    -   Asio (rede assíncrona)
    -   spdlog (logging)

## 🚀 Como Compilar e Executar

### Pré-requisitos
-   Um compilador C++17 (g++, clang++ ou MSVC).
-   CMake 3.17+.
-   Python 3 (para o script de teste).

### Compilação
1.  **Crie um diretório de build:**
    ```bash
    mkdir build && cd build
    ```
2.  **Configure o CMake:**
    ```bash
    cmake ..
    ```
    *Se estiver no Windows com CLion, o CLion gerencia isso automaticamente. Certifique-se de ter o `ninja` instalado no WSL (`sudo apt install ninja-build`).*
3.  **Compile o projeto:**
    ```bash
    cmake --build . -j $(nproc) # ou -j 14 no Windows
    ```

### Execução
1.  **Inicie o Gateway:**
    ```bash
    ./gateway_visa
    ```
    O servidor estará escutando na porta `8889`.
2.  **Execute o Cliente de Teste:**
    ```bash
    python3 client_test.py
    ```
    Este script enviará transações de exemplo para o gateway.

## 🔒 Segurança (Simulada)

-   **Mask PAN:** O sistema mascara automaticamente o PAN em todos os logs para evitar exposição de dados sensíveis.
-   **HSM Simulado:** Um módulo stub de HSM está presente para simular operações como criptografia/decriptografia de dados e tokenização, embora não esteja totalmente integrado ao fluxo atual.

## 📈 Próximos Passos de Desenvolvimento

Para evoluir este simulador para um sistema mais robusto e fiel a uma rede de pagamentos real, as seguintes funcionalidades são sugeridas:

1.  **Integração com Banco de Dados (PostgreSQL):**
    -   Substituir o roteamento e a validação de cartão em memória por consultas dinâmicas a um banco de dados PostgreSQL.
    -   Implementar um `DatabaseManager` Singleton com pool de conexões e cache em memória para alta performance.
    -   Consultar tabelas `visanet_config`, `visanet_bin_table`, `visanet_simulated_cards` para lógica de negócio.
    -   Logar transações em `visanet_network_log`.

2.  **Integração (Lock-Free):**
    -   Publicações/consumo via processamento local lock-free; Kafka foi removido desta versão.

    Configuração e uso (Lock‑Free):
    -  Variáveis/flags: use o campo "development.enable_network_bypass" no config.json para habilitar cenários de bypass em desenvolvimento.
    -  Dependências: nenhuma externa necessária — o fluxo utiliza IssuerService e DatabaseManager (em memória/DB).
    -  Como operar: garanta que a tabela de emissores/roteamento (visa.visanet_issuers / visanet_bin_table) aponte para um handler do tipo "INTERNAL" ou "IssuerLocal"; as requisições serão processadas localmente sem broker.
    -  Integração futura: para conectar uma fila lock‑free concreta (ring buffer, SPSC queue), forneça os headers/assinaturas e a integração será adicionada ao fluxo.

    Observação: logs e HSM continuam disponíveis para criptografia e auditoria local.

3.  **Controle de Concorrência e Performance:**
    -   Implementar um `ThreadPool` para separar threads de I/O e threads de processamento de lógica de negócio.
    -   Mecanismos de idempotência para evitar reprocessamento de transações duplicadas.

4.  **Resiliência:**
    -   **Circuit Breaker:** Monitorar falhas por emissor e abrir o circuito para evitar sobrecarga.
    -   **Timeout e Retry:** Implementar timeouts para comunicação com emissores e retries controlados.

5.  **Observabilidade Avançada:**
    -   Métricas de TPS, latência média e contagem de códigos de resposta.
    -   Exportação de logs estruturados (JSON).

6.  **Funcionalidades ISO 8583 Expandidas:**
    -   Suporte a mais MTIs (ex: 0400/0410 para Reversal).
    -   Validação rigorosa de campos e tamanhos.

---
