# SA-MP 0.3.7 in reVC: implementation status

Target: **ordinary SA-MP 0.3.7 servers**, not Black Russia. The separate
[`michael-fa/samp`](https://github.com/michael-fa/samp) tree calls itself a leak,
has no explicit licence, uses GTA SA x86 addresses/D3D9 hooks, and says its
0.3.7 packet reception still crashes. **No code from it is included here.**

## What works now: opt-in server discovery, NOT joining

The game can send an SA-MP UDP **info query** (`SAMP` + IPv4 + little-endian
port + `i`) to a configured server and print the reply in `debug.log`.
It validates the source IP/port and echoed query header, bounds-checks all
strings, discards malformed responses, retries once, and never blocks a frame
waiting for a reply. No server is contacted without configuration.

Add the following to the game's `reVC.ini` (use a numeric IPv4 address):

```ini
[SAMP]
QueryIPv4=127.0.0.1
QueryPort=7777
```

Replace the sample address with **your own test server's IP**. The default
port is 7777. Remove `QueryIPv4` to disable the probe. A successful
`SA-MP server info ...` log line proves only that UDP **query** works; it
explicitly says `game session NOT connected`. A timed-out query can mean that
queries are disabled by that server, not that gameplay login is impossible.
The server-controlled text is sanitized before logging. No password, nickname,
or login details are read or sent.

The parser and local UDP responder run without game assets in
`tests/SampQueryTests.cpp`; the game integration runs only on explicit opt-in.
The sandbox cannot contact arbitrary public SA-MP servers, so a real-server
query must be confirmed on the user's machine via a new `debug.log`.

## What is still needed for a playable client

1. **A legally usable RakNet/SA-MP 0.3.7 transport.** Offline cookie challenge,
   SA-MP datagram cipher, reliable/ordered messaging, ACK/NAK and timeouts,
   AuthKey and ClientJoin; test byte-for-byte against an authorized local
   SA-MP 0.3.7 server. A raw UDP query is not a RakNet session. Do not drop in
   an unmodified modern RakNet library or the leaked GTA SA DLL and claim it
   will connect.
2. **Network-to-game bridge.** Validate and decode join/spawn/chat/player
   RPCs into bounded data structures. Create/destroy remote VC peds on the
   *game thread* with stable network-ID-to-pool-handle mapping and verified
   model/animation compatibility. Do not store stale `CPed *` across deletion.
3. **State synchronization.** Local on-foot and vehicle updates, interpolation
   of remote actors, occupants, damage/weapon/animation differences, interior
   and world handling, server-driven objects and an in-game chat UI. VC and SA
   do not have identical model IDs, physics, animation sets or script APIs.

Compatibility with every public server cannot be inferred from the info query:
some servers require client-specific features or anti-cheat. Each milestone
needs protocol tests and a real test-server run before it can be called done.

Query layout was checked against the open-source
[open.mp server query handler](https://github.com/openmultiplayer/open.mp/blob/master/Server/Components/LegacyNetwork/Query/query.cpp)
and the [MIT-licensed protocol notes](https://github.com/xykal/sampdroid/blob/main/docs/protocol/raknet-samp.md).
Only the *public wire format* is used; no third-party source is copied into
this implementation.
