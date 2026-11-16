#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// Owns immutable snapshots (e.g. custom wavetable sets) that the audio
// thread reads through raw pointers, and frees an old snapshot only once
// the audio thread has reported that nothing can read it any more.
//
// Protocol
//  - Message thread: publish() and reclaim(). Callers serialise them
//    (ObjectSynth holds customTableMutex). Ids grow with every publish.
//  - Audio thread, once per processed block:
//      1. at the start, read nextId() BEFORE loading the current snapshot;
//      2. at the end, reportOldestInUse(min of: the id of the snapshot it
//         loaded (or the nextId() value if it loaded none), and the ids
//         latched by every voice that is still sounding).
//    A block can only read the snapshot it loaded at its start and the
//    ones its voices latched earlier. The current snapshot only ever
//    moves to newer ids, so nothing older than the report is reachable.
//  - The newest snapshot is never freed here.
//
// No JUCE dependency: unit-testable on its own.
template <typename Snapshot>
class SnapshotStore final
{
public:
    using Id = std::uint64_t;

    // Snapshot needs a public `Id id` member; it is assigned here.
    const Snapshot* publish(std::unique_ptr<Snapshot> snapshot)
    {
        const Id id = newestId.load(std::memory_order_relaxed) + 1;
        snapshot->id = id;
        versions.push_back(std::move(snapshot));
        newestId.store(id, std::memory_order_release);
        reclaim();
        return versions.back().get();
    }

    // Frees every snapshot older than the last report. Returns how many.
    std::size_t reclaim()
    {
        const Id oldest = oldestInUse.load(std::memory_order_acquire);
        const Id newest = newestId.load(std::memory_order_relaxed);
        const auto before = versions.size();
        versions.erase(std::remove_if(versions.begin(), versions.end(),
                           [oldest, newest](const std::unique_ptr<const Snapshot>& s)
                           { return s->id < oldest && s->id != newest; }),
                       versions.end());
        return before - versions.size();
    }

    // Audio thread. Lock-free.
    Id nextId() const noexcept
    {
        return newestId.load(std::memory_order_acquire) + 1;
    }

    void reportOldestInUse(Id id) noexcept
    {
        oldestInUse.store(id, std::memory_order_release);
    }

    std::size_t size() const noexcept { return versions.size(); }

private:
    std::vector<std::unique_ptr<const Snapshot>> versions;
    std::atomic<Id> newestId {0};
    // 0 = no report yet: free nothing until the audio thread has run once.
    std::atomic<Id> oldestInUse {0};
};
