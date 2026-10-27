#include "profiler_core.hpp"

#include <cassert>
#include <chrono>
#include <thread>

int main()
{
    ProfilerCore &profiler = ProfilerCore::Instance();
    const auto id = profiler.RegisterSection("test-section");
    assert(id != 0);
    assert(id == profiler.RegisterSection("test-section"));
    assert(profiler.RegisterSection("") == 0);

    profiler.SetEnabled(false);
    assert(profiler.Begin(id) == 0);

    profiler.SetEnabled(true);
    const auto start = profiler.Begin(id);
    assert(start != 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    profiler.End(id, start);

    ProfilerCore::Snapshot snapshot{};
    assert(profiler.CopySnapshots(&snapshot, 1) == 1);
    assert(snapshot.id == id);
    assert(snapshot.calls == 1);
    assert(snapshot.totalNanoseconds > 0);
    assert(snapshot.maximumNanoseconds > 0);

    profiler.Reset();
    assert(profiler.CopySnapshots(&snapshot, 1) == 1);
    assert(snapshot.calls == 0);
    return 0;
}
