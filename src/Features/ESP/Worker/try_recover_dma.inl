static bool TryRecoverDma()
{
    bool expected = false;
    if (!s_dmaRecovering.compare_exchange_strong(expected, true))
        return false;

    const DmaLogLevel previousLogLevel = DmaGetLogLevel();
    DmaSetLogLevel(DmaLogLevel::Silent);
    mem.SetDirectReadWarningSuppressed(true);

    
    
    
    
    
    if (mem.vHandle) {
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_TLB_PARTIAL, 1);
        VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_FREQ_FAST, 1);
    }

    const uintptr_t previousClientBase = g::clientBase;
    const uintptr_t previousEngine2Base = g::engine2Base;
    auto resolveReadableModules = [&]() -> std::pair<uintptr_t, uintptr_t> {
        const uintptr_t clientBase = mem.GetBaseDaddy("client.dll");
        const uintptr_t engine2Base = mem.GetBaseDaddy("engine2.dll");
        if (!clientBase || !engine2Base)
            return { 0, 0 };

        uint16_t mz = 0;
        if (!mem.Read(clientBase, &mz, sizeof(mz)) || mz != 0x5A4D)
            return { 0, 0 };

        return { clientBase, engine2Base };
    };

    auto [clientBase, engine2Base] = resolveReadableModules();
    bool attached = (clientBase != 0 && engine2Base != 0);
    if (!attached) {
        mem.ResetProcessState();
        attached = mem.vHandle && mem.AttachToProcess("cs2.exe", true);
        if (attached && mem.vHandle)
            VMMDLL_ConfigSet(mem.vHandle, VMMDLL_OPT_REFRESH_ALL, 1);
        if (attached)
            std::tie(clientBase, engine2Base) = resolveReadableModules();
    }

    mem.SetDirectReadWarningSuppressed(false);
    DmaSetLogLevel(previousLogLevel);

    const bool modulesResolved = attached && clientBase != 0 && engine2Base != 0;
    if (modulesResolved) {
        g::clientBase = clientBase;
        g::engine2Base = engine2Base;
        s_dmaConsecutiveFailures.store(0, std::memory_order_relaxed);
        s_dmaTotalRecoveries.fetch_add(1, std::memory_order_relaxed);
        RecordDmaEvent("recovery_ok", "modules_resolved");
    } else {
        const uint32_t failures = s_dmaConsecutiveFailures.fetch_add(1, std::memory_order_relaxed) + 1;
        RecordDmaEvent("recovery_fail", "modules_unresolved");
        const DWORD liveCs2Pid = mem.GetPidFromName("cs2.exe");
        if (liveCs2Pid != 0 && previousClientBase != 0 && previousEngine2Base != 0) {
            g::clientBase = previousClientBase;
            g::engine2Base = previousEngine2Base;
        } else {
            g::clientBase = 0;
            g::engine2Base = 0;
        }

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
