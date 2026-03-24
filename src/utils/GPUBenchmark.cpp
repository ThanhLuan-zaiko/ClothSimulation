#include "GPUBenchmark.h"
#include "renderer/Shader.h"
#include <iostream>
#include <chrono>
#include <cstring>
#include <vector>
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

// OpenGL 4.3+ constant
#ifndef GL_MAX_COMPUTE_SHADER_INVOCATIONS
#define GL_MAX_COMPUTE_SHADER_INVOCATIONS 0x826D
#endif

namespace cloth {

// Benchmark Shader Sources
const char* COMPUTE_BENCHMARK_SRC = R"(
#version 430 core
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Data { float values[]; };
void main() {
    uint idx = gl_GlobalInvocationID.x;
    float v = values[idx];
    for(int i = 0; i < 1000; i++) {
        v = fma(v, 1.0001, 0.0001);
        v = fma(v, 0.9999, -0.0001);
    }
    values[idx] = v;
}
)";

const char* BANDWIDTH_BENCHMARK_SRC = R"(
#version 430 core
layout(local_size_x = 256) in;
layout(std430, binding = 0) buffer Source { float src[]; };
layout(std430, binding = 1) buffer Dest { float dst[]; };
void main() {
    uint idx = gl_GlobalInvocationID.x;
    dst[idx] = src[idx];
}
)";

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
    , hasARBComputeShader5(false)
    , hasARBTextureGather(false)
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

GPUBenchmark::GPUBenchmark() {}
GPUBenchmark::~GPUBenchmark() {}

GPUCaps GPUBenchmark::RunBenchmark() {
    std::cout << "\n[GPUBenchmark] Starting Hardware-Accelerated GPU Benchmark..." << std::endl;
    
    GPUCaps caps = QueryHardware();
    
    if (!caps.hasComputeShader) {
        std::cerr << "[GPUBenchmark] Error: Compute Shaders not supported!" << std::endl;
        return caps;
    }
    
    // Step 2: Run compute performance test (TFLOPS)
    std::cout << "[GPUBenchmark] Measuring Compute TFLOPS..." << std::endl;
    caps.computeTFLOPS = RunComputeShaderBenchmark(10); // 10 dispatches for averaging
    
    // Step 3: Run memory bandwidth test (GB/s)
    std::cout << "[GPUBenchmark] Measuring Memory Bandwidth..." << std::endl;
    caps.memoryBandwidthGBS = RunMemoryBandwidthTest(64 * 1024 * 1024); // 64 MB buffer
    
    // Step 4: Calculate overall score (0-10000)
    // TFLOPS weight: 600, Bandwidth weight: 20, VRAM weight: 0.1
    float score = (caps.computeTFLOPS * 600.0f) + (caps.memoryBandwidthGBS * 20.0f) + (caps.dedicatedMemoryMB * 0.1f);
    caps.benchmarkScore = std::min(10000, static_cast<int>(score));
    
    std::cout << "[GPUBenchmark] Benchmark Results:" << std::endl;
    std::cout << "  - Compute: " << caps.computeTFLOPS << " TFLOPS" << std::endl;
    std::cout << "  - Memory:  " << caps.memoryBandwidthGBS << " GB/s" << std::endl;
    std::cout << "  - Score:   " << caps.benchmarkScore << " / 10000" << std::endl;
    
    return caps;
}

GPUCaps GPUBenchmark::RunQuickBenchmark() {
    return RunBenchmark(); // Now fast enough to run as default
}

GPUCaps GPUBenchmark::QueryHardware() {
    GPUCaps caps;
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    if (renderer) caps.gpuName = reinterpret_cast<const char*>(renderer);
    if (vendor) caps.vendor = reinterpret_cast<const char*>(vendor);
    
    caps.isIntegrated = (caps.gpuName.find("Intel") != std::string::npos || caps.gpuName.find("UHD") != std::string::npos || caps.gpuName.find("Iris") != std::string::npos);
    
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    caps.hasComputeShader = (major > 4 || (major == 4 && minor >= 3));
    caps.hasSSBO = caps.hasComputeShader;
    caps.hasPersistentMapping = (major > 4 || (major == 4 && minor >= 4));
    
    const char* extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (extensions) {
        caps.hasARBComputeShader5 = strstr(extensions, "GL_ARB_compute_shader_5") != nullptr;
        caps.hasARBTextureGather = strstr(extensions, "GL_ARB_texture_gather") != nullptr;
        caps.hasAsyncCompute = caps.hasARBComputeShader5 && !caps.isIntegrated;
    }
    
    GLint workGroupSize[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, workGroupSize);
    caps.maxWorkGroupSize = static_cast<unsigned int>(workGroupSize[0]);
    
    GLint maxInvocations;
    glGetIntegerv(GL_MAX_COMPUTE_SHADER_INVOCATIONS, &maxInvocations);
    caps.maxComputeShaderInvocations = static_cast<unsigned int>(maxInvocations);
    
    QueryGPUMemoryInfo(caps.dedicatedMemoryMB, caps.sharedMemoryMB);
    EstimateComputeUnits(caps.computeUnits);
    caps.architectureGeneration = EstimateArchitectureGeneration();
    
    return caps;
}

float GPUBenchmark::RunComputeShaderBenchmark(int iterations) {
    const int numElements = 1024 * 1024; // 1M elements
    Shader benchShader = Shader::CreateComputeShaderFromSource(COMPUTE_BENCHMARK_SRC);
    if (benchShader.GetID() == 0) return 0.0f;

    unsigned int ssbo;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    std::vector<float> data(numElements, 1.0f);
    glBufferData(GL_SHADER_STORAGE_BUFFER, numElements * sizeof(float), data.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    auto start = std::chrono::high_resolution_clock::now();
    benchShader.Bind();
    for(int i = 0; i < iterations; i++) {
        glDispatchCompute(numElements / 256, 1, 1);
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();
    auto end = std::chrono::high_resolution_clock::now();
    
    glDeleteBuffers(1, &ssbo);
    
    float totalTime = std::chrono::duration<float>(end - start).count();
    float avgTime = totalTime / iterations;
    
    // Each invocation does 2000 FMAs (1000 * 2) = 4000 flops
    float totalFlops = (float)numElements * 4000.0f;
    return (totalFlops / avgTime) / 1e12f; // TFLOPS
}

float GPUBenchmark::RunMemoryBandwidthTest(size_t bufferSize) {
    const int numElements = static_cast<int>(bufferSize / sizeof(float));
    Shader benchShader = Shader::CreateComputeShaderFromSource(BANDWIDTH_BENCHMARK_SRC);
    if (benchShader.GetID() == 0) return 0.0f;

    unsigned int ssboSrc, ssboDst;
    glGenBuffers(1, &ssboSrc);
    glGenBuffers(1, &ssboDst);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSrc);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboDst);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_STATIC_DRAW);

    auto start = std::chrono::high_resolution_clock::now();
    benchShader.Bind();
    const int iterations = 20;
    for(int i = 0; i < iterations; i++) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboSrc);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboDst);
        glDispatchCompute(numElements / 256, 1, 1);
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();
    auto end = std::chrono::high_resolution_clock::now();

    glDeleteBuffers(1, &ssboSrc);
    glDeleteBuffers(1, &ssboDst);

    float totalTime = std::chrono::duration<float>(end - start).count();
    float avgTime = totalTime / iterations;
    
    // Bytes transferred = bufferSize (read) + bufferSize (write)
    float totalBytes = static_cast<float>(bufferSize) * 2.0f;
    return (totalBytes / avgTime) / 1e9f; // GB/s
}

void GPUBenchmark::QueryGPUMemoryInfo(size_t& dedicated, size_t& shared) {
    GLint memInfo[4] = {0};
    glGetIntegerv(GL_GPU_MEM_INFO_DEDICATED_VIDMEM_NVX, memInfo);
    if (memInfo[0] != 0) {
        dedicated = static_cast<size_t>(memInfo[0]) / 1024;
    } else {
        glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, memInfo);
        if (memInfo[0] != 0) dedicated = static_cast<size_t>(memInfo[0]) / 1024;
        else dedicated = 1024; // Conservative fallback
    }
    shared = 2048; // Estimate
}

void GPUBenchmark::EstimateComputeUnits(unsigned int& units) {
    const std::string name = (const char*)glGetString(GL_RENDERER);
    if (name.find("4090") != std::string::npos) units = 16384;
    else if (name.find("4080") != std::string::npos) units = 9728;
    else if (name.find("3080") != std::string::npos) units = 8704;
    else if (name.find("3060") != std::string::npos) units = 3584;
    else if (name.find("RTX") != std::string::npos) units = 2000;
    else units = 128;
}

int GPUBenchmark::EstimateArchitectureGeneration() {
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    int version = major * 10 + minor;
    if (version >= 46) return 5;
    if (version >= 45) return 4;
    return 2;
}

float GPUBenchmark::MeasureComputePerformance() { return RunComputeShaderBenchmark(5); }
float GPUBenchmark::MeasureMemoryBandwidth() { return RunMemoryBandwidthTest(32 * 1024 * 1024); }
bool GPUBenchmark::CheckFeatureSupport() { 
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    return (major > 4 || (major == 4 && minor >= 3));
}
int GPUBenchmark::GetOpenGLVersionScore() {
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    return major * 10 + minor;
}

} // namespace cloth
