#include "SampQuery.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <thread>

namespace {
uint32_t NowMilliseconds()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        std::fprintf(stderr, "Usage: samp-probe <numeric IPv4> <port>\n"
                             "Queries server info only; does NOT join the SA-MP game.\n");
        return 2;
    }
    errno = 0;
    char *end = nullptr;
    unsigned long port = std::strtoul(argv[2], &end, 10);
    if(errno || *argv[2] == '\0' || *end != '\0' || port == 0 || port > 65535) {
        std::fprintf(stderr, "Invalid UDP port (expected 1..65535).\n");
        return 2;
    }

    sampquery::Probe probe;
    if(!probe.Start(argv[1], static_cast<uint16_t>(port), NowMilliseconds())) {
        std::fprintf(stderr, "Query failed: %s\n", probe.GetError().c_str());
        return 1;
    }
    sampquery::Status state;
    do {
        state = probe.Poll(NowMilliseconds());
        if(state == sampquery::WAITING)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while(state == sampquery::WAITING);

    if(state == sampquery::SUCCEEDED) {
        const sampquery::ServerInfo &info = probe.GetInfo();
        std::printf("%s:%lu | %s | %u/%u players | mode=%s | language=%s | password=%s\n",
                    argv[1], port, info.hostname.c_str(), info.players,
                    info.maxPlayers, info.gamemode.c_str(), info.language.c_str(),
                    info.password ? "yes" : "no");
        std::puts("Info query succeeded. No RakNet/game session was established.");
        return 0;
    }
    std::fprintf(stderr, "No server info: %s. This does not prove game login would fail.\n",
                 state == sampquery::TIMED_OUT ? "query timeout" : probe.GetError().c_str());
    return 1;
}
