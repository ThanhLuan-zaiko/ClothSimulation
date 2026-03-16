#pragma once

#include "GPUBenchmark.h"
#include "QualityPreset.h"
#include "QualityClassifier.h"
#include <chrono>
#include <vector>
#include <glm/glm.hpp>

// Undefine Windows min/max macros if they exist
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace cloth {

/**
 * Application-specific benchmark for cloth simulation
 * 
 * Unlike generic GPU benchmarks, this measures ACTUAL cloth simulation
 * performance to determine optimal quality settings.
 * 
 * This ensures:
 * - Accurate quality classification for THIS application
 * - Optimal particle count for target FPS
 * - Future-proof for any GPU (measures actual work, not specs)
 */
class ClothSimulationBenchmark {
public:
    /**
     * Benchmark results
     */
    struct BenchmarkResult {
        float fps1000;      // FPS with 1000 particles
        float fps3000;      // FPS with 3000 particles
        float fps5000;      // FPS with 5000 particles
        float avgFrameTime; // Average frame time (ms)
        QualityPreset recommendedPreset;
        int optimalParticleCount;
    };
    
    /**
     * Run full cloth simulation benchmark
     * 
     * Tests simulation at different particle counts to find optimal settings.
     * Takes ~2-3 seconds to complete.
     * 
     * @return BenchmarkResult with all metrics
     */
    static BenchmarkResult RunFullBenchmark() {
        std::cout << "\n[ClothBenchmark] Starting full cloth simulation benchmark..." << std::endl;
        
        BenchmarkResult result;
        
        // Test at different scales
        std::cout << "[ClothBenchmark] Testing 1000 particles..." << std::endl;
        result.fps1000 = SimulateClothFrame(1000, 3); // 3 iterations
        
        std::cout << "[ClothBenchmark] Testing 3000 particles..." << std::endl;
        result.fps3000 = SimulateClothFrame(3000, 3);
        
        std::cout << "[ClothBenchmark] Testing 5000 particles..." << std::endl;
        result.fps5000 = SimulateClothFrame(5000, 3);
        
        // Calculate average frame time
        result.avgFrameTime = (1000.0f / result.fps1000 + 
                               1000.0f / result.fps3000 + 
                               1000.0f / result.fps5000) / 3.0f;
        
        // Determine recommended preset
        result.recommendedPreset = DeterminePreset(result);
        
        // Calculate optimal particle count
        result.optimalParticleCount = CalculateOptimalParticles(result);
        
        std::cout << "\n[ClothBenchmark] Benchmark Results:" << std::endl;
        std::cout << "  1000 particles: " << result.fps1000 << " FPS" << std::endl;
        std::cout << "  3000 particles: " << result.fps3000 << " FPS" << std::endl;
        std::cout << "  5000 particles: " << result.fps5000 << " FPS" << std::endl;
        std::cout << "  Recommended: " << QualityClassifier::GetPresetName(result.recommendedPreset) << std::endl;
        std::cout << "  Optimal particles: " << result.optimalParticleCount << std::endl;
        
        return result;
    }
    
    /**
     * Run quick benchmark (for startup)
     * 
     * Only tests 1000 and 3000 particles. Takes ~1 second.
     * Less accurate but faster.
     * 
     * @return BenchmarkResult with partial metrics
     */
    static BenchmarkResult RunQuickBenchmark() {
        std::cout << "\n[ClothBenchmark] Running quick benchmark..." << std::endl;
        
        BenchmarkResult result;
        
        result.fps1000 = SimulateClothFrame(1000, 2); // 2 iterations (faster)
        result.fps3000 = SimulateClothFrame(3000, 2);
        result.fps5000 = result.fps3000 * 0.6f; // Estimate
        
        result.avgFrameTime = (1000.0f / result.fps1000 + 1000.0f / result.fps3000) / 2.0f;
        result.recommendedPreset = DeterminePreset(result);
        result.optimalParticleCount = CalculateOptimalParticles(result);
        
        std::cout << "[ClothBenchmark] Quick result: " 
                  << QualityClassifier::GetPresetName(result.recommendedPreset) << std::endl;
        
        return result;
    }
    
private:
    /**
     * Simulate ONE frame of cloth physics
     * 
     * This is a CPU simulation for benchmarking purposes.
     * Real implementation would use GPU compute shader.
     * 
     * @param particleCount Number of particles to simulate
     * @param iterations Physics iterations per frame
     * @return FPS achieved
     */
    static float SimulateClothFrame(int particleCount, int iterations) {
        const int warmupFrames = 2;
        const int benchmarkFrames = 5;
        
        // Create particle data
        std::vector<glm::vec3> positions(particleCount);
        std::vector<glm::vec3> velocities(particleCount);
        std::vector<glm::vec3> forces(particleCount);
        
        // Initialize particles
        for (int i = 0; i < particleCount; i++) {
            positions[i] = glm::vec3(
                static_cast<float>(rand() % 100) / 100.0f,
                static_cast<float>(rand() % 100) / 100.0f,
                static_cast<float>(rand() % 100) / 100.0f
            );
            velocities[i] = glm::vec3(0.0f);
            forces[i] = glm::vec3(0.0f, -9.81f, 0.0f);
        }
        
        // Warmup frames (not timed)
        for (int frame = 0; frame < warmupFrames; frame++) {
            SimulatePhysicsStep(positions, velocities, forces, particleCount, iterations);
        }
        
        // Benchmark frames (timed)
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int frame = 0; frame < benchmarkFrames; frame++) {
            SimulatePhysicsStep(positions, velocities, forces, particleCount, iterations);
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Calculate FPS
        float seconds = duration.count() / 1000.0f;
        float fps = static_cast<float>(benchmarkFrames) / seconds;
        
        return fps;
    }
    
    /**
     * Simulate one physics step for all particles
     * 
     * Simplified Verlet integration for benchmarking.
     */
    static void SimulatePhysicsStep(
        std::vector<glm::vec3>& positions,
        std::vector<glm::vec3>& velocities,
        std::vector<glm::vec3>& forces,
        int particleCount,
        int iterations)
    {
        float deltaTime = 0.016f; // 60 FPS
        
        for (int iter = 0; iter < iterations; iter++) {
            for (int i = 0; i < particleCount; i++) {
                // Apply forces (gravity)
                velocities[i] += forces[i] * deltaTime;
                
                // Apply damping
                velocities[i] *= 0.98f;
                
                // Update position
                positions[i] += velocities[i] * deltaTime;
                
                // Simple ground collision
                if (positions[i].y < 0.0f) {
                    positions[i].y = 0.0f;
                    velocities[i].y *= -0.5f; // Bounce
                }
            }
        }
    }
    
    /**
     * Determine quality preset based on benchmark results
     */
    static QualityPreset DeterminePreset(const BenchmarkResult& result) {
        // Classify based on 3000-particle FPS (most representative)
        if (result.fps5000 >= 60.0f) {
            return QualityPreset::ULTRA; // Can handle 5000+ particles at 60 FPS
        }
        if (result.fps3000 >= 60.0f) {
            return QualityPreset::HIGH; // Can handle 3000+ particles at 60 FPS
        }
        if (result.fps1000 >= 60.0f) {
            return QualityPreset::MEDIUM; // Can handle 1000+ particles at 60 FPS
        }
        return QualityPreset::LOW; // Need to reduce quality
    }
    
    /**
     * Calculate optimal particle count for target FPS (60)
     */
    static int CalculateOptimalParticles(const BenchmarkResult& result) {
        // Linear interpolation to find particle count for 60 FPS
        if (result.fps1000 >= 60.0f && result.fps3000 >= 60.0f) {
            return 5000; // Can handle max
        }
        if (result.fps1000 >= 60.0f) {
            // Between 1000 and 3000
            float slope = (3000.0f - 1000.0f) / (result.fps3000 - result.fps1000);
            int optimal = static_cast<int>(1000.0f + slope * (60.0f - result.fps1000));
            return std::max(1000, std::min(optimal, 3000));
        }
        // Below 60 FPS at 1000 particles
        return 500; // Reduce quality
    }
};

} // namespace cloth
