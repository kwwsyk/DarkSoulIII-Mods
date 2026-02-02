#include <Windows.h>
#include <xmmintrin.h>

//You may Configurate your IDE for those includes
//For VisualStudio, its projectIcon=>properties(last)=>C/C++=>include(entries that has such word)
//Thanks for techiew's ModUtils.h: https://github.com/techiew/EldenRingMods
#include "../ModUtils.h"
#include "../util.h"
#include "../ceInj.h"


using namespace ModUtils;
using namespace mINI;
using namespace Suit;

#define KEY_MOD_NAME "soul_multiplier"

float base = 1.0F;
float level_mul = 0.0f;
int level_base = 0;
uint32_t* level;

bool flag = true;

uintptr_t aobResult;
uintptr_t GameDataMan;

__forceinline bool TryRead32(const uint32_t* p, uint32_t& out);
bool ReadLevelPointer();

extern "C" {
    void SoulMultiplier();
    uintptr_t returnAddress = 0;
    float getMultiplier() {
        if (level_mul == 0) return base;
        if (!level && flag) {
            ReadLevelPointer();
            flag = false;
        }
        if (!level) {
            return base;
        }
        int lvl = *level;
        return base + level_mul * (lvl > level_base ? lvl - level_base : 0);
    }
}

static inline std::string ConfigPath() {
    return getDllPath() + modName + "_config.ini"; // ...\RuneMultiplier_config.ini
}

__forceinline bool TryRead32(const uint32_t* p, uint32_t& out) {
    __try { out = *p; return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

#define k ini[KEY_MOD_NAME]

static void readConfig() {
    INIFile config(ConfigPath());
    INIStructure ini;
    //auto k = ini[KEY_MOD_NAME];

    bool existed = config.read(ini);
    bool dirty = false;

    auto ensureF = [&](const char* key, const char* def, float& out) {
        std::string v = k.get(key);
        Log("Reading Config: k=", key, " v=", v);
        try { out = v.empty() ? std::stof(def) : std::stof(v); }
        catch (...) { out = std::stof(def); dirty = true; }
        if (!k.has(key) || v.empty()) { k[key] = def; dirty = true; }
        else { k[key] = std::to_string(out); }
        };
    auto ensureI = [&](const char* key, const char* def, int& out) {
        std::string v = k.get(key);
        Log("Reading Config: k=", key, " v=", v);
        try { out = v.empty() ? std::stoi(def) : std::stoi(v); }
        catch (...) { out = std::stoi(def); dirty = true; }
        if (!k.has(key) || v.empty()) { k[key] = def; dirty = true; }
        else { k[key] = std::to_string(out); }
        };

    ensureF("base", "1.0", base);
    ensureF("level_mul", "0.0", level_mul);
    ensureI("level_base", "0", level_base);

    if (!existed || dirty) {
        if (config.write(ini, true))// pretty print
        {
            Log("Wrote/updated ini: ", ConfigPath());
        }
        else
        {
            Log("Failed to write config ini ", ConfigPath());
        }
    }

    Log("Applying Config: ", "base=", base, " level_mul=", level_mul, " level_base=", level_base);
}

static void initializeLevelAob() {
    Log("Initializing Level Pointer Aob");
    std::string aob = "48 8b 05 ? ? ? ? 48 85 c0 ? ? 48 8b 40 ? c3";//copied from CT, 8 bytes will be replaced
    aobResult = AobScan(aob);
    GameDataMan = aobResult;
    Log("GameDataMan: ", NumberToHexString(GameDataMan));
}

inline bool ReadPtrSafe(void* p, void* out, SIZE_T sz) {
    SIZE_T got = 0;
    return ::ReadProcessMemory(::GetCurrentProcess(), p, out, sz, &got) && got == sz;
}

uintptr_t adr, nxt, of;
static inline void logUpdate(uintptr_t p_adr, uintptr_t p_nxt, uintptr_t p_of) {
    if (p_adr != adr || p_nxt != nxt || p_of != of) {
        adr = p_adr; nxt = p_nxt; of = p_of;
        Log("addr=", NumberToHexString(p_adr), " next=", NumberToHexString(p_nxt), " off=", NumberToHexString(p_of));
    }
}

bool ResolvePtrChain(uintptr_t base, std::vector<uintptr_t> offsets, uintptr_t& out) {
    uintptr_t addr = base;
    for (auto off : offsets) {
        uintptr_t next = 0;
        if (!ReadPtrSafe((uintptr_t*)(addr), &next, sizeof next)) return false;
        logUpdate(addr, next, off);
        addr = next + off;
    }
    out = addr;
    return true;
}

//return false for exception
bool ReadLevelPointer() {
    try{
        uintptr_t pLevel = 0;
        if (!ResolvePtrChain(GameDataMan, { 0x10,0x70 }, pLevel)) return 1;//when the pointer has not been initialized yet, it would parse wrong

        if (!pLevel) return 0;
        level = (uint32_t*)pLevel;
        return true;
    }catch(...) {
		return false;
	}
}

[[noreturn]] static void FatalExit(const std::wstring& msg, DWORD code) {
    MessageBoxW(nullptr, msg.c_str(), L"Game Crash (Mod)", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TOPMOST);
    TerminateProcess(GetCurrentProcess(), code);
    ExitProcess(code);
}

static LONG WINAPI UEFFilter(EXCEPTION_POINTERS* ep) {
    DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    void* addr = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionAddress : nullptr;

    std::wstringstream ss;
    ss << L"Unhandled exception\nCode: 0x" << std::hex << code << L"\nAddr: " << addr;

    FatalExit(ss.str(), code);
}

void InstallCrashPopupAndExit() {
    SetUnhandledExceptionFilter(UEFFilter);
}