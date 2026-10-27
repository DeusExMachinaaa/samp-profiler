#pragma once

#include <cstddef>
#include <cstdint>

#include "plugincommon.h"

extern "C" {

PLUGIN_EXPORT std::uint16_t PLUGIN_CALL SP_RegisterSection(const char *name);
PLUGIN_EXPORT std::uint64_t PLUGIN_CALL SP_BeginSection(std::uint16_t id);
PLUGIN_EXPORT void PLUGIN_CALL SP_EndSection(std::uint16_t id, std::uint64_t startNanoseconds);
PLUGIN_EXPORT void PLUGIN_CALL SP_SetEnabled(bool enabled);
PLUGIN_EXPORT bool PLUGIN_CALL SP_IsEnabled();
PLUGIN_EXPORT void PLUGIN_CALL SP_Reset();
PLUGIN_EXPORT void PLUGIN_CALL SP_Dump(std::uint64_t thresholdMicroseconds);

}

class SampProfilerScope final {
public:
    explicit SampProfilerScope(std::uint16_t id)
        : id_(id), start_(SP_BeginSection(id)) {}
    ~SampProfilerScope()
    {
        if (start_ != 0) SP_EndSection(id_, start_);
    }

    SampProfilerScope(const SampProfilerScope &) = delete;
    SampProfilerScope &operator=(const SampProfilerScope &) = delete;

private:
    std::uint16_t id_{};
    std::uint64_t start_{};
};
