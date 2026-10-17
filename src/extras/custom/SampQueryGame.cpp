#include "common.h"
#include "SampQueryGame.h"
#include "SampQuery.h"

#include <cstdio>
#include <cstring>

namespace SampQueryGame {
namespace {
    sampquery::Probe probe;
    char serverIPv4[16] = {};
    uint16_t serverPort = 0;
    bool attempted = false;
    bool reported = false;
}

void Configure(const char *ipv4, uint32_t port)
{
    probe.Reset();
    attempted = reported = false;
    serverPort = 0;
    serverIPv4[0] = '\0';
    uint8_t octets[4];
    if(port < 1 || port > 65535 || !sampquery::ParseIPv4(ipv4, octets)) {
        REVC_LOG_WARNING("SA-MP server-info query disabled: use numeric IPv4 and port 1..65535");
        return;
    }
    std::snprintf(serverIPv4, sizeof(serverIPv4), "%u.%u.%u.%u",
                  octets[0], octets[1], octets[2], octets[3]);
    serverPort = (uint16_t)port;
    REVC_LOG_INFO("SA-MP server-info query configured for %s:%u (no game connection)",
                  serverIPv4, serverPort);
}

void Process(uint32_t nowMs)
{
    if(serverPort == 0 || reported) return;
    if(!attempted) {
        attempted = true;
        if(!probe.Start(serverIPv4, serverPort, nowMs)) {
            REVC_LOG_WARNING("SA-MP server-info query failed to start: %s",
                              probe.GetError().c_str());
            reported = true;
            return;
        }
    }
    sampquery::Status state = probe.Poll(nowMs);
    if(state == sampquery::SUCCEEDED) {
        const sampquery::ServerInfo &info = probe.GetInfo();
        REVC_LOG_INFO("SA-MP server info %s:%u: %s, %u/%u players, mode=%s, language=%s, password=%d; game session NOT connected",
                      serverIPv4, serverPort, info.hostname.c_str(), info.players,
                      info.maxPlayers, info.gamemode.c_str(), info.language.c_str(), info.password);
        reported = true;
    }else if(state == sampquery::TIMED_OUT || state == sampquery::FAILED) {
        REVC_LOG_WARNING("SA-MP server-info query %s:%u: %s; game session NOT connected",
                          serverIPv4, serverPort,
                          state == sampquery::TIMED_OUT ? "no response (timeout)" : probe.GetError().c_str());
        reported = true;
    }
}

void Shutdown()
{
    probe.Reset();
    attempted = reported = false;
}

} // namespace SampQueryGame
