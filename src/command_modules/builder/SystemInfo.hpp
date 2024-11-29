#pragma once

#include <fmt/core.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <type_traits>

enum class SizeTypes { Bytes, KiloBytes, MegaBytes, GigaBytes, TeraBytes };

template <typename T>
concept isNumber = std::is_floating_point_v<T> || std::is_integral_v<T>;

template <typename NumTo, typename NumFrom>
    requires(isNumber<NumFrom> && isNumber<NumTo>)
NumTo assert_downcast(const NumFrom num) {
    if constexpr (sizeof(NumFrom) >= sizeof(NumTo)) {
        if (std::numeric_limits<NumTo>::max() < num) {
            throw std::overflow_error(
                "Downcasting to larger type resulted in overflow");
        }
    }
    return static_cast<NumTo>(num);
}

struct ConvertedBytes {
    using size_type_floating = double;

    size_type_floating value;
    SizeTypes type;

    operator size_type_floating() const { return value; }
};

struct Bytes {
    static constexpr std::array<std::string_view,
                                static_cast<int>(SizeTypes::TeraBytes) + 1>
        units = {"B", "KB", "MB", "GB", "TB"};
    using size_type = uint64_t;
    using size_type_floating = long double;
    static constexpr size_type_floating factor_floating = 1024.0F;
    static constexpr size_type factor = 1024;

    size_type value{};
    constexpr static SizeTypes type = SizeTypes::Bytes;

    explicit operator size_type() const { return value; }
    Bytes(size_type value) : value(value) {}
    Bytes() = default;

    template <SizeTypes type>
    [[nodiscard]] ConvertedBytes to() const {
        size_type_floating v = value;
        v /= std::pow(Bytes::factor_floating, static_cast<int>(type));
        return {assert_downcast<double>(v), type};
    }
};

template <>
struct fmt::formatter<Bytes> {
    // Parses the format specification.
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    // Formats the Bytes object.
    template <typename FormatContext>
    auto format(const Bytes& bytes, FormatContext& ctx) {
        Bytes::size_type_floating num = bytes.value;
        int unitIndex = 0;

        while (num >= Bytes::factor_floating &&
               unitIndex < Bytes::units.size() - 1) {
            unitIndex++;
            num /= Bytes::factor_floating;
        }

        // Format the result with the determined unit.
        return fmt::format_to(ctx.out(), "{:.2f} {}", num, Bytes::units[unitIndex]);
    }
};

struct Percent {
    static constexpr int MAX = 100;
    static constexpr int MIN = 0;

    double value;
};

struct CPUInfo {
    CPUInfo();

    long coreCount;
    std::string cpuVendor;
    std::string cpuModel;
    std::string cpuMHz;
    Percent usage;
};

struct MemoryInfo {
    MemoryInfo();

    Bytes totalMemory;
    Bytes freeMemory;
    Bytes usedMemory;
    Percent usage;
};

struct DiskInfo {
    explicit DiskInfo(std::filesystem::path path = "/");

    std::filesystem::path path_;

    Bytes totalSpace;
    Bytes availableSpace;
    Percent usage{};
};

struct SystemSummary {
    SystemSummary();

    DiskInfo diskInfo;
    DiskInfo cwdDiskInfo;
    MemoryInfo memoryInfo;
    CPUInfo cpuInfo;

    bool cwdDiskInfoValid = false;
};

extern std::ostream& operator<<(std::ostream& os, CPUInfo const& info);
extern std::ostream& operator<<(std::ostream& os, Bytes const& bytes);
extern std::ostream& operator<<(std::ostream& os, MemoryInfo const& info);
extern std::ostream& operator<<(std::ostream& os, DiskInfo const& info);
extern std::ostream& operator<<(std::ostream& os, SystemSummary const& summary);
extern std::ostream& operator<<(std::ostream& os, ConvertedBytes const& bytes);
extern std::ostream& operator<<(std::ostream& os, Percent const& percent);