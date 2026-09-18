#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace svc
{

struct HitEvent
{
    int note = 0;
    int channel = 1;
    float inputVelocity = 0.0f;
    float outputVelocity = 0.0f;
    int outputMidi2 = -1;
    bool isMidi2 = false;
    std::uint64_t timestamp = 0;
};

// Lock-free single-producer / single-consumer ring for audio thread -> UI thread.
class HitEventFifo
{
public:
    static constexpr uint64_t capacity = 256;

    bool push (const HitEvent& event) noexcept
    {
        const auto write = writeSeq.load (std::memory_order_relaxed);
        auto& slot = buffer[static_cast<size_t> (write % capacity)];
        const auto v = slot.version.load (std::memory_order_relaxed);
        slot.version.store (v + 1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        slot.event = event;
        slot.version.store (v + 2, std::memory_order_release);
        writeSeq.store (write + 1, std::memory_order_release);
        return true;
    }

    bool hasPending() const noexcept
    {
        return readSeq.load (std::memory_order_relaxed)
            < writeSeq.load (std::memory_order_acquire);
    }

    bool pop (HitEvent& event) noexcept
    {
        auto read = readSeq.load (std::memory_order_relaxed);
        const auto write = writeSeq.load (std::memory_order_acquire);

        if (read >= write)
            return false;

        if (write - read > capacity)
            read = write - capacity;

        auto& slot = buffer[static_cast<size_t> (read % capacity)];
        bool success = false;
        for (int retry = 0; retry < 3; ++retry)
        {
            const auto v1 = slot.version.load (std::memory_order_acquire);
            if ((v1 & 1) != 0)
                continue;
            event = slot.event;
            std::atomic_thread_fence (std::memory_order_acquire);
            const auto v2 = slot.version.load (std::memory_order_acquire);
            if (v1 == v2)
            {
                success = true;
                break;
            }
        }

        readSeq.store (read + 1, std::memory_order_release);
        return success;
    }

    void clear() noexcept
    {
        readSeq.store (writeSeq.load (std::memory_order_acquire), std::memory_order_release);
    }

private:
    struct Slot
    {
        std::atomic<uint32_t> version { 0 };
        HitEvent event {};
    };

    std::array<Slot, capacity> buffer {};
    std::atomic<uint64_t> writeSeq { 0 };
    std::atomic<uint64_t> readSeq { 0 };
};

} // namespace svc
