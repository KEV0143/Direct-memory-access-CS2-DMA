namespace {
    std::mt19937_64& WorkerJitterRng()
    {
        thread_local std::mt19937_64 rng([] {
            const auto nowSeed = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            const auto tidSeed = static_cast<uint64_t>(std::hash<std::thread::id> {}(std::this_thread::get_id()));
            std::seed_seq seq {
                static_cast<uint32_t>(nowSeed),
                static_cast<uint32_t>(nowSeed >> 32),
                static_cast<uint32_t>(tidSeed),
                static_cast<uint32_t>(tidSeed >> 32)
            };
            return std::mt19937_64(seq);
        }());
        return rng;
    }

    template <typename Duration>
    Duration JitterDuration(Duration base, int jitterPercent)
    {
        const auto baseCount = static_cast<int64_t>(base.count());
        if (baseCount <= 1 || jitterPercent <= 0)
            return base;

        const auto maxDelta = std::max<int64_t>(1, (baseCount * jitterPercent) / 100);
        std::uniform_int_distribution<int64_t> distribution(-maxDelta, maxDelta);
        const auto jitteredCount = std::max<int64_t>(1, baseCount + distribution(WorkerJitterRng()));
        return Duration(static_cast<typename Duration::rep>(jitteredCount));
    }

    
    
    
    
    
    constexpr int kDataWorkerJitterPercent = 8;
    constexpr int kCameraWorkerJitterPercent = 1;
}
