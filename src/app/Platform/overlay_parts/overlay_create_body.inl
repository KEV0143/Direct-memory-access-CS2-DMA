    const HINSTANCE hInst = GetModuleHandleW(nullptr);
    const HICON hIcon = LoadIconW(hInst, L"IDI_APPICON");

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance      = hInst;
    wc.lpszClassName  = L"KevqDMA_Overlay";
    wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon          = hIcon;
    wc.hIconSm        = hIcon;
    RegisterClassExW(&wc);

    static const std::string windowTitleUtf8 = app::build_info::RuntimeTitle();
    static const std::wstring windowTitle(windowTitleUtf8.begin(), windowTitleUtf8.end());

    s_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        windowTitle.c_str(),
        WS_POPUP | WS_VISIBLE,
        0, 0, width, height,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!s_hwnd) return false;

    ShowWindow(s_hwnd, SW_SHOWNA);
    UpdateWindow(s_hwnd);

    s_tearingSupported = CheckTearingSupport();

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &s_device, &featureLevel, &s_context);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> dxgiAdapter;
    ComPtr<IDXGIFactory2> dxgiFactory;

    hr = s_device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return false;

    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC1 sd1 = {};
    sd1.Width       = width;
    sd1.Height      = height;
    sd1.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd1.SampleDesc  = { 1, 0 };
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.BufferCount = 2;
    sd1.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    UINT swapChainFlags =
        (s_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u) |
        DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    sd1.Flags = swapChainFlags;
    hr = dxgiFactory->CreateSwapChainForHwnd(s_device.Get(), s_hwnd, &sd1, nullptr, nullptr, &s_swapChain);
    if (FAILED(hr)) {
        swapChainFlags &= ~DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd1.Flags = swapChainFlags;
        hr = dxgiFactory->CreateSwapChainForHwnd(s_device.Get(), s_hwnd, &sd1, nullptr, nullptr, &s_swapChain);
    }
    if (FAILED(hr)) return false;

    s_swapChain2.Reset();
    s_waitableSwapChainEnabled = false;
    if (s_frameLatencyWaitableObject) {
        CloseHandle(s_frameLatencyWaitableObject);
        s_frameLatencyWaitableObject = nullptr;
    }
    if ((swapChainFlags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) != 0u &&
        SUCCEEDED(s_swapChain.As(&s_swapChain2)) &&
        s_swapChain2 &&
        SUCCEEDED(s_swapChain2->SetMaximumFrameLatency(1))) {
        s_frameLatencyWaitableObject = s_swapChain2->GetFrameLatencyWaitableObject();
        s_waitableSwapChainEnabled = (s_frameLatencyWaitableObject != nullptr);
        if (!s_waitableSwapChainEnabled)
            s_swapChain2.Reset();
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = s_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer) return false;

    hr = s_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &s_rtv);
    if (FAILED(hr)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    static std::string s_imguiIniPath;
    if (s_imguiIniPath.empty()) {
        const auto iniPath = ResolveImGuiIniPath();
        if (!iniPath.empty())
            s_imguiIniPath = iniPath.string();
    }
    io.IniFilename = s_imguiIniPath.empty() ? nullptr : s_imguiIniPath.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    static const ImWchar glyphRanges[] = {
        0x0020, 0x00FF,
        0x0400, 0x052F,
        0x2000, 0x206F,
        0x2190, 0x21FF,
        0x2500, 0x257F,
        0x25A0, 0x25FF,
        0x2600, 0x26FF,
        0x2700, 0x27BF,
        0
    };
    const float baseFontSize = 16.0f;

    std::string segoeRegularPath, segoeBoldPath, comicRegularPath, comicBoldPath;
    {
        PWSTR fontsDir = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &fontsDir))) {
            std::filesystem::path fontsPath(fontsDir);
            CoTaskMemFree(fontsDir);
            segoeRegularPath = (fontsPath / "segoeui.ttf").string();
            segoeBoldPath = (fontsPath / "segoeuib.ttf").string();
            comicRegularPath = (fontsPath / "comic.ttf").string();
            comicBoldPath = (fontsPath / "comicbd.ttf").string();
        } else {
            segoeRegularPath = "C:\\Windows\\Fonts\\segoeui.ttf";
            segoeBoldPath = "C:\\Windows\\Fonts\\segoeuib.ttf";
            comicRegularPath = "C:\\Windows\\Fonts\\comic.ttf";
            comicBoldPath = "C:\\Windows\\Fonts\\comicbd.ttf";
        }
    }

    g::fontDefault = nullptr;
    g::fontSegoeBold = nullptr;
    g::fontComicSans = nullptr;
    g::fontWeaponIcons = nullptr;

    if (std::filesystem::exists(comicRegularPath))
        g::fontDefault = io.Fonts->AddFontFromFileTTF(comicRegularPath.c_str(), baseFontSize, nullptr, glyphRanges);
    else if (std::filesystem::exists(segoeRegularPath))
        g::fontDefault = io.Fonts->AddFontFromFileTTF(segoeRegularPath.c_str(), baseFontSize, nullptr, glyphRanges);

    if (std::filesystem::exists(comicBoldPath))
        g::fontSegoeBold = io.Fonts->AddFontFromFileTTF(comicBoldPath.c_str(), g::espNameFontSize, nullptr, glyphRanges);
    else if (std::filesystem::exists(segoeBoldPath))
        g::fontSegoeBold = io.Fonts->AddFontFromFileTTF(segoeBoldPath.c_str(), g::espNameFontSize, nullptr, glyphRanges);
    if (std::filesystem::exists(comicRegularPath)) {
        ImFontConfig comicCfg = {};
        comicCfg.OversampleH = 2;
        comicCfg.OversampleV = 2;
        g::fontComicSans = io.Fonts->AddFontFromFileTTF(comicRegularPath.c_str(), 18.0f, &comicCfg, glyphRanges);
    }

    if (io.Fonts->Fonts.empty()) {
        g::fontDefault = io.Fonts->AddFontDefault();
    }
    if (!g::fontDefault) {
        g::fontDefault = io.Fonts->Fonts[0];
    }

    ImFontConfig weaponCfg = {};
    weaponCfg.PixelSnapH = true;
    weaponCfg.OversampleH = 1;
    weaponCfg.OversampleV = 1;
    weaponCfg.FontDataOwnedByAtlas = false;
    static const ImWchar weaponRanges[] = { 0x20, 0x7E, 0 };
    g::fontWeaponIcons = io.Fonts->AddFontFromMemoryTTF(
        resources::fonts::weapons,
        sizeof(resources::fonts::weapons),
        17.0f,
        &weaponCfg,
        weaponRanges);

    LoadEspUiIconTextures();

    ui::ApplyStyle();

    ImGui_ImplWin32_Init(s_hwnd);
    ImGui_ImplWin32_EnableAlphaCompositing(s_hwnd);
    ImGui_ImplDX11_Init(s_device.Get(), s_context.Get());

    ApplyOverlayWindowMode(g::menuOpen);
    SyncOverlayBounds();

    return true;
