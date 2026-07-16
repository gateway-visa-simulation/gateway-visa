#include "session_registry.h"
#include "session.h"
#include <map>
#include <mutex>

using namespace std;

struct SessionRegistryImpl {
    map<string, weak_ptr<Session>> sessions;
    mutex mtx;
};

static SessionRegistryImpl g_impl;

SessionRegistry& SessionRegistry::get() {
    static SessionRegistry instance;
    return instance;
}

SessionRegistry::SessionRegistry() {}

void SessionRegistry::register_session(const std::string& stan, std::shared_ptr<Session> session) {
    lock_guard<mutex> lock(g_impl.mtx);
    g_impl.sessions[stan] = session;
}

std::shared_ptr<Session> SessionRegistry::pop_session(const std::string& stan) {
    lock_guard<mutex> lock(g_impl.mtx);
    auto it = g_impl.sessions.find(stan);
    if (it == g_impl.sessions.end()) return nullptr;
    auto sp = it->second.lock();
    g_impl.sessions.erase(it);
    return sp;
}
