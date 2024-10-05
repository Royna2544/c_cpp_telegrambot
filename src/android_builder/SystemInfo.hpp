#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <ostream>

enum class SizeTypes { Bytes, KiloBytes, MegaBytes, GigaBytes, TeraBytes };

struct ConvertedBytes {
    double value;
    SizeTypes type;

    operator double() const { return value; }
};

struct Bytes {
    static constexpr std::array<std::string_view, 6> units = {"B", "KB", "MB",
                                                              "GB", "TB"};
    uint64_t value{};
    constexpr static SizeTypes type = SizeTypes::Bytes;

    static constexpr long double factor = 1024.0F;

    operator uint64_t() const { return value; }
    Bytes(uint64_t value) : value(value) {}
    Bytes() = default;

    template <SizeTypes type>
    [[nodiscard]] ConvertedBytes to() const {
        long double v = value;
        v /= std::pow(Bytes::factor, static_cast<int>(type));
        assert(v < std::numeric_limits<double>::max());
        return {static_cast<double>(v), type};
    }
};

struct CPUInfo {
    CPUInfo();

    long coreCount;
    std::string cpuVendor;
    std::string cpuModel;
    std::string cpuMHz;
};

struct MemoryInfo {
    MemoryInfo();

    Bytes totalMemory;
    Bytes freeMemory;
    Bytes usedMemory;

    struct Percent {
        static constexpr int MAX = 100;
        static constexpr int MIN = 0;

        int value;
    };
    [[nodiscard]] Percent usage() const {
        long double ret = static_cast<long double>(usedMemory.value) / totalMemory.value * Percent::MAX;
        assert(ret >= Percent::MIN && ret <= Percent::MAX);
        return {static_cast<int>(ret)};
    }
};

struct DiskInfo {
    explicit DiskInfo(std::filesystem::path path = "/");

    std::filesystem::path path_;

    Bytes totalSpace;
    Bytes availableSpace;
};

struct SystemSummary {
    DiskInfo diskInfo;
    MemoryInfo memoryInfo;
    CPUInfo cpuInfo;
};

extern std::ostream& operator<<(std::ostream& os, CPUInfo const& info);
extern std::ostream& operator<<(std::ostream& os, Bytes const& bytes);
extern std::ostream& operator<<(std::ostream& os, MemoryInfo const& info);
extern std::ostream& operator<<(std::ostream& os, DiskInfo const& info);
extern std::ostream& operator<<(std::ostream& os, SystemSummary const& summary);
extern std::ostream& operator<<(std::ostream& os, ConvertedBytes const& bytes);
extern std::ostream& operator<<(std::ostream& os,
                                MemoryInfo::Percent const& percent);