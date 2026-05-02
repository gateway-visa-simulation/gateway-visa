#include "issuer_service.h"
#include "../common/logger.h"
#include "../security/hsm.h"
#include "../database/database_manager.h"
#include <thread>
#include <chrono>
#include <random>

IssuerService& IssuerService::get_instance() {
    static IssuerService instance;
    return instance;
}

std::string IssuerService::process_transaction(const IsoMessage& req) {
    std::string pan = req.get_field(2);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(card.simulation_delay_ms));
    }

    // A validação de PIN e Limites na versão "IssuerLocal" foi simplificada aqui
    // pois o DatabaseManager lookupCard atual só traz dados parciais.
    // Em uma versão real, o DatabaseManager buscaria saldo e limites e executaria UPDATE atômico.
    // Para manter a compatibilidade com a implementação atual de DatabaseManager, vamos apenas aprovar
    // cartões válidos sem estourar código que falta na interface de database_manager.h atual.

    // Se passou por todas as regras negativas acima, consideraremos aprovado (Mock simplificado)
    LOG_INFO("Transação processada localmente e APROVADA para PAN: {}", req.mask_pan());
    return "00"; // Approved
}

std::string IssuerService::process_reversal(const IsoMessage& req) {
    std::string pan = req.get_field(2);
    if (pan.empty()) return "14";

    // Busca o cartão para garantir que existe
    auto card_opt = DatabaseManager::getInstance().lookupCard(pan);
    if (!card_opt) {
        return "14"; // Invalid card number
    }

    // Em uma implementação real com banco, faríamos um UPDATE para devolver saldo.
    // Como a interface atual do DatabaseManager simplificada não tem isso, retornamos sucesso mock.
    LOG_INFO("Reversal processado localmente e APROVADO para PAN: {}", req.mask_pan());
    return "00";
}
