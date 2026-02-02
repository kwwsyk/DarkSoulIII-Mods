//You may Configurate your IDE for those includes
//For VisualStudio, its projectIcon=>properties(last)=>C/C++=>include(entries that has such word)
//Thanks for techiew's ModUtils.h: https://github.com/techiew/EldenRingMods

#include "../ModUtils.h"

static constexpr uintptr_t kCallSiteRVA = 0x486531;

#ifdef SAFETY_TEST
static constexpr uintptr_t kTargetRVA = 0x4d2150;

static std::array<uint8_t, 5> MakeCallBytes(uintptr_t callSite, uintptr_t target)
{
    int32_t rel = static_cast<int32_t>(target - (callSite + 5));
    return { 0xE8,
             (uint8_t)(rel & 0xFF),
             (uint8_t)((rel >> 8) & 0xFF),
             (uint8_t)((rel >> 16) & 0xFF),
             (uint8_t)((rel >> 24) & 0xFF) };
}
#endif // SAFETY_TEST


DWORD WINAPI MainThread(LPVOID lpParam)
{
    //Your code here
    auto baseAddress = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    if (!baseAddress) {
        ModUtils::Log("Failed to get base Addr");
        return 1;
    }

#ifdef SAFETY_TEST
    auto expected = MakeCallBytes(callSite, target);
    uint8_t cur[5]{};
    MemCopy((uintptr_t)cur, callSite, 5);
    if (std::memcmp(cur, expected.data(), 5) != 0) {
        ModUtils::Log("To replace bytes do not match expected.");
        return 1;
    }
#endif // SAFETY_TEST

    uintptr_t callSite = baseAddress + kCallSiteRVA;

    static const uint8_t nops[5]{ 0x90,0x90,0x90,0x90,0x90 };
    ModUtils::MemCopy(callSite, (uintptr_t)nops, 5);

    ModUtils::Log("Disable soul loss loaded.");
    ModUtils::CloseLog();

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  reason,
                       LPVOID lpReserved
                     )
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        ModUtils::Log("Disable soul loss DLL injected.");
        std::cout <<  "Disable soul loss DLL injected." << std::endl;
        DisableThreadLibraryCalls(hModule);
        CreateThread(0, 0, &MainThread, 0, 0, NULL);
    }
    return 1;
}


