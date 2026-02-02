// minimal_jmp_hook.cpp
#include <windows.h>
#include <cstdint>
#include <iostream>

namespace Suit{
    bool Write5ByteJmp(void* injectAddr, void* targetAddr, HANDLE hProcess = GetCurrentProcess())
    {
        uint8_t patch[5];
        patch[0] = 0xE9;  // jmp rel32

        intptr_t src = reinterpret_cast<intptr_t>(injectAddr);
        intptr_t dst = reinterpret_cast<intptr_t>(targetAddr);
        intptr_t rel = dst - (src + 5);

        if (rel < INT32_MIN || rel > INT32_MAX)
        {
            std::cerr << "[Write5ByteJmp] target too far for rel32\n";
            return false;
        }

        *reinterpret_cast<int32_t*>(patch + 1) = static_cast<int32_t>(rel);

        DWORD oldProtect = 0;
        if (!VirtualProtectEx(hProcess, injectAddr, sizeof(patch),
            PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            std::cerr << "[Write5ByteJmp] VirtualProtectEx failed\n";
            return false;
        }

        SIZE_T written = 0;
        if (!WriteProcessMemory(hProcess, injectAddr, patch, sizeof(patch), &written) ||
            written != sizeof(patch))
        {
            std::cerr << "[Write5ByteJmp] WriteProcessMemory failed\n";
            VirtualProtectEx(hProcess, injectAddr, sizeof(patch), oldProtect, &oldProtect);
            return false;
        }

        FlushInstructionCache(hProcess, injectAddr, sizeof(patch));
        VirtualProtectEx(hProcess, injectAddr, sizeof(patch), oldProtect, &oldProtect);
        return true;
    }

    void* AllocNearExecutablePage(HANDLE hProcess, void* nearAddr, size_t size)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);

        const size_t gran = si.dwAllocationGranularity; 
        const uintptr_t minAddr = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
        const uintptr_t maxAddr = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);

        uintptr_t base = reinterpret_cast<uintptr_t>(nearAddr);
        
        base &= ~(static_cast<uintptr_t>(gran) - 1u);

        const uint64_t MaxDistance = 0x7FFF0000ull; // < 2GB

        
        for (uint64_t step = 0; step * gran <= MaxDistance; ++step)
        {
            
            if (base > step * gran)
            {
                uintptr_t addrDown = base - step * gran;
                if (addrDown >= minAddr)
                {
                    void* p = VirtualAllocEx(
                        hProcess,
                        reinterpret_cast<void*>(addrDown),
                        size,
                        MEM_RESERVE | MEM_COMMIT,
                        PAGE_EXECUTE_READWRITE
                    );
                    if (p)
                        return p;
                }
            }

            
            uintptr_t addrUp = base + step * gran;
            if (addrUp + size < maxAddr)
            {
                void* p = VirtualAllocEx(
                    hProcess,
                    reinterpret_cast<void*>(addrUp),
                    size,
                    MEM_RESERVE | MEM_COMMIT,
                    PAGE_EXECUTE_READWRITE
                );
                if (p)
                    return p;
            }
        }

        return nullptr;
    }

    
    bool WriteAbsoluteJmp64(HANDLE hProcess, void* stubAddr, void* targetAddr)
    {
        uint8_t code[12];
        code[0] = 0x48;
        code[1] = 0xB8; // mov rax, imm64
        *reinterpret_cast<uint64_t*>(code + 2) =
            reinterpret_cast<uint64_t>(targetAddr);
        code[10] = 0xFF;
        code[11] = 0xE0; // jmp rax

        DWORD oldProtect = 0;
        if (!VirtualProtectEx(hProcess, stubAddr, sizeof(code),
            PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            std::cerr << "[WriteAbsoluteJmp64] VirtualProtectEx failed\n";
            return false;
        }

        SIZE_T written = 0;
        if (!WriteProcessMemory(hProcess, stubAddr, code, sizeof(code), &written) ||
            written != sizeof(code))
        {
            std::cerr << "[WriteAbsoluteJmp64] WriteProcessMemory failed\n";
            VirtualProtectEx(hProcess, stubAddr, sizeof(code), oldProtect, &oldProtect);
            return false;
        }

        FlushInstructionCache(hProcess, stubAddr, sizeof(code));
        VirtualProtectEx(hProcess, stubAddr, sizeof(code), oldProtect, &oldProtect);
        return true;
    }

    
    bool HookWithRelayJmp(HANDLE hProcess, void* injAddr, void* targetAddr)
    {
        if (!hProcess)
            hProcess = GetCurrentProcess();

        
        void* relay = AllocNearExecutablePage(hProcess, injAddr, 64); 
        if (!relay) {
            std::cerr << "[HookWithRelayJmp] AllocNearExecutablePage failed\n";
            return false;
        }

       
        intptr_t src = reinterpret_cast<intptr_t>(injAddr);
        intptr_t mid = reinterpret_cast<intptr_t>(relay);
        intptr_t rel = mid - (src + 5);
        if (rel < INT32_MIN || rel > INT32_MAX)
        {
            std::cerr << "[HookWithRelayJmp] relay too far for rel32\n";
            return false;
        }

        
        if (!WriteAbsoluteJmp64(hProcess, relay, targetAddr))
        {
            std::cerr << "[HookWithRelayJmp] WriteAbsoluteJmp64 failed\n";
            return false;
        }

        
        if (!Write5ByteJmp(injAddr, relay, hProcess))
        {
            std::cerr << "[HookWithRelayJmp] Write5ByteJmp failed\n";
            return false;
        }

        return true;
    }

    bool HookWithRelayJmp(void* injAddr, void* targetAddr)
    {
        return HookWithRelayJmp(GetCurrentProcess(), injAddr, targetAddr);
    }

}