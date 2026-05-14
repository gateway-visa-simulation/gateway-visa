#pragma once
#include <string>
#include <memory>
#include <rdkafkacpp.h>

using namespace std;

class KafkaProducer {
public:
    KafkaProducer(const string& brokers);
    ~KafkaProducer();

    // Envia mensagem assincronamente.
    // Retorna false apenas se a fila de envio estiver cheia.
    bool send(const string& topic, const string& key, const string& payload);

    // Processa callbacks (delivery reports) pendentes.
    void poll(int timeout_ms = 0);

private:
    class DeliveryReportCb : public RdKafka::DeliveryReportCb {
    public:
        void dr_cb(RdKafka::Message &message) override;
    };

    unique_ptr<RdKafka::Producer> producer_;
    DeliveryReportCb dr_cb_;
};
