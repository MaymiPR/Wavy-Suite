#pragma once
#include <atomic>
#include <algorithm>
#include <cmath>

// Meter ballistics identified on the reference meters (tools/MeterCapture + nulltest/fit_meter.py, see MEASUREMENTS.md
// "Meter ballistics"): each editor frame takes the highest peak since the last frame; the bar jumps up to it at once and
// otherwise moves toward it as a one-pole on linear amplitude with a 0.325 s time constant. In silence that is a
// straight 26.7 dB/s fall; after a level drop with the signal still playing the bar settles gently onto the new level.
// The audio thread only records peaks, so the bars move the same at any buffer size.
namespace wavy
{
// audio thread -> editor: the highest peak (dB) since the last take()
struct PeakTap
{
    std::atomic<float> db { -80.0f };

    void push (float v) noexcept
    {
        float old = db.load (std::memory_order_relaxed);
        while (v > old && ! db.compare_exchange_weak (old, v, std::memory_order_relaxed)) {}
    }
    float take() noexcept { return db.exchange (-80.0f, std::memory_order_relaxed); }
};

// editor side: instant rise, one-pole release toward the current peak (linear amplitude); an optional second stage
// of the same kind after it (the RVox Energy meter: 0.341 s then 0.094 s, which makes its fall start gently)
struct MeterBallistics
{
    static constexpr double floorDb = -80.0;
    double tau = 0.325, tau2 = 0.0;
    double level = 0.0, level2 = 0.0;   // linear

    MeterBallistics() = default;
    MeterBallistics (double t, double t2 = 0.0) : tau (t), tau2 (t2) {}

    float update (float peakDb, double dtSeconds) noexcept
    {
        const double dt = std::min (dtSeconds, 0.25);
        const double peak = peakDb > floorDb ? std::pow (10.0, peakDb / 20.0) : 0.0;
        level = follow (level, peak, dt, tau);
        const double shown = tau2 > 0.0 ? (level2 = follow (level2, level, dt, tau2)) : level;
        return shown > 1.0e-4 ? (float) (20.0 * std::log10 (shown)) : (float) floorDb;
    }

private:
    static double follow (double state, double target, double dt, double t) noexcept
    {
        return target >= state ? target : state + (target - state) * (1.0 - std::exp (-dt / t));
    }
};
}
