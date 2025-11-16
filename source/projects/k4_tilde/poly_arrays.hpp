#ifndef KARMA_POLY_ARRAYS_HPP
#define KARMA_POLY_ARRAYS_HPP

#include "ext.h"  // For sysmem_newptrclear, sysmem_freeptr

// =============================================================================
// KARMA POLY ARRAYS - RAII Memory Management for Multichannel Processing
// =============================================================================
// Manages dynamically allocated arrays for channels beyond the fixed struct
// fields (channels 5+). Uses RAII to ensure proper cleanup on exceptions
// or early returns.

namespace karma {

/**
 * @brief RAII wrapper for multichannel processing arrays
 *
 * Manages four parallel arrays needed for multichannel audio processing:
 * - osamp: Output samples
 * - oprev: Previous output samples (for interpolation)
 * - odif: Output differences (for smooth transitions)
 * - recin: Record input samples
 *
 * Uses Max SDK's sysmem allocator for compatibility.
 */
class PolyArrays {
public:
    /**
     * @brief Construct and allocate arrays for specified channel count
     *
     * @param max_channels Maximum number of channels to allocate for
     * @throws Allocation failure is indicated by is_valid() returning false
     */
    explicit PolyArrays(long max_channels)
        : max_channels_(max_channels)
        , osamp_(nullptr)
        , oprev_(nullptr)
        , odif_(nullptr)
        , recin_(nullptr)
    {
        if (max_channels <= 0) {
            return;  // Invalid, all pointers remain nullptr
        }

        // Allocate arrays one at a time, checking for failure
        osamp_ = static_cast<double*>(
            sysmem_newptrclear(max_channels * sizeof(double)));
        if (!osamp_) {
            cleanup();
            return;
        }

        oprev_ = static_cast<double*>(
            sysmem_newptrclear(max_channels * sizeof(double)));
        if (!oprev_) {
            cleanup();
            return;
        }

        odif_ = static_cast<double*>(
            sysmem_newptrclear(max_channels * sizeof(double)));
        if (!odif_) {
            cleanup();
            return;
        }

        recin_ = static_cast<double*>(
            sysmem_newptrclear(max_channels * sizeof(double)));
        if (!recin_) {
            cleanup();
            return;
        }
    }

    // Disable copy (these arrays should not be copied)
    PolyArrays(const PolyArrays&) = delete;
    PolyArrays& operator=(const PolyArrays&) = delete;

    // Enable move semantics
    PolyArrays(PolyArrays&& other) noexcept
        : max_channels_(other.max_channels_)
        , osamp_(other.osamp_)
        , oprev_(other.oprev_)
        , odif_(other.odif_)
        , recin_(other.recin_)
    {
        other.max_channels_ = 0;
        other.osamp_ = nullptr;
        other.oprev_ = nullptr;
        other.odif_ = nullptr;
        other.recin_ = nullptr;
    }

    PolyArrays& operator=(PolyArrays&& other) noexcept {
        if (this != &other) {
            cleanup();
            max_channels_ = other.max_channels_;
            osamp_ = other.osamp_;
            oprev_ = other.oprev_;
            odif_ = other.odif_;
            recin_ = other.recin_;

            other.max_channels_ = 0;
            other.osamp_ = nullptr;
            other.oprev_ = nullptr;
            other.odif_ = nullptr;
            other.recin_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Destructor - automatically frees all arrays
     */
    ~PolyArrays() {
        cleanup();
    }

    /**
     * @brief Check if all arrays were successfully allocated
     */
    bool is_valid() const noexcept {
        return osamp_ && oprev_ && odif_ && recin_;
    }

    // Accessors (return raw pointers for compatibility with C API)
    double* osamp() noexcept { return osamp_; }
    double* oprev() noexcept { return oprev_; }
    double* odif() noexcept { return odif_; }
    double* recin() noexcept { return recin_; }

    const double* osamp() const noexcept { return osamp_; }
    const double* oprev() const noexcept { return oprev_; }
    const double* odif() const noexcept { return odif_; }
    const double* recin() const noexcept { return recin_; }

    long max_channels() const noexcept { return max_channels_; }

private:
    void cleanup() noexcept {
        if (recin_) {
            sysmem_freeptr(recin_);
            recin_ = nullptr;
        }
        if (odif_) {
            sysmem_freeptr(odif_);
            odif_ = nullptr;
        }
        if (oprev_) {
            sysmem_freeptr(oprev_);
            oprev_ = nullptr;
        }
        if (osamp_) {
            sysmem_freeptr(osamp_);
            osamp_ = nullptr;
        }
        max_channels_ = 0;
    }

    long max_channels_;
    double* osamp_;
    double* oprev_;
    double* odif_;
    double* recin_;
};

} // namespace karma

#endif // KARMA_POLY_ARRAYS_HPP
