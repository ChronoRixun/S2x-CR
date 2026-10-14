#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../src/client/component/server_admin_policy.hpp"
#include "../../src/client/component/server_admin_pipe.hpp"
#include <iostream>
#include <thread>
#include <future>
#include <stdexcept>
using namespace std::chrono_literals;
namespace policy = server_admin::policy;
namespace io = server_admin::pipe_io;
void check(bool condition, const char* name) { if (!condition) throw std::runtime_error(name); std::cout << "PASS " << name << '\n'; }
int main()
{
    try
    {
        check(policy::frame_length(8192) && !policy::frame_length(0) && !policy::frame_length(8193), "frame allocation bounds");
        check(policy::nonce("0123456789abcdef0123456789abcdef") && !policy::nonce("../bad"), "launch nonce shape");
        check(policy::request_id("12345678-1234-1234-1234-123456789abc") && !policy::request_id("1"), "request correlation GUID");
        check(policy::operation("kick") && !policy::operation("exec") && !policy::operation("rcon"), "operation allowlist excludes arbitrary commands");
        check(policy::message(std::string(160,'a')) && !policy::message(std::string(161,'a')), "message byte bound");
        check(policy::message("hello \xf0\x9f\x91\x8b") && !policy::message("a\nquit") && !policy::message("\xc0\xaf") && !policy::message("\xed\xa0\x80") && !policy::message("\xe2\x80\xa8"), "UTF8 validity and single-line controls");
        check(policy::same_connection(1,1,2,2,3,3,4,4) && !policy::same_connection(1,9,2,2,3,3,4,4)
            && !policy::same_connection(1,1,2,9,3,3,4,4) && !policy::same_connection(1,1,2,2,3,9,4,4)
            && !policy::same_connection(1,1,2,2,3,3,4,9), "GUID address qport and connect-time must all match");
        check(policy::target_allowed(false,false,true) && !policy::target_allowed(true,false,true)
            && !policy::target_allowed(false,true,true) && !policy::target_allowed(false,false,false), "bot host and incomplete identities protected");
        std::atomic<policy::work_state> state{policy::work_state::pending};
        policy::cancel(state);
        check(!policy::begin(state, std::chrono::steady_clock::now()+1s), "cancelled queued action cannot start later");
        state=policy::work_state::pending;
        check(!policy::begin(state, std::chrono::steady_clock::now()-1ms), "expired queued action cannot start");
        state=policy::work_state::pending;
        check(policy::begin(state,std::chrono::steady_clock::now()+1s) && !policy::begin(state,std::chrono::steady_clock::now()+1s), "action starts at most once");
        PSECURITY_DESCRIPTOR sd{};
        auto sddl=io::security_descriptor();
        check(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr)!=FALSE,"owner SID descriptor constructed");
        BOOL present{},defaulted{};PACL acl{};
        check(GetSecurityDescriptorDacl(sd,&present,&acl,&defaulted) && present && acl && acl->AceCount==1,"exactly one owner-only allow ACE");
        SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision{};
        check(GetSecurityDescriptorControl(sd,&control,&revision) && (control&SE_DACL_PROTECTED),"DACL excludes inherited access");
        SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
        auto name=L"\\\\.\\pipe\\S2x.ServerAdmin.Test."+std::to_wstring(GetCurrentProcessId());
        io::handle pipe(CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,8196,8196,0,&sa));
        LocalFree(sd);
        check(pipe.value!=INVALID_HANDLE_VALUE,"local-only production-style pipe created");
        io::handle stop(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        io::handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        OVERLAPPED ov{};ov.hEvent=event.value;DWORD bytes{};
        auto connected=ConnectNamedPipe(pipe.value,&ov);
        check(!connected && GetLastError()==ERROR_IO_PENDING,"connect is overlapped");
        std::atomic_bool stopping{};
        auto client=std::async(std::launch::async,[&]
        {
            io::handle c(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));
            if(c.value==INVALID_HANDLE_VALUE)throw std::runtime_error("client connect");
            DWORD sent{};unsigned int size=5;
            // Fragment the four-byte length and body across writes.
            WriteFile(c.value,&size,2,&sent,nullptr);WriteFile(c.value,reinterpret_cast<char*>(&size)+2,2,&sent,nullptr);
            WriteFile(c.value,"hello",5,&sent,nullptr);
            char response[5]{};ReadFile(c.value,response,5,&sent,nullptr);
            return std::string(response,sent);
        });
        check(io::wait_io(pipe.value,ov,bytes,std::chrono::steady_clock::now()+2s,stop.value),"overlapped local connection completed");
        unsigned int size{};check(io::transfer(pipe.value,&size,4,false,stop.value,stopping)&&size==5,"fragmented length read");
        char body[5]{};check(io::transfer(pipe.value,body,5,false,stop.value,stopping)&&std::string(body,5)=="hello","framed body read");
        check(io::transfer(pipe.value,body,5,true,stop.value,stopping)&&client.get()=="hello","reply delivered");
        DisconnectNamedPipe(pipe.value);
        // The server keeps one instance for its lifetime; the name is never released between clients.
        ResetEvent(event.value);ov={};ov.hEvent=event.value;
        check(!ConnectNamedPipe(pipe.value,&ov) && GetLastError()==ERROR_IO_PENDING,"disconnected instance listens again");
        auto second=std::async(std::launch::async,[&]
        {
            io::handle c(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));
            if(c.value==INVALID_HANDLE_VALUE)throw std::runtime_error("second client connect");
            DWORD sent{};unsigned int length=5;
            WriteFile(c.value,&length,4,&sent,nullptr);WriteFile(c.value,"again",5,&sent,nullptr);
            char response[5]{};ReadFile(c.value,response,5,&sent,nullptr);
            return std::string(response,sent);
        });
        check(io::wait_io(pipe.value,ov,bytes,std::chrono::steady_clock::now()+2s,stop.value),"reused instance accepted a second client");
        check(io::transfer(pipe.value,&size,4,false,stop.value,stopping)&&size==5&&io::transfer(pipe.value,body,5,false,stop.value,stopping)
            &&io::transfer(pipe.value,body,5,true,stop.value,stopping)&&second.get()=="again","second request served on the same instance");
        DisconnectNamedPipe(pipe.value);
        ResetEvent(event.value);ov={};ov.hEvent=event.value;
        ConnectNamedPipe(pipe.value,&ov);
        auto start=std::chrono::steady_clock::now();SetEvent(stop.value);
        check(!io::wait_io(pipe.value,ov,bytes,start+1h,stop.value)&&std::chrono::steady_clock::now()-start<1s,"shutdown cancels idle pipe connect promptly");
        std::cout<<"All native admin policy/pipe tests passed.\n";return 0;
    }
    catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
}
