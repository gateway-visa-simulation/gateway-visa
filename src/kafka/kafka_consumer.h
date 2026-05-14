#pragma once
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <rdkafkacpp.h>
#include <atomic>

using namespace std;

class KafkaConsumer {
public:
    using MessageHandler = function<void(const string& key, const string& payload)>;

    KafkaConsumer(const string& brokers, const string& group_id);
    ~KafkaConsumer();

    // Inscreve no tópico
    bool subscribe(const vector<string>& topics);

    // Inicia o loop de consumo (bloqueante - deve rodar em thread separada)
    void start_consuming(MessageHandler handler);

    // Sinaliza parada do consumo
    void stop();

private:
    unique_ptr<RdKafka::KafkaConsumer> consumer_;
    atomic<bool> run_{false};
};
