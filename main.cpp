#include "dxutil.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "scene.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

const D3D_FEATURE_LEVEL kMinFeatureLevel = D3D_FEATURE_LEVEL_11_0;
const int kSwapChainBufferCount = 3;
const DXGI_FORMAT kSwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
const UINT kSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

static HWND g_hWnd = NULL;
static bool g_ShouldClose = false;
static ID3D11Device* g_Device;
static ID3D11DeviceContext* g_DeviceContext;
static IDXGISwapChain* g_SwapChain;
static HANDLE g_FrameLatencyWaitableObject;
static D3D11_RENDER_TARGET_VIEW_DESC g_SwapChainRTVDesc;

void RendererResize(int width, int height);

IMGUI_API LRESULT ImGui_ImplDX11_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGui_ImplDX11_WndProcHandler(hWnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_SIZE:
        if (g_Device && wParam != SIZE_MINIMIZED)
        {
            int windowWidth = (int)LOWORD(lParam);
            int windowHeight = (int)HIWORD(lParam);
            RendererResize(windowWidth, windowHeight);
        }
		return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_CLOSE:
        g_ShouldClose = true;
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void WindowInit(int width, int height, const char* title)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = L"WindowClass";
    CHECKWIN32(RegisterClassExW(&wc));

    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    DWORD dwExStyle = 0;
    RECT wr = { 0, 0, width, height };
    CHECKWIN32(AdjustWindowRectEx(&wr, dwStyle, FALSE, dwExStyle));

    std::wstring wtitle = WideFromMultiByte(title);

    g_hWnd = CreateWindowExW(
        dwExStyle, L"WindowClass", wtitle.c_str(), dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, GetModuleHandleW(NULL), NULL);
    CHECKWIN32(g_hWnd != NULL);

    ShowWindow(g_hWnd, SW_SHOWDEFAULT);
}

void RendererInit()
{
    ComPtr<IDXGIFactory> pDXGIFactory;
    CHECKHR(CreateDXGIFactory(IID_PPV_ARGS(&pDXGIFactory)));

    ComPtr<IDXGIAdapter> pDXGIAdapter;
    CHECKHR(pDXGIFactory->EnumAdapters(0, &pDXGIAdapter));

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferDesc.Format = kSwapChainFormat;
    scd.SampleDesc.Count = 1;
    scd.BufferCount = kSwapChainBufferCount;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = g_hWnd;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    UINT flags = 0;
#if _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;
    ComPtr<IDXGISwapChain2> pSwapChain;
    HANDLE hFrameLatencyWaitableObject;

    D3D_FEATURE_LEVEL kFeatureLevels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    D3D_FEATURE_LEVEL featureLevel;
    ComPtr<IDXGISwapChain> pTmpSwapChain;
    CHECKHR(D3D11CreateDeviceAndSwapChain(
        pDXGIAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, flags,
        kFeatureLevels, _countof(kFeatureLevels),
        D3D11_SDK_VERSION,
        &scd, &pTmpSwapChain,
        &pDevice, &featureLevel, &pDeviceContext));
    CHECKHR(pTmpSwapChain.As(&pSwapChain));

    if (featureLevel < kMinFeatureLevel)
    {
        SimpleMessageBox_FatalError(
            "Minimum D3D feature level not satisfied:\n"
            "Minimum feature level: %d.%d\n"
            "Actual feature level: %d.%d\n",
            (kMinFeatureLevel >> 12) & 0x0F, (kMinFeatureLevel >> 8) & 0x0F,
            (featureLevel >> 12) & 0x0F, (featureLevel >> 8) & 0x0F);
    }

    hFrameLatencyWaitableObject = pSwapChain->GetFrameLatencyWaitableObject();
    CHECKHR(pDXGIFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_WINDOW_CHANGES));

    g_Device = pDevice.Get();
	g_Device->AddRef();
    g_DeviceContext = pDeviceContext.Get();
	g_DeviceContext->AddRef();
	g_SwapChain = pSwapChain.Get();
	g_SwapChain->AddRef();
	g_FrameLatencyWaitableObject = hFrameLatencyWaitableObject;
}

void RendererResize(int width, int height)
{
	CHECKHR(g_SwapChain->ResizeBuffers(
		kSwapChainBufferCount,
		width, height,
		kSwapChainFormat, kSwapChainFlags));

	g_SwapChainRTVDesc.Format = kSwapChainFormat;
	g_SwapChainRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	SceneResize(width, height);
}

void RendererPaint()
{
	ID3D11Device* dev = g_Device;
	ID3D11DeviceContext* dc = g_DeviceContext;
	IDXGISwapChain* sc = g_SwapChain;

	// Wait until the previous frame is presented before drawing the next frame
	CHECKWIN32(WaitForSingleObject(g_FrameLatencyWaitableObject, INFINITE) == WAIT_OBJECT_0);

	// grab the current backbuffer
	ComPtr<ID3D11Texture2D> pBackBufferTex2D;
	ComPtr<ID3D11RenderTargetView> pBackBufferRTV;
	CHECKHR(sc->GetBuffer(0, IID_PPV_ARGS(&pBackBufferTex2D)));
	CHECKHR(dev->CreateRenderTargetView(pBackBufferTex2D.Get(), &g_SwapChainRTVDesc, &pBackBufferRTV));

	ScenePaint(pBackBufferRTV.Get());

	// Render ImGui
	ID3D11RenderTargetView* imguiRTVs[] = { pBackBufferRTV.Get() };
	dc->OMSetRenderTargets(_countof(imguiRTVs), imguiRTVs, NULL);
	ImGui::Render();
	dc->OMSetRenderTargets(0, NULL, NULL);

	// finally present
	CHECKHR(sc->Present(0, 0));
}

int main()
{
    WindowInit(1280, 720, "trianglebin");
    RendererInit();
    ImGui_ImplDX11_Init(g_hWnd, g_Device, g_DeviceContext);
	SceneInit(g_Device, g_DeviceContext);

    RECT cr;
    CHECKWIN32(GetClientRect(g_hWnd, &cr));
    RendererResize(cr.right - cr.left, cr.bottom - cr.top);

    for (;;)
    {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (g_ShouldClose)
        {
            break;
        }

        ImGui_ImplDX11_NewFrame();

        RendererPaint();
    }
}