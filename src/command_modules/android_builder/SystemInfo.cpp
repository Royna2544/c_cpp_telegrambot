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
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>

struct CPUData {
    long user, nice, system, idle, iowait, irq, softirq, steal;

    CPUData();
    [[nodiscard]] long getTotalTime() const;
    [[nodiscard]] long getActiveTime() const;
    [[nodiscard]] static Percent getUsage();
};

CPUData::CPUData() {
    std::ifstream file("/proc/stat");
    std::string line;

    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string cpu;
        iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >>
            softirq >> steal;
    }
}

long CPUData::getTotalTime() const {
    return user + nice + system + idle + iowait + irq + softirq + steal;
}

long CPUData::getActiveTime() const {
    return user + nice + system + irq + softirq + steal;
}

Percent CPUData::getUsage() {
    CPUData prevData;
    std::this_thread::sleep_for(std::chrono::seconds(1));  // 1 second delay
    CPUData currData;

    long prevTotalTime = prevData.getTotalTime();
    long currTotalTime = currData.getTotalTime();

    long prevActiveTime = prevData.getActiveTime();
    long currActiveTime = currData.getActiveTime();

    const auto percent = double(currActiveTime - prevActiveTime) /
                         double(currTotalTime - prevTotalTime);

    return {(assert_downcast<double>(percent)) * 100.0};
}

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
    usage = CPUData::getUsage();
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

    usage = {assert_downcast<double>(
        static_cast<Bytes::size_type_floating>(usedMemory.value) *
        Percent::MAX / totalMemory.value)};
}

// Get disk info
DiskInfo::DiskInfo(std::filesystem::path path) : path_(std::move(path)) {
    struct statvfs stat {};

    if (statvfs(path_.c_str(), &stat) != 0) {
        PLOG(ERROR) << "Could not retrieve disk information";
        return;
    }

    availableSpace = stat.f_bsize * stat.f_bavail;
    totalSpace = stat.f_bsize * stat.f_blocks;
    usage = {assert_downcast<double>(
        static_cast<Bytes::size_type_floating>(totalSpace - availableSpace) *
        Percent::MAX / totalSpace)};
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
    os << "CPU Usage: " << info.usage << "\n";
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
    os << "Usage: " << info.usage << "\n";
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

std::ostream& operator<<(std::ostream& os, Percent const& percent) {
    if (percent.value >= Percent::MIN && percent.value <= Percent::MAX) {
        os << percent.value << "%";
    } else {
        os << "Invalid percent value(" << percent.value << ")";
    }

    return os;
}