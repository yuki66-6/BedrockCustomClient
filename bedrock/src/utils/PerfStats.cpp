#include "utils/PerfStats.h"

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <thread>
#include <vector>

namespace bcc {

void PerfStats::beginFrame(double now) {
    frameStart_ = now;
    if (fpsWindowStart_ < 0) fpsWindowStart_ = now;
    ++fpsWindowFrames_;
    ++frames_;
    if (now - fpsWindowStart_ >= 0.5) {
        fps_ = static_cast<double>(fpsWindowFrames_) / (now - fpsWindowStart_);
        fpsWindowStart_ = now;
        fpsWindowFrames_ = 0;
    }
}

void PerfStats::endFrameWork(double now) {
    double ms = (now - frameStart_) * 1000.0;
    frameMs_[pos_] = ms;
    pos_ = (pos_ + 1) % kSamples;
    count_ = std::min(count_ + 1, kSamples);
    history_[historyPos_] = static_cast<float>(ms);
    historyPos_ = (historyPos_ + 1) % history_.size();
}

double PerfStats::avgFrameMs() const {
    if (!count_) return 0;
    double s = 0;
    for (size_t i = 0; i < count_; ++i) s += frameMs_[i];
    return s / static_cast<double>(count_);
}

double PerfStats::maxFrameMs() const {
    double m = 0;
    for (size_t i = 0; i < count_; ++i) m = std::max(m, frameMs_[i]);
    return m;
}

double PerfStats::p99FrameMs() const {
    if (!count_) return 0;
    std::vector<double> v(frameMs_.begin(), frameMs_.begin() + static_cast<std::ptrdiff_t>(count_));
    size_t idx = std::min(count_ - 1, static_cast<size_t>(static_cast<double>(count_) * 0.99));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(idx), v.end());
    return v[idx];
}

void PerfStats::sampleProcess(double now) {
    if (now - lastSample_ < 1.0) return;
    if (!cores_) cores_ = std::max(1u, std::thread::hardware_concurrency());

    FILETIME creation, exit, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        auto toU64 = [](FILETIME f) { return (static_cast<uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
        uint64_t cpu = toU64(kernel) + toU64(user);
        if (lastCpu100ns_ && now > lastCpuWall_) {
            double cpuSec = static_cast<double>(cpu - lastCpu100ns_) / 1e7;
            cpuPercent_ = 100.0 * cpuSec / (now - lastCpuWall_);
        }
        lastCpu100ns_ = cpu;
        lastCpuWall_ = now;
    }

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        workingSetMb_ = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        peakWorkingSetMb_ = static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
        privateMb_ = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }
    lastSample_ = now;
}

} // namespace bcc
