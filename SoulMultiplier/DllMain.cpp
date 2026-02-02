#include "SoulMultiplier.h"

static constexpr uintptr_t addSoulCallOff = 0x5A5410;
static constexpr uintptr_t hookAddrOff = 0x5A541F;//mov ebx,edx ;2 bytes
//5A5421: mov rdi,rcx ; 3 bytes
//5A5424: call getCurrentSoulCall ;5 bytes
static constexpr uintptr_t gameDataManOff = 0x47572B8;

static bool installHook() {

    auto baseAddress = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    if (!baseAddress) {
        ModUtils::Log("Failed to get base Addr");
        return 0;
    }
    
    uintptr_t hookAddress = baseAddress + hookAddrOff; //after call instruction

    returnAddress = baseAddress + 0x5A5424;

    GameDataMan = baseAddress + gameDataManOff;

    if (!HookWithRelayJmp((void*)hookAddress, &SoulMultiplier)) {
        Log("Failed to hook");
        return 0;
    }

    //log_bytes("---hook.before", (void*)hookAddress, (void*)hookAddress, 16);
    //log_bytes("---hook.after", (void*)hookAddress, (void*)hookAddress, 16);
    //log_bytes("returnAddress: ", (void*)returnAddress, (void*)returnAddress, 6);//48 8B 8B 70 05 00 00
    //log_bytes("SoulMultiplier proc: ", &SoulMultiplier, &SoulMultiplier, 64);
    return 1;
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
#ifdef _DEBUG
    InstallCrashPopupAndExit();
#endif
    Log("Activating SoulMultiplier");

    readConfig();

    if (!installHook()) {
        Log("Failed to intall hook, SoulMultiplier will not work.");
        return 1;
    }

    while (!level) {
        if (!ReadLevelPointer()) {//this means exception occurred instead of failed to read when level data has not been initialized yet.
            Log("Error reading level pointer, the multiplier will only depend on base.");
            return 1;
        }
    }
    uint32_t lvl;
    if (ReadPtrSafe(level, &lvl, sizeof lvl)) {
        Log("Read level pointer: ", level, "current level=", lvl);
    }
    else
    {
        Log("Read invaid level pointer, the multiplier will only depend on base.");
        level = 0;
        return 1;
    }

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        CreateThread(0, 0, &MainThread, 0, 0, NULL);
    }
    return 1;
}