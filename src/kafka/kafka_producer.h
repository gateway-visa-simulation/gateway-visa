#pragma once
#include <string>
#include <memory>
#include <rdkafkacpp.h>

class KafkaProducer {
public:
    KafkaProducer(const std::string& brokers);
    ~KafkaProducer();

    // Envia mensagem assincronamente.
    // Retorna false apenas se a fila de envio estiver cheia.
    bool send(const std::string& topic, const std::string& key, const std::string& payload);

    // Processa callbacks (delivery reports) pendentes.
    void poll(int timeout_ms = 0);

private:
    class DeliveryReportCb : public RdKafka::DeliveryReportCb {
    public:
        void dr_cb(RdKafka::Message &message) override;
    };

    std::unique_ptr<RdKafka::Producer> producer_;
    DeliveryReportCb dr_cb_;
};
