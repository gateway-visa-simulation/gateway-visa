#pragma once
#include <string>
#include <memory>

class Session;

class SessionRegistry {
public:
    static SessionRegistry& get();

    // Register a session for a given STAN (DE11)
    void register_session(const std::string& stan, std::shared_ptr<Session> session);

    // Pop (get and remove) a session for a given STAN
    std::shared_ptr<Session> pop_session(const std::string& stan);

private:
    SessionRegistry();
};
