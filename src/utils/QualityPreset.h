#pragma once

namespace cloth {

/**
 * Quality preset enumeration
 * Used for GPU-based quality classification
 */
enum class QualityPreset {
    LOW,        // Integrated GPU (Intel UHD, Iris Xe)
    MEDIUM,     // Entry-level dedicated (GTX 1650, RX 6400)
    HIGH,       // Mid-range (RTX 3060, RX 6700)
    ULTRA,      // High-end (RTX 4080+, RX 7900+)
    CUSTOM      // User-defined
};

} // namespace cloth
