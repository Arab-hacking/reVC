#include "SampQuery.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// CHECK must run in Release as well (unlike assert, which compiles out under
// NDEBUG). In particular, all network setup is part of the test.
#define CHECK(expr) do { \
    if(!(expr)) { \
        std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        std::abort(); \
    } \
} while(false)

static void U16(std::vector<uint8_t> &b, uint16_t v)
{
    b.push_back((uint8_t)v);
    b.push_back((uint8_t)(v >> 8));
}

static void U32(std::vector<uint8_t> &b, uint32_t v)
{
    U16(b, (uint16_t)v);
    U16(b, (uint16_t)(v >> 16));
}

static void Text(std::vector<uint8_t> &b, const std::string &text)
{
    U32(b, (uint32_t)text.size());
    b.insert(b.end(), text.begin(), text.end());
}

static std::vector<uint8_t> Reply(const uint8_t query[11])
{
    std::vector<uint8_t> reply(query, query+11);
    reply.push_back(1); // passworded
    U16(reply, 14);
    U16(reply, 100);
    Text(reply, "Server\nName");
    Text(reply, "Free roam");
    Text(reply, "Russian");
    return reply;
}

static void TestWireFormat()
{
    uint8_t ip[4], query[11];
    CHECK(sampquery::ParseIPv4("127.0.0.1", ip));
    CHECK(ip[0] == 127 && ip[1] == 0 && ip[2] == 0 && ip[3] == 1);
    CHECK(!sampquery::ParseIPv4("127.0.0", ip));
    CHECK(!sampquery::ParseIPv4("127.0.0.256", ip));
    CHECK(!sampquery::ParseIPv4("1.2.3.4:7777", ip));
    CHECK(!sampquery::ParseIPv4("1.2.3.-1", ip));
    CHECK(!sampquery::ParseIPv4("1.2.3.4\n", ip));
    CHECK(!sampquery::ParseIPv4("1.2.3.4.5", ip));
    CHECK(!sampquery::ParseIPv4("1234.2.3.4", ip));
    CHECK(!sampquery::ParseIPv4("", ip));
    CHECK(!sampquery::ParseIPv4(nullptr, ip));
    CHECK(!sampquery::BuildInfoQuery("127.0.0.1", 0, query));
    CHECK(sampquery::BuildInfoQuery("127.0.0.1", 7777, query));
    const uint8_t expected[11] = { 'S','A','M','P',127,0,0,1,0x61,0x1e,'i' };
    CHECK(std::memcmp(query, expected, sizeof(query)) == 0);

    std::vector<uint8_t> reply = Reply(query);
    sampquery::ServerInfo info;
    CHECK(sampquery::ParseInfoResponse(reply.data(), reply.size(), query, info));
    CHECK(info.password && info.players == 14 && info.maxPlayers == 100);
    CHECK(info.hostname == "Server Name" && info.gamemode == "Free roam" &&
           info.language == "Russian");

    std::vector<uint8_t> broken = reply;
    broken[4] = 10; // echoed IPv4, port and opcode are checked too
    CHECK(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[9] ^= 1;
    CHECK(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[10] = 'c';
    CHECK(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[11] = 2; // invalid password flag
    CHECK(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[16] = 0xff; broken[17] = 0xff;
    broken[18] = 0xff; broken[19] = 0xff; // forged 4-GiB hostname
    CHECK(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken.pop_back();
    CHECK(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken.push_back(0); // unsolicited trailing bytes
    CHECK(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    CHECK(!sampquery::ParseInfoResponse(nullptr, reply.size(), query, info));
    CHECK(info.hostname == "Server Name"); // malformed packets cannot replace result
}

#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
#ifdef _WIN32
using LocalSocket = SOCKET;
using LocalSocklen = int;
static bool ValidSocket(LocalSocket fd) { return fd != INVALID_SOCKET; }
static void CloseSocket(LocalSocket fd) { closesocket(fd); }
#else
using LocalSocket = int;
using LocalSocklen = socklen_t;
static bool ValidSocket(LocalSocket fd) { return fd >= 0; }
static void CloseSocket(LocalSocket fd) { close(fd); }
#endif

static void TestLocalUDP()
{
#ifdef _WIN32
    WSADATA ws = {};
    CHECK(WSAStartup(MAKEWORD(2, 2), &ws) == 0);
#endif
    LocalSocket server = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(ValidSocket(server));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(server, (const sockaddr *)&addr, sizeof(addr)) == 0);
    LocalSocklen len = sizeof(addr);
    CHECK(getsockname(server, (sockaddr *)&addr, &len) == 0);
    uint16_t port = ntohs(addr.sin_port);
#ifdef _WIN32
    int timeout = 3000; // Winsock expects SO_RCVTIMEO in milliseconds.
#else
    timeval timeout = { 3, 0 };
#endif
    CHECK(setsockopt(server, SOL_SOCKET, SO_RCVTIMEO,
                     (const char *)&timeout, sizeof(timeout)) == 0);

    sampquery::Probe probe;
    CHECK(probe.GetStatus() == sampquery::DISABLED);
    CHECK(!probe.Start("not-an-ip", port, 1));
    CHECK(probe.GetStatus() == sampquery::FAILED);
    CHECK(probe.Start("127.0.0.1", port, 10));
    uint8_t query[11];
    sockaddr_in from = {};
    len = sizeof(from);
    int got = (int)recvfrom(server, (char *)query, sizeof(query), 0, (sockaddr *)&from, &len);
    CHECK(got == 11);
    uint8_t expected[11];
    CHECK(sampquery::BuildInfoQuery("127.0.0.1", port, expected));
    CHECK(std::memcmp(query, expected, 11) == 0);

    std::vector<uint8_t> reply = Reply(query);
    LocalSocket wrongPort = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(ValidSocket(wrongPort));
    CHECK(sendto(wrongPort, (const char *)reply.data(), (int)reply.size(), 0,
                  (sockaddr *)&from, sizeof(from)) == (int)reply.size());
    CHECK(probe.Poll(11) == sampquery::WAITING); // forged matching header, wrong source
    CloseSocket(wrongPort);
    CHECK(sendto(server, (const char *)reply.data(), 16, 0,
                  (sockaddr *)&from, sizeof(from)) == 16);
    CHECK(probe.Poll(12) == sampquery::WAITING); // truncated server reply
    CHECK(sendto(server, (const char *)reply.data(), (int)reply.size(), 0,
                  (sockaddr *)&from, sizeof(from)) == (int)reply.size());
    // Windows may enqueue a loopback datagram slightly after sendto returns.
    sampquery::Status state = sampquery::WAITING;
    for(int i = 0; i < 100 && state == sampquery::WAITING; ++i) {
        state = probe.Poll(13);
        if(state == sampquery::WAITING)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(state == sampquery::SUCCEEDED);
    CHECK(probe.GetInfo().hostname == "Server Name");
    CHECK(probe.Poll(9999) == sampquery::SUCCEEDED);
    probe.Reset();
    CHECK(probe.GetStatus() == sampquery::DISABLED);
    CHECK(probe.Start("127.0.0.1", port, 100));
    len = sizeof(from);
    CHECK(recvfrom(server, (char *)query, sizeof(query), 0,
                   (sockaddr *)&from, &len) == 11);
    CHECK(probe.Poll(1100) == sampquery::WAITING); // send exactly one retry
    len = sizeof(from);
    CHECK(recvfrom(server, (char *)query, sizeof(query), 0,
                   (sockaddr *)&from, &len) == 11);
    CHECK(probe.Poll(3700) == sampquery::TIMED_OUT);
    CHECK(probe.Poll(3800) == sampquery::TIMED_OUT);
    CloseSocket(server);
#ifdef _WIN32
    CHECK(WSACleanup() == 0);
#endif
}
#endif

int main()
{
    TestWireFormat();
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
    TestLocalUDP();
#endif
    std::puts("SA-MP 0.3.7 UDP server-info query tests passed (not game login)");
}
