#include "../include/Core/Memory.hpp"
#include "../include/Game/Entity.hpp"
#include "../include/Overlay/Overlay.hpp"
#include "../include/Utils/Math.hpp"
#include <iostream>
#include <vector>

// Global config
struct Config {
    bool espEnabled = true;
    bool espBox = true;
    bool espHealth = true;
    bool espTeam = false;
    bool espSnaplines = false;
    bool bhopEnabled = true;
    bool menuOpen = true;
} g_Config;

void DrawHealthBar(float x, float y, float height, int health) {
    float fill = (float)health / 100.0f;
    ImColor color = ImColor(1.0f - fill, fill, 0.0f, 1.0f); // Red to Green

    ImGui::GetBackgroundDrawList()->AddRectFilled({ x - 6.0f, y }, { x - 2.0f, y + height }, ImColor(0, 0, 0, 150));
    ImGui::GetBackgroundDrawList()->AddRectFilled({ x - 5.0f, y + height - (height * fill) }, { x - 3.0f, y + height }, color);
}

void RenderMenu() {
    if (g_Config.menuOpen) {
        ImGui::Begin("Educational ESP - CS:S v93", &g_Config.menuOpen, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::CollapsingHeader("ESP Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Master ESP", &g_Config.espEnabled);
            if (g_Config.espEnabled) {
                ImGui::Indent();
                ImGui::Checkbox("Box", &g_Config.espBox);
                ImGui::Checkbox("Health Bar", &g_Config.espHealth);
                ImGui::Checkbox("Snaplines", &g_Config.espSnaplines);
                ImGui::Checkbox("Show Teammates", &g_Config.espTeam);
                ImGui::Unindent();
            }
        }

        if (ImGui::CollapsingHeader("Misc Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("BunnyHop", &g_Config.bhopEnabled);
        }

        ImGui::Separator();
        ImGui::Text("INSERT to toggle menu");
        ImGui::Text("END to exit");
        ImGui::End();
    }
}

// Helper to find window by PID
struct EnumData {
    DWORD pid;
    HWND hwnd;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumData* data = (EnumData*)lParam;
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid == data->pid && GetWindow(hwnd, GW_OWNER) == NULL && IsWindowVisible(hwnd)) {
        data->hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND FindWindowByPID(DWORD pid) {
    EnumData data = { pid, NULL };
    EnumWindows(EnumWindowsProc, (LPARAM)&data);
    return data.hwnd;
}

int main() {
    Core::Memory mem;
    std::cout << "[!] Educational ESP - CS:S v93" << std::endl;
    std::cout << "[!] Make sure to run this program AS ADMINISTRATOR!" << std::endl;
    std::cout << "[*] Searching for process: cstrike_win64.exe..." << std::endl;

    DWORD targetPid = 0;
    while (true) {
        targetPid = mem.GetProcessId(L"cstrike_win64.exe");
        if (targetPid != 0) {
            std::cout << "[+] Found process PID: " << targetPid << std::endl;
            if (mem.Attach(L"cstrike_win64.exe")) {
                std::cout << "[+] Successfully attached to process!" << std::endl;
                break;
            } else {
                std::cout << "[-] Failed to open process handle. Try running as Admin!" << std::endl;
            }
        }
        Sleep(1000);
    }

    uintptr_t clientBase = mem.GetModuleBase(L"client.dll");
    uintptr_t engineBase = mem.GetModuleBase(L"engine.dll");

    std::cout << "[*] Client.dll: 0x" << std::hex << clientBase << std::dec << std::endl;
    std::cout << "[*] Engine.dll: 0x" << std::hex << engineBase << std::dec << std::endl;

    while (!clientBase || !engineBase) {
        clientBase = mem.GetModuleBase(L"client.dll");
        engineBase = mem.GetModuleBase(L"engine.dll");
        Sleep(1000);
    }

    std::cout << "[*] Searching for game window..." << std::endl;
    HWND gameHwnd = FindWindowByPID(targetPid);

    if (!gameHwnd) {
        std::cout << "[-] Failed to find game window associated with PID: " << targetPid << std::endl;
        return 1;
    }

    Overlay::Window overlay;
    std::cout << "[*] Creating overlay..." << std::endl;
    if (!overlay.Create(gameHwnd)) {
        std::cout << "[-] Failed to create overlay! Ensure the game is in Windowed/Borderless mode." << std::endl;
        return 1;
    }
    std::cout << "[+] Overlay created successfully!" << std::endl;

    bool insertPressed = false;

    while (overlay.IsOpen()) {
        overlay.BeginFrame();

        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            if (!insertPressed) {
                g_Config.menuOpen = !g_Config.menuOpen;
                overlay.ToggleInput(g_Config.menuOpen);
                insertPressed = true;
            }
        } else {
            insertPressed = false;
        }

        if (GetAsyncKeyState(VK_END) & 0x8000) {
            overlay.Stop();
        }

        overlay.UpdatePosition(gameHwnd);
        RenderMenu();

        bool f3Pressed = (GetAsyncKeyState(VK_F3) & 0x8000);

        uintptr_t localPlayerBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dw_BaseEntity);
        if (localPlayerBase) {
            Game::Entity localPlayer(localPlayerBase, mem);

            // BunnyHop Logic
            if (g_Config.bhopEnabled && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                int flags = mem.Read<int>(localPlayerBase + Game::Offsets::m_fFlags);
                if (flags & (1 << 0)) {
                    mem.Write<int>(clientBase + Game::Offsets::ForceJump, 5);
                } else {
                    mem.Write<int>(clientBase + Game::Offsets::ForceJump, 4);
                }
            }

            // ESP Logic
            if (g_Config.espEnabled) {
                Matrix4x4 viewMatrix = mem.Read<Matrix4x4>(engineBase + Game::Offsets::dw_ViewMatrix);
                int localTeam = localPlayer.GetTeam();
                int entitiesFound = 0;
                int screenTransforms = 0;

                for (int i = 1; i < 64; i++) {
                    uintptr_t entityBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dw_BaseEntity + (i * 0x20));
                    if (!entityBase || entityBase == localPlayerBase) continue;

                    Game::Entity entity(entityBase, mem);
                    int hp = entity.GetHealth();
                    Vector3 pos = entity.GetPosition();

                    if (hp <= 1 || hp > 100) continue;
                    if (std::abs(pos.x) < 1.0f && std::abs(pos.y) < 1.0f) continue;

                    int entityTeam = entity.GetTeam();
                    if (!g_Config.espTeam && entityTeam == localTeam) continue;

                    entitiesFound++;
                    if (f3Pressed) {
                        std::cout << "Idx: " << i << " | HP: " << hp << " | Pos: " << pos.x << ", " << pos.y << std::endl;
                    }

                    Vector3 viewOffset = mem.Read<Vector3>(entityBase + Game::Offsets::m_vecViewOffset);
                    Vector3 headPos = pos + viewOffset;
                    headPos.z += 8.0f;

                    Vector3 screenPos, screenHeadPos;
                    if (Utils::WorldToScreen(pos, screenPos, viewMatrix, overlay.GetWidth(), overlay.GetHeight()) &&
                        Utils::WorldToScreen(headPos, screenHeadPos, viewMatrix, overlay.GetWidth(), overlay.GetHeight())) {
                        
                        screenTransforms++;
                        float height = std::abs(screenPos.y - screenHeadPos.y);
                        float width = height / 2.1f;
                        ImColor color = (entityTeam == localTeam) ? ImColor(0, 255, 0) : ImColor(255, 0, 0);
                        auto drawList = ImGui::GetForegroundDrawList();

                        if (g_Config.espBox) {
                            drawList->AddRect({ screenHeadPos.x - width / 2.0f, screenHeadPos.y }, { screenPos.x + width / 2.0f, screenPos.y }, color, 0.0f, 0, 1.5f);
                        }

                        if (g_Config.espHealth) {
                            DrawHealthBar(screenHeadPos.x - width / 2.0f, screenHeadPos.y, height, hp);
                        }

                        if (g_Config.espSnaplines) {
                            drawList->AddLine({ (float)overlay.GetWidth() / 2.0f, (float)overlay.GetHeight() }, { screenPos.x, screenPos.y }, color, 1.0f);
                        }
                    }
                }
            }
        }

        overlay.EndFrame();
        Sleep(1);
    }

    return 0;
}
