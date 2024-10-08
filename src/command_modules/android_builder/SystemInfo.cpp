#include "SystemInfo.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <TryParseStr.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

CPUInfo::CPUInfo() : coreCount(sysconf(_SC_NPROCESSORS_ONLN)) {
    std::ifstream cpuInfoFile("/proc/cpuinfo");
    std::string line;

    if (!cpuInfoFile.is_open()) {
        PLOG(ERROR) << "Could not open /proc/cpuinfo" << std::endl;
        return;
    }

    while (std::getline(cpuInfoFile, line)) {
        if (line.find("vendor_id") != std::string::npos) {
            cpuVendor = line.substr(line.find(':') + 2);
        }
        if (line.find("model name") != std::string::npos) {
            cpuModel = line.substr(line.find(':') + 2);
        }
        if (line.find("cpu MHz") != std::string::npos) {
            cpuMHz = line.substr(line.find(':') + 2);
        }
    }
}

MemoryInfo::MemoryInfo() {
    // Use /proc/meminfo instead
    std::unordered_map<std::string, Bytes> kMemoryMap;

    std::ifstream memInfoFile("/proc/meminfo");
    std::string line;
    if (!memInfoFile.is_open()) {
        PLOG(ERROR) << "Could not open /proc/meminfo" << std::endl;
        return;
    }
    while (std::getline(memInfoFile, line)) {
        std::vector<std::string> pair =
            absl::StrSplit(line, ' ', absl::SkipWhitespace());

        if (pair.size() == 3 && absl::AsciiStrToLower(pair[2]) == "kb") {
            std::string name(absl::StripSuffix(pair[0], ":"));
            Bytes::size_type value{};
            if (try_parse(pair[1], &value)) {
                kMemoryMap[name] = Bytes(value * Bytes::factor);
            } else {
                LOG(WARNING) << "Failed to parse memory value: " << pair[1];
            }
        }
    }
    totalMemory = kMemoryMap["MemTotal"];
    freeMemory = kMemoryMap["MemFree"];
    usedMemory =
        totalMemory - freeMemory - kMemoryMap["Buffers"] - kMemoryMap["Cached"];
}

// Get disk info
DiskInfo::DiskInfo(std::filesystem::path path) : path_(std::move(path)) {
    struct statvfs stat {};

    if (statvfs(path_.c_str(), &stat) != 0) {
        PLOG(ERROR) << "Could not retrieve disk information";
    }

    // 0 * 0, so 0.
    availableSpace = stat.f_bsize * stat.f_bavail;
    totalSpace = stat.f_bsize * stat.f_blocks;
}

SystemSummary::SystemSummary() {
    std::error_code ec;
    auto path = std::filesystem::current_path(ec);
    if (ec) {
        LOG(ERROR) << "Failed to get current cwd: " << ec.message();
        return;
    }
    cwdDiskInfo = DiskInfo(path);
    cwdDiskInfoValid = true;
}

std::ostream& operator<<(std::ostream& os, CPUInfo const& info) {
    os << "CPU cores: " << info.coreCount << "\n";
    os << "CPU model: " << info.cpuModel << "\n";
    os << "CPU frequency: " << info.cpuMHz << "Mhz\n";
    os << "CPU vendor: " << info.cpuVendor << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, Bytes const& bytes) {
    Bytes::size_type_floating num = bytes.value;
    int unitIndex = 0;

    while (num >= Bytes::factor_floating &&
           unitIndex < Bytes::units.size() - 1) {
        unitIndex++;
        num /= Bytes::factor_floating;
    }

    os << ConvertedBytes{
        assert_downcast<ConvertedBytes::size_type_floating>(num),
        static_cast<SizeTypes>(unitIndex)};
    return os;
}

std::ostream& operator<<(std::ostream& os, MemoryInfo const& info) {
    os << "Memory Info:\n";
    os << "Total Memory: " << info.totalMemory << "\n";
    os << "Free Memory: " << info.freeMemory << "\n";
    os << "Used Memory: " << info.usedMemory << "\n";
    os << "Usage: " << info.usage() << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, DiskInfo const& info) {
    os << "Disk Free (" << info.path_.string() << "): " << info.availableSpace
       << " / " << info.totalSpace << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, SystemSummary const& summary) {
    os << "System Summary:\n";
    os << summary.cpuInfo << "\n";
    os << summary.memoryInfo << "\n";
    os << summary.diskInfo;
    if (summary.cwdDiskInfoValid) {
        os << summary.cwdDiskInfo;
    }
    os << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, ConvertedBytes const& bytes) {
    const auto value = bytes.value;
    auto type = (int)bytes.type;

    CHECK_LT(type, Bytes::units.size()) << "Overflow";

    os << std::fixed << std::setprecision(2) << value << " "
       << Bytes::units[type];
    return os;
}

std::ostream& operator<<(std::ostream& os, MemoryInfo::Percent const& percent) {
    if (percent.value >= MemoryInfo::Percent::MIN &&
        percent.value <= MemoryInfo::Percent::MAX) {
        os << percent.value << "%";
    } else {
        os << "Invalid percent value(" << percent.value << ")";
    }

    return os;
}