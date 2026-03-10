#pragma once

#include <chrono>

namespace cloth {

class Timer {
public:
    Timer() : m_Started(false), m_StartTime(0), m_PausedTime(0) {}

    void Start() {
        m_StartTime = std::chrono::high_resolution_clock::now();
        m_Started = true;
        m_PausedTime = 0;
    }

    void Stop() {
        m_Started = false;
        m_PausedTime = 0;
    }

    void Pause() {
        if (m_Started && m_PausedTime == 0) {
            m_PausedTime = std::chrono::high_resolution_clock::now();
        }
    }

    void Resume() {
        if (m_Started && m_PausedTime != 0) {
            auto now = std::chrono::high_resolution_clock::now();
            m_StartTime += (now - m_PausedTime);
            m_PausedTime = 0;
        }
    }

    // Get elapsed time in seconds
    float Elapsed() const {
        if (!m_Started) return 0.0f;

        auto endTime = m_PausedTime != 0 ? m_PausedTime : std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_StartTime);
        return duration.count() / 1000000.0f;
    }

    // Get elapsed time in milliseconds
    float ElapsedMillis() const {
        return Elapsed() * 1000.0f;
    }

    // Reset and start again
    void Reset() {
        Start();
    }

    bool IsRunning() const { return m_Started; }

private:
    bool m_Started;
    std::chrono::high_resolution_clock::time_point m_StartTime;
    std::chrono::high_resolution_clock::time_point m_PausedTime;
};

} // namespace cloth
