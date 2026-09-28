#pragma once
#include <Windows.h>
#include <sddl.h>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <stdexcept>
#pragma comment(lib, "advapi32.lib")
namespace server_admin::pipe_io
{
    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;
        struct handle
        {
            HANDLE value{INVALID_HANDLE_VALUE};
            explicit handle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
            ~handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
            handle(const handle&) = delete;
            handle& operator=(const handle&) = delete;
        };
        inline bool wait_io(HANDLE pipe, OVERLAPPED& operation, DWORD& bytes, clock::time_point deadline, HANDLE stop_event)
        {
            HANDLE events[] = {stop_event, operation.hEvent};
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now()).count();
            if (left > 0 && WaitForMultipleObjects(2, events, false, static_cast<DWORD>(left)) == WAIT_OBJECT_0 + 1)
                return GetOverlappedResult(pipe, &operation, &bytes, false) != false;
            CancelIoEx(pipe, &operation);
            // Cancellation completion must precede destroying OVERLAPPED/event storage.
            GetOverlappedResult(pipe, &operation, &bytes, true);
            return false;
        }
        inline bool transfer(HANDLE pipe, void* data, DWORD size, bool writing, HANDLE stop_event, const std::atomic_bool& stopping)
        {
            auto deadline = clock::now() + 3s;
            DWORD done = 0;
            while (done < size && !stopping)
            {
                handle event(CreateEventW(nullptr, true, false, nullptr));
                if (!event.value) return false;
                OVERLAPPED operation{}; operation.hEvent = event.value;
                DWORD bytes = 0;
                auto* position = static_cast<char*>(data) + done;
                BOOL ok = writing ? WriteFile(pipe, position, size - done, &bytes, &operation) : ReadFile(pipe, position, size - done, &bytes, &operation);
                if (!ok && (GetLastError() != ERROR_IO_PENDING || !wait_io(pipe, operation, bytes, deadline, stop_event))) return false;
                if (!bytes) return false;
                done += bytes;
            }
            return done == size;
        }
        inline std::wstring security_descriptor()
        {
            handle token;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) throw std::runtime_error("owner token unavailable");
            DWORD needed = 0; GetTokenInformation(token.value, TokenUser, nullptr, 0, &needed);
            std::vector<unsigned char> storage(needed);
            if (!needed || !GetTokenInformation(token.value, TokenUser, storage.data(), needed, &needed)) throw std::runtime_error("owner SID unavailable");
            LPWSTR sid{};
            if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(storage.data())->User.Sid, &sid)) throw std::runtime_error("owner SID conversion failed");
            std::wstring descriptor = L"D:P(A;;GA;;;"; descriptor += sid; descriptor += L")"; LocalFree(sid);
            return descriptor;
        }
}
