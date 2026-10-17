#include "../src/extras/custom/SampQuery.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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
    assert(sampquery::ParseIPv4("127.0.0.1", ip));
    assert(ip[0] == 127 && ip[1] == 0 && ip[2] == 0 && ip[3] == 1);
    assert(!sampquery::ParseIPv4("127.0.0", ip));
    assert(!sampquery::ParseIPv4("127.0.0.256", ip));
    assert(!sampquery::ParseIPv4("1.2.3.4:7777", ip));
    assert(!sampquery::ParseIPv4("1.2.3.-1", ip));
    assert(!sampquery::ParseIPv4("1.2.3.4\n", ip));
    assert(!sampquery::ParseIPv4("1.2.3.4.5", ip));
    assert(!sampquery::ParseIPv4("1234.2.3.4", ip));
    assert(!sampquery::ParseIPv4("", ip));
    assert(!sampquery::ParseIPv4(nullptr, ip));
    assert(!sampquery::BuildInfoQuery("127.0.0.1", 0, query));
    assert(sampquery::BuildInfoQuery("127.0.0.1", 7777, query));
    const uint8_t expected[11] = { 'S','A','M','P',127,0,0,1,0x61,0x1e,'i' };
    assert(std::memcmp(query, expected, sizeof(query)) == 0);

    std::vector<uint8_t> reply = Reply(query);
    sampquery::ServerInfo info;
    assert(sampquery::ParseInfoResponse(reply.data(), reply.size(), query, info));
    assert(info.password && info.players == 14 && info.maxPlayers == 100);
    assert(info.hostname == "Server Name" && info.gamemode == "Free roam" &&
           info.language == "Russian");

    std::vector<uint8_t> broken = reply;
    broken[4] = 10; // echoed IPv4, port and opcode are checked too
    assert(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[9] ^= 1;
    assert(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[10] = 'c';
    assert(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[11] = 2; // invalid password flag
    assert(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken[16] = 0xff; broken[17] = 0xff;
    broken[18] = 0xff; broken[19] = 0xff; // forged 4-GiB hostname
    assert(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken.pop_back();
    assert(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    broken = reply; broken.push_back(0); // unsolicited trailing bytes
    assert(!sampquery::ParseInfoResponse(broken.data(), broken.size(), query, info));
    assert(!sampquery::ParseInfoResponse(nullptr, reply.size(), query, info));
    assert(info.hostname == "Server Name"); // malformed packets cannot replace result
}

#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
static void TestLocalUDP()
{
    int server = socket(AF_INET, SOCK_DGRAM, 0);
    assert(server >= 0);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(server, (const sockaddr *)&addr, sizeof(addr)) == 0);
    socklen_t len = sizeof(addr);
    assert(getsockname(server, (sockaddr *)&addr, &len) == 0);
    uint16_t port = ntohs(addr.sin_port);
    timeval timeout = { 1, 0 };
    assert(setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0);

    sampquery::Probe probe;
    assert(probe.GetStatus() == sampquery::DISABLED);
    assert(!probe.Start("not-an-ip", port, 1));
    assert(probe.GetStatus() == sampquery::FAILED);
    assert(probe.Start("127.0.0.1", port, 10));
    uint8_t query[11];
    sockaddr_in from = {};
    len = sizeof(from);
    int got = (int)recvfrom(server, query, sizeof(query), 0, (sockaddr *)&from, &len);
    assert(got == 11);
    uint8_t expected[11];
    assert(sampquery::BuildInfoQuery("127.0.0.1", port, expected));
    assert(std::memcmp(query, expected, 11) == 0);

    std::vector<uint8_t> reply = Reply(query);
    int wrongPort = socket(AF_INET, SOCK_DGRAM, 0);
    assert(wrongPort >= 0);
    assert(sendto(wrongPort, reply.data(), reply.size(), 0,
                  (sockaddr *)&from, sizeof(from)) == (int)reply.size());
    assert(probe.Poll(11) == sampquery::WAITING); // forged matching header, wrong source
    close(wrongPort);
    assert(sendto(server, reply.data(), 16, 0,
                  (sockaddr *)&from, sizeof(from)) == 16);
    assert(probe.Poll(12) == sampquery::WAITING); // truncated server reply
    assert(sendto(server, reply.data(), reply.size(), 0,
                  (sockaddr *)&from, sizeof(from)) == (int)reply.size());
    assert(probe.Poll(13) == sampquery::SUCCEEDED);
    assert(probe.GetInfo().hostname == "Server Name");
    assert(probe.Poll(9999) == sampquery::SUCCEEDED);
    probe.Reset();
    assert(probe.GetStatus() == sampquery::DISABLED);
    assert(probe.Start("127.0.0.1", port, 100));
    assert(probe.Poll(3700) == sampquery::TIMED_OUT);
    assert(probe.Poll(3800) == sampquery::TIMED_OUT);
    close(server);
}
#endif

int main()
{
    TestWireFormat();
#if !defined(_WIN32) && (defined(__unix__) || defined(__APPLE__))
    TestLocalUDP();
#endif
    std::puts("SA-MP 0.3.7 UDP server-info query tests passed (not game login)");
}
