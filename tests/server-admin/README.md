# Native local administration tests

The harness compiles the same policy and pipe helper headers used by server_admin.cpp. It opens only a uniquely named local test pipe; no game server is contacted or moderated.

From an x64 Visual Studio developer command prompt:

```bat
mkdir build\server-admin-tests
cl /nologo /std:c++20 /EHsc /W4 /Febuild\server-admin-tests\native-tests.exe /Fobuild\server-admin-tests\native-tests.obj tests\server-admin\native_policy_pipe.cpp
build\server-admin-tests\native-tests.exe
```

Coverage: nonce/request-ID shape; exact operation allowlist; UTF8/message byte bounds and control rejection; full GUID/address/qport/connect-time equality; bot/host/incomplete-identity protection; cancellation/expiry/at-most-once start; owner SID single protected DACL; production-style local-only pipe creation; fragmented reads; reply transfer; a second client served by the same instance after disconnect; shutdown cancellation of pending connect. Runtime-specific script delivery and game player enumeration require separate private dedicated-server validation.

Protocol: dedicated launch opt-in `-server-manager-admin <32 hex nonce>` creates `\\.\pipe\S2x.ServerAdmin.<nonce>`. Owner SID only, rejects remote clients, first-instance name acquisition. That one instance is kept for the life of the process and disconnected between clients, so the name is never free for another process to take. Nonce correlates a launch; it is not an authentication secret. Manager additionally verifies the pipe server process. Four-byte little-endian length (1–8192), then UTF8 JSON, one request/reply per connection. Version 1, hyphenated GUID id, matching instance, operation `hello|players|announce|warn|kick`; target is an opaque token from the current roster, never a slot. Messages are at most 160 UTF8 bytes, single line. Kick reason is optional audit text; engine disconnect reason is always EXE_PLAYERKICKED without blacklisting.

Hello is read-only atomic state. Other operations run on scheduler::server. Queued callbacks expire after 2 s and cannot execute once cancelled; at most 8 callbacks may wait for server frames. A callback already running at the deadline gets another 500 ms to report; after that its result is reported unknown, never retried automatically. Completed request IDs deduplicated in a 128-entry bounded cache. Roster tokens expire 30 s, are replaced by a fresh roster and invalidate on map lifecycle change. Every target operation rechecks all connection identity fields, prohibits bots/loopback hosts and requires a running match. Notice success means queued to an identity-bound registered GSC listener, not proof of on-screen rendering.

All pipe IO uses overlapped operations with a stop event. Read/write phases are bounded 3 s; no unbounded FlushFileBuffers. After sending, the pipe stays connected until reader closes or a bounded wait expires, avoiding discarded unread replies. Shutdown cancels pending IO and queued work. No TCP/RCON endpoint or generic console-command operation exists.
