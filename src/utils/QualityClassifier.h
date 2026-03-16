#pragma once

#include "GPUBenchmark.h"
#include "QualityPreset.h"
#include <string>
#include <algorithm>

// Undefine Windows min/max macros if they exist
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace cloth {

/**
 * Score-based GPU quality classification
 * 
 * Instead of hardcoding GPU names (which breaks with new GPUs),
 * we classify based on measured performance scores.
 * 
 * This is FUTURE-PROOF:
 * - RTX 5090, 6090, etc. will auto-classify based on TFLOPS
 * - New vendors (Google, Samsung) will be benchmarked and classified
 * - No code changes needed for future GPUs
 */
class QualityClassifier {
public:
    /**
     * Quality thresholds for classification
     * These are based on PERFORMANCE, not GPU names
     */
    struct Thresholds {
        // Score thresholds (0-10000)
        int ultraMin = 8000;
        int highMin = 5000;
        int mediumMin = 2000;
        
        // Compute TFLOPS thresholds
        float ultraTFLOPS = 15.0f;
        float highTFLOPS = 5.0f;
        float mediumTFLOPS = 2.0f;
        
        // Memory bandwidth thresholds (GB/s)
        float ultraBandwidth = 500.0f;
        float highBandwidth = 200.0f;
        float mediumBandwidth = 50.0f;
        
        // VRAM thresholds (MB)
        size_t ultraVRAM = 8192;
        size_t highVRAM = 4096;
        size_t mediumVRAM = 2048;
    };
    
    /**
     * Classify GPU quality preset based on benchmark results
     * 
     * @param caps GPU capabilities from benchmark
     * @return QualityPreset (LOW, MEDIUM, HIGH, ULTRA)
     */
    static QualityPreset Classify(const GPUCaps& caps) {
        // Calculate weighted performance score
        int score = CalculateScore(caps);
        
        // Classify by score
        if (score >= Thresholds().ultraMin) {
            return QualityPreset::ULTRA;
        }
        if (score >= Thresholds().highMin) {
            return QualityPreset::HIGH;
        }
        if (score >= Thresholds().mediumMin) {
            return QualityPreset::MEDIUM;
        }
        return QualityPreset::LOW;
    }
    
    /**
     * Get detailed classification report
     * 
     * @param caps GPU capabilities
     * @return Human-readable report
     */
    static std::string GetClassificationReport(const GPUCaps& caps) {
        QualityPreset preset = Classify(caps);
        int score = CalculateScore(caps);
        
        std::string report = "\n=== GPU Classification Report ===\n";
        report += "GPU: " + caps.gpuName + "\n";
        report += "Quality Preset: " + GetPresetName(preset) + "\n";
        report += "Performance Score: " + std::to_string(score) + "/10000\n";
        report += "\nBreakdown:\n";
        report += "  Compute Score: " + std::to_string(CalculateComputeScore(caps)) + "/4000\n";
        report += "  Memory Score: " + std::to_string(CalculateMemoryScore(caps)) + "/3000\n";
        report += "  VRAM Score: " + std::to_string(CalculateVRAMScore(caps)) + "/2000\n";
        report += "  Feature Score: " + std::to_string(CalculateFeatureScore(caps)) + "/1000\n";
        report += "\nMetrics:\n";
        report += "  Compute: " + std::to_string(caps.computeTFLOPS) + " TFLOPS\n";
        report += "  Memory: " + std::to_string(caps.memoryBandwidthGBS) + " GB/s\n";
        report += "  VRAM: " + std::to_string(caps.dedicatedMemoryMB) + " MB\n";
        
        return report;
    }
    
    /**
     * Get preset name as string
     */
    static std::string GetPresetName(QualityPreset preset) {
        switch (preset) {
            case QualityPreset::LOW: return "LOW";
            case QualityPreset::MEDIUM: return "MEDIUM";
            case QualityPreset::HIGH: return "HIGH";
            case QualityPreset::ULTRA: return "ULTRA";
            case QualityPreset::CUSTOM: return "CUSTOM";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * Get recommended settings for preset
     */
    struct RecommendedSettings {
        int clothResolution;      // e.g., 30, 50, 60
        int physicsIterations;    // e.g., 2, 3, 5
        int collisionSubsteps;    // e.g., 1, 2, 3
        int batchCount;           // e.g., 4, 2, 1
        int workGroupSize;        // e.g., 128, 256
        int targetFPS;            // e.g., 60
    };
    
    static RecommendedSettings GetRecommendedSettings(QualityPreset preset) {
        RecommendedSettings settings;
        
        switch (preset) {
            case QualityPreset::LOW:
                settings.clothResolution = 30;
                settings.physicsIterations = 2;
                settings.collisionSubsteps = 1;
                settings.batchCount = 4;  // More batches for stability
                settings.workGroupSize = 128;
                settings.targetFPS = 30;
                break;
                
            case QualityPreset::MEDIUM:
                settings.clothResolution = 45;
                settings.physicsIterations = 3;
                settings.collisionSubsteps = 2;
                settings.batchCount = 2;
                settings.workGroupSize = 192;
                settings.targetFPS = 60;
                break;
                
            case QualityPreset::HIGH:
                settings.clothResolution = 55;
                settings.physicsIterations = 4;
                settings.collisionSubsteps = 2;
                settings.batchCount = 1;  // Less batches for performance
                settings.workGroupSize = 256;
                settings.targetFPS = 60;
                break;
                
            case QualityPreset::ULTRA:
                settings.clothResolution = 60;
                settings.physicsIterations = 5;
                settings.collisionSubsteps = 3;
                settings.batchCount = 1;
                settings.workGroupSize = 256;
                settings.targetFPS = 144;
                break;
                
            default:
                settings.clothResolution = 30;
                settings.physicsIterations = 2;
                settings.collisionSubsteps = 1;
                settings.batchCount = 4;
                settings.workGroupSize = 128;
                settings.targetFPS = 30;
                break;
        }
        
        return settings;
    }
    
private:
    /**
     * Calculate total performance score (0-10000)
     */
    static int CalculateScore(const GPUCaps& caps) {
        int computeScore = CalculateComputeScore(caps);    // 0-4000
        int memoryScore = CalculateMemoryScore(caps);      // 0-3000
        int vramScore = CalculateVRAMScore(caps);          // 0-2000
        int featureScore = CalculateFeatureScore(caps);    // 0-1000
        
        return computeScore + memoryScore + vramScore + featureScore;
    }
    
    /**
     * Compute shader performance score (0-4000)
     * Weight: 40% of total score
     */
    static int CalculateComputeScore(const GPUCaps& caps) {
        // Score = TFLOPS * 400, capped at 4000
        // RTX 4090 (~100 TFLOPS) → 4000
        // RTX 3060 (~13 TFLOPS) → 2600
        // Intel UHD (~0.5 TFLOPS) → 200
        float score = caps.computeTFLOPS * 400.0f;
        return static_cast<int>(std::min(score, 4000.0f));
    }
    
    /**
     * Memory bandwidth score (0-3000)
     * Weight: 30% of total score
     */
    static int CalculateMemoryScore(const GPUCaps& caps) {
        // Score = Bandwidth * 6, capped at 3000
        // RTX 4090 (~1000 GB/s) → 3000
        // RTX 3060 (~360 GB/s) → 2160
        // Intel UHD (~50 GB/s) → 300
        float score = caps.memoryBandwidthGBS * 6.0f;
        return static_cast<int>(std::min(score, 3000.0f));
    }
    
    /**
     * VRAM capacity score (0-2000)
     * Weight: 20% of total score
     */
    static int CalculateVRAMScore(const GPUCaps& caps) {
        // Score = VRAM_MB / 2, capped at 2000
        // 16GB → 2000
        // 8GB → 1000
        // 2GB → 200
        float score = static_cast<float>(caps.dedicatedMemoryMB) / 2.0f;
        return static_cast<int>(std::min(score, 2000.0f));
    }
    
    /**
     * Feature support score (0-1000)
     * Weight: 10% of total score
     */
    static int CalculateFeatureScore(const GPUCaps& caps) {
        int score = 0;
        
        // Basic features (required)
        if (caps.hasComputeShader) score += 300;
        if (caps.hasSSBO) score += 300;
        
        // Advanced features (bonus)
        if (caps.hasPersistentMapping) score += 200;
        if (caps.hasAsyncCompute) score += 100;
        if (caps.hasRayTracing) score += 50;
        if (caps.hasMeshShaders) score += 50;
        
        return std::min(score, 1000);
    }
};

} // namespace cloth
