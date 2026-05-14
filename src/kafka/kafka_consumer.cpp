#include "kafka_consumer.h"
#include "../common/logger.h"



KafkaConsumer::KafkaConsumer(const string& brokers, const string& group_id) {
    string errstr;

    unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    if (conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) {
        throw runtime_error("Kafka Conf failed: " + errstr);
    }

    if (conf->set("group.id", group_id, errstr) != RdKafka::Conf::CONF_OK) {
        throw runtime_error("Group ID failed: " + errstr);
    }

    if (conf->set("auto.offset.reset", "earliest", errstr) != RdKafka::Conf::CONF_OK) {
        LOG_WARN("auto.offset.reset earliest error: {}", errstr);
    }

    if (conf->set("enable.auto.commit", "false", errstr) != RdKafka::Conf::CONF_OK) {
        LOG_WARN("enable.auto.commit false error: {}", errstr);
    }

    consumer_.reset(RdKafka::KafkaConsumer::create(conf.get(), errstr));
    if (!consumer_) {
        LOG_ERROR("Falha ao criar o consumidor Kafka: {}", errstr);
        throw runtime_error("Consumer create failed: " + errstr);
    }

    LOG_INFO("Consumidor Kafka criado. Brokers: {}, Group: {}", brokers, group_id);
}

KafkaConsumer::~KafkaConsumer() {
    stop();
    if (consumer_) {
        LOG_INFO("Fechando conexão do consumidor...");
        consumer_->close();
    }
}

bool KafkaConsumer::subscribe(const vector<string>& topics) {
    if (!consumer_) return false;

    RdKafka::ErrorCode err = consumer_->subscribe(topics);
    if (err != RdKafka::ERR_NO_ERROR) {
        LOG_ERROR("Falha ao se inscrever nos tópicos: {}", RdKafka::err2str(err));
        return false;
    }
    LOG_INFO("Consumidor Kafka inscrito em {} topicos.", topics.size());
    return true;
}

void KafkaConsumer::stop() {
    run_ = false;
}

void KafkaConsumer::start_consuming(MessageHandler handler) {
    if (!consumer_) return;

    run_ = true;
    LOG_INFO("Iniciando loop de consumo Kafka...");

    while (run_) {
        unique_ptr<RdKafka::Message> msg(consumer_->consume(500));

        if (msg->err() == RdKafka::ERR__TIMED_OUT) {
            continue;
        }

        if (msg->err() == RdKafka::ERR_NO_ERROR) {
            LOG_DEBUG("Lida nova mensagem no tópico {} partition {} offset {}",
                      msg->topic_name(), msg->partition(), msg->offset());

            string key(msg->key() ? *msg->key() : "");
            string payload(static_cast<const char*>(msg->payload()), msg->len());

            try {
                handler(key, payload);
                consumer_->commitSync(msg.get());
            } catch (const exception& e) {
                LOG_ERROR("Erro ao processar mensagem do Kafka (key: {}): {}", key, e.what());
            }
        } else if (msg->err() == RdKafka::ERR__PARTITION_EOF) {
            LOG_DEBUG("Fim da partição {}/{}", msg->topic_name(), msg->partition());
        } else if (msg->err() == RdKafka::ERR__UNKNOWN_PARTITION || msg->err() == RdKafka::ERR__UNKNOWN_TOPIC) {
            // Em ambiente de teste onde o topico ainda não existe (auto-create-topics no broker),
            // isso vai spammar de inicio e não deve parar a thread de consumo
            continue;
        } else {
            // "Broker: Unknown topic or partition"
            string errStr = msg->errstr();
            if (errStr.find("Unknown topic") != string::npos) {
                 continue; // Ignora auto-create aguardando o producer
            }

            LOG_ERROR("Erro fatal no consumo Kafka: {}", errStr);
            run_ = false; // Interrompe loop apenas para erros realmente criticos de IO
        }
    }

    LOG_INFO("Loop de consumo Kafka encerrado.");
}
