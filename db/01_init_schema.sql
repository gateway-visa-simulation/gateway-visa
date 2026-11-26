-- ==========================================
-- ESTRUTURA DO BANCO DE DADOS (DDL)
-- ==========================================

-- Criação do Schema
CREATE SCHEMA IF NOT EXISTS visa;

-- 1. Tabela de Emissores (Issuers)
-- Armazena os bancos ou entidades que emitem os cartões.
CREATE TABLE IF NOT EXISTS visa.visanet_issuers (
    issuer_id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    handler_type VARCHAR(20),
    endpoint_config JSONB,
    is_online BOOLEAN DEFAULT true
);

-- 2. Tabela de Adquirentes (Acquirers)
-- Armazena as empresas que processam os pagamentos (ex: a própria OrionPay).
CREATE TABLE IF NOT EXISTS visa.visanet_acquirers (
    acquirer_id VARCHAR(10) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    ip_address VARCHAR(45),
    status VARCHAR(20) DEFAULT 'ACTIVE',
    created_at TIMESTAMP WITHOUT TIME ZONE DEFAULT now()
);

-- 3. Tabela de Roteamento de BINs
-- Define para qual emissor um determinado BIN deve ser roteado.
CREATE TABLE IF NOT EXISTS visa.visanet_bin_table (
    bin_prefix VARCHAR(19) PRIMARY KEY,
    issuer_id VARCHAR(50) REFERENCES visa.visanet_issuers(issuer_id),
    card_brand VARCHAR(20) DEFAULT 'VISA',
    product_type VARCHAR(20),
    country_code VARCHAR(3) DEFAULT 'BRA',
    description VARCHAR(255)
);

-- 4. Tabela de Cartões Simulados
-- Contém os cartões virtuais ou físicos gerados para teste.
CREATE TABLE IF NOT EXISTS visa.visanet_simulated_cards (
    card_number_masked VARCHAR(19) PRIMARY KEY,
    pan VARCHAR(19) NOT NULL UNIQUE,
    bin_prefix VARCHAR(19) REFERENCES visa.visanet_bin_table(bin_prefix),
    issuer_id VARCHAR(50) REFERENCES visa.visanet_issuers(issuer_id),
    balance DECIMAL(15, 2) DEFAULT 0.00,
    daily_limit DECIMAL(15, 2) DEFAULT 5000.00,
    daily_spent DECIMAL(15, 2) DEFAULT 0.00,
    cvv_hash VARCHAR(64),
    is_active BOOLEAN DEFAULT true,
    product_type VARCHAR(20),
    force_response_code VARCHAR(3),
    simulation_delay INTEGER DEFAULT 0,
    pin_block_key VARCHAR(48) -- ZPK em hex (TDES 2-key = 32, 3-key = 48). NULL = chave de teste.
);

-- Migração para bancos já existentes (CREATE TABLE IF NOT EXISTS não adiciona colunas novas)
ALTER TABLE visa.visanet_simulated_cards ADD COLUMN IF NOT EXISTS pin_block_key VARCHAR(48);

-- 5. Tabela de Logs da Rede
-- O coração do monitoramento. Cada transação processada pelo motor C++ deve gerar um registro aqui.
CREATE TABLE IF NOT EXISTS visa.visanet_network_log (
    id SERIAL PRIMARY KEY,
    stan VARCHAR(12),
    mti VARCHAR(4),
    pan_masked VARCHAR(20),
    amount NUMERIC(15, 2),
    processing_code VARCHAR(6),
    terminal_id VARCHAR(8),
    merchant_id VARCHAR(15),
    acquirer_id VARCHAR(10) REFERENCES visa.visanet_acquirers(acquirer_id),
    issuer_id VARCHAR(50) REFERENCES visa.visanet_issuers(issuer_id),
    response_code VARCHAR(20),
    status VARCHAR(50),
    raw_payload_id VARCHAR(100),
    request_timestamp TIMESTAMP WITHOUT TIME ZONE,
    processed_at TIMESTAMP WITHOUT TIME ZONE DEFAULT now()
);

-- 6. Tabela de Códigos de Resposta
-- Tabela de referência para popular seletores na UI e dar contexto aos erros.
CREATE TABLE IF NOT EXISTS visa.visanet_response_codes (
    code VARCHAR(3) PRIMARY KEY,
    description_en VARCHAR(100),
    description_pt VARCHAR(255) NOT NULL,
    is_error BOOLEAN NOT NULL DEFAULT TRUE
);

-- 7. Tabela de Configuração Global do Caos
-- Um registro único para controlar o comportamento geral do simulador.
CREATE TABLE IF NOT EXISTS visa.visanet_config (
    id SERIAL PRIMARY KEY,
    global_latency_ms INTEGER DEFAULT 0,
    simulate_network_instability BOOLEAN DEFAULT false,
    error_probability INTEGER DEFAULT 0,
    cache_refresh_interval_sec INTEGER DEFAULT 30,
    default_response_code VARCHAR(3) DEFAULT '00',
    updated_at TIMESTAMP DEFAULT NOW()
);

-- ==========================================
-- CARGA INICIAL DE DADOS (DML)
-- ==========================================

-- Carga: Configuração Global
INSERT INTO visa.visanet_config (global_latency_ms, simulate_network_instability, error_probability, cache_refresh_interval_sec)
VALUES (0, false, 0, 30);

-- Carga: Códigos de Resposta
INSERT INTO visa.visanet_response_codes (code, description_en, description_pt, is_error) VALUES
('00', 'Approved or completed successfully', 'Transação aprovada com sucesso', false),
('01', 'Refer to card issuer', 'Consulte o emissor', true),
('03', 'Invalid merchant', 'Lojista inválido', true),
('04', 'Pick-up card', 'Retive o cartão (Restrito)', true),
('05', 'Do not honor', 'Transação não autorizada', true),
('12', 'Invalid transaction', 'Transação inválida', true),
('13', 'Invalid amount', 'Valor inválido', true),
('14', 'Invalid card number (PAN)', 'Cartão inválido', true),
('30', 'Format error', 'Erro de formato', true),
('41', 'Lost card', 'Cartão perdido, contate o emissor', true),
('43', 'Stolen card', 'Cartão roubado, contate o emissor', true),
('51', 'Insufficient funds', 'Saldo insuficiente', true),
('54', 'Expired card', 'Cartão vencido', true),
('55', 'Incorrect personal identification number', 'Senha incorreta', true),
('57', 'Transaction not permitted to cardholder', 'Transação não permitida para o cartão', true),
('58', 'Transaction not permitted to terminal', 'Transação não permitida para o terminal', true),
('61', 'Exceeds withdrawal amount limit', 'Excede o limite de valor', true),
('62', 'Restricted card', 'Cartão com restrição', true),
('75', 'Allowable number of PIN tries exceeded', 'Limite de tentativas de senha excedido', true),
('82', 'CVV error', 'Cartão inválido ou erro de CVV', true),
('91', 'Issuer or switch is inoperative', 'Emissor temporariamente indisponível', true),
('96', 'System malfunction', 'Falha no sistema, tente novamente', true)
ON CONFLICT (code) DO UPDATE SET description_pt = EXCLUDED.description_pt, is_error = EXCLUDED.is_error;

-- Carga: Emissores
INSERT INTO visa.visanet_issuers (issuer_id, name, handler_type, endpoint_config, is_online) VALUES
('ORION_ISS', 'Orion Issuing Local', 'IssuerLocal', '{}', true),
('SIMULATOR_ISS', 'Visa Test Issuer', 'INTERNAL', '{}', true)
ON CONFLICT (issuer_id) DO NOTHING;

-- Carga: Tabela de BINs
INSERT INTO visa.visanet_bin_table (bin_prefix, issuer_id, card_brand, product_type, country_code, description) VALUES
('435087', 'ORION_ISS', 'VISA', 'Standard', 'BRA', 'VISA DEFAULT'),
('411111', 'ISS-001', 'VISA', 'Classic', 'BRA', 'VISA Classic'),
('402602', 'SIMULATOR_ISS', 'VISA', 'Electron', 'BRA', 'VISA Electron'),
('422222', 'SIMULATOR_ISS', 'VISA', 'Platinum', 'BRA', 'VISA Platinum'),
('400000', 'SIMULATOR_ISS', 'VISA', 'Debit', 'BRA', 'VISA Debit'),
('444433', 'SIMULATOR_ISS', 'VISA', 'Infinite', 'BRA', 'VISA Infinite'),
('453978', 'SIMULATOR_ISS', 'VISA', 'Corporate', 'BRA', 'VISA Corporate'),
('455611', 'SIMULATOR_ISS', 'VISA', 'Purchasing', 'BRA', 'VISA Purchasing'),
('491622', 'SIMULATOR_ISS', 'VISA', 'Business', 'BRA', 'VISA Business'),
('448500', 'SIMULATOR_ISS', 'VISA', 'Gold', 'BRA', 'VISA Gold'),
('471611', 'SIMULATOR_ISS', 'VISA', 'Signature', 'BRA', 'VISA Signature')
ON CONFLICT (bin_prefix) DO UPDATE SET product_type = EXCLUDED.product_type, description = EXCLUDED.description;

-- Carga: Cartão Simulado Inicial (Baseado nos testes anteriores)
INSERT INTO visa.visanet_simulated_cards (card_number_masked, pan, bin_prefix, issuer_id, balance, is_active)
VALUES ('435087******2776', '4350870193822776', '435087', 'ORION_ISS', 1000.00, true)
ON CONFLICT (pan) DO UPDATE SET issuer_id = EXCLUDED.issuer_id, is_active = true, balance = EXCLUDED.balance;
