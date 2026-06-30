#include "../include/Core/Memory.hpp"
#include "../include/Game/Entity.hpp"
#include "../include/Overlay/Overlay.hpp"
#include "../include/Utils/Math.hpp"
#include <cstdint>
#include <cmath>
#include <iostream>
#include <vector>

// Global config
struct Config {
    bool espEnabled = true;
    bool espBox = false;
    bool espHeadDot = true;
    bool espHealth = false;
    bool espTeam = false;
    bool espSnaplines = false;
    bool espSkeleton = true;
    bool espName = false;
    bool espHpText = true;
    bool bhopEnabled = true;
    bool menuOpen = true;
} g_Config;

// CS:S v93 x64 bone IDs (from Articulador/UC)
// 0=pelvis 1=Lhip 2=Lknee 3=Lankle 5=Rhip 6=Rknee 7=Rankle
// 10=spine 11=chest 12=neck 14=head
// 16=Lshoulder 17=Lelbow 18=Lhand 29=Rshoulder 30=Relbow 31=Rhand
struct BoneLink { int from, to; };
const BoneLink g_Skeleton[] = {
    {0, 10}, {10, 11}, {11, 12}, {12, 14},         // pelvis → spine → chest → neck → head
    {12, 16}, {16, 17}, {17, 18},                    // neck → L arm
    {12, 29}, {29, 30}, {30, 31},                    // neck → R arm
    {0, 1}, {1, 2}, {2, 3},                          // pelvis → L leg
    {0, 5}, {5, 6}, {6, 7},                          // pelvis → R leg
};
constexpr int g_BoneCount = 32;
constexpr int g_SkeletonLinks = sizeof(g_Skeleton) / sizeof(g_Skeleton[0]);

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
                ImGui::Checkbox("Head Dot", &g_Config.espHeadDot);
                ImGui::Checkbox("Health Bar", &g_Config.espHealth);
                ImGui::Checkbox("Snaplines", &g_Config.espSnaplines);
                ImGui::Checkbox("Skeleton", &g_Config.espSkeleton);
                ImGui::Checkbox("Name", &g_Config.espName);
                ImGui::Checkbox("HP Text", &g_Config.espHpText);
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

                // Auto-detected offsets (0 = not yet found)
    static uintptr_t g_BoneOff = 0x810;
    static uintptr_t g_LifeOff = 0xCF;
    static Vector3 g_LastPos[128] = {};
    static int g_StaleFrames[128] = {};
    static int g_FrameCounter = 0;

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

        g_FrameCounter++;

        bool f3Pressed = (GetAsyncKeyState(VK_F3) & 0x8000);

        uintptr_t localPlayerBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dwLocalPlayer);
        if (localPlayerBase) {
            Game::Entity localPlayer(localPlayerBase, mem);

            // BunnyHop Logic
            if (g_Config.bhopEnabled && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                int flags = mem.Read<int>(localPlayerBase + Game::Offsets::m_fFlags);
                if (flags & (1 << 0)) {
                    // DISABLED: bhop writes to game memory (ForceJump). kept read-only
                    // for the walls-only build. uncomment to re-enable on local servers.
                    // mem.Write<int>(clientBase + Game::Offsets::ForceJump, 5);
                } else {
                    // mem.Write<int>(clientBase + Game::Offsets::ForceJump, 4);
                }
            }

            // ESP Logic
            if (g_Config.espEnabled) {
                Matrix4x4 viewMatrix = mem.Read<Matrix4x4>(engineBase + Game::Offsets::dw_ViewMatrix);
                int localTeam = localPlayer.GetTeam();
                Vector3 localPos = localPlayer.GetPosition();
                int entitiesFound = 0;
                int screenTransforms = 0;
                uintptr_t nameListBase = mem.Read<uintptr_t>(clientBase + 0x609D68);

                for (int i = 1; i < 128; i++) {
                    uintptr_t entityBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dw_BaseEntity + (i * 0x10));
                    if (!entityBase || entityBase == localPlayerBase || entityBase < 0x10000) continue;

                    Game::Entity entity(entityBase, mem);
                    int hp = entity.GetHealth();
                    int entityTeam = entity.GetTeam();
                    Vector3 pos = entity.GetPosition();

                    // Kill ghosts: lifeState != 0 means dead/dying/spec
                    uint8_t lifeState = mem.Read<uint8_t>(entityBase + g_LifeOff);
                    if (lifeState != 0) continue;

                    // Cheap checks first: dead, invalid HP, junk team, origin zero
                    if (hp <= 0 || hp > 100) continue;
                    if (entityTeam < 2 || entityTeam > 3) continue;
                    if (std::abs(pos.x) < 1.0f && std::abs(pos.y) < 1.0f && std::abs(pos.z) < 1.0f) continue;
                    if (!g_Config.espTeam && entityTeam == localTeam) continue;

                    // Model index: real players have model > 0
                    int modelIdx = mem.Read<int>(entityBase + 0xCC);
                    if (modelIdx <= 0) continue;

                    // Name check: empty name = disconnected slot
                    bool hasName = false;
                    if (nameListBase && nameListBase >= 0x10000) {
                        uintptr_t np = mem.Read<uintptr_t>(nameListBase + 0x798 + (i * 0x4));
                        if (np && np >= 0x10000) {
                            char nb[4] = {};
                            if (ReadProcessMemory(mem.GetHandle(), (LPCVOID)np, nb, 3, nullptr) && nb[0])
                                hasName = true;
                        }
                    }
                    if (!hasName) continue;

                    // MoveType: real players = 2 (WALK)
                    int moveType = mem.Read<int>(entityBase + Game::Offsets::m_MoveType);
                    if (moveType < 2 || moveType > 11) continue;

                    // Stale check LAST: frozen in place for 60+ frames
                    if (g_LastPos[i].x == pos.x && g_LastPos[i].y == pos.y && g_LastPos[i].z == pos.z)
                        g_StaleFrames[i]++;
                    else { g_StaleFrames[i] = 0; g_LastPos[i] = pos; }
                    if (g_StaleFrames[i] > 60) continue;

                    // Distance-based alpha: close = solid, far = ghosted
                    float dx = pos.x - localPos.x;
                    float dy = pos.y - localPos.y;
                    float dz = pos.z - localPos.z;
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    int alpha = 255 - (int)(dist / 8.0f);
                    if (alpha < 30) alpha = 30;
                    if (alpha > 255) alpha = 255;

                    // Velocity prediction: extrapolate ~1 tick ahead so dots don't trail
                    Vector3 velocity = entity.GetVelocity();
                    Vector3 predictedPos = {
                        pos.x + velocity.x * 0.03f,
                        pos.y + velocity.y * 0.03f,
                        pos.z + velocity.z * 0.03f
                    };

                    entitiesFound++;

                    if (f3Pressed) {
                        std::cout << "Idx: " << i << " | HP: " << hp << " | Team: " << entityTeam
                                  << " | Life: " << (int)lifeState << " | Dist: " << (int)dist
                                  << " | Vel: " << velocity.x << "," << velocity.y << "," << velocity.z << std::endl;
                    }

                    // Head position: read head bone (14) for accuracy, fallback to viewOffset
                    Vector3 headPos;
                    if (g_BoneOff) {
                        uintptr_t bm = mem.Read<uintptr_t>(entityBase + g_BoneOff);
                        if (bm && bm >= 0x10000)
                            headPos = mem.Read<Matrix3x4>(bm + 14 * 48).GetOrigin();
                        else
                            headPos = predictedPos + mem.Read<Vector3>(entityBase + Game::Offsets::m_vecViewOffset);
                    } else {
                        headPos = predictedPos + mem.Read<Vector3>(entityBase + Game::Offsets::m_vecViewOffset);
                    }

                    Vector3 screenFeet, screenHead;
                    if (Utils::WorldToScreen(predictedPos, screenFeet, viewMatrix, overlay.GetWidth(), overlay.GetHeight()) &&
                        Utils::WorldToScreen(headPos, screenHead, viewMatrix, overlay.GetWidth(), overlay.GetHeight())) {
                        
                        screenTransforms++;
                        ImColor color = (entityTeam == localTeam)
                            ? ImColor(0, 255, 0, alpha)
                            : ImColor(255, 0, 0, alpha);
                        auto drawList = ImGui::GetForegroundDrawList();

                        // Skeleton: read bone positions and draw stick figure
                        if (g_Config.espSkeleton) {
                            uintptr_t bm = mem.Read<uintptr_t>(entityBase + g_BoneOff);
                            if (bm && bm >= 0x10000) {
                                Vector3 bones[g_BoneCount];
                                for (int bn = 0; bn < g_BoneCount; bn++)
                                    bones[bn] = mem.Read<Matrix3x4>(bm + bn * 48).GetOrigin();
                                for (int b = 0; b < g_SkeletonLinks; b++) {
                                    Vector3 scrA, scrB;
                                    if (Utils::WorldToScreen(bones[g_Skeleton[b].from], scrA, viewMatrix, overlay.GetWidth(), overlay.GetHeight()) &&
                                        Utils::WorldToScreen(bones[g_Skeleton[b].to], scrB, viewMatrix, overlay.GetWidth(), overlay.GetHeight())) {
                                        drawList->AddLine({ scrA.x, scrA.y }, { scrB.x, scrB.y }, color, 1.5f);
                                    }
                                }
                            }
                        }

                        // Head dot: filled circle with dark outline for contrast
                        if (g_Config.espHeadDot) {
                            float radius = 4.0f;
                            drawList->AddCircleFilled({ screenHead.x, screenHead.y }, radius, color);
                            drawList->AddCircle({ screenHead.x, screenHead.y }, radius + 1.0f, ImColor(0, 0, 0, alpha / 2), 0, 1.5f);
                        }

                        // HP text: health number above head
                        if (g_Config.espHpText) {
                            char buf[16];
                            snprintf(buf, sizeof(buf), "%d", hp);
                            drawList->AddText({ screenHead.x - 8.0f, screenHead.y - 14.0f }, ImColor(255, 255, 255, alpha), buf);
                        }

                        // Name ESP — reuse cached name from staleness check
                        if (g_Config.espName && hasName && nameListBase && nameListBase >= 0x10000) {
                            uintptr_t namePtr = mem.Read<uintptr_t>(nameListBase + 0x798 + (i * 0x4));
                            if (namePtr && namePtr >= 0x10000) {
                                char nameBuf[32] = {};
                                if (ReadProcessMemory(mem.GetHandle(), (LPCVOID)namePtr, nameBuf, sizeof(nameBuf) - 1, nullptr) && nameBuf[0]) {
                                    drawList->AddText({ screenHead.x - 30.0f, screenHead.y - 26.0f },
                                        ImColor(255, 255, 255, alpha), nameBuf);
                                }
                            }
                        }

                        float height = std::abs(screenFeet.y - screenHead.y);
                        float width = height / 2.1f;

                        if (g_Config.espBox) {
                            drawList->AddRect({ screenHead.x - width / 2.0f, screenHead.y }, { screenFeet.x + width / 2.0f, screenFeet.y }, color, 0.0f, 0, 1.5f);
                        }

                        if (g_Config.espHealth) {
                            DrawHealthBar(screenHead.x - width / 2.0f, screenHead.y, height, hp);
                        }

                        if (g_Config.espSnaplines) {
                            drawList->AddLine({ (float)overlay.GetWidth() / 2.0f, (float)overlay.GetHeight() }, { screenFeet.x, screenFeet.y }, color, 1.0f);
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
