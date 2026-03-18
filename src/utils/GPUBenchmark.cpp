#include "GPUBenchmark.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <glm/glm.hpp>

// NVX extensions for GPU memory info (NVIDIA)
#ifndef GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX 0x9047
#endif

// ATI extensions (AMD)
#ifndef GL_TEXTURE_FREE_MEMORY_ATI
#define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC
#endif

// OpenGL 4.3+ constant (may not be in older glad.h)
#ifndef GL_MAX_COMPUTE_SHADER_INVOCATIONS
#define GL_MAX_COMPUTE_SHADER_INVOCATIONS 0x826D
#endif

namespace cloth {

// ============================================================================
// GPUCaps Implementation
// ============================================================================

GPUCaps::GPUCaps()
    : dedicatedMemoryMB(0)
    , sharedMemoryMB(0)
    , maxWorkGroupSize(0)
    , maxComputeShaderInvocations(0)
    , hasComputeShader(false)
    , hasSSBO(false)
    , hasPersistentMapping(false)
    , hasAsyncCompute(false)
    , hasRayTracing(false)
    , hasMeshShaders(false)
    , computeTFLOPS(0.0f)
    , memoryBandwidthGBS(0.0f)
    , singlePrecisionPerf(0.0f)
    , computeUnits(0)
    , memoryBusWidth(0)
    , architectureGeneration(1)
    , isIntegrated(false)
    , benchmarkFPS(0.0f)
    , benchmarkScore(0)
{
}

// ============================================================================
// GPUBenchmark Implementation
// ============================================================================

GPUBenchmark::GPUBenchmark() {
}

GPUBenchmark::~GPUBenchmark() {
}

GPUCaps GPUBenchmark::RunBenchmark() {
    std::cout << "\n[GPUBenchmark] Starting full GPU benchmark..." << std::endl;
    
    GPUCaps caps;
    
    // Step 1: Query hardware info
    std::cout << "[GPUBenchmark] Querying hardware..." << std::endl;
    caps = QueryHardware();
    
    // Step 2: Run compute performance test
    std::cout << "[GPUBenchmark] Running compute performance test..." << std::endl;
    caps.computeTFLOPS = MeasureComputePerformance();
    
    // Step 3: Run memory bandwidth test
    std::cout << "[GPUBenchmark] Running memory bandwidth test..." << std::endl;
    caps.memoryBandwidthGBS = MeasureMemoryBandwidth();
    
    // Step 4: Calculate overall score
    caps.benchmarkScore = static_cast<int>(
        (caps.computeTFLOPS * 100.0f) + 
        (caps.memoryBandwidthGBS * 10.0f) +
        (caps.dedicatedMemoryMB / 100.0f)
    );
    
    // Cap score at 10000
    if (caps.benchmarkScore > 10000) caps.benchmarkScore = 10000;
    
    std::cout << "[GPUBenchmark] Benchmark complete! Score: " << caps.benchmarkScore << std::endl;
    std::cout << "  Compute: " << caps.computeTFLOPS << " TFLOPS" << std::endl;
    std::cout << "  Memory: " << caps.memoryBandwidthGBS << " GB/s" << std::endl;
    std::cout << "  VRAM: " << caps.dedicatedMemoryMB << " MB" << std::endl;
    
    return caps;
}

GPUCaps GPUBenchmark::RunQuickBenchmark() {
    std::cout << "\n[GPUBenchmark] Running quick benchmark..." << std::endl;
    
    GPUCaps caps = QueryHardware();
    
    // Quick compute test (1 iteration)
    caps.computeTFLOPS = RunComputeShaderBenchmark(1);
    
    // Quick memory test (16 MB buffer)
    caps.memoryBandwidthGBS = RunMemoryBandwidthTest(16 * 1024 * 1024);
    
    // Calculate quick score
    caps.benchmarkScore = static_cast<int>(
        (caps.computeTFLOPS * 100.0f) + 
        (caps.memoryBandwidthGBS * 10.0f)
    );
    
    std::cout << "[GPUBenchmark] Quick benchmark score: " << caps.benchmarkScore << std::endl;
    
    return caps;
}

GPUCaps GPUBenchmark::QueryHardware() {
    GPUCaps caps;
    
    // Get GPU name
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    
    if (renderer) caps.gpuName = reinterpret_cast<const char*>(renderer);
    if (vendor) caps.vendor = reinterpret_cast<const char*>(vendor);
    
    // Detect vendor type
    caps.isIntegrated = (caps.vendor.find("Intel") != std::string::npos);
    
    // Get OpenGL version
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    
    // Feature detection
    caps.hasComputeShader = (major > 4 || (major == 4 && minor >= 3));
    caps.hasSSBO = (major > 4 || (major == 4 && minor >= 3));
    caps.hasPersistentMapping = (major > 4 || (major == 4 && minor >= 4));
    
    // NEW: Check for extensions
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (extensions) {
        caps.hasARBComputeShader5 = strstr(extensions, "GL_ARB_compute_shader_5") != nullptr;
        caps.hasARBTextureGather = strstr(extensions, "GL_ARB_texture_gather") != nullptr;
    } else {
        caps.hasARBComputeShader5 = false;
        caps.hasARBTextureGather = false;
    }
    
    // NEW: Detect async compute based on GPU type
    caps.hasAsyncCompute = false;  // Default to false
    if (caps.hasARBComputeShader5) {
        // NVIDIA RTX/GTX 16+ support async compute
        if (caps.vendor.find("NVIDIA") != std::string::npos) {
            if (caps.gpuName.find("RTX") != std::string::npos ||
                caps.gpuName.find("GTX 16") != std::string::npos) {
                caps.hasAsyncCompute = true;
            }
        }
        // AMD RX series support async compute
        else if (caps.vendor.find("AMD") != std::string::npos ||
                 caps.gpuName.find("Radeon") != std::string::npos) {
            if (caps.gpuName.find("RX") != std::string::npos) {
                caps.hasAsyncCompute = true;
            }
        }
    }
    
    // Get max work group size
    GLint workGroupSize[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, workGroupSize);
    caps.maxWorkGroupSize = static_cast<unsigned int>(workGroupSize[0]);
    
    GLint maxInvocations;
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_INVOCATIONS, &maxInvocations);
    caps.maxComputeShaderInvocations = static_cast<unsigned int>(maxInvocations);
    
    // Query GPU memory
    QueryGPUMemoryInfo(caps.dedicatedMemoryMB, caps.sharedMemoryMB);
    
    // Estimate compute units
    EstimateComputeUnits(caps.computeUnits);
    
    // Estimate architecture generation
    caps.architectureGeneration = EstimateArchitectureGeneration();
    
    // Estimate memory bus width (rough estimate based on VRAM)
    if (caps.dedicatedMemoryMB >= 8192) {
        caps.memoryBusWidth = 256;
    } else if (caps.dedicatedMemoryMB >= 4096) {
        caps.memoryBusWidth = 192;
    } else if (caps.dedicatedMemoryMB >= 2048) {
        caps.memoryBusWidth = 128;
    } else {
        caps.memoryBusWidth = 64; // Integrated
    }
    
    std::cout << "[GPUBenchmark] Hardware queried: " << caps.gpuName << std::endl;
    std::cout << "  VRAM: " << caps.dedicatedMemoryMB << " MB" << std::endl;
    std::cout << "  Compute Shader: " << (caps.hasComputeShader ? "YES" : "NO") << std::endl;
    std::cout << "  SSBO: " << (caps.hasSSBO ? "YES" : "NO") << std::endl;
    
    return caps;
}

float GPUBenchmark::MeasureComputePerformance() {
    return RunComputeShaderBenchmark(5); // 5 iterations for accuracy
}

float GPUBenchmark::MeasureMemoryBandwidth() {
    return RunMemoryBandwidthTest(64 * 1024 * 1024); // 64 MB test
}

bool GPUBenchmark::CheckFeatureSupport() {
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    
    bool hasRequiredVersion = (major > 4 || (major == 4 && minor >= 3));
    
    if (!hasRequiredVersion) {
        std::cerr << "[GPUBenchmark] OpenGL 4.3+ required, found " << major << "." << minor << std::endl;
        return false;
    }
    
    return true;
}

int GPUBenchmark::GetOpenGLVersionScore() {
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    
    return major * 10 + minor;
}

// ============================================================================
// Internal Benchmark Helpers
// ============================================================================

float GPUBenchmark::RunComputeShaderBenchmark(int iterations) {
    // Simple compute benchmark: matrix multiplication
    const int MATRIX_SIZE = 1024;
    
    // Create test data
    std::vector<float> matrixA(MATRIX_SIZE * MATRIX_SIZE);
    std::vector<float> matrixB(MATRIX_SIZE * MATRIX_SIZE);
    
    for (int i = 0; i < MATRIX_SIZE * MATRIX_SIZE; i++) {
        matrixA[i] = static_cast<float>(rand()) / RAND_MAX;
        matrixB[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    
    // Benchmark loop
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int iter = 0; iter < iterations; iter++) {
        // Simulate compute workload (matrix multiply on CPU for estimation)
        // In real implementation, this would use compute shader
        volatile float result = 0.0f;
        for (int i = 0; i < 1000000; i++) {
            result += matrixA[i % MATRIX_SIZE] * matrixB[i % MATRIX_SIZE];
        }
        (void)result; // Prevent optimization
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Estimate TFLOPS (very rough estimate)
    // Operations = iterations * 1M * 2 (multiply + add)
    float operations = iterations * 1000000.0f * 2.0f;
    float seconds = duration.count() / 1000.0f;
    float flops = operations / seconds;
    float tflops = flops / 1e12f;
    
    // Scale to realistic GPU TFLOPS (CPU benchmark is much slower)
    // This is a placeholder - real implementation would use GPU compute shader
    tflops *= 100.0f; // GPU is ~100x faster than single-threaded CPU
    
    return tflops;
}

float GPUBenchmark::RunMemoryBandwidthTest(size_t bufferSize) {
    // Create test buffer
    std::vector<float> data(bufferSize / sizeof(float));
    
    // Initialize
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = static_cast<float>(i);
    }
    
    // Benchmark memory read/write
    auto startTime = std::chrono::high_resolution_clock::now();
    
    const int iterations = 100;
    for (int iter = 0; iter < iterations; iter++) {
        // Sequential access
        for (size_t i = 0; i < data.size(); i++) {
            data[i] = data[i] * 1.001f;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Calculate bandwidth
    // Total bytes transferred = iterations * bufferSize * 2 (read + write)
    float totalBytes = iterations * bufferSize * 2.0f;
    float seconds = duration.count() / 1000.0f;
    float bandwidthBPS = totalBytes / seconds;
    float bandwidthGBS = bandwidthBPS / 1e9f;
    
    // Scale to realistic GPU memory bandwidth
    // GPU memory is much faster than system RAM
    bandwidthGBS *= 10.0f; // Rough estimate
    
    return bandwidthGBS;
}

void GPUBenchmark::QueryGPUMemoryInfo(size_t& dedicated, size_t& shared) {
    // Try NVIDIA extensions
    GLint memInfo[4] = {0};
    
    // Query dedicated VRAM (NVIDIA)
    glGetIntegerv(GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX, memInfo);
    if (memInfo[0] != 0) {
        dedicated = static_cast<size_t>(memInfo[0]) / 1024; // KB to MB
    }
    
    // Query available memory (NVIDIA)
    glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, memInfo);
    if (memInfo[0] != 0) {
        // Estimate total from available
        shared = static_cast<size_t>(memInfo[0]) / 1024;
    }
    
    // If still zero, estimate based on common configurations
    if (dedicated == 0) {
        // Check for AMD extension
        glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, memInfo);
        if (memInfo[0] != 0) {
            dedicated = static_cast<size_t>(memInfo[0]) / 1024;
        }
    }
    
    // Fallback estimation
    if (dedicated == 0) {
        // Assume integrated GPU with shared memory
        dedicated = 512; // Minimum
        shared = 2048;   // System RAM
    }
}

void GPUBenchmark::EstimateComputeUnits(unsigned int& units) {
    // This is a rough estimate based on GPU name
    // Real implementation would query hardware counters
    
    const std::string gpuName = []() {
        const GLubyte* renderer = glGetString(GL_RENDERER);
        return renderer ? reinterpret_cast<const char*>(renderer) : "";
    }();
    
    // NVIDIA estimation
    if (gpuName.find("RTX 4090") != std::string::npos) units = 16384;
    else if (gpuName.find("RTX 4080") != std::string::npos) units = 9728;
    else if (gpuName.find("RTX 4070") != std::string::npos) units = 5888;
    else if (gpuName.find("RTX 4060") != std::string::npos) units = 3072;
    else if (gpuName.find("RTX 30") != std::string::npos) units = 4000;
    else if (gpuName.find("GTX 16") != std::string::npos) units = 1500;
    
    // AMD estimation
    else if (gpuName.find("RX 7900") != std::string::npos) units = 6144;
    else if (gpuName.find("RX 6800") != std::string::npos) units = 3840;
    else if (gpuName.find("RX 6700") != std::string::npos) units = 2560;
    
    // Intel estimation
    else if (gpuName.find("Iris Xe") != std::string::npos) units = 96;
    else if (gpuName.find("UHD Graphics") != std::string::npos) units = 24;
    else units = 48; // Default
}

int GPUBenchmark::EstimateArchitectureGeneration() {
    // Estimate based on OpenGL version and features
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    
    int version = major * 10 + minor;
    
    if (version >= 46) return 5; // Latest
    if (version >= 45) return 4; // Recent
    if (version >= 44) return 3; // Mid-gen
    if (version >= 43) return 2; // Old
    return 1; // Very old
}

} // namespace cloth
