#include "SystemInfo.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

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

// Get memory info using sysinfo
MemoryInfo::MemoryInfo() {
    struct sysinfo info {};
    if (sysinfo(&info) == 0) {
        totalMemory = info.totalram * info.mem_unit;
        freeMemory = info.freeram * info.mem_unit;
        usedMemory = totalMemory - freeMemory;
    } else {
        LOG(INFO) << "Failed to get memory info using sysinfo" << std::endl;
    }
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

std::ostream& operator<<(std::ostream& os, CPUInfo const& info) {
    os << "CPU cores: " << info.coreCount << "\n";
    os << "CPU model: " << info.cpuModel << "\n";
    os << "CPU frequency: " << info.cpuMHz << "Mhz\n";
    os << "CPU vendor: " << info.cpuVendor << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, Bytes const& bytes) {
    auto num = bytes.value;
    int unitIndex = 0;

    while (num >= Bytes::factor && unitIndex < Bytes::units.size() - 1) {
        unitIndex++;
        num /= Bytes::factor;
    }

    assert(num < std::numeric_limits<double>::max());
    os << ConvertedBytes{static_cast<double>(num),
                         static_cast<SizeTypes>(unitIndex)};
    return os;
}

std::ostream& operator<<(std::ostream& os, MemoryInfo const& info) {
    os << "Memory Info:\n";
    os << "Total Memory: " << info.totalMemory << "\n";
    os << "Free Memory: " << info.freeMemory << "\n";
    os << "Used Memory: " << info.usedMemory << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, DiskInfo const& info) {
    os << "Disk Free (" << info.path_.string() << "): " << info.availableSpace
       << " / " << info.totalSpace << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, SystemSummary const& summary) {
    os << "System Summary:\n";
    os << summary.diskInfo << "\n";
    os << summary.memoryInfo << "\n";
    os << summary.cpuInfo << "\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, ConvertedBytes const& bytes) {
    long double value = bytes.value;
    auto type = (int)bytes.type;

    CHECK_LE(type, Bytes::units.size()) << "Overflow";

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