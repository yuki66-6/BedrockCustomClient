#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bcc {

// Mesures de performance du processus de l'overlay (et non du jeu) :
// temps CPU par image, FPS de l'overlay, CPU du processus, mémoire.
class PerfStats {
public:
    void beginFrame(double now);
    void endFrameWork(double now); // fin du travail CPU (avant l'attente vsync)
    void sampleProcess(double now); // CPU/mémoire, au plus une fois par seconde

    double fps() const { return fps_; }
    double avgFrameMs() const;
    double maxFrameMs() const;
    double p99FrameMs() const;
    double cpuPercent() const { return cpuPercent_; } // % d'un cœur logique
    double cpuPercentAllCores() const { return cpuPercent_ / (cores_ ? cores_ : 1); }
    double workingSetMb() const { return workingSetMb_; }
    double privateMb() const { return privateMb_; }
    double peakWorkingSetMb() const { return peakWorkingSetMb_; }
    uint64_t frames() const { return frames_; }
    const std::array<float, 120>& history() const { return history_; }
    size_t historyOffset() const { return historyPos_; }

private:
    static constexpr size_t kSamples = 240;
    std::array<double, kSamples> frameMs_{};
    size_t count_ = 0, pos_ = 0;
    std::array<float, 120> history_{};
    size_t historyPos_ = 0;
    double frameStart_ = 0;
    double fpsWindowStart_ = -1;
    uint64_t fpsWindowFrames_ = 0;
    double fps_ = 0;
    uint64_t frames_ = 0;

    double lastSample_ = -10;
    uint64_t lastCpu100ns_ = 0;
    double lastCpuWall_ = 0;
    double cpuPercent_ = 0;
    double workingSetMb_ = 0, privateMb_ = 0, peakWorkingSetMb_ = 0;
    unsigned cores_ = 0;
};

} // namespace bcc
