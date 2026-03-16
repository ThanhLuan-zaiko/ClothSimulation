#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>

namespace cloth {

/**
 * GPU capability and performance metrics
 * Collected via hardware query and micro-benchmarks
 */
struct GPUCaps {
    // === Hardware Info (Queried) ===
    std::string gpuName;
    std::string vendor;
    size_t dedicatedMemoryMB;     // Dedicated VRAM in MB
    size_t sharedMemoryMB;        // Shared system memory in MB
    unsigned int maxWorkGroupSize;
    unsigned int maxComputeShaderInvocations;
    
    // === Feature Support (Boolean) ===
    bool hasComputeShader;
    bool hasSSBO;                 // Shader Storage Buffer Object
    bool hasPersistentMapping;    // GL_ARB_buffer_storage
    bool hasAsyncCompute;         // Async compute queues
    bool hasRayTracing;           // Ray tracing extensions
    bool hasMeshShaders;          // Mesh/Task shaders
    
    // === Performance Metrics (Benchmarked) ===
    float computeTFLOPS;          // TFLOPS from compute benchmark
    float memoryBandwidthGBS;     // GB/s from memory benchmark
    float singlePrecisionPerf;    // Relative perf score (0-100)
    
    // === Architecture Info ===
    unsigned int computeUnits;    // CUDA cores / Stream processors
    unsigned int memoryBusWidth;  // Memory bus width in bits
    int architectureGeneration;   // 1=old, 5=new (estimated)
    bool isIntegrated;            // Integrated vs discrete
    
    // === Benchmark Results ===
    float benchmarkFPS;           // FPS from micro-benchmark
    int benchmarkScore;           // Overall score (0-10000)
    
    GPUCaps();
    
    // Get quality preset based on caps
    // (Implementation in GPUDetection.h to avoid circular dependency)
};

/**
 * GPU Micro-Benchmark Suite
 * Measures raw GPU performance for quality preset detection
 */
class GPUBenchmark {
public:
    GPUBenchmark();
    ~GPUBenchmark();
    
    /**
     * Run complete benchmark suite
     * @return GPUCaps with all metrics filled
     */
    static GPUCaps RunBenchmark();
    
    /**
     * Quick benchmark (for startup)
     * @return GPUCaps with basic metrics
     */
    static GPUCaps RunQuickBenchmark();
    
    /**
     * Query hardware capabilities (no benchmark)
     * @return GPUCaps with hardware info only
     */
    static GPUCaps QueryHardware();
    
    /**
     * Measure compute shader performance
     * @return TFLOPS estimate
     */
    static float MeasureComputePerformance();
    
    /**
     * Measure memory bandwidth
     * @return GB/s estimate
     */
    static float MeasureMemoryBandwidth();
    
    /**
     * Check if GPU supports required features
     * @return true if all required features supported
     */
    static bool CheckFeatureSupport();
    
    /**
     * Get OpenGL version score
     * @return version score (43 = 4.3, 46 = 4.6, etc.)
     */
    static int GetOpenGLVersionScore();
    
private:
    // Internal benchmark helpers
    static float RunComputeShaderBenchmark(int iterations);
    static float RunMemoryBandwidthTest(size_t bufferSize);
    static void QueryGPUMemoryInfo(size_t& dedicated, size_t& shared);
    static void EstimateComputeUnits(unsigned int& units);
    static int EstimateArchitectureGeneration();
};

/**
 * Benchmark progress callback
 * Called during benchmark to update UI
 */
using BenchmarkProgressCallback = void(float progress, const std::string& status);

} // namespace cloth
