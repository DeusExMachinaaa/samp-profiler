#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class ProfilerCore final {
public:
    static constexpr std::uint16_t kMaxSections = 256;
    static constexpr std::size_t kNameCapacity = 64;

    struct Snapshot {
        std::uint16_t id{};
        char name[kNameCapacity]{};
        std::uint64_t calls{};
        std::uint64_t totalNanoseconds{};
        std::uint64_t maximumNanoseconds{};
        std::uint64_t p95Nanoseconds{};
    };

    static ProfilerCore &Instance();

    std::uint16_t RegisterSection(const char *name);
    bool IsValid(std::uint16_t id) const;

    // These methods are intentionally allocation-free and lock-free on the hot path.
    // They must be called from the SA-MP main thread.
    std::uint64_t Begin(std::uint16_t id) const;
    void End(std::uint16_t id, std::uint64_t startNanoseconds);

    void SetEnabled(bool enabled);
    bool Enabled() const;
    void Reset();

    std::size_t CopySnapshots(Snapshot *destination, std::size_t capacity) const;
    void Dump(void (*logprintf)(const char *format, ...), std::uint64_t thresholdMicroseconds) const;

private:
    ProfilerCore() = default;
    ProfilerCore(const ProfilerCore &) = delete;
    ProfilerCore &operator=(const ProfilerCore &) = delete;

    struct Section {
        bool used{};
        char name[kNameCapacity]{};
        std::uint64_t calls{};
        std::uint64_t totalNanoseconds{};
        std::uint64_t maximumNanoseconds{};
        std::array<std::uint64_t, 32> histogram{};
    };

    static std::uint64_t NowNanoseconds();
    static std::size_t HistogramBucket(std::uint64_t nanoseconds);
    static std::uint64_t P95FromHistogram(const Section &section);

    std::array<Section, kMaxSections> sections_{};
    std::uint16_t sectionCount_{};
    bool enabled_{};
};
