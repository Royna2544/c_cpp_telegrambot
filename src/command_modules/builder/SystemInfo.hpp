#pragma once

#include <fmt/core.h>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <ostream>

#include "BytesConversion.hpp"

struct Percent {
    static constexpr int MAX = 100;
    static constexpr int MIN = 0;

    double value;
};

struct CPUInfo {
    CPUInfo();

    static constexpr std::string_view UNKNOWN = "unknown";

    long coreCount;
    std::string cpuVendor{UNKNOWN};
    std::string cpuModel{UNKNOWN};
    std::string cpuMHz{UNKNOWN};
    Percent usage;
};

struct MemoryInfo {
    MemoryInfo();

    MegaBytes totalMemory;
    MegaBytes freeMemory;
    MegaBytes usedMemory;
    Percent usage;
};

struct DiskInfo {
    explicit DiskInfo(std::filesystem::path path = "/");

    std::filesystem::path path_;

    GigaBytes totalSpace;
    GigaBytes availableSpace;
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
extern std::ostream& operator<<(std::ostream& os, MemoryInfo const& info);
extern std::ostream& operator<<(std::ostream& os, DiskInfo const& info);
extern std::ostream& operator<<(std::ostream& os, SystemSummary const& summary);
extern std::ostream& operator<<(std::ostream& os, Percent const& percent);
