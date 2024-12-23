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
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>

#include "BytesConversion.hpp"

template <typename T>
concept isNumber = std::is_floating_point_v<T> || std::is_integral_v<T>;

template <typename NumTo, typename NumFrom>
    requires(isNumber<NumFrom> && isNumber<NumTo>)
NumTo assert_downcast(const NumFrom num) {
    if constexpr (sizeof(NumFrom) >= sizeof(NumTo)) {
        if (std::numeric_limits<NumTo>::max() < num) {
            throw std::overflow_error("Overflow detected during downcasting.");
        }
    }
    return static_cast<NumTo>(num);
}

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
        PLOG(ERROR) << "Could not open /proc/cpuinfo";
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
    std::unordered_map<std::string, MegaBytes> kMemoryMap;

    std::ifstream memInfoFile("/proc/meminfo");
    std::string line;
    if (!memInfoFile.is_open()) {
        PLOG(ERROR) << "Could not open /proc/meminfo";
        return;
    }
    while (std::getline(memInfoFile, line)) {
        std::vector<std::string> pair =
            absl::StrSplit(line, ' ', absl::SkipWhitespace());

        if (pair.size() == 3 && absl::AsciiStrToLower(pair[2]) == "kb") {
            std::string name(absl::StripSuffix(pair[0], ":"));
            std::int64_t value{};
            if (try_parse(pair[1], &value)) {
                kMemoryMap.emplace(name, value * boost::units::data::kilobytes);
            } else {
                LOG(WARNING) << "Failed to parse memory value: " << pair[1];
            }
        }
    }
    totalMemory = kMemoryMap["MemTotal"];
    freeMemory = kMemoryMap["MemFree"];
    usedMemory =
        totalMemory - freeMemory - kMemoryMap["Buffers"] - kMemoryMap["Cached"];

    usage = {assert_downcast<double>(usedMemory.value() * Percent::MAX /
                                     totalMemory.value())};
}

// Get disk info
DiskInfo::DiskInfo(std::filesystem::path path) : path_(std::move(path)) {
    struct statvfs stat {};

    if (statvfs(path_.c_str(), &stat) != 0) {
        PLOG(ERROR) << "Could not retrieve disk information";
        return;
    }

    availableSpace =
        GigaBytes(stat.f_bsize * stat.f_bavail * boost::units::data::bytes);
    totalSpace =
        GigaBytes(stat.f_bsize * stat.f_blocks * boost::units::data::bytes);
    usage = {assert_downcast<double>((totalSpace - availableSpace).value() *
                                     Percent::MAX / totalSpace.value())};
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

std::ostream& operator<<(std::ostream& os, Percent const& percent) {
    if (percent.value >= Percent::MIN && percent.value <= Percent::MAX) {
        os << percent.value << "%";
    } else {
        os << "Invalid percent value(" << percent.value << ")";
    }

    return os;
}