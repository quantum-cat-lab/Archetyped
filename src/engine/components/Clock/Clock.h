#pragma once
#include <chrono>

/**
 * Clock - Core Component.
 * Tracks time and provides monotonic seconds on demand.
 * Modules must not depend on the engine frame loop: `nowSeconds()` is a
 * live monotonic reading available at any time, so each module can run its
 * own tick loop (accumulator, fixed timestep, etc.) independently.
 */
class Clock {
    public:
    Clock();
    ~Clock() = default;

    /**
     * Resets the clock's start and last time points.
     */
    void resetClock();
    /**
     * Updates the clock's cached delta/total time. Optional: modules that
     * need live readings should use nowSeconds() instead.
     */
    void update();
    /**
     * Time elapsed since the last update().
     * @return The delta time in seconds.
     */
    float getDeltaTime() const;
    /**
     * Total time elapsed since the clock started (live reading).
     * @return The total time in seconds.
     */
    float getTotalTime() const;
    /**
     * Live monotonic seconds since the first call (or resetClock()).
     * Steady clock — never jumps, unaffected by wall-clock changes.
     */
    static double nowSeconds();

    private:
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_time;
        float deltaTime = 0.0f;
        float totalTime = 0.0f;
        static std::chrono::steady_clock::time_point s_epoch;
};
