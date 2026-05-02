#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <rdkafkacpp.h>
#include <atomic>

class KafkaConsumer {
public:
    using MessageHandler = std::function<void(const std::string& key, const std::string& payload)>;

    KafkaConsumer(const std::string& brokers, const std::string& group_id);
    ~KafkaConsumer();

    // Inscreve no tópico
    bool subscribe(const std::vector<std::string>& topics);

    // Inicia o loop de consumo (bloqueante - deve rodar em thread separada)
    void start_consuming(MessageHandler handler);

    // Sinaliza parada do consumo
    void stop();

private:
    std::unique_ptr<RdKafka::KafkaConsumer> consumer_;
    std::atomic<bool> run_{false};
};
