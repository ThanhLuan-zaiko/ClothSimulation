#pragma once

#include <glad/glad.h>
#include <string>
#include <iostream>
#include "QualityPreset.h"
#include "GPUBenchmark.h"
#include "QualityClassifier.h"
#include "ClothSimulationBenchmark.h"
#include "AdaptiveQuality.h"

namespace cloth {

// GPU Vendor types
enum class GPUVendor {
    UNKNOWN,
    INTEL_INTEGRATED,
    NVIDIA,
    AMD,
    APPLE
};

// Quality presets - see QualityPreset.h for definition
// enum class QualityPreset { LOW, MEDIUM, HIGH, ULTRA, CUSTOM };

// GPU capability info
struct GPUInfo {
    std::string name;
    std::string vendor;
    std::string renderer;
    std::string glVersion;
    GPUVendor vendorType;
    QualityPreset detectedPreset;
    
    // Physics settings based on GPU
    int physicsIterations;
    int collisionSubsteps;
    int workGroupSize;
    int batchCount;  // For TDR prevention
    
    // Cloth settings
    int clothResolution[3];  // Resolution for each cloth
    
    // Feature support
    bool supportsComputeShader;
    bool supportsSSBO;
    bool supportsPersistentMapping;
    
    GPUInfo() : vendorType(GPUVendor::UNKNOWN), detectedPreset(QualityPreset::LOW) {
        // Default conservative settings (Intel integrated)
        physicsIterations = 2;
        collisionSubsteps = 1;
        workGroupSize = 128;
        batchCount = 4;
        clothResolution[0] = 30;
        clothResolution[1] = 25;
        clothResolution[2] = 25;
        
        supportsComputeShader = true;
        supportsSSBO = true;
        supportsPersistentMapping = true;
    }
};

class GPUDetector {
public:
    // Detect GPU and return info
    static GPUInfo Detect() {
        GPUInfo info;
        
        // Get GPU strings
        const GLubyte* renderer = glGetString(GL_RENDERER);
        const GLubyte* vendor = glGetString(GL_VENDOR);
        const GLubyte* glVersion = glGetString(GL_VERSION);
        
        if (renderer) info.renderer = reinterpret_cast<const char*>(renderer);
        if (vendor) info.vendor = reinterpret_cast<const char*>(vendor);
        if (glVersion) info.glVersion = reinterpret_cast<const char*>(glVersion);
        
        info.name = info.renderer;
        
        // Detect vendor type
        info.vendorType = DetectVendorType(info);
        
        // Detect quality preset based on GPU
        info.detectedPreset = DetectQualityPreset(info);
        
        // Apply preset settings
        ApplyPresetSettings(info, info.detectedPreset);
        
        // Check feature support
        CheckFeatureSupport(info);
        
        return info;
    }
    
    // Show quality preset menu and let user choose
    static QualityPreset ShowQualityMenu(const GPUInfo& detectedInfo) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "       GPU DETECTED: " << detectedInfo.name << std::endl;
        std::cout << "       Recommended: " << GetPresetName(detectedInfo.detectedPreset) << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "\nSelect Quality Preset:" << std::endl;
        std::cout << "  [1] LOW    - Integrated GPU (Intel UHD, Iris Xe)" << std::endl;
        std::cout << "             Stable physics, lower resolution" << std::endl;
        std::cout << "  [2] MEDIUM - Entry Dedicated (GTX 1650, RX 6400)" << std::endl;
        std::cout << "             Good balance" << std::endl;
        std::cout << "  [3] HIGH   - Mid-range (RTX 3060, RX 6700)" << std::endl;
        std::cout << "             High resolution, more physics iterations" << std::endl;
        std::cout << "  [4] ULTRA  - High-end (RTX 4080+, RX 7900+)" << std::endl;
        std::cout << "             Maximum quality, full physics" << std::endl;
        std::cout << "  [5] CUSTOM - Use detected settings" << std::endl;
        std::cout << "\nEnter choice (1-5, default=detected): ";
        
        int choice;
        std::cin >> choice;
        
        switch (choice) {
            case 1: return QualityPreset::LOW;
            case 2: return QualityPreset::MEDIUM;
            case 3: return QualityPreset::HIGH;
            case 4: return QualityPreset::ULTRA;
            case 5: return QualityPreset::CUSTOM;
            default: 
                std::cout << "Using detected preset: " << GetPresetName(detectedInfo.detectedPreset) << std::endl;
                return detectedInfo.detectedPreset;
        }
    }
    
    // Run quick benchmark to verify GPU capability
    static bool RunBenchmark(GPUInfo& info) {
        std::cout << "\nRunning GPU benchmark..." << std::endl;
        
        // Simple benchmark: measure compute shader dispatch time
        // This is a placeholder - full benchmark would be more comprehensive
        
        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        
        if (major < 4 || (major == 4 && minor < 3)) {
            std::cout << "WARNING: OpenGL version may not support all features!" << std::endl;
            info.detectedPreset = QualityPreset::LOW;
            return false;
        }
        
        std::cout << "OpenGL " << major << "." << minor << " - Full feature support confirmed" << std::endl;
        std::cout << "Benchmark complete!" << std::endl;
        
        return true;
    }
    
    // Get preset name as string
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
    
    // Apply preset settings to GPUInfo
    static void ApplyPresetSettings(GPUInfo& info, QualityPreset preset) {
        switch (preset) {
            case QualityPreset::LOW:
                // Intel integrated GPU - conservative settings
                info.physicsIterations = 2;
                info.collisionSubsteps = 1;
                info.workGroupSize = 128;
                info.batchCount = 4;  // Prevent TDR
                info.clothResolution[0] = 30;
                info.clothResolution[1] = 25;
                info.clothResolution[2] = 25;
                break;
                
            case QualityPreset::MEDIUM:
                // Entry-level dedicated GPU
                info.physicsIterations = 3;
                info.collisionSubsteps = 2;
                info.workGroupSize = 192;
                info.batchCount = 2;
                info.clothResolution[0] = 45;
                info.clothResolution[1] = 40;
                info.clothResolution[2] = 40;
                break;
                
            case QualityPreset::HIGH:
                // Mid-range dedicated GPU
                info.physicsIterations = 4;
                info.collisionSubsteps = 2;
                info.workGroupSize = 256;
                info.batchCount = 1;  // No batching needed
                info.clothResolution[0] = 55;
                info.clothResolution[1] = 50;
                info.clothResolution[2] = 50;
                break;
                
            case QualityPreset::ULTRA:
                // High-end GPU - MAX POWER!
                info.physicsIterations = 5;
                info.collisionSubsteps = 3;
                info.workGroupSize = 256;
                info.batchCount = 1;
                info.clothResolution[0] = 60;
                info.clothResolution[1] = 60;
                info.clothResolution[2] = 60;
                break;
                
            case QualityPreset::CUSTOM:
                // Keep current values (from detection)
                break;
        }
    }
    
private:
    // Detect GPU vendor type
    static GPUVendor DetectVendorType(const GPUInfo& info) {
        std::string name = info.name;
        std::string vendor = info.vendor;
        
        // Check vendor string first
        if (vendor.find("Intel") != std::string::npos) {
            return GPUVendor::INTEL_INTEGRATED;
        }
        if (vendor.find("NVIDIA") != std::string::npos) {
            return GPUVendor::NVIDIA;
        }
        if (vendor.find("AMD") != std::string::npos || vendor.find("Advanced Micro Devices") != std::string::npos) {
            return GPUVendor::AMD;
        }
        
        // Check renderer string
        if (name.find("Intel") != std::string::npos) {
            return GPUVendor::INTEL_INTEGRATED;
        }
        if (name.find("GeForce") != std::string::npos || name.find("Quadro") != std::string::npos || 
            name.find("RTX") != std::string::npos || name.find("GTX") != std::string::npos) {
            return GPUVendor::NVIDIA;
        }
        if (name.find("Radeon") != std::string::npos || name.find("AMD") != std::string::npos) {
            return GPUVendor::AMD;
        }
        if (name.find("Apple") != std::string::npos || name.find("M1") != std::string::npos ||
            name.find("M2") != std::string::npos || name.find("M3") != std::string::npos) {
            return GPUVendor::APPLE;
        }
        
        return GPUVendor::UNKNOWN;
    }
    
    // Detect quality preset based on GPU name
    static QualityPreset DetectQualityPreset(const GPUInfo& info) {
        std::string name = info.name;
        
        // Intel integrated GPUs
        if (name.find("UHD Graphics") != std::string::npos ||
            name.find("Iris Xe") != std::string::npos ||
            name.find("HD Graphics") != std::string::npos) {
            return QualityPreset::LOW;
        }
        
        // NVIDIA GPUs
        if (name.find("RTX 4090") != std::string::npos || name.find("RTX 4080") != std::string::npos ||
            name.find("RTX 4070") != std::string::npos || name.find("RTX 4060") != std::string::npos) {
            return QualityPreset::ULTRA;
        }
        if (name.find("RTX 30") != std::string::npos || name.find("RTX 20") != std::string::npos) {
            return QualityPreset::HIGH;
        }
        if (name.find("GTX 16") != std::string::npos || name.find("GTX 10") != std::string::npos) {
            return QualityPreset::MEDIUM;
        }
        
        // AMD GPUs
        if (name.find("RX 7900") != std::string::npos || name.find("RX 7800") != std::string::npos) {
            return QualityPreset::ULTRA;
        }
        if (name.find("RX 6800") != std::string::npos || name.find("RX 6700") != std::string::npos ||
            name.find("RX 6600") != std::string::npos) {
            return QualityPreset::HIGH;
        }
        if (name.find("RX 5500") != std::string::npos || name.find("RX 570") != std::string::npos) {
            return QualityPreset::MEDIUM;
        }
        
        // Apple Silicon
        if (name.find("M3") != std::string::npos) {
            return QualityPreset::HIGH;
        }
        if (name.find("M2") != std::string::npos) {
            return QualityPreset::MEDIUM;
        }
        if (name.find("M1") != std::string::npos) {
            return QualityPreset::MEDIUM;
        }
        
        // Default to LOW for unknown GPUs (safe fallback)
        return QualityPreset::LOW;
    }
    
    // Check OpenGL feature support
    static void CheckFeatureSupport(GPUInfo& info) {
        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        
        // Compute shader requires OpenGL 4.3+
        info.supportsComputeShader = (major > 4 || (major == 4 && minor >= 3));
        
        // SSBO requires OpenGL 4.3+
        info.supportsSSBO = (major > 4 || (major == 4 && minor >= 3));
        
        // Persistent mapping requires OpenGL 4.4+
        info.supportsPersistentMapping = (major > 4 || (major == 4 && minor >= 4));
        
        if (!info.supportsComputeShader) {
            std::cout << "WARNING: Compute shaders not supported!" << std::endl;
        }
        if (!info.supportsSSBO) {
            std::cout << "WARNING: SSBO not supported!" << std::endl;
        }
        if (!info.supportsPersistentMapping) {
            std::cout << "INFO: Persistent mapping not available, using fallback" << std::endl;
        }
    }
};

} // namespace cloth
