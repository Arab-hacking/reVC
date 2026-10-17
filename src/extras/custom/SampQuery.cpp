#include "SampQuery.h"

#include <cstring>

#if defined(_WIN32)
// Winsock must precede windows.h (and is deliberately absent from the header).
#include <winsock2.h>
#include <ws2tcpip.h>
#elif (defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)) && !defined(__SWITCH__)
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#define REVC_SAMP_NO_SOCKET
#endif

namespace sampquery {
namespace {

static const uintptr_t NO_SOCKET = ~uintptr_t(0);
static const uint32_t QUERY_TIMEOUT_MS = 3500;

bool ReadU16(const uint8_t *data, size_t size, size_t &pos, uint16_t &out)
{
    if(pos > size || size-pos < 2) return false;
    out = uint16_t(data[pos]) | uint16_t(data[pos+1]) << 8;
    pos += 2;
    return true;
}

bool ReadText(const uint8_t *data, size_t size, size_t &pos, size_t max,
              std::string &out)
{
    if(pos > size || size-pos < 4) return false;
    uint32_t length = uint32_t(data[pos]) | uint32_t(data[pos+1]) << 8 |
                      uint32_t(data[pos+2]) << 16 | uint32_t(data[pos+3]) << 24;
    pos += 4;
    if(length > max || length > size-pos) return false;
    out.assign(reinterpret_cast<const char *>(data+pos), length);
    pos += length;
    // A server name is untrusted text; never allow it to inject extra lines
    // or terminal control characters into debug.log.
    for(size_t i = 0; i < out.size(); i++)
        if((uint8_t)out[i] < 32 || (uint8_t)out[i] == 127) out[i] = ' ';
    return true;
}

} // namespace

bool ParseIPv4(const char *text, uint8_t octets[4])
{
    if(!text || !octets) return false;
    const char *p = text;
    for(int i = 0; i < 4; i++) {
        if(*p < '0' || *p > '9') return false;
        unsigned value = 0, digits = 0;
        while(*p >= '0' && *p <= '9') {
            if(++digits > 3) return false;
            value = value*10 + (*p++ - '0');
        }
        if(value > 255) return false;
        octets[i] = (uint8_t)value;
        if(i < 3) {
            if(*p++ != '.') return false;
        }else if(*p != '\0') return false;
    }
    return true;
}

bool BuildInfoQuery(const char *ipv4, uint16_t port, uint8_t out[11])
{
    if(!out || port == 0 || !ParseIPv4(ipv4, out+4)) return false;
    std::memcpy(out, "SAMP", 4);
    out[8] = (uint8_t)port;
    out[9] = (uint8_t)(port >> 8);
    out[10] = 'i';
    return true;
}

bool ParseInfoResponse(const uint8_t *data, size_t length,
                       const uint8_t request[11], ServerInfo &out)
{
    if(!data || !request || length < 11 + 1 + 2 + 2 + 12 || length > 2048 ||
       std::memcmp(data, request, 11) != 0) return false;
    size_t pos = 11;
    ServerInfo candidate;
    if(data[pos] > 1) return false;
    candidate.password = data[pos++] != 0;
    if(!ReadU16(data, length, pos, candidate.players) ||
       !ReadU16(data, length, pos, candidate.maxPlayers) ||
       !ReadText(data, length, pos, 255, candidate.hostname) ||
       !ReadText(data, length, pos, 64, candidate.gamemode) ||
       !ReadText(data, length, pos, 64, candidate.language) ||
       pos != length) return false;
    out = candidate; // never expose a partial result on a malformed packet
    return true;
}

Probe::Probe() : socket_(NO_SOCKET), socketReady_(false), ownsWinsock_(false),
                 peerAddress_(0), peerPort_(0), resent_(false), startedMs_(0),
                 status_(DISABLED)
{
    std::memset(query_, 0, sizeof(query_));
}

Probe::~Probe()
{
    Close();
}

void Probe::Close()
{
#ifndef REVC_SAMP_NO_SOCKET
    if(socketReady_) {
#ifdef _WIN32
        closesocket((SOCKET)socket_);
#else
        close((int)socket_);
#endif
    }
#ifdef _WIN32
    if(ownsWinsock_) WSACleanup();
#endif
#endif
    socket_ = NO_SOCKET;
    socketReady_ = ownsWinsock_ = false;
}

void Probe::Reset()
{
    Close();
    peerAddress_ = 0;
    peerPort_ = 0;
    resent_ = false;
    startedMs_ = 0;
    status_ = DISABLED;
    info_ = ServerInfo();
    error_.clear();
}

void Probe::Fail(const char *reason)
{
    error_ = reason;
    status_ = FAILED;
    Close();
}

bool Probe::SendRequest()
{
#ifdef REVC_SAMP_NO_SOCKET
    return false;
#else
    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = peerAddress_;
    to.sin_port = peerPort_;
#ifdef _WIN32
    int sent = sendto((SOCKET)socket_, (const char *)query_, sizeof(query_), 0,
                      (sockaddr *)&to, sizeof(to));
#else
    int sent = (int)sendto((int)socket_, query_, sizeof(query_), 0,
                           (sockaddr *)&to, sizeof(to));
#endif
    return sent == (int)sizeof(query_);
#endif
}

bool Probe::Start(const char *ipv4, uint16_t port, uint32_t nowMs)
{
    Reset();
    if(!BuildInfoQuery(ipv4, port, query_)) {
        Fail("expected numeric IPv4 and port 1..65535");
        return false;
    }
#ifdef REVC_SAMP_NO_SOCKET
    Fail("UDP sockets are not supported on this platform");
    return false;
#else
#ifdef _WIN32
    WSADATA ws;
    if(WSAStartup(MAKEWORD(2, 2), &ws) != 0) {
        Fail("Winsock initialization failed");
        return false;
    }
    ownsWinsock_ = true;
    SOCKET fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(fd == INVALID_SOCKET) {
        Fail("UDP socket creation failed");
        return false;
    }
    socket_ = (uintptr_t)fd;
    socketReady_ = true;
    u_long nonblocking = 1;
    if(ioctlsocket(fd, FIONBIO, &nonblocking) != 0) {
        Fail("cannot make UDP socket non-blocking");
        return false;
    }
#else
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0) {
        Fail("UDP socket creation failed");
        return false;
    }
    socket_ = (uintptr_t)fd;
    socketReady_ = true;
    if(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
        Fail("cannot make UDP socket non-blocking");
        return false;
    }
#endif
    std::memcpy(&peerAddress_, query_+4, 4);
    peerPort_ = htons(port);
    startedMs_ = nowMs;
    if(!SendRequest()) {
        Fail("UDP query send failed");
        return false;
    }
    status_ = WAITING;
    return true;
#endif
}

Status Probe::Poll(uint32_t nowMs)
{
    if(status_ != WAITING) return status_;
#ifndef REVC_SAMP_NO_SOCKET
    // At most 16 datagrams per frame, so a hostile UDP sender cannot stall
    // the render/game thread. Ignore unsolicited sources and malformed data.
    for(int i = 0; i < 16; i++) {
        uint8_t bytes[2048];
        sockaddr_in from = {};
#ifdef _WIN32
        int fromSize = sizeof(from);
        int count = recvfrom((SOCKET)socket_, (char *)bytes, sizeof(bytes), 0,
                             (sockaddr *)&from, &fromSize);
        if(count == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if(error == WSAEWOULDBLOCK || error == WSAEINTR) break;
            Fail("UDP query receive failed");
            return status_;
        }
#else
        socklen_t fromSize = sizeof(from);
        int count = (int)recvfrom((int)socket_, bytes, sizeof(bytes), 0,
                                  (sockaddr *)&from, &fromSize);
        if(count < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
            Fail("UDP query receive failed");
            return status_;
        }
#endif
        if(fromSize != sizeof(from) || from.sin_family != AF_INET ||
           from.sin_addr.s_addr != peerAddress_ || from.sin_port != peerPort_)
            continue;
        ServerInfo candidate;
        if(ParseInfoResponse(bytes, count, query_, candidate)) {
            info_ = candidate;
            status_ = SUCCEEDED;
            Close();
            return status_;
        }
    }
    const uint32_t elapsed = nowMs - startedMs_; // safe across clock wrap
    if(elapsed >= QUERY_TIMEOUT_MS) {
        status_ = TIMED_OUT;
        Close();
    } else if(!resent_ && elapsed >= 1000) {
        resent_ = true;
        if(!SendRequest()) Fail("UDP query retry failed");
    }
#endif
    return status_;
}

} // namespace sampquery
