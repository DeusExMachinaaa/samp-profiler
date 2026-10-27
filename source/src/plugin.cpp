#include "profiler_core.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#include "amx/amx.h"
#include "plugincommon.h"

extern void *pAMXFunctions;

namespace {

using LogPrintf = void (*)(const char *format, ...);
LogPrintf g_logprintf = nullptr;

constexpr std::size_t kMaxAmxContexts = 64;
constexpr std::size_t kMaxNestedScopes = 32;
constexpr long kContextTag = 0x53505246L; // "SPRF"

struct ScopeFrame {
    std::uint16_t id{};
    std::uint64_t start{};
    std::uint32_t token{};
};

struct AmxContext {
    AMX *amx{};
    std::uint32_t depth{};
    std::array<ScopeFrame, kMaxNestedScopes> scopes{};
};

std::array<AmxContext, kMaxAmxContexts> g_contexts{};

const char *PawnString(AMX *amx, cell address, char (&buffer)[ProfilerCore::kNameCapacity])
{
    buffer[0] = '\0';
    if (amx == nullptr || address == 0) return buffer;
    cell *physical = nullptr;
    if (amx_GetAddr(amx, address, &physical) != AMX_ERR_NONE || physical == nullptr) return buffer;
    amx_GetString(buffer, physical, 0, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    return buffer;
}

AmxContext *FindContext(AMX *amx)
{
    void *value = nullptr;
    if (amx == nullptr || amx_GetUserData(amx, kContextTag, &value) != AMX_ERR_NONE) return nullptr;
    return static_cast<AmxContext *>(value);
}

cell NativeRegister(AMX *amx, cell *params)
{
    if (params[0] < static_cast<cell>(sizeof(cell))) return 0;
    char name[ProfilerCore::kNameCapacity];
    return static_cast<cell>(ProfilerCore::Instance().RegisterSection(PawnString(amx, params[1], name)));
}

cell NativeBegin(AMX *amx, cell *params)
{
    if (params[0] < static_cast<cell>(sizeof(cell))) return 0;
    AmxContext *context = FindContext(amx);
    if (context == nullptr || context->depth >= kMaxNestedScopes) return 0;

    const auto id = static_cast<std::uint16_t>(params[1]);
    const auto start = ProfilerCore::Instance().Begin(id);
    if (start == 0) return 0;

    ScopeFrame &frame = context->scopes[context->depth];
    frame.id = id;
    frame.start = start;
    frame.token = ++context->depth;
    return static_cast<cell>(frame.token);
}

cell NativeEnd(AMX *amx, cell *params)
{
    if (params[0] < static_cast<cell>(sizeof(cell))) return 0;
    AmxContext *context = FindContext(amx);
    if (context == nullptr || context->depth == 0) return 0;

    ScopeFrame &frame = context->scopes[context->depth - 1];
    if (frame.token != static_cast<std::uint32_t>(params[1])) return 0;
    ProfilerCore::Instance().End(frame.id, frame.start);
    --context->depth;
    return 1;
}

cell NativeSetEnabled(AMX *, cell *params)
{
    if (params[0] < static_cast<cell>(sizeof(cell))) return 0;
    ProfilerCore::Instance().SetEnabled(params[1] != 0);
    return 1;
}

cell NativeIsEnabled(AMX *, cell *)
{
    return ProfilerCore::Instance().Enabled() ? 1 : 0;
}

cell NativeReset(AMX *, cell *)
{
    ProfilerCore::Instance().Reset();
    return 1;
}

cell NativeDump(AMX *, cell *params)
{
    const auto threshold = params[0] >= static_cast<cell>(sizeof(cell)) && params[1] > 0
        ? static_cast<std::uint64_t>(params[1]) : 0;
    ProfilerCore::Instance().Dump(g_logprintf, threshold);
    return 1;
}

AMX_NATIVE_INFO g_natives[] = {
    {"SP_Register", NativeRegister},
    {"SP_Begin", NativeBegin},
    {"SP_End", NativeEnd},
    {"SP_SetEnabled", NativeSetEnabled},
    {"SP_IsEnabled", NativeIsEnabled},
    {"SP_Reset", NativeReset},
    {"SP_Dump", NativeDump},
    {nullptr, nullptr}
};

} // namespace

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **data)
{
    g_logprintf = reinterpret_cast<LogPrintf>(data[PLUGIN_DATA_LOGPRINTF]);
    pAMXFunctions = data[PLUGIN_DATA_AMX_EXPORTS];
    if (g_logprintf != nullptr) {
        g_logprintf("[samp-profiler] samp-profiler by DeusExMachinaaa");
        g_logprintf("[samp-profiler] GitHub: https://github.com/DeusExMachinaaa");
        g_logprintf("[samp-profiler] loaded (explicit low-overhead mode)");
    }
    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
    ProfilerCore::Instance().Dump(g_logprintf, 0);
    g_contexts = {};
    g_logprintf = nullptr;
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx)
{
    AmxContext *available = nullptr;
    for (auto &context : g_contexts) {
        if (context.amx == nullptr) {
            context = {};
            context.amx = amx;
            available = &context;
            break;
        }
    }
    if (available == nullptr || amx_SetUserData(amx, kContextTag, available) != AMX_ERR_NONE) {
        if (available != nullptr) *available = {};
        return AMX_ERR_INIT;
    }
    const int result = amx_Register(amx, g_natives, -1);
    if (result != AMX_ERR_NONE) {
        amx_SetUserData(amx, kContextTag, nullptr);
        *available = {};
    }
    return result;
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx)
{
    if (AmxContext *context = FindContext(amx)) {
        amx_SetUserData(amx, kContextTag, nullptr);
        *context = {};
    }
    return AMX_ERR_NONE;
}

PLUGIN_EXPORT std::uint16_t PLUGIN_CALL SP_RegisterSection(const char *name)
{
    return ProfilerCore::Instance().RegisterSection(name);
}

PLUGIN_EXPORT std::uint64_t PLUGIN_CALL SP_BeginSection(std::uint16_t id)
{
    return ProfilerCore::Instance().Begin(id);
}

PLUGIN_EXPORT void PLUGIN_CALL SP_EndSection(std::uint16_t id, std::uint64_t startNanoseconds)
{
    ProfilerCore::Instance().End(id, startNanoseconds);
}

PLUGIN_EXPORT void PLUGIN_CALL SP_SetEnabled(bool enabled)
{
    ProfilerCore::Instance().SetEnabled(enabled);
}

PLUGIN_EXPORT bool PLUGIN_CALL SP_IsEnabled()
{
    return ProfilerCore::Instance().Enabled();
}

PLUGIN_EXPORT void PLUGIN_CALL SP_Reset()
{
    ProfilerCore::Instance().Reset();
}

PLUGIN_EXPORT void PLUGIN_CALL SP_Dump(std::uint64_t thresholdMicroseconds)
{
    ProfilerCore::Instance().Dump(g_logprintf, thresholdMicroseconds);
}
