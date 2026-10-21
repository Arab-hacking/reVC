#ifndef SAMP_CLIENT_QUERY_H
#define SAMP_CLIENT_QUERY_H

// Standalone SA-MP 0.3.7 UDP *server-info query*. NOT a RakNet session:
// this cannot join, send chat, spawn players, or authenticate a game client.
// This project has no dependency on reVC or RenderWare.
#include <stddef.h>
#include <stdint.h>
#include <string>

namespace sampquery {

struct ServerInfo {
    bool password;
    uint16_t players, maxPlayers;
    std::string hostname, gamemode, language;
    ServerInfo() : password(false), players(0), maxPlayers(0) {}
};

bool ParseIPv4(const char *text, uint8_t octets[4]);
bool BuildInfoQuery(const char *ipv4, uint16_t port, uint8_t out[11]);
bool ParseInfoResponse(const uint8_t *data, size_t length,
                       const uint8_t request[11], ServerInfo &out);

enum Status { DISABLED, WAITING, SUCCEEDED, TIMED_OUT, FAILED };

// Non-blocking, single-use IPv4 UDP probe. Only a matching source endpoint
// AND the echoed 11-byte query header may update its result. Poll from the
// caller's event loop; the network library never touches game entities.
class Probe {
public:
    Probe();
    ~Probe();
    Probe(const Probe &) = delete;
    Probe &operator=(const Probe &) = delete;

    bool Start(const char *ipv4, uint16_t port, uint32_t nowMs);
    Status Poll(uint32_t nowMs);
    void Reset();
    Status GetStatus() const { return status_; }
    const ServerInfo &GetInfo() const { return info_; }
    const std::string &GetError() const { return error_; }

private:
    void Close();
    bool SendRequest();
    void Fail(const char *reason);

    uintptr_t socket_;
    bool socketReady_;
    bool ownsWinsock_;
    uint8_t query_[11];
    uint32_t peerAddress_;
    uint16_t peerPort_;
    bool resent_;
    uint32_t startedMs_;
    Status status_;
    ServerInfo info_;
    std::string error_;
};

} // namespace sampquery
#endif // SAMP_CLIENT_QUERY_H
