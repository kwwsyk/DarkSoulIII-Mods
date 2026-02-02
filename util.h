#include <functional>
#include <sstream>
#include <Windows.h>
#include <TlHelp32.h>

#include "ModUtils.h"



namespace Suit
{
    using namespace ModUtils;
    
    static uintptr_t ResolveAddress(uintptr_t base, const std::vector<uintptr_t>& offsets) {
        Log("Resolving pointer: base =", NumberToHexString(base));
        uintptr_t addr = base;
        if (offsets.empty()) return addr;
        for (size_t i = 0; i < offsets.size(); ++i) {
            Log("Resoving: time=", i," offset=",offsets[i]);
            addr = *(uintptr_t*)(addr + offsets[i]);
            Log("Resoving: time=", i, " newAddr=", NumberToHexString(addr));
            if (!addr) return 0;
        }
        return addr;
    }

    template<typename T>
    static bool ReadPtrChain(uintptr_t base, const std::vector<uintptr_t>& offsets, T& out) {
        uintptr_t finalAddr = ResolveAddress(base, offsets);
        if (!finalAddr) return false;
        out = *reinterpret_cast<T*>(finalAddr);
        return true;
    }

    //--- Helper with mod module (.dll) path ---
    //Based on and big thanks to NymicRazor for his Elden Ring Uncapped source.

    /**
    * Get current dll file name without (.dll) affix
    *   e.g. invoked in RuneMultiplier.dll -> return "RuneMultiplier"
    */
    std::string getDLLName()
    {
        char path[MAX_PATH];
        HMODULE hm = NULL;

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&getDLLName, &hm) == 0)
        {
            int ret = GetLastError();
        }
        if (GetModuleFileNameA(hm, path, sizeof(path)) == 0)
        {
            int ret = GetLastError();
        }
        std::string pathStr(path);

        return pathStr.substr(0, pathStr.size() - 4);
    }

    std::string modName = getDLLName();

    /**
    * Get current dll file path but without file name and file affix.
    * 
    * 
    */
    std::string getDllPath()
    {
        char path[MAX_PATH];
        HMODULE hm = NULL;

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&getDllPath, &hm) == 0)
        {
            int ret = GetLastError();
        }
        if (GetModuleFileNameA(hm, path, sizeof(path)) == 0)
        {
            int ret = GetLastError();
        }
        std::string pathStr(path);

        return pathStr.substr(0, pathStr.size() - (modName.size() + 4));
    }

    //---End---


    static std::string hex_dump(const void* data, size_t len) {
        const auto* p = static_cast<const unsigned char*>(data);
        std::ostringstream oss;
        oss.setf(std::ios::hex, std::ios::basefield);
        oss.setf(std::ios::uppercase);
        for (size_t i = 0; i < len; ++i) {
            oss << std::setw(2) << std::setfill('0') << (unsigned)p[i];
            if (i + 1 != len) oss << ' ';
        }
        return oss.str();
    }

    static std::string ptr_str(const void* addr) {
        std::ostringstream oss;
        oss.setf(std::ios::hex, std::ios::basefield);
        oss.setf(std::ios::uppercase);
        oss << "0x" << (uintptr_t)addr;
        return oss.str();
    }

    static void log_bytes(const char* tag, const void* addr, const void* data, size_t len) {
        Log(tag, " [", (uint64_t)len, "] @ ", ptr_str(addr), " : ", hex_dump(data, len));
    }

    static LONG WINAPI Veh(LPEXCEPTION_POINTERS p) {
        auto code = p->ExceptionRecord->ExceptionCode;
        auto rip = p->ContextRecord->Rip;
        Log("VEH code=0x%08X rip=%p rsp=%p", code, (void*)rip, (void*)p->ContextRecord->Rsp);
        log_bytes("rip[32]", (void*)rip, (void*)rip, 32);
        return EXCEPTION_CONTINUE_SEARCH;
    }
}