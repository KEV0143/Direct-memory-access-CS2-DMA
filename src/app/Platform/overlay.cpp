#include "app/Platform/overlay.h"
#include "app/Core/build_info.h"
#include "app/Core/globals.h"
#include "app/Config/project_paths.h"
#include "Features/ESP/esp.h"
#include "app/UI/MenuShell/ui_style.h"
#include "Features/WebRadar/webradar.h"
#include "fonts/weapons.hpp"
#include <DMALibrary/Memory/Memory.h>

#include <Windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_5.h>

using Microsoft::WRL::ComPtr;
#include <filesystem>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <ShlObj.h>


#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HWND                       s_hwnd = nullptr;
static ComPtr<ID3D11Device>       s_device;
static ComPtr<ID3D11DeviceContext> s_context;
static ComPtr<IDXGISwapChain1>    s_swapChain;
static ComPtr<IDXGISwapChain2>    s_swapChain2;
static ComPtr<ID3D11RenderTargetView> s_rtv;
static bool                    s_tearingSupported = false;
static bool                    s_waitableSwapChainEnabled = false;
static bool                    s_overlayInteractive = false;
static HANDLE                  s_frameLatencyWaitableObject = nullptr;
static std::atomic<uint64_t>   s_overlayFrameUs = 0;
static std::atomic<uint64_t>   s_overlayMaxFrameUs = 0;
static std::atomic<uint64_t>   s_overlaySyncUs = 0;
static std::atomic<uint64_t>   s_overlayDrawUs = 0;
static std::atomic<uint64_t>   s_overlayPresentUs = 0;
static std::atomic<uint64_t>   s_overlayPacingWaitUs = 0;

static void RecreateRenderTarget(UINT width, UINT height)
{
    if (!s_swapChain || !s_device || !s_context)
        return;

    s_rtv.Reset();
    s_context->OMSetRenderTargets(0, nullptr, nullptr);

    const UINT flags =
        (s_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u) |
        (s_waitableSwapChainEnabled ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0u);
    if (FAILED(s_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, flags)))
        return;

    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(s_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) || !backBuffer)
        return;

    s_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &s_rtv);
}

static void ApplyOverlayWindowMode(bool interactive)
{
    if (!s_hwnd)
        return;

    LONG_PTR exStyle = GetWindowLongPtrW(s_hwnd, GWL_EXSTYLE);
    LONG_PTR desiredExStyle = exStyle | WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (interactive) {
        desiredExStyle &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        desiredExStyle &= ~static_cast<LONG_PTR>(WS_EX_NOACTIVATE);
    } else {
        desiredExStyle |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    }

    if (desiredExStyle != exStyle)
        SetWindowLongPtrW(s_hwnd, GWL_EXSTYLE, desiredExStyle);

    SetLayeredWindowAttributes(s_hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    SetWindowPos(
        s_hwnd,
        nullptr,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_SHOWWINDOW |
        SWP_FRAMECHANGED | (interactive ? 0u : SWP_NOACTIVATE));

    s_overlayInteractive = interactive;
}

namespace
{
    struct OverlayTargetWindowSearch
    {
        DWORD pid = 0;
        HWND hwnd = nullptr;
        RECT rect = {};
        LONG bestArea = 0;
    };

    BOOL CALLBACK EnumWindowsForOverlayTarget(HWND hwnd, LPARAM lParam)
    {
        auto* search = reinterpret_cast<OverlayTargetWindowSearch*>(lParam);
        if (!search)
            return TRUE;
        if (!IsWindowVisible(hwnd))
            return TRUE;
        if (GetWindow(hwnd, GW_OWNER) != nullptr)
            return TRUE;

        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        if (windowPid != search->pid)
            return TRUE;

        RECT windowRect = {};
        if (!GetClientRect(hwnd, &windowRect))
            return TRUE;

        POINT origin = { windowRect.left, windowRect.top };
        if (!ClientToScreen(hwnd, &origin))
            return TRUE;

        windowRect.left = origin.x;
        windowRect.top = origin.y;
        windowRect.right += origin.x;
        windowRect.bottom += origin.y;

        const LONG width = windowRect.right - windowRect.left;
        const LONG height = windowRect.bottom - windowRect.top;
        const LONG area = width * height;
        if (width <= 0 || height <= 0 || area <= 0)
            return TRUE;

        if (!search->hwnd || area > search->bestArea) {
            search->hwnd = hwnd;
            search->rect = windowRect;
            search->bestArea = area;
        }

        return TRUE;
    }

    bool TryGetCs2ClientWindow(HWND* outHwnd, RECT* outRect, DWORD* outPid)
    {
        if (outHwnd)
            *outHwnd = nullptr;
        if (outRect)
            *outRect = RECT{};
        if (outPid)
            *outPid = 0;

        const DWORD cs2Pid = mem.GetPidFromName("cs2.exe");
        if (!cs2Pid)
            return false;

        OverlayTargetWindowSearch search = {};
        search.pid = cs2Pid;
        EnumWindows(EnumWindowsForOverlayTarget, reinterpret_cast<LPARAM>(&search));
        if (!search.hwnd)
            return false;

        if (outHwnd)
            *outHwnd = search.hwnd;
        if (outRect)
            *outRect = search.rect;
        if (outPid)
            *outPid = cs2Pid;
        return true;
    }

    bool TryGetFallbackOverlayRect(HWND anchorHwnd, RECT* outRect)
    {
        if (outRect)
            *outRect = RECT{};

        HMONITOR monitor = nullptr;
        if (anchorHwnd && IsWindow(anchorHwnd))
            monitor = MonitorFromWindow(anchorHwnd, MONITOR_DEFAULTTONEAREST);

        if (!monitor) {
            HWND foregroundWindow = GetForegroundWindow();
            if (foregroundWindow)
                monitor = MonitorFromWindow(foregroundWindow, MONITOR_DEFAULTTONEAREST);
        }

        if (!monitor) {
            HWND consoleWindow = GetConsoleWindow();
            if (consoleWindow)
                monitor = MonitorFromWindow(consoleWindow, MONITOR_DEFAULTTONEAREST);
        }

        if (!monitor) {
            const POINT origin = { 0, 0 };
            monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
        }

        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
            return false;

        if (outRect)
            *outRect = monitorInfo.rcMonitor;
        return true;
    }

    void PruneLegacyImGuiIniEntries(const std::filesystem::path& iniPath)
    {
        if (iniPath.empty())
            return;

        std::ifstream input(iniPath, std::ios::binary);
        if (!input.is_open())
            return;

        std::vector<std::string> keptLines;
        keptLines.reserve(256);
        bool skipSection = false;
        bool changed = false;
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (!line.empty() && line.front() == '[') {
                const bool isLegacyMainWindow =
                    line.rfind("[Window][KevqDMA ", 0) == 0 &&
                    line.find("###main_menu]") == std::string::npos;
                skipSection = isLegacyMainWindow;
                changed = changed || isLegacyMainWindow;
                if (skipSection)
                    continue;
            }

            if (skipSection)
                continue;

            keptLines.push_back(line);
        }

        if (!changed)
            return;

        std::ofstream output(iniPath, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
            return;

        for (const std::string& keptLine : keptLines)
            output << keptLine << "\r\n";
    }
}

static void SyncOverlayBounds()
{
    if (!s_hwnd || !s_swapChain)
        return;

    static bool s_lastTopmost = true;
    static uint64_t s_lastTargetRefreshMs = 0;
    static bool s_cachedHasTargetWindow = false;
    static HWND s_cachedTargetHwnd = nullptr;
    static RECT s_cachedTargetRect = {};
    static DWORD s_cachedCs2Pid = 0;

    const uint64_t nowMs = GetTickCount64();
    const bool shouldRefreshTarget =
        s_overlayInteractive ||
        (nowMs - s_lastTargetRefreshMs) >= 50u ||
        !IsWindow(s_cachedTargetHwnd);
    if (shouldRefreshTarget) {
        s_cachedTargetHwnd = nullptr;
        s_cachedTargetRect = {};
        s_cachedCs2Pid = 0;
        s_cachedHasTargetWindow = TryGetCs2ClientWindow(&s_cachedTargetHwnd, &s_cachedTargetRect, &s_cachedCs2Pid);
        s_lastTargetRefreshMs = nowMs;
    }

    HWND targetHwnd = s_cachedTargetHwnd;
    RECT targetRect = s_cachedTargetRect;
    DWORD cs2Pid = s_cachedCs2Pid;
    const bool hasTargetWindow = s_cachedHasTargetWindow;
    if (!hasTargetWindow) {
        if (!TryGetFallbackOverlayRect(GetForegroundWindow(), &targetRect)) {
            targetRect.left = 0;
            targetRect.top = 0;
            targetRect.right = GetSystemMetrics(SM_CXSCREEN);
            targetRect.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
    }

    const int targetX = targetRect.left;
    const int targetY = targetRect.top;
    const int targetWidth = targetRect.right - targetRect.left;
    const int targetHeight = targetRect.bottom - targetRect.top;
    if (targetWidth <= 0 || targetHeight <= 0)
        return;

    RECT windowRect = {};
    GetWindowRect(s_hwnd, &windowRect);
    const int currentWidth = windowRect.right - windowRect.left;
    const int currentHeight = windowRect.bottom - windowRect.top;
    HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (foregroundWindow)
        GetWindowThreadProcessId(foregroundWindow, &foregroundPid);
    const bool overlayShouldBeTopmost =
        s_overlayInteractive ||
        foregroundWindow == s_hwnd ||
        (hasTargetWindow && foregroundWindow == targetHwnd) ||
        foregroundPid == cs2Pid;

    ShowWindow(s_hwnd, s_overlayInteractive ? SW_SHOW : SW_SHOWNA);

    const bool boundsChanged =
        windowRect.left != targetX ||
        windowRect.top != targetY ||
        currentWidth != targetWidth ||
        currentHeight != targetHeight;
    const bool topmostChanged = overlayShouldBeTopmost != s_lastTopmost;
    const bool windowHidden = !IsWindowVisible(s_hwnd);

    if (boundsChanged) {
        SetWindowPos(
            s_hwnd,
            overlayShouldBeTopmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            targetX, targetY, targetWidth, targetHeight,
            SWP_NOOWNERZORDER | SWP_SHOWWINDOW | (s_overlayInteractive ? 0u : SWP_NOACTIVATE));
        RecreateRenderTarget(static_cast<UINT>(targetWidth), static_cast<UINT>(targetHeight));
        s_lastTopmost = overlayShouldBeTopmost;
    } else if (topmostChanged || windowHidden) {
        SetWindowPos(
            s_hwnd,
            overlayShouldBeTopmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            targetX, targetY, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW |
            (s_overlayInteractive ? 0u : SWP_NOACTIVATE));
        s_lastTopmost = overlayShouldBeTopmost;
    }

    g::screenWidth = targetWidth;
    g::screenHeight = targetHeight;
}

static std::filesystem::path ResolveImGuiIniPath()
{
    const auto settingsDir = app::paths::GetSettingsDirectory();
    if (settingsDir.empty())
        return {};

    const auto targetPath = settingsDir / "imgui.ini";
    std::error_code ec;
    std::filesystem::create_directories(targetPath.parent_path(), ec);
    std::filesystem::remove(settingsDir / "imgui.build", ec);
    PruneLegacyImGuiIniEntries(targetPath);

    return targetPath;
}

static bool CheckTearingSupport()
{
    ComPtr<IDXGIFactory4> factory4;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory4));
    if (FAILED(hr) || !factory4)
        return false;

    ComPtr<IDXGIFactory5> factory5;
    hr = factory4.As(&factory5);
    if (FAILED(hr) || !factory5)
        return false;

    BOOL allowTearing = FALSE;
    hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    return SUCCEEDED(hr) && allowTearing;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        g::running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool overlay::Create(int width, int height)
{
    #include "overlay_parts/overlay_create_body.inl"
}

void overlay::Run()
{
    #include "overlay_parts/overlay_run_body.inl"
}

overlay::PerfStats overlay::GetPerfStats()
{
    PerfStats stats = {};
    stats.frameUs = s_overlayFrameUs.load(std::memory_order_relaxed);
    stats.maxFrameUs = s_overlayMaxFrameUs.load(std::memory_order_relaxed);
    stats.syncUs = s_overlaySyncUs.load(std::memory_order_relaxed);
    stats.drawUs = s_overlayDrawUs.load(std::memory_order_relaxed);
    stats.presentUs = s_overlayPresentUs.load(std::memory_order_relaxed);
    stats.pacingWaitUs = s_overlayPacingWaitUs.load(std::memory_order_relaxed);
    return stats;
}

void overlay::Destroy()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (s_frameLatencyWaitableObject) {
        CloseHandle(s_frameLatencyWaitableObject);
        s_frameLatencyWaitableObject = nullptr;
    }

    s_rtv.Reset();
    s_swapChain.Reset();
    s_swapChain2.Reset();
    s_context.Reset();
    s_device.Reset();

    if (s_hwnd) DestroyWindow(s_hwnd);
}
