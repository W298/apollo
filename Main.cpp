#include "pch.h"
#include "Apollo.h"

#include "ApolloArgument.h"
#include "imgui_impl_win32.h"

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

using namespace DirectX;

UINT                        g_dwSize = sizeof(RAWINPUT);
BYTE                        g_lpb[sizeof(RAWINPUT)];

LPCWSTR                     g_szAppName = L"apollo";
std::unique_ptr<Apollo>     g_apollo;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;

    // imgui procedure handler.
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_CREATE:
        break;

    case WM_PAINT:
        if (s_in_sizemove && g_apollo)
        {
            g_apollo->Tick();
        }
        else
        {
            PAINTSTRUCT ps;
            std::ignore = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            if (!s_minimized)
            {
                s_minimized = true;
                if (!s_in_suspend && g_apollo)
                    g_apollo->OnSuspending();
                s_in_suspend = true;
            }
        }
        else if (s_minimized)
        {
            s_minimized = false;
            if (s_in_suspend && g_apollo)
                g_apollo->OnResuming();
            s_in_suspend = false;
        }
        else if (!s_in_sizemove && g_apollo)
        {
            g_apollo->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_KEYDOWN:
        g_apollo->OnKeyDown(static_cast<UINT8>(wParam));
        break;

    case WM_KEYUP:
		g_apollo->OnKeyUp(static_cast<UINT8>(wParam));
		break;

    case WM_MOUSEWHEEL:
        g_apollo->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
		break;

    case WM_INPUT:
    {
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, g_lpb, &g_dwSize, sizeof(RAWINPUTHEADER));

        const auto* raw = reinterpret_cast<RAWINPUT*>(g_lpb);
        if (raw->header.dwType == RIM_TYPEMOUSE)
            g_apollo->OnMouseMove(raw->data.mouse.lLastX, raw->data.mouse.lLastY);
        break;
    }

    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;

    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        if (g_apollo)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);

            g_apollo->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
        }
        break;

    case WM_GETMINMAXINFO:
        if (lParam)
        {
	        const auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320;
            info->ptMinTrackSize.y = 200;
        }
        break;

    case WM_ACTIVATEAPP:
        if (g_apollo)
        {
            if (wParam)
            {
                g_apollo->OnActivated();
            }
            else
            {
                g_apollo->OnDeactivated();
            }
        }
        break;

    case WM_POWERBROADCAST:
        switch (wParam)
        {
        case PBT_APMQUERYSUSPEND:
            if (!s_in_suspend && g_apollo)
                g_apollo->OnSuspending();
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if (!s_minimized)
            {
                if (s_in_suspend && g_apollo)
                    g_apollo->OnResuming();
                s_in_suspend = false;
            }
            return TRUE;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!XMVerifyCPUSupport())
        return 1;

    Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialize))
        return 1;

    g_apollo = std::make_unique<Apollo>();

    // Register class and create window
    {
        // Register class
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_ACTIVECAPTION);
        wcex.lpszClassName = L"apolloWindowClass";
        wcex.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);
        if (!RegisterClassExW(&wcex))
            return 1;

        // Collect arguments
        const ApolloArgument arguments = CollectApolloArgument();

        RECT rc = { 0, 0, static_cast<LONG>(arguments.Width), static_cast<LONG>(arguments.Height) };
        const DWORD dwStyle = arguments.FullScreenMode ? WS_POPUP : WS_OVERLAPPEDWINDOW;

    	AdjustWindowRect(&rc, dwStyle, FALSE);

        HWND hwnd = CreateWindowExW(0, L"apolloWindowClass", g_szAppName, dwStyle,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance,
            g_apollo.get());

        if (!hwnd)
            return 1;

        ShowWindow(hwnd, nCmdShow);
    	GetClientRect(hwnd, &rc);

        POINT pt = { arguments.Width / 2, arguments.Height / 2 };
        ClientToScreen(hwnd, &pt);
        SetCursorPos(pt.x, pt.y);
        ShowCursor(FALSE);

        // Register raw input handler.
        RAWINPUTDEVICE Rid[1];
        Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        Rid[0].dwFlags = RIDEV_INPUTSINK;
        Rid[0].hwndTarget = hwnd;
        RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

        g_apollo->InitializeD3DResources(
            hwnd, rc.right - rc.left, rc.bottom - rc.top, 
            arguments.SubDivideCount, 
            arguments.ShadowMapSize, 
            arguments.FullScreenMode);
    }

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_apollo->Tick();
        }
    }

    g_apollo.reset();

    return static_cast<int>(msg.wParam);
}

// Exit helper
void ExitGame() noexcept
{
    PostQuitMessage(0);
}