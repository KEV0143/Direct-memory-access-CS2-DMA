namespace {
DWORD_PTR BuildWorkerAffinityMask(unsigned backFromLastCore)
{
    constexpr unsigned kMaskBits = static_cast<unsigned>(sizeof(DWORD_PTR) * 8u);
    unsigned cpuCount = std::thread::hardware_concurrency();
    if (cpuCount == 0)
        cpuCount = 1;
    cpuCount = std::min(cpuCount, kMaskBits);

    const unsigned clampedBack = std::min(backFromLastCore, cpuCount - 1u);
    const unsigned coreIndex = cpuCount - 1u - clampedBack;
    return static_cast<DWORD_PTR>(1ull) << coreIndex;
}

void ApplyWorkerThreadTuning(unsigned backFromLastCore)
{
    const DWORD_PTR affinityMask = BuildWorkerAffinityMask(backFromLastCore);
    if (affinityMask != 0)
        SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}
}

void esp::StartDataWorker()
{
    if (s_dataWorkerRunning.exchange(true, std::memory_order_relaxed))
        return;

    s_dataWorkerStopRequested.store(false, std::memory_order_relaxed);
    const uint64_t nowUs = TickNowUs();
    s_sessionStartUs.store(nowUs, std::memory_order_relaxed);
    s_dataWorkerCycleUs.store(0, std::memory_order_relaxed);
    s_dataWorkerMaxCycleUs.store(0, std::memory_order_relaxed);
    s_dataWorkerLastLoopStartUs.store(0, std::memory_order_relaxed);
    s_dataWorkerLastLoopEndUs.store(0, std::memory_order_relaxed);
    s_dataWorkerInFlightSinceUs.store(0, std::memory_order_relaxed);
    s_dataWorkerUpdateInFlight.store(false, std::memory_order_relaxed);
    ResetCameraSnapshot();
    s_dataWorker = std::thread([]() {
        ApplyWorkerThreadTuning(0);
        DataWorkerLoop();
    });
    s_cameraWorker = std::thread([]() {
        const unsigned cpuCount = std::thread::hardware_concurrency();
        ApplyWorkerThreadTuning(cpuCount >= 4 ? 1u : 0u);
        CameraWorkerLoop();
    });
}

void esp::StopDataWorker()
{
    s_dataWorkerStopRequested.store(true, std::memory_order_relaxed);
    if (s_cameraWorker.joinable())
        s_cameraWorker.join();
    if (s_dataWorker.joinable())
        s_dataWorker.join();
    s_cameraWorkerRunning.store(false, std::memory_order_relaxed);
    s_dataWorkerRunning.store(false, std::memory_order_relaxed);
    s_dataWorkerUpdateInFlight.store(false, std::memory_order_relaxed);
    s_dataWorkerInFlightSinceUs.store(0, std::memory_order_relaxed);
}
