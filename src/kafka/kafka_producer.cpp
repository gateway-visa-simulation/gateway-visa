#include "kafka_producer.h"
#include "../common/logger.h"
#include <iostream>

void KafkaProducer::DeliveryReportCb::dr_cb(RdKafka::Message &message) {
    if (message.err()) {
        LOG_ERROR("Falha ao entregar mensagem para tópico {} [{}] - Erro: {}",
                  message.topic_name(), message.partition(), message.errstr());
    } else {
        LOG_DEBUG("Mensagem entregue para tópico {} [{}] offset {}",
                  message.topic_name(), message.partition(), message.offset());
    }
}

KafkaProducer::KafkaProducer(const string& brokers) {
    string errstr;

    unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    if (conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERROR("Erro ao configurar brokers Kafka: {}", errstr);
        throw runtime_error("Kafka Conf falhou: " + errstr);
    }

    if (conf->set("acks", "all", errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERROR("Erro ao configurar acks: {}", errstr);
    }

    if (conf->set("enable.idempotence", "true", errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERROR("Erro ao configurar idempotencia: {}", errstr);
    }

    if (conf->set("retries", "3", errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERROR("Erro ao configurar retries: {}", errstr);
    }

    if (conf->set("dr_cb", &dr_cb_, errstr) != RdKafka::Conf::CONF_OK) {
        LOG_ERROR("Erro ao configurar dr_cb: {}", errstr);
    }

    producer_.reset(RdKafka::Producer::create(conf.get(), errstr));
    if (!producer_) {
        LOG_ERROR("Falha ao criar o produtor Kafka: {}", errstr);
        throw runtime_error("Kafka Producer failed to create: " + errstr);
    }

    LOG_INFO("Produtor Kafka criado com sucesso. Alvo: {}", brokers);
}

KafkaProducer::~KafkaProducer() {
    if (producer_) {
        LOG_INFO("Flushing produtor Kafka (esperando entregas pendentes)...");
        producer_->flush(5000);

        if (producer_->outq_len() > 0) {
            LOG_WARN("Produtor destruído, mas {} mensagens não foram entregues.", producer_->outq_len());
        } else {
            LOG_INFO("Flush concluído com sucesso.");
        }
    }
}

bool KafkaProducer::send(const string& topic, const string& key, const string& payload) {
    if (!producer_) return false;

    RdKafka::ErrorCode err = producer_->produce(
        topic,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY,
        const_cast<char*>(payload.c_str()), payload.size(),
        key.c_str(), key.size(),
        0,
        nullptr,
        nullptr
    );

    if (err == RdKafka::ERR__QUEUE_FULL) {
        producer_->poll(100);
        err = producer_->produce(
            topic,
            RdKafka::Topic::PARTITION_UA,
            RdKafka::Producer::RK_MSG_COPY,
            const_cast<char*>(payload.c_str()), payload.size(),
            key.c_str(), key.size(),
            0,
            nullptr,
            nullptr
        );
    }

    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Falha ao produzir mensagem (key: {}): {}", key, RdKafka::err2str(err));
        return false;
    } else {
        LOG_DEBUG("Mensagem na fila de envio do Kafka. Key: {}", key);
    }

    producer_->poll(0);
    return true;
}

void KafkaProducer::poll(int timeout_ms) {
    if (producer_) {
        producer_->poll(timeout_ms);
    }
}
