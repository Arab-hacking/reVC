#ifndef REVC_SAMP_QUERY_GAME_H
#define REVC_SAMP_QUERY_GAME_H

#include <stdint.h>

// Opt-in server discovery for the planned SA-MP client. No game-session or
// player synchronization is performed by this probe.
namespace SampQueryGame {
    void Configure(const char *ipv4, uint32_t port);
    void Process(uint32_t nowMs);
    void Shutdown();
}

#endif
