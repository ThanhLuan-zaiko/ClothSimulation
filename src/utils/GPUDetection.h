#pragma once

// NOMINMAX prevents Windows.h from defining min/max macros that conflict with std::min/std::max
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#include <glad/glad.h>
#include <string>
#include <iostream>
#include <algorithm>  // For std::min/std::max
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
    
    // NEW: Async compute and advanced feature capability
    bool supportsAsyncCompute;        // Hardware/driver supports async compute
    bool hasARBComputeShader5;        // ARB_compute_shader_5 extension
    bool hasARBTextureGather;         // ARB_texture_gather extension
    
    // Reflection quality settings
    int reflectionResolution;         // 256, 512, 1024
    bool reflectionHDR;               // Enable HDR rendering
    bool reflectionLOD;               // Enable LOD bias for performance
    float reflectionExposure;         // HDR exposure value

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

        // NEW: Async compute defaults (conservative)
        supportsAsyncCompute = false;
        hasARBComputeShader5 = false;
        hasARBTextureGather = false;
        
        // Reflection defaults (LOW preset)
        reflectionResolution = 256;
        reflectionHDR = true;          // HDR enabled even for LOW
        reflectionLOD = true;          // LOD blur enabled for performance
        reflectionExposure = 1.0f;
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
    
    // Run benchmark to verify GPU capability and auto-detect preset
    static bool RunBenchmark(GPUInfo& info) {
        std::cout << "\n[GPU] Running Hardware-Accelerated Benchmark..." << std::endl;
        
        GPUCaps caps = GPUBenchmark::RunBenchmark();
        
        // Advanced classification based on real performance
        QualityPreset detected = QualityPreset::LOW;
        
        if (caps.computeTFLOPS > 10.0f || caps.benchmarkScore > 8000) {
            detected = QualityPreset::ULTRA; // High-end (RTX 3080+, RX 6800+)
        } else if (caps.computeTFLOPS > 4.0f || caps.benchmarkScore > 5000) {
            detected = QualityPreset::HIGH;  // Mid-range (RTX 3060, RX 6600)
        } else if (caps.computeTFLOPS > 1.5f || caps.benchmarkScore > 2500) {
            detected = QualityPreset::MEDIUM; // Entry-level (GTX 1650, RX 580)
        } else {
            detected = QualityPreset::LOW;    // Integrated or old
        }
        
        // Update info with detected preset
        info.detectedPreset = detected;
        ApplyPresetSettings(info, detected);
        
        std::cout << "[GPU] Benchmark-based recommendation: " << GetPresetName(detected) << std::endl;
        
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
                info.physicsIterations = 5;  // +2 (was 3) - more stable
                info.collisionSubsteps = 1;
                info.workGroupSize = 128;
                info.batchCount = 4;  // Prevent TDR
                info.clothResolution[0] = 35;  // +5 (was 30)
                info.clothResolution[1] = 35;  // +5 (was 30)
                info.clothResolution[2] = 35;  // +5 (was 30)
                // Reflection: HDR enabled, but lower resolution + LOD blur for performance
                info.reflectionResolution = 256;
                info.reflectionHDR = true;
                info.reflectionLOD = true;
                info.reflectionExposure = 1.0f;
                break;

            case QualityPreset::MEDIUM:
                // Entry-level dedicated GPU
                info.physicsIterations = 8;  // +4 (was 4) - tighter cloth
                info.collisionSubsteps = 2;
                info.workGroupSize = 192;
                info.batchCount = 2;
                info.clothResolution[0] = 55;  // +10 (was 45)
                info.clothResolution[1] = 50;  // +10 (was 40)
                info.clothResolution[2] = 50;  // +10 (was 40)
                // Reflection: Good balance with HDR and LOD
                info.reflectionResolution = 512;
                info.reflectionHDR = true;
                info.reflectionLOD = true;
                info.reflectionExposure = 1.1f;
                break;

            case QualityPreset::HIGH:
                // Mid-range dedicated GPU
                info.physicsIterations = 12;  // +7 (was 5) - even tighter
                info.collisionSubsteps = 2;
                info.workGroupSize = 256;
                info.batchCount = 1;  // No batching needed
                info.clothResolution[0] = 65;  // +10 (was 55)
                info.clothResolution[1] = 60;  // +10 (was 50)
                info.clothResolution[2] = 60;  // +10 (was 50)
                // Reflection: High quality with HDR, no LOD blur
                info.reflectionResolution = 512;
                info.reflectionHDR = true;
                info.reflectionLOD = false;
                info.reflectionExposure = 1.2f;
                break;

            case QualityPreset::ULTRA:
                // High-end GPU - MAX POWER!
                info.physicsIterations = 16;  // +10 (was 6) - maximum tightness
                info.collisionSubsteps = 3;
                info.workGroupSize = 256;
                info.batchCount = 1;
                info.clothResolution[0] = 70;  // +10 (was 60)
                info.clothResolution[1] = 70;  // +10 (was 60)
                info.clothResolution[2] = 70;  // +10 (was 60)
                // Reflection: Maximum quality - 1024x1024, HDR, no LOD
                info.reflectionResolution = 1024;
                info.reflectionHDR = true;
                info.reflectionLOD = false;
                info.reflectionExposure = 1.3f;
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
    
    // Check OpenGL feature support (ENHANCED with async compute detection)
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

        // NEW: Check for ARB_compute_shader_5 (enables advanced compute features)
        const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
        if (extensions) {
            info.hasARBComputeShader5 = strstr(extensions, "GL_ARB_compute_shader_5") != nullptr;
            info.hasARBTextureGather = strstr(extensions, "GL_ARB_texture_gather") != nullptr;
        }

        // NEW: Detect async compute capability based on GPU and extensions
        info.supportsAsyncCompute = DetectAsyncComputeSupport(info);

        // Apply GPU-specific optimizations
        ApplyGPUOptimizations(info);

        if (!info.supportsComputeShader) {
            std::cout << "WARNING: Compute shaders not supported!" << std::endl;
        }
        if (!info.supportsSSBO) {
            std::cout << "WARNING: SSBO not supported!" << std::endl;
        }
        if (!info.supportsPersistentMapping) {
            std::cout << "INFO: Persistent mapping not available, using fallback" << std::endl;
        }
        
        // NEW: Print async compute status
        std::cout << "Async Compute: " << (info.supportsAsyncCompute ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "ARB_compute_shader_5: " << (info.hasARBComputeShader5 ? "YES" : "NO") << std::endl;
        std::cout << "ARB_texture_gather: " << (info.hasARBTextureGather ? "YES" : "NO") << std::endl;
    }
    
    // NEW: Detect async compute support based on GPU type and extensions
    static bool DetectAsyncComputeSupport(const GPUInfo& info) {
        // Must have compute shader support first
        if (!info.supportsComputeShader) return false;
        
        // Must have ARB_compute_shader_5 for advanced features
        if (!info.hasARBComputeShader5) return false;
        
        // GPU-specific detection
        switch (info.vendorType) {
            case GPUVendor::NVIDIA:
                // NVIDIA RTX 30/40 series have excellent async compute support
                // GTX 16xx and RTX 20xx also support it
                if (info.name.find("RTX 40") != std::string::npos ||
                    info.name.find("RTX 30") != std::string::npos ||
                    info.name.find("RTX 20") != std::string::npos ||
                    info.name.find("GTX 16") != std::string::npos) {
                    return true;
                }
                // GTX 10xx series has limited async compute
                if (info.name.find("GTX 10") != std::string::npos) {
                    return info.hasARBComputeShader5;
                }
                return false;
                
            case GPUVendor::AMD:
                // AMD GCN and RDNA architectures have good async compute support
                if (info.name.find("RX 7") != std::string::npos ||
                    info.name.find("RX 6") != std::string::npos ||
                    info.name.find("RX 5") != std::string::npos ||
                    info.name.find("Radeon RX") != std::string::npos) {
                    return true;
                }
                return info.hasARBComputeShader5;
                
            case GPUVendor::APPLE:
                // Apple Silicon supports async compute via Metal, but OpenGL is limited
                return false;  // OpenGL on macOS doesn't expose async compute
                
            case GPUVendor::INTEL_INTEGRATED:
            default:
                // Intel integrated GPUs (UHD, Iris) don't have hardware async compute
                return false;
        }
    }
    
    // NEW: Apply GPU-specific optimizations
    static void ApplyGPUOptimizations(GPUInfo& info) {
        // Set optimal work group size based on GPU architecture
        switch (info.vendorType) {
            case GPUVendor::NVIDIA:
                // NVIDIA GPUs perform best with 256 work group size
                info.workGroupSize = 256;
                info.batchCount = 1;  // No TDR issues on dedicated GPU
                break;
                
            case GPUVendor::AMD:
                // AMD RDNA/GCN performs well with 192-256
                info.workGroupSize = 192;
                info.batchCount = 1;
                break;
                
            case GPUVendor::APPLE:
                // Apple Silicon performs well with 256
                info.workGroupSize = 256;
                info.batchCount = 1;
                break;
                
            case GPUVendor::INTEL_INTEGRATED:
            default:
                // Intel integrated GPUs need conservative settings
                info.workGroupSize = 128;
                info.batchCount = 4;  // Prevent TDR timeout
                break;
        }
        
        // Override based on quality preset (use explicit template to avoid type ambiguity)
        switch (info.detectedPreset) {
            case QualityPreset::LOW:
                info.workGroupSize = std::min<int>(info.workGroupSize, 128);
                info.batchCount = std::max<int>(info.batchCount, 4);
                break;
            case QualityPreset::MEDIUM:
                info.workGroupSize = std::min<int>(info.workGroupSize, 192);
                info.batchCount = std::max<int>(info.batchCount, 2);
                break;
            case QualityPreset::HIGH:
            case QualityPreset::ULTRA:
                info.workGroupSize = std::min<int>(info.workGroupSize, 256);
                info.batchCount = 1;
                break;
            default:
                break;
        }
    }
};

} // namespace cloth
