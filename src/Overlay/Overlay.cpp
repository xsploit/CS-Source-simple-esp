#include "../../include/Overlay/Overlay.hpp"
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {
    Window::Window() {
        SetProcessDPIAware();
    }

    Window::~Window() {
        Destroy();
    }

    bool Window::Create(HWND targetHwnd) {
        if (!targetHwnd) return false;

        RECT clientRect;
        GetClientRect(targetHwnd, &clientRect);
        m_Width = clientRect.right - clientRect.left;
        m_Height = clientRect.bottom - clientRect.top;

        POINT windowPos = { 0, 0 };
        ClientToScreen(targetHwnd, &windowPos);

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"OverlayClass", NULL };
        RegisterClassExW(&wc);

        m_Hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, L"OverlayClass", L"Overlay", WS_POPUP, windowPos.x, windowPos.y, m_Width, m_Height, NULL, NULL, wc.hInstance, NULL);

        SetLayeredWindowAttributes(m_Hwnd, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);
        DWM_BLURBEHIND bb = { DWM_BB_ENABLE | DWM_BB_BLURREGION, TRUE, CreateRectRgn(0, 0, -1, -1), FALSE };
        DwmEnableBlurBehindWindow(m_Hwnd, &bb);

        if (!InitD3D()) return false;

        ShowWindow(m_Hwnd, SW_SHOWDEFAULT);
        UpdateWindow(m_Hwnd);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(m_Hwnd);
        ImGui_ImplDX9_Init(m_Device);

        return true;
    }

    void Window::Destroy() {
        if (m_Device) {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            m_Device->Release();
            m_Device = nullptr;
        }
        if (m_D3D) {
            m_D3D->Release();
            m_D3D = nullptr;
        }
        if (m_Hwnd) {
            DestroyWindow(m_Hwnd);
            m_Hwnd = nullptr;
        }
    }

    bool Window::InitD3D() {
        m_D3D = Direct3DCreate9(D3D_SDK_VERSION);
        if (!m_D3D) return false;

        m_D3DPP.Windowed = TRUE;
        m_D3DPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
        m_D3DPP.BackBufferFormat = D3DFMT_A8R8G8B8;

        HRESULT hr = m_D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_Hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &m_D3DPP, &m_Device);
        return SUCCEEDED(hr);
    }

    void Window::BeginFrame() {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) m_Running = false;
        }

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void Window::EndFrame() {
        ImGui::EndFrame();
        m_Device->SetRenderState(D3DRS_ZENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        m_Device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        m_Device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
        if (m_Device->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            m_Device->EndScene();
        }
        m_Device->Present(NULL, NULL, NULL, NULL);
    }

    void Window::UpdatePosition(HWND targetHwnd) {
        RECT cRect;
        GetClientRect(targetHwnd, &cRect);
        POINT wPos = { 0, 0 };
        ClientToScreen(targetHwnd, &wPos);
        MoveWindow(m_Hwnd, wPos.x, wPos.y, cRect.right - cRect.left, cRect.bottom - cRect.top, FALSE);
        m_Width = cRect.right - cRect.left;
        m_Height = cRect.bottom - cRect.top;
    }

    void Window::ToggleInput(bool enable) {
        if (enable) {
            SetWindowLong(m_Hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TOPMOST);
        } else {
            SetWindowLong(m_Hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST);
        }
    }

    LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;
        if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}
