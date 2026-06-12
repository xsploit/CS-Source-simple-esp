#pragma once
#include <windows.h>
#include <d3d9.h>
#include <dwmapi.h>
#include "../../imgui/imgui.h"
#include "../../imgui/imgui_impl_dx9.h"
#include "../../imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dwmapi.lib")

namespace Overlay {
    class Window {
    public:
        Window();
        ~Window();

        bool Create(HWND targetHwnd);
        void Destroy();

        void BeginFrame();
        void EndFrame();

        bool IsOpen() const { return m_Running; }
        void Stop() { m_Running = false; }

        HWND GetHWND() const { return m_Hwnd; }
        IDirect3DDevice9* GetDevice() const { return m_Device; }
        
        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }

        void UpdatePosition(HWND targetHwnd);
        void ToggleInput(bool enable);

    private:
        bool InitD3D();
        static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HWND m_Hwnd = nullptr;
        IDirect3D9* m_D3D = nullptr;
        IDirect3DDevice9* m_Device = nullptr;
        D3DPRESENT_PARAMETERS m_D3DPP = {};

        int m_Width = 0;
        int m_Height = 0;
        bool m_Running = true;
    };
}
