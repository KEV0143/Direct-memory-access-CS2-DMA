#pragma once

#include <cstdint>

namespace overlay {
    struct PerfStats {
        uint64_t frameUs = 0;
        uint64_t maxFrameUs = 0;
        uint64_t syncUs = 0;
        uint64_t drawUs = 0;
        uint64_t presentUs = 0;
        uint64_t pacingWaitUs = 0;
    };

    bool Create(int width, int height);
    void Run();
    PerfStats GetPerfStats();
    void Destroy();
}
