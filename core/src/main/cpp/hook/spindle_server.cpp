#include "hook/spindle_server.h"

#include "box_core.h"

#include <android/log.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#define SPDL_TAG "SPDL"
#define SPDL_MAX_FRAME (1 * 1024 * 1024)

namespace {

struct HostLink {
    int fd = -1;
    std::mutex mu;
};

struct Agent {
    int id = 0;
    std::string name;
    int pid = 0;
    int uid = 0;
    bool attached = false;
    bool ready = false;
    std::shared_ptr<HostLink> host;
};

struct SpawnReq {
    std::string name;
    std::vector<int> skip;
    int paused_pid = 0;
};

std::mutex g_mu;
std::vector<std::shared_ptr<Agent>> g_agents;
std::vector<SpawnReq> g_spawning;
int g_next_id = 1;
bool g_started = false;

bool recv_all(int fd, void *buf, size_t len) {
    auto *p = static_cast<uint8_t *>(buf);
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n == 0) {
            return false;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        p += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool send_all(int fd, const void *buf, size_t len) {
    auto *p = static_cast<const uint8_t *>(buf);
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        p += n;
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool send_frame(int fd, const void *payload, uint32_t n) {
    return send_all(fd, &n, sizeof(n)) && (n == 0 || send_all(fd, payload, n));
}

bool send_json(int fd, const std::string &body) {
    return send_frame(fd, body.data(), static_cast<uint32_t>(body.size()));
}

bool recv_frame(int fd, std::string *out) {
    uint32_t n = 0;
    if (!recv_all(fd, &n, sizeof(n)) || n > SPDL_MAX_FRAME) {
        return false;
    }
    out->assign(n, '\0');
    return n == 0 || recv_all(fd, out->data(), n);
}

bool json_true(const std::string &json, const char *key) {
    return json.find(std::string("\"") + key + "\":true") != std::string::npos ||
           json.find(std::string("\"") + key + "\": true") != std::string::npos;
}

std::string json_string(const std::string &json, const char *key) {
    std::string pat = std::string("\"") + key + "\":\"";
    size_t i = json.find(pat);
    if (i == std::string::npos) {
        pat = std::string("\"") + key + "\": \"";
        i = json.find(pat);
    }
    if (i == std::string::npos) {
        return "";
    }
    i += pat.size();
    std::string out;
    while (i < json.size() && json[i] != '"') {
        if (json[i] == '\\' && i + 1 < json.size()) {
            out.push_back(json[i + 1]);
            i += 2;
            continue;
        }
        out.push_back(json[i]);
        i++;
    }
    return out;
}

std::string process_name() {
    char buf[256];
    memset(buf, 0, sizeof(buf));
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return "";
    }
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        return "";
    }
    buf[n] = '\0';
    return std::string(buf);
}

bool black_process(const std::string &name) {
    static const char suffix[] = ":black";
    size_t n = sizeof(suffix) - 1;
    return name.size() >= n && name.compare(name.size() - n, n, suffix) == 0;
}

int listen_port() {
    return 2408;
}

bool is_log_frame(const std::string &frame) {
    return frame.size() >= 4 && frame.compare(0, 3, "LOG") == 0 && frame[3] == '\0';
}

bool is_rpc_frame(const std::string &frame) {
    return frame.size() >= 4 && frame.compare(0, 3, "RPC") == 0 && frame[3] == '\0';
}

void send_host_frame(const std::shared_ptr<HostLink> &link, const void *payload, uint32_t n) {
    if (link == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(link->mu);
    if (link->fd >= 0) {
        send_frame(link->fd, payload, n);
    }
}

std::string list_json() {
    std::lock_guard<std::mutex> lock(g_mu);
    std::string body = "{\"ok\":true,\"agents\":[";
    for (size_t i = 0; i < g_agents.size(); i++) {
        if (i != 0) {
            body += ',';
        }
        body += "{\"name\":\"";
        body += g_agents[i]->name;
        body += "\",\"pid\":";
        body += std::to_string(g_agents[i]->pid);
        body += ",\"uid\":";
        body += std::to_string(g_agents[i]->uid);
        body += '}';
    }
    body += "]}";
    return body;
}

std::string package_of(const std::string &name) {
    size_t colon = name.find(':');
    if (colon == std::string::npos) {
        return name;
    }
    return name.substr(0, colon);
}

bool is_main_name(const std::string &name, const std::string &target) {
    if (name == target) {
        return true;
    }
    return name.find(':') == std::string::npos && package_of(name) == package_of(target);
}

bool pid_skipped(const std::vector<int> &skip, int pid) {
    for (int old : skip) {
        if (old == pid) {
            return true;
        }
    }
    return false;
}

int match_rank(const std::shared_ptr<Agent> &agent, const std::string &target) {
    if (target.compare(0, 4, "pid:") == 0) {
        return agent->pid == atoi(target.c_str() + 4) ? 0 : -1;
    }
    if (agent->name == target) {
        return 0;
    }
    if (package_of(agent->name) == package_of(target)) {
        return 1;
    }
    return -1;
}

bool should_pause(const std::string &name, int pid) {
    std::lock_guard<std::mutex> lock(g_mu);
    for (SpawnReq &req : g_spawning) {
        if (!is_main_name(name, req.name)) {
            continue;
        }
        if (pid_skipped(req.skip, pid)) {
            continue;
        }
        if (req.paused_pid != 0) {
            return false;
        }
        req.paused_pid = pid;
        return true;
    }
    return false;
}

void begin_spawn(const std::string &target, std::vector<int> *skip_out) {
    std::lock_guard<std::mutex> lock(g_mu);
    SpawnReq req;
    req.name = target;
    for (const std::shared_ptr<Agent> &agent : g_agents) {
        if (package_of(agent->name) == package_of(target)) {
            req.skip.push_back(agent->pid);
        }
    }
    *skip_out = req.skip;
    g_spawning.push_back(std::move(req));
}

void clear_spawn(const std::string &target) {
    std::lock_guard<std::mutex> lock(g_mu);
    for (auto it = g_spawning.begin(); it != g_spawning.end();) {
        if (it->name == target) {
            it = g_spawning.erase(it);
        } else {
            ++it;
        }
    }
}

bool wait_for_agent(const std::string &target, const std::vector<int> &skip, int seconds) {
    for (int i = 0; i < seconds * 10; i++) {
        {
            std::lock_guard<std::mutex> lock(g_mu);
            for (const std::shared_ptr<Agent> &agent : g_agents) {
                if (!agent->ready) {
                    continue;
                }
                if (!is_main_name(agent->name, target)) {
                    continue;
                }
                if (pid_skipped(skip, agent->pid)) {
                    continue;
                }
                return true;
            }
        }
        usleep(100 * 1000);
    }
    return false;
}

std::shared_ptr<Agent> claim_agent(const std::string &target, const std::vector<int> *skip, bool newest,
                                    std::string *err) {
    std::lock_guard<std::mutex> lock(g_mu);
    std::shared_ptr<Agent> found;
    int found_rank = 99;
    bool busy = false;
    for (const std::shared_ptr<Agent> &agent : g_agents) {
        int rank = match_rank(agent, target);
        if (rank < 0) {
            continue;
        }
        if (skip != nullptr && pid_skipped(*skip, agent->pid)) {
            continue;
        }
        if (agent->attached) {
            busy = true;
            continue;
        }
        bool better = found == nullptr || rank < found_rank ||
                      (rank == found_rank && (newest ? agent->pid > found->pid : agent->pid < found->pid));
        if (better) {
            found = agent;
            found_rank = rank;
        }
    }
    if (found == nullptr) {
        if (err != nullptr) {
            *err = busy ? "agent busy" : "no such agent";
        }
        return nullptr;
    }
    found->attached = true;
    return found;
}

void release_agent(const std::shared_ptr<Agent> &agent) {
    std::lock_guard<std::mutex> lock(g_mu);
    agent->attached = false;
    agent->host.reset();
}

bool deliver_to_agent(const std::shared_ptr<Agent> &agent, const std::string &frame, std::string *reply) {
    return BoxCore::deliverSpindle(agent->id, frame.data(), static_cast<int>(frame.size()), reply);
}

void serve_host(int fd, const std::string &hello) {
    if (json_true(hello, "list")) {
        send_json(fd, list_json());
        close(fd);
        return;
    }
    std::string target = json_string(hello, "target");
    if (target.empty()) {
        send_json(fd, "{\"ok\":false,\"e\":\"target required\"}");
        close(fd);
        return;
    }
    bool spawn = json_true(hello, "spawn") || json_true(hello, "start");
    std::vector<int> skip;
    if (spawn) {
        if (target.compare(0, 4, "pid:") == 0) {
            send_json(fd, "{\"ok\":false,\"e\":\"spawn needs a process name\"}");
            close(fd);
            return;
        }
        begin_spawn(target, &skip);
        __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "spawn %s", target.c_str());
        std::string pkg = package_of(target);
        if (!BoxCore::launchForSpindle(pkg.c_str())) {
            clear_spawn(target);
            send_json(fd, "{\"ok\":false,\"e\":\"spawn start failed\"}");
            close(fd);
            return;
        }
        if (!wait_for_agent(target, skip, 15)) {
            __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "spawn %s waiting, asking main", target.c_str());
            if (!BoxCore::launchForSpindleOnMain(pkg.c_str()) || !wait_for_agent(target, skip, 15)) {
                clear_spawn(target);
                send_json(fd, "{\"ok\":false,\"e\":\"spawn timed out\"}");
                close(fd);
                return;
            }
        }
    }
    std::string err;
    std::shared_ptr<Agent> agent = claim_agent(target, spawn ? &skip : nullptr, spawn, &err);
    if (spawn) {
        clear_spawn(target);
    }
    if (agent == nullptr) {
        send_json(fd, std::string("{\"ok\":false,\"e\":\"") + err + "\"}");
        close(fd);
        return;
    }
    auto link = std::make_shared<HostLink>();
    link->fd = fd;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        agent->host = link;
    }
    if (!send_json(fd, "{\"ok\":true}")) {
        release_agent(agent);
        close(fd);
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "host attached %s", target.c_str());
    for (;;) {
        std::string frame;
        if (!recv_frame(fd, &frame) || frame.empty()) {
            break;
        }
        std::string reply;
        if (frame == "UNLOCK") {
            if (!deliver_to_agent(agent, frame, &reply)) {
                break;
            }
            continue;
        }
        if (is_rpc_frame(frame)) {
            if (!deliver_to_agent(agent, frame, &reply) || reply.empty() || !send_json(fd, reply)) {
                break;
            }
            continue;
        }
        if (!deliver_to_agent(agent, frame, &reply)) {
            break;
        }
        if (is_log_frame(reply)) {
            send_host_frame(link, reply.data(), static_cast<uint32_t>(reply.size()));
        }
    }
    if (spawn) {
        std::string reply;
        deliver_to_agent(agent, "UNLOCK", &reply);
    }
    release_agent(agent);
    close(fd);
    __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "host detached %s", target.c_str());
}

void handle_client(int fd) {
    std::string frame;
    if (!recv_frame(fd, &frame) || frame.size() < 5) {
        close(fd);
        return;
    }
    if (frame.compare(0, 4, "HOST") == 0 && frame[4] == '\0') {
        serve_host(fd, frame.substr(5));
        return;
    }
    send_json(fd, "{\"ok\":false,\"e\":\"expected HOST\"}");
    close(fd);
}

void *client_thread(void *arg) {
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(arg));
    handle_client(fd);
    return nullptr;
}

void *accept_loop(void *arg) {
    int fd = static_cast<int>(reinterpret_cast<intptr_t>(arg));
    for (;;) {
        int client = accept(fd, nullptr, nullptr);
        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        pthread_t thread;
        pthread_create(&thread, nullptr, client_thread, reinterpret_cast<void *>(static_cast<intptr_t>(client)));
        pthread_detach(thread);
    }
    close(fd);
    return nullptr;
}

}  // namespace

void SpindleServer::start() {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_started) {
        return;
    }
    std::string name = process_name();
    if (!black_process(name)) {
        return;
    }
    int port = listen_port();
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "socket failed errno=%d", errno);
        return;
    }
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 || listen(fd, 8) != 0) {
        __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "listen %d failed errno=%d", port, errno);
        close(fd);
        return;
    }
    g_started = true;
    pthread_t thread;
    pthread_create(&thread, nullptr, accept_loop, reinterpret_cast<void *>(static_cast<intptr_t>(fd)));
    pthread_detach(thread);
    __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "spindle_server :black port=%d", port);
}

int SpindleServer::addAgent(const char *name, int pid, int uid) {
    if (name == nullptr || name[0] == '\0' || strchr(name, '"') != nullptr || strchr(name, '\\') != nullptr) {
        return 0;
    }
    auto agent = std::make_shared<Agent>();
    agent->name = name;
    agent->pid = pid;
    agent->uid = uid;
    should_pause(agent->name, pid);
    {
        std::lock_guard<std::mutex> lock(g_mu);
        agent->id = g_next_id++;
        if (g_next_id <= 0) {
            g_next_id = 1;
        }
        agent->ready = true;
        g_agents.push_back(agent);
    }
    __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "registered %s pid=%d id=%d", name, pid, agent->id);
    return agent->id;
}

void SpindleServer::hostLog(int pid, const char *text) {
    if (text == nullptr) {
        return;
    }
    std::shared_ptr<HostLink> link;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        for (const std::shared_ptr<Agent> &agent : g_agents) {
            if (agent->pid == pid) {
                link = agent->host;
                break;
            }
        }
    }
    std::string frame("LOG", 3);
    frame.push_back('\0');
    frame.append(text);
    send_host_frame(link, frame.data(), static_cast<uint32_t>(frame.size()));
}

void SpindleServer::dropAgent(int id) {
    std::shared_ptr<Agent> agent;
    std::shared_ptr<HostLink> link;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        for (auto it = g_agents.begin(); it != g_agents.end(); ++it) {
            if ((*it)->id == id) {
                agent = *it;
                link = agent->host;
                g_agents.erase(it);
                break;
            }
        }
    }
    if (agent == nullptr) {
        return;
    }
    if (link != nullptr) {
        static const char bye[] = "EXIT";
        send_host_frame(link, bye, sizeof(bye) - 1);
        std::lock_guard<std::mutex> lock(link->mu);
        if (link->fd >= 0) {
            shutdown(link->fd, SHUT_RDWR);
        }
    }
    __android_log_print(ANDROID_LOG_INFO, SPDL_TAG, "agent gone %s pid=%d", agent->name.c_str(), agent->pid);
}
