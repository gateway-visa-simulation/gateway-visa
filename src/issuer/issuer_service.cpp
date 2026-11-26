#include "issuer_service.h"
#include "../common/logger.h"
#include "../security/hsm.h"
#include "../database/database_manager.h"
#include <thread>
#include <chrono>
#include <random>
#include <sstream> // Para std::stringstream
#include <locale>  // Para std::locale::classic()

IssuerService& IssuerService::get_instance() {
    static IssuerService instance;
    return instance;
}

string IssuerService::process_transaction(const IsoMessage& req) {
    string pan = req.get_field(2);
    if (pan.empty()) {
        LOG_WARN("PAN não fornecido na transação.");
        return "14"; // Invalid card number
    }

    // Passo 3: Card Validation (Buscar na visa.visanet_simulated_cards o cartão correspondente)
    // Utiliza Lazy Loading via DatabaseManager
    auto card_opt = DatabaseManager::getInstance().lookupCard(pan);
    if (!card_opt) {
        LOG_WARN("Cartão não encontrado no banco: {}", req.mask_pan());
        return "14"; // Invalid card number
    }
    const auto& card = *card_opt;

    // Se is_active for false, responder RC '04' (Capture Card)
    if (!card.is_active) {
        LOG_WARN("Cartão inativo: {}", req.mask_pan());
        return "04";
    }

    // Se force_response_code não for nulo, ignorar o processamento e usar esse código
    if (card.force_response_code.has_value() && !card.force_response_code.value().empty()) {
        LOG_INFO("Aplicando RC forçado '{}' para o cartão {}", card.force_response_code.value(), req.mask_pan());
        return card.force_response_code.value();
    }

    // Aplicar o simulation_delay específico do cartão, se houver.
    if (card.simulation_delay_ms > 0) {
        LOG_DEBUG("Aplicando delay simulado específico de {}ms para o cartão {}", card.simulation_delay_ms, req.mask_pan());
        this_thread::sleep_for(chrono::milliseconds(card.simulation_delay_ms));
    }

    // Realizar a atualização do saldo e validar fundos
    double amount = 0.0;
    if (req.has_field(4)) {
        // CORREÇÃO LOCALE-SAFE: Usa um stringstream com o locale "classic" para garantir que '.' é o separador.
        std::stringstream ss(req.get_field(4));
        ss.imbue(std::locale::classic());
        double amount_in_cents;
        ss >> amount_in_cents;
        amount = amount_in_cents / 100.0;
    }

    if (card.daily_limit > 0 && (card.daily_spent + amount > card.daily_limit)) {
        LOG_WARN("Transação negada: Limite diário excedido para o PAN: {}", req.mask_pan());
        return "61"; // Exceeds withdrawal limit
    }

    if (!DatabaseManager::getInstance().updateCardBalance(pan, amount)) {
        LOG_WARN("Transação negada: Saldo insuficiente ou cartão inativo para o PAN: {}", req.mask_pan());
        return "51"; // Not sufficient funds
    }

    LOG_INFO("Transação processada localmente e APROVADA para PAN: {}", req.mask_pan());
    return "00"; // Approved
}

string IssuerService::process_reversal(const IsoMessage& req) {
    string pan = req.get_field(2);
    if (pan.empty()) return "14";

    // Busca o cartão para garantir que existe
    auto card_opt = DatabaseManager::getInstance().lookupCard(pan);
    if (!card_opt) {
        return "14"; // Invalid card number
    }

    double amount = 0.0;
    if (req.has_field(4)) {
        // CORREÇÃO LOCALE-SAFE: Usa um stringstream com o locale "classic"
        std::stringstream ss(req.get_field(4));
        ss.imbue(std::locale::classic());
        double amount_in_cents;
        ss >> amount_in_cents;
        amount = amount_in_cents / 100.0;
    }

    // Realiza o estorno do saldo
    if (!DatabaseManager::getInstance().refundCardBalance(pan, amount)) {
        LOG_WARN("Falha ao processar estorno e devolver saldo para o PAN: {}", req.mask_pan());
        return "96"; // System Malfunction
    }

    LOG_INFO("Reversal processado localmente e APROVADO para PAN: {}", req.mask_pan());
    return "00";
}