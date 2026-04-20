    using Clock = std::chrono::steady_clock;
    auto lastFrameTime = Clock::now();

    MSG msg = {};
    while (g::running) {
        const auto frameStart = Clock::now();
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                g::running = false;
        }
        if (!g::running)
            break;

        if (GetAsyncKeyState(g::menuToggleKey) & 1)
            g::menuOpen = !g::menuOpen;
        if (GetAsyncKeyState(VK_END) & 1)
            g::running = false;

        if (g::menuOpen != s_overlayInteractive)
            ApplyOverlayWindowMode(g::menuOpen);
        const auto syncStart = Clock::now();
        SyncOverlayBounds();
        const auto syncEnd = Clock::now();
        if (!s_rtv) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        uint64_t pacingWaitUs = 0;
        if (g::vsyncEnabled && s_waitableSwapChainEnabled && s_frameLatencyWaitableObject) {
            const auto pacingStart = Clock::now();
            const DWORD waitResult = WaitForSingleObjectEx(s_frameLatencyWaitableObject, 1000, FALSE);
            if (waitResult == WAIT_OBJECT_0) {
                pacingWaitUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - pacingStart).count());
            } else if (waitResult == WAIT_FAILED || waitResult == WAIT_ABANDONED) {
                s_waitableSwapChainEnabled = false;
            }
        }

        const auto drawStart = Clock::now();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        webradar::ApplySettingsFromGlobals();
        webradar::CaptureFromEsp();

        esp::Draw();

        ui::RenderMenu();

        ImGui::Render();

        const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ID3D11RenderTargetView* rtvRaw = s_rtv.Get();
        s_context->OMSetRenderTargets(1, &rtvRaw, nullptr);
        s_context->ClearRenderTargetView(s_rtv.Get(), clearColor);

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        const auto drawEnd = Clock::now();

        const auto presentStart = Clock::now();
        if (g::vsyncEnabled) {
            s_swapChain->Present(1, 0);
        } else {
            const UINT flags = s_tearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0u;
            s_swapChain->Present(0, flags);
        }
        const auto presentEnd = Clock::now();

        if (!g::vsyncEnabled && g::fpsLimit > 0) {
            const auto targetDuration = std::chrono::microseconds(1000000 / g::fpsLimit);
            const auto targetTime = lastFrameTime + targetDuration;
            const auto now = Clock::now();
            if (now < targetTime) {
                const auto pacingStart = now;
                const auto sleepUntil = targetTime - std::chrono::microseconds(1200);
                if (now < sleepUntil)
                    std::this_thread::sleep_until(sleepUntil);
                while (Clock::now() < targetTime) {}
                pacingWaitUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - pacingStart).count());
            }
        }
        const auto frameEnd = Clock::now();
        const uint64_t frameUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart).count());
        const uint64_t syncUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(syncEnd - syncStart).count());
        const uint64_t drawUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(drawEnd - drawStart).count());
        const uint64_t presentUs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(presentEnd - presentStart).count());
        s_overlayFrameUs.store(frameUs, std::memory_order_relaxed);
        s_overlaySyncUs.store(syncUs, std::memory_order_relaxed);
        s_overlayDrawUs.store(drawUs, std::memory_order_relaxed);
        s_overlayPresentUs.store(presentUs, std::memory_order_relaxed);
        s_overlayPacingWaitUs.store(pacingWaitUs, std::memory_order_relaxed);
        uint64_t prevMax = s_overlayMaxFrameUs.load(std::memory_order_relaxed);
        while (frameUs > prevMax &&
               !s_overlayMaxFrameUs.compare_exchange_weak(prevMax, frameUs, std::memory_order_relaxed))
            ;
        lastFrameTime = frameEnd;
    }
