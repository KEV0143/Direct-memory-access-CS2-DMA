static bool TryRecoverDma()
{
    bool expected = false;
    if (!s_dmaRecovering.compare_exchange_strong(expected, true))
        return false;

    const DmaLogLevel previousLogLevel = DmaGetLogLevel();
    DmaSetLogLevel(DmaLogLevel::Silent);

    
    
    
    
    
    if (mem.vHandle) {
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB_PARTIAL, 1);
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_FAST, 1);
    }

    
    
    
    
    
    
    mem.ResetProcessState();

    const bool attached = mem.vHandle && mem.AttachToProcess("cs2.exe", true);

    
    
    if (attached && mem.vHandle)
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_ALL, 1);

    const uintptr_t clientBase = attached ? mem.GetBaseDaddy("client.dll") : 0;
    const uintptr_t engine2Base = attached ? mem.GetBaseDaddy("engine2.dll") : 0;

    DmaSetLogLevel(previousLogLevel);

    bool memoryReadable = false;
    if (attached && clientBase) {
        uint16_t mz = 0;
        if (mem.Read(clientBase, &mz, sizeof(mz)) && mz == 0x5A4D) {
            memoryReadable = true;
        }
    }

    const bool modulesResolved = attached && clientBase != 0 && engine2Base != 0 && memoryReadable;
    if (modulesResolved) {
        g::clientBase = clientBase;
        g::engine2Base = engine2Base;
        s_dmaConsecutiveFailures.store(0, std::memory_order_relaxed);
        s_dmaTotalRecoveries.fetch_add(1, std::memory_order_relaxed);
    } else {
        const uint32_t failures = s_dmaConsecutiveFailures.fetch_add(1, std::memory_order_relaxed) + 1;
        g::clientBase = 0;
        g::engine2Base = 0;

        if (failures >= 5) {
            DmaSetLogLevel(DmaLogLevel::Info);
            DmaLogPrintf("[WARN] DMA stuck on stale state. Hard resetting VMM connection...");
            mem.CloseDma();
            mem.InitDma(true, false);
            s_dmaConsecutiveFailures.store(0, std::memory_order_relaxed);
        }
    }

    s_dmaRecovering.store(false, std::memory_order_relaxed);
    return modulesResolved;
}
