#include "profiler_core.hpp"

#include <algorithm>
#include <cstring>

#if defined(_WIN32) || defined(WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

namespace {

void CopyName(char (&destination)[ProfilerCore::kNameCapacity], const char *source)
{
    destination[0] = '\0';
    if (source == nullptr) return;
    for (std::size_t i = 0; i + 1 < ProfilerCore::kNameCapacity && source[i] != '\0'; ++i) {
        destination[i] = source[i];
        destination[i + 1] = '\0';
    }
}

} // namespace

ProfilerCore &ProfilerCore::Instance()
{
    static ProfilerCore instance;
    return instance;
}

std::uint16_t ProfilerCore::RegisterSection(const char *name)
{
    if (name == nullptr || name[0] == '\0') return 0;

    for (std::uint16_t i = 0; i < sectionCount_; ++i) {
        if (std::strncmp(sections_[i].name, name, kNameCapacity) == 0) {
            return static_cast<std::uint16_t>(i + 1);
        }
    }
    if (sectionCount_ == kMaxSections) return 0;

    Section &section = sections_[sectionCount_];
    CopyName(section.name, name);
    section.used = true;
    ++sectionCount_;
    return sectionCount_;
}

bool ProfilerCore::IsValid(std::uint16_t id) const
{
    return id != 0 && id <= sectionCount_ && sections_[id - 1].used;
}

std::uint64_t ProfilerCore::Begin(std::uint16_t id) const
{
    if (!enabled_ || !IsValid(id)) return 0;
    return NowNanoseconds();
}

void ProfilerCore::End(std::uint16_t id, std::uint64_t startNanoseconds)
{
    if (startNanoseconds == 0 || !IsValid(id)) return;

    const std::uint64_t now = NowNanoseconds();
    const std::uint64_t elapsed = now >= startNanoseconds ? now - startNanoseconds : 0;
    Section &section = sections_[id - 1];
    ++section.calls;
    section.totalNanoseconds += elapsed;
    section.maximumNanoseconds = std::max(section.maximumNanoseconds, elapsed);
    ++section.histogram[HistogramBucket(elapsed)];
}

void ProfilerCore::SetEnabled(bool enabled)
{
    enabled_ = enabled;
}

bool ProfilerCore::Enabled() const
{
    return enabled_;
}

void ProfilerCore::Reset()
{
    for (std::uint16_t i = 0; i < sectionCount_; ++i) {
        Section &section = sections_[i];
        section.calls = 0;
        section.totalNanoseconds = 0;
        section.maximumNanoseconds = 0;
        section.histogram.fill(0);
    }
}

std::size_t ProfilerCore::CopySnapshots(Snapshot *destination, std::size_t capacity) const
{
    if (destination == nullptr) return 0;
    const std::size_t count = std::min<std::size_t>(capacity, sectionCount_);
    for (std::size_t i = 0; i < count; ++i) {
        const Section &source = sections_[i];
        Snapshot &target = destination[i];
        target = {};
        target.id = static_cast<std::uint16_t>(i + 1);
        CopyName(target.name, source.name);
        target.calls = source.calls;
        target.totalNanoseconds = source.totalNanoseconds;
        target.maximumNanoseconds = source.maximumNanoseconds;
        target.p95Nanoseconds = P95FromHistogram(source);
    }
    return count;
}

void ProfilerCore::Dump(void (*logprintf)(const char *format, ...),
                        std::uint64_t thresholdMicroseconds) const
{
    if (logprintf == nullptr) return;

    logprintf("[samp-profiler] sections=%u enabled=%d", sectionCount_, enabled_ ? 1 : 0);
    for (std::uint16_t i = 0; i < sectionCount_; ++i) {
        const Section &section = sections_[i];
        if (section.calls == 0) continue;
        const std::uint64_t averageUs = section.totalNanoseconds / section.calls / 1000;
        const std::uint64_t maximumUs = section.maximumNanoseconds / 1000;
        if (maximumUs < thresholdMicroseconds && averageUs < thresholdMicroseconds) continue;
        const std::uint64_t p95Us = P95FromHistogram(section) / 1000;
        logprintf("[samp-profiler] #%u %-32s calls=%llu avg=%lluus p95~=%lluus max=%lluus",
                  i + 1, section.name,
                  static_cast<unsigned long long>(section.calls),
                  static_cast<unsigned long long>(averageUs),
                  static_cast<unsigned long long>(p95Us),
                  static_cast<unsigned long long>(maximumUs));
    }
}

std::uint64_t ProfilerCore::NowNanoseconds()
{
#if defined(_WIN32) || defined(WIN32)
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value;
    }();
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return static_cast<std::uint64_t>((static_cast<long double>(counter.QuadPart) * 1000000000.0L) /
                                      static_cast<long double>(frequency.QuadPart));
#else
    timespec value{};
    clock_gettime(CLOCK_MONOTONIC, &value);
    return static_cast<std::uint64_t>(value.tv_sec) * 1000000000ull +
           static_cast<std::uint64_t>(value.tv_nsec);
#endif
}

std::size_t ProfilerCore::HistogramBucket(std::uint64_t nanoseconds)
{
    std::size_t bucket = 0;
    while (nanoseconds > 1 && bucket < 31) {
        nanoseconds >>= 1;
        ++bucket;
    }
    return bucket;
}

std::uint64_t ProfilerCore::P95FromHistogram(const Section &section)
{
    if (section.calls == 0) return 0;
    const std::uint64_t target = (section.calls * 95 + 99) / 100;
    std::uint64_t accumulated = 0;
    for (std::size_t i = 0; i < section.histogram.size(); ++i) {
        accumulated += section.histogram[i];
        if (accumulated >= target) return 1ull << i;
    }
    return section.maximumNanoseconds;
}
