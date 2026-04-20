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
        
        
        
        
        
        
        const DWORD_PTR coreMask = 1ull << (std::thread::hardware_concurrency() - 1);
        SetThreadAffinityMask(GetCurrentThread(), coreMask);
        
        
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        DataWorkerLoop();
    });
    s_cameraWorker = std::thread([]() {
        
        
        
        const DWORD nCpus = static_cast<DWORD>(std::thread::hardware_concurrency());
        const DWORD_PTR camCore = nCpus >= 4 ? (1ull << (nCpus - 2)) : (1ull << (nCpus - 1));
        SetThreadAffinityMask(GetCurrentThread(), camCore);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
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
