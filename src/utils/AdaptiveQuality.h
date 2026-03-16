#pragma once

#include "QualityPreset.h"
#include "QualityClassifier.h"
#include <iostream>
#include <algorithm>
#include <vector>

// Undefine Windows min/max macros if they exist
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace cloth {

/**
 * Adaptive Quality System
 * 
 * Automatically adjusts quality settings at runtime based on FPS.
 * This ensures optimal performance on ANY GPU, including future ones.
 * 
 * How it works:
 * 1. Monitor FPS every N frames
 * 2. If FPS > target + threshold → increase quality
 * 3. If FPS < target - threshold → decrease quality
 * 4. Find the "sweet spot" for each GPU automatically
 */
class AdaptiveQuality {
public:
    /**
     * Configuration for adaptive quality
     */
    struct Config {
        float targetFPS = 60.0f;        // Target FPS to maintain
        float upperThreshold = 10.0f;   // FPS above target to increase quality
        float lowerThreshold = 10.0f;   // FPS below target to decrease quality
        int sampleFrames = 60;          // Number of frames to sample before adjusting
        float minSampleTime = 1.0f;     // Minimum seconds between adjustments
        
        QualityPreset minPreset = QualityPreset::LOW;    // Minimum quality
        QualityPreset maxPreset = QualityPreset::ULTRA;  // Maximum quality
    };
    
    /**
     * Quality adjustment event callback
     */
    using QualityChangeCallback = void(QualityPreset oldPreset, QualityPreset newPreset, const std::string& reason);
    
    AdaptiveQuality() 
        : m_CurrentPreset(QualityPreset::MEDIUM)
        , m_FrameCount(0)
        , m_CurrentFPS(60.0f)
        , m_LastAdjustmentTime(0.0f)
        , m_IsEnabled(true)
    {
    }
    
    /**
     * Initialize adaptive quality system
     * 
     * @param initialPreset Starting quality preset
     * @param config Configuration options
     */
    void Initialize(QualityPreset initialPreset, const Config& config = Config()) {
        m_CurrentPreset = initialPreset;
        m_Config = config;
        m_FrameCount = 0;
        m_FPSHistory.clear();
        m_FPSHistory.reserve(config.sampleFrames);
        
        std::cout << "[AdaptiveQuality] Initialized with preset: " 
                  << QualityClassifier::GetPresetName(m_CurrentPreset) << std::endl;
        std::cout << "  Target FPS: " << m_Config.targetFPS << std::endl;
        std::cout << "  Sample frames: " << m_Config.sampleFrames << std::endl;
    }
    
    /**
     * Update adaptive quality system (call every frame)
     * 
     * @param deltaTime Time since last frame
     * @param totalTime Total elapsed time
     */
    void Update(float deltaTime, float totalTime) {
        if (!m_IsEnabled) return;
        
        // Calculate current FPS
        m_CurrentFPS = 1.0f / std::max(deltaTime, 0.0001f);
        
        // Add to FPS history
        m_FPSHistory.push_back(m_CurrentFPS);
        if (m_FPSHistory.size() > static_cast<size_t>(m_Config.sampleFrames)) {
            m_FPSHistory.erase(m_FPSHistory.begin());
        }
        
        m_FrameCount++;
        
        // Check if it's time to adjust quality
        if (m_FrameCount >= m_Config.sampleFrames) {
            float avgFPS = GetAverageFPS();
            float timeSinceLastAdjustment = totalTime - m_LastAdjustmentTime;
            
            if (timeSinceLastAdjustment >= m_Config.minSampleTime) {
                AdjustQuality(avgFPS, totalTime);
                m_FrameCount = 0;
            }
        }
    }
    
    /**
     * Get current quality preset
     */
    QualityPreset GetCurrentPreset() const {
        return m_CurrentPreset;
    }
    
    /**
     * Set current quality preset manually
     */
    void SetCurrentPreset(QualityPreset preset) {
        QualityPreset oldPreset = m_CurrentPreset;
        m_CurrentPreset = preset;
        
        if (oldPreset != preset) {
            std::cout << "[AdaptiveQuality] Manual preset change: " 
                      << QualityClassifier::GetPresetName(oldPreset) 
                      << " → " << QualityClassifier::GetPresetName(preset) << std::endl;
        }
    }
    
    /**
     * Enable/disable adaptive quality
     */
    void SetEnabled(bool enabled) {
        m_IsEnabled = enabled;
        std::cout << "[AdaptiveQuality] " << (enabled ? "Enabled" : "Disabled") << std::endl;
    }
    
    /**
     * Check if adaptive quality is enabled
     */
    bool IsEnabled() const {
        return m_IsEnabled;
    }
    
    /**
     * Get current FPS
     */
    float GetCurrentFPS() const {
        return m_CurrentFPS;
    }
    
    /**
     * Get average FPS over sample period
     */
    float GetAverageFPS() const {
        if (m_FPSHistory.empty()) return m_CurrentFPS;
        
        float sum = 0.0f;
        for (float fps : m_FPSHistory) {
            sum += fps;
        }
        return sum / static_cast<float>(m_FPSHistory.size());
    }
    
    /**
     * Get recommended settings for current preset
     */
    QualityClassifier::RecommendedSettings GetRecommendedSettings() const {
        return QualityClassifier::GetRecommendedSettings(m_CurrentPreset);
    }
    
    /**
     * Force quality adjustment (for user input)
     */
    void IncreaseQuality() {
        if (m_CurrentPreset < m_Config.maxPreset) {
            QualityPreset oldPreset = m_CurrentPreset;
            m_CurrentPreset = static_cast<QualityPreset>(
                static_cast<int>(m_CurrentPreset) + 1
            );
            
            std::cout << "[AdaptiveQuality] User increased quality: " 
                      << QualityClassifier::GetPresetName(oldPreset) 
                      << " → " << QualityClassifier::GetPresetName(m_CurrentPreset) << std::endl;
            
            if (m_OnQualityChange) {
                m_OnQualityChange(oldPreset, m_CurrentPreset, "User input");
            }
        }
    }
    
    /**
     * Force quality decrease (for user input)
     */
    void DecreaseQuality() {
        if (m_CurrentPreset > m_Config.minPreset) {
            QualityPreset oldPreset = m_CurrentPreset;
            m_CurrentPreset = static_cast<QualityPreset>(
                static_cast<int>(m_CurrentPreset) - 1
            );
            
            std::cout << "[AdaptiveQuality] User decreased quality: " 
                      << QualityClassifier::GetPresetName(oldPreset) 
                      << " → " << QualityClassifier::GetPresetName(m_CurrentPreset) << std::endl;
            
            if (m_OnQualityChange) {
                m_OnQualityChange(oldPreset, m_CurrentPreset, "User input");
            }
        }
    }
    
    /**
     * Set quality change callback
     */
    void SetQualityChangeCallback(QualityChangeCallback* callback) {
        m_OnQualityChange = callback;
    }
    
    /**
     * Reset FPS history
     */
    void Reset() {
        m_FPSHistory.clear();
        m_FrameCount = 0;
        m_LastAdjustmentTime = 0.0f;
    }
    
private:
    /**
     * Adjust quality based on average FPS
     */
    void AdjustQuality(float avgFPS, float currentTime) {
        QualityPreset oldPreset = m_CurrentPreset;
        bool changed = false;
        std::string reason;
        
        // Check if we should increase quality
        if (avgFPS > m_Config.targetFPS + m_Config.upperThreshold) {
            if (m_CurrentPreset < m_Config.maxPreset) {
                m_CurrentPreset = static_cast<QualityPreset>(
                    std::min(static_cast<int>(m_CurrentPreset) + 1, 
                            static_cast<int>(m_Config.maxPreset))
                );
                changed = true;
                reason = "FPS above target (" + std::to_string(static_cast<int>(avgFPS)) + " > " + 
                         std::to_string(static_cast<int>(m_Config.targetFPS + m_Config.upperThreshold)) + ")";
            }
        }
        // Check if we should decrease quality
        else if (avgFPS < m_Config.targetFPS - m_Config.lowerThreshold) {
            if (m_CurrentPreset > m_Config.minPreset) {
                m_CurrentPreset = static_cast<QualityPreset>(
                    std::max(static_cast<int>(m_CurrentPreset) - 1, 
                            static_cast<int>(m_Config.minPreset))
                );
                changed = true;
                reason = "FPS below target (" + std::to_string(static_cast<int>(avgFPS)) + " < " + 
                         std::to_string(static_cast<int>(m_Config.targetFPS - m_Config.lowerThreshold)) + ")";
            }
        }
        
        // Notify if quality changed
        if (changed) {
            m_LastAdjustmentTime = currentTime;
            
            std::cout << "[AdaptiveQuality] Auto-adjusted: " 
                      << QualityClassifier::GetPresetName(oldPreset) 
                      << " → " << QualityClassifier::GetPresetName(m_CurrentPreset) << std::endl;
            std::cout << "  Reason: " << reason << std::endl;
            std::cout << "  Avg FPS: " << static_cast<int>(avgFPS) << std::endl;
            
            if (m_OnQualityChange) {
                m_OnQualityChange(oldPreset, m_CurrentPreset, reason);
            }
        }
    }
    
    // State
    QualityPreset m_CurrentPreset;
    Config m_Config;
    int m_FrameCount;
    float m_CurrentFPS;
    std::vector<float> m_FPSHistory;
    float m_LastAdjustmentTime;
    bool m_IsEnabled;
    
    // Callback
    QualityChangeCallback* m_OnQualityChange = nullptr;
};

} // namespace cloth
