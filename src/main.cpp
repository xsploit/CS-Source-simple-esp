#include "../include/Core/Memory.hpp"
#include "../include/Core/Config.hpp"
#include "../include/Game/Entity.hpp"
#include "../include/Overlay/Overlay.hpp"
#include "../include/Utils/Math.hpp"
#include <cstdint>
#include <cmath>
#include <iostream>
#include <vector>

// global persistent config (defined here, declared extern in Config.hpp)
Core::Config g_Config;

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

// draw a box in one of three styles, using the configured thickness
static void DrawBox(ImDrawList* dl, const ImVec2& head, const ImVec2& feet,
                    float width, ImU32 color, int style, float thickness) {
    float left = head.x - width / 2.0f, right = head.x + width / 2.0f;
    float top = head.y, bot = feet.y;
    if (style == 1) { // filled
        dl->AddRectFilled({ left, top }, { right, bot }, color);
    } else if (style == 2) { // corner
        float cw = width * 0.35f;       // corner arm length
        // 8 short segments at the corners
        dl->AddLine({ left,  top }, { left + cw, top }, color, thickness);
        dl->AddLine({ left,  top }, { left, top + cw }, color, thickness);
        dl->AddLine({ right, top }, { right - cw, top }, color, thickness);
        dl->AddLine({ right, top }, { right, top + cw }, color, thickness);
        dl->AddLine({ left,  bot }, { left + cw, bot }, color, thickness);
        dl->AddLine({ left,  bot }, { left, bot - cw }, color, thickness);
        dl->AddLine({ right, bot }, { right - cw, bot }, color, thickness);
        dl->AddLine({ right, bot }, { right, bot - cw }, color, thickness);
    } else { // outline (default)
        dl->AddRect({ left, top }, { right, bot }, color, 0.0f, 0, thickness);
    }
}

void DrawHealthBar(float x, float y, float height, int health, ImU32 color) {
    float fill = (float)health / 100.0f;
    ImGui::GetBackgroundDrawList()->AddRectFilled({ x - 6.0f, y }, { x - 2.0f, y + height }, ImColor(0, 0, 0, 150));
    ImGui::GetBackgroundDrawList()->AddRectFilled({ x - 5.0f, y + height - (height * fill) }, { x - 3.0f, y + height }, color);
}

// bone-glow chams: thick translucent tube per bone segment + bright core line
// on top + filled joint dots at major joints. reads as a glowing body shape
// that tracks the model's real pose (unlike a screen-space box, this can't
// float off the player because it's locked to their bones every frame).
// externally we can't hook the game's renderer for true chams; this is the
// closest an overlay gets to the chams feel.
void DrawChams(ImDrawList* dl, const Vector3 bonesScreen[32],
               ImU32 skelColor, ImU32 coreColor,
               float thickness, float coreThick, float jointRad) {
    static const int majorJoints[] = { 14, 12, 11, 10, 0, 16, 29, 18, 31, 3, 7 };

    // outer glow tubes — thick translucent lines along each bone link
    for (int b = 0; b < g_SkeletonLinks; b++) {
        const Vector3& a = bonesScreen[g_Skeleton[b].from];
        const Vector3& c = bonesScreen[g_Skeleton[b].to];
        dl->AddLine({ a.x, a.y }, { c.x, c.y }, skelColor, thickness);
    }
    // bright core lines on top — gives the "neon tube" pop
    for (int b = 0; b < g_SkeletonLinks; b++) {
        const Vector3& a = bonesScreen[g_Skeleton[b].from];
        const Vector3& c = bonesScreen[g_Skeleton[b].to];
        dl->AddLine({ a.x, a.y }, { c.x, c.y }, coreColor, coreThick);
    }
    // joint dots — fills out the silhouette so overlapping limbs still read
    for (int j : majorJoints) {
        const Vector3& p = bonesScreen[j];
        dl->AddCircleFilled({ p.x, p.y }, jointRad, skelColor);
        dl->AddCircleFilled({ p.x, p.y }, jointRad * 0.5f, coreColor);
    }
}

void RenderMenu() {
    if (!g_Config.menuOpen) return;

    ImGui::Begin("Educational ESP - CS:S v93", &g_Config.menuOpen, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginTabBar("##tabs")) {

        // ---------- ESP tab ----------
        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Checkbox("Master ESP", &g_Config.espEnabled);
            if (g_Config.espEnabled) {
                ImGui::Separator();
                ImGui::Checkbox("Box",         &g_Config.espBox);
                ImGui::Checkbox("Head Dot",    &g_Config.espHeadDot);
                ImGui::Checkbox("Health Bar",  &g_Config.espHealth);
                ImGui::Checkbox("Snaplines",   &g_Config.espSnaplines);
                ImGui::Checkbox("Skeleton",    &g_Config.espSkeleton);
                ImGui::Checkbox("Name",        &g_Config.espName);
                ImGui::Checkbox("HP Text",     &g_Config.espHpText);
                ImGui::Checkbox("Show Teammates", &g_Config.espTeam);
            }
            ImGui::EndTabItem();
        }

        // ---------- Style tab ----------
        if (ImGui::BeginTabItem("Style")) {
            ImGui::TextDisabled("-- box --");
            const char* styles[] = { "Outline", "Filled", "Corner" };
            ImGui::Combo("box style", &g_Config.boxStyle, styles, 3);
            ImGui::SliderFloat("box thickness", &g_Config.boxThickness, 0.5f, 4.0f, "%.1f");

            ImGui::Separator();
            ImGui::TextDisabled("-- distance --");
            ImGui::Checkbox("show metres (vs raw units)", &g_Config.distanceInMetres);
            ImGui::SliderFloat("fade distance", &g_Config.maxFadeDist, 500.0f, 6000.0f, "%.0f");

            ImGui::Separator();
            ImGui::TextDisabled("-- chams --");
            ImGui::SliderFloat("chams thickness", &g_Config.chamsThickness, 4.0f, 20.0f, "%.1f");
            ImGui::SliderFloat("chams core",      &g_Config.chamsCore,      1.0f, 8.0f,  "%.1f");
            ImGui::SliderFloat("chams joint",     &g_Config.chamsJointRad,  2.0f, 12.0f, "%.1f");

            ImGui::Separator();
            ImGui::TextDisabled("-- colors (click swatch) --");
            ImGui::ColorEdit4("enemy",     &g_Config.colEnemy.x,   ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("team",      &g_Config.colTeam.x,    ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("skeleton",  &g_Config.colSkeleton.x,ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("head dot",  &g_Config.colHeadDot.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("name",      &g_Config.colName.x,    ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("hp text",   &g_Config.colHpText.x,  ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("snapline",  &g_Config.colSnapline.x,ImGuiColorEditFlags_NoInputs);

            ImGui::EndTabItem();
        }

        // ---------- Config tab ----------
        if (ImGui::BeginTabItem("Config")) {
            if (ImGui::Button("Save")) g_Config.Save();
            ImGui::SameLine();
            if (ImGui::Button("Reload")) g_Config.Load();
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", g_Config.path.c_str());

            ImGui::Separator();
            ImGui::Checkbox("BunnyHop (writes disabled — walls-only)", &g_Config.bhopEnabled);

            ImGui::EndTabItem();
        }
    }
    ImGui::EndTabBar();

    ImGui::Separator();
    ImGui::Text("INSERT toggle menu | END exit");
    ImGui::End();
}

// Helper to find window by PID
struct EnumData { DWORD pid; HWND hwnd; };
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

    // load config first — path defaults to config.json next to cwd
    g_Config.Load();

    std::cout << "[*] Searching for process: cstrike_win64.exe..." << std::endl;
    DWORD targetPid = 0;
    while (true) {
        targetPid = mem.GetProcessId(L"cstrike_win64.exe");
        if (targetPid != 0) {
            std::cout << "[+] Found process PID: " << targetPid << std::endl;
            if (mem.Attach(L"cstrike_win64.exe")) {
                std::cout << "[+] Successfully attached to process!" << std::endl;
                break;
            }
            std::cout << "[-] Failed to open process handle. Try running as Admin!" << std::endl;
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

    HWND gameHwnd = FindWindowByPID(targetPid);
    if (!gameHwnd) {
        std::cout << "[-] Failed to find game window for PID " << targetPid << std::endl;
        return 1;
    }

    Overlay::Window overlay;
    if (!overlay.Create(gameHwnd)) {
        std::cout << "[-] Failed to create overlay! Game must be Windowed/Borderless." << std::endl;
        return 1;
    }
    std::cout << "[+] Overlay created!" << std::endl;

    bool insertPressed = false;
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
        } else insertPressed = false;

        if (GetAsyncKeyState(VK_END) & 0x8000) {
            g_Config.Save();        // autosave on exit
            overlay.Stop();
        }

        overlay.UpdatePosition(gameHwnd);
        RenderMenu();
        g_FrameCounter++;
        bool f3Pressed = (GetAsyncKeyState(VK_F3) & 0x8000);

        uintptr_t localPlayerBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dwLocalPlayer);
        if (localPlayerBase) {
            Game::Entity localPlayer(localPlayerBase, mem);

            // bhop: reads only — writes disabled for the walls-only build
            if (g_Config.bhopEnabled && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                int flags = mem.Read<int>(localPlayerBase + Game::Offsets::m_fFlags);
                (void)flags; // was: if (flags & 1) mem.Write(...ForceJump, 5)
            }

            if (g_Config.espEnabled) {
                Matrix4x4 viewMatrix = mem.Read<Matrix4x4>(engineBase + Game::Offsets::dw_ViewMatrix);
                int localTeam = localPlayer.GetTeam();
                Vector3 localPos = localPlayer.GetPosition();
                uintptr_t nameListBase = mem.Read<uintptr_t>(clientBase + 0x609D68);

                for (int i = 1; i < 128; i++) {
                    uintptr_t entityBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dw_BaseEntity + (i * 0x10));
                    if (!entityBase || entityBase == localPlayerBase || entityBase < 0x10000) continue;

                    Game::Entity entity(entityBase, mem);
                    int hp = entity.GetHealth();
                    int entityTeam = entity.GetTeam();
                    Vector3 pos = entity.GetPosition();

                    // cheap filters first
                    uint8_t lifeState = mem.Read<uint8_t>(entityBase + g_LifeOff);
                    if (lifeState != 0) continue;
                    if (hp <= 0 || hp > 100) continue;
                    if (entityTeam < 2 || entityTeam > 3) continue;
                    if (std::abs(pos.x) < 1.0f && std::abs(pos.y) < 1.0f && std::abs(pos.z) < 1.0f) continue;
                    if (!g_Config.espTeam && entityTeam == localTeam) continue;

                    int modelIdx = mem.Read<int>(entityBase + 0xCC);
                    if (modelIdx <= 0) continue;

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

                    int moveType = mem.Read<int>(entityBase + Game::Offsets::m_MoveType);
                    if (moveType < 2 || moveType > 11) continue;

                    // stale check LAST
                    if (g_LastPos[i].x == pos.x && g_LastPos[i].y == pos.y && g_LastPos[i].z == pos.z)
                        g_StaleFrames[i]++;
                    else { g_StaleFrames[i] = 0; g_LastPos[i] = pos; }
                    if (g_StaleFrames[i] > 60) continue;

                    // distance-based alpha with configurable fade
                    float dx = pos.x - localPos.x, dy = pos.y - localPos.y, dz = pos.z - localPos.z;
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    int alpha = (int)(255.0f * (1.0f - (dist / g_Config.maxFadeDist)));
                    if (alpha < 30) alpha = 30;
                    if (alpha > 255) alpha = 255;

                    // pick color + apply fade
                    bool isMate = (entityTeam == localTeam);
                    ImU32 baseColor  = Core::ColorFor(g_Config, isMate, alpha);
                    ImU32 skelColor  = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colSkeleton.x, g_Config.colSkeleton.y, g_Config.colSkeleton.z,
                               (g_Config.colSkeleton.w * alpha) / 255.0f));
                    ImU32 headColor  = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colHeadDot.x, g_Config.colHeadDot.y, g_Config.colHeadDot.z,
                               (g_Config.colHeadDot.w * alpha) / 255.0f));
                    ImU32 nameColor  = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colName.x, g_Config.colName.y, g_Config.colName.z,
                               (g_Config.colName.w * alpha) / 255.0f));
                    ImU32 hpColor    = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colHpText.x, g_Config.colHpText.y, g_Config.colHpText.z,
                               (g_Config.colHpText.w * alpha) / 255.0f));
                    ImU32 snapColor  = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colSnapline.x, g_Config.colSnapline.y, g_Config.colSnapline.z,
                               (g_Config.colSnapline.w * alpha) / 255.0f));

                    Vector3 velocity = entity.GetVelocity();
                    Vector3 predictedPos = { pos.x + velocity.x * 0.03f, pos.y + velocity.y * 0.03f, pos.z + velocity.z * 0.03f };

                    if (f3Pressed) {
                        std::cout << "Idx: " << i << " | HP: " << hp << " | Team: " << entityTeam
                                  << " | Dist: " << (int)(g_Config.distanceInMetres ? dist / 39.37f : dist)
                                  << (g_Config.distanceInMetres ? "m" : "u") << std::endl;
                    }

                    // head bone
                    Vector3 headPos;
                    if (g_BoneOff) {
                        uintptr_t bm = mem.Read<uintptr_t>(entityBase + g_BoneOff);
                        if (bm && bm >= 0x10000) headPos = mem.Read<Matrix3x4>(bm + 14 * 48).GetOrigin();
                        else headPos = predictedPos + mem.Read<Vector3>(entityBase + Game::Offsets::m_vecViewOffset);
                    } else {
                        headPos = predictedPos + mem.Read<Vector3>(entityBase + Game::Offsets::m_vecViewOffset);
                    }

                    Vector3 screenFeet, screenHead;
                    if (Utils::WorldToScreen(predictedPos, screenFeet, viewMatrix, overlay.GetWidth(), overlay.GetHeight()) &&
                        Utils::WorldToScreen(headPos, screenHead, viewMatrix, overlay.GetWidth(), overlay.GetHeight())) {

                        auto drawList = ImGui::GetForegroundDrawList();

                        // skeleton
                        // skeleton AND chams both need the projected bone positions —
                        // read+project once, draw either/both from the same array.
                        if (g_Config.espSkeleton || g_Config.espChams) {
                            uintptr_t bm = mem.Read<uintptr_t>(entityBase + g_BoneOff);
                            if (bm && bm >= 0x10000) {
                                // world-space bones
                                Vector3 bonesWorld[g_BoneCount];
                                for (int bn = 0; bn < g_BoneCount; bn++)
                                    bonesWorld[bn] = mem.Read<Matrix3x4>(bm + bn * 48).GetOrigin();
                                // project all 32 to screen once
                                Vector3 bonesScreen[g_BoneCount];
                                bool allOnscreen = true;
                                for (int bn = 0; bn < g_BoneCount; bn++) {
                                    if (!Utils::WorldToScreen(bonesWorld[bn], bonesScreen[bn], viewMatrix, overlay.GetWidth(), overlay.GetHeight())) {
                                        allOnscreen = false; break;
                                    }
                                }
                                if (allOnscreen) {
                                    // thin skeleton first (under chams if both on)
                                    if (g_Config.espSkeleton) {
                                        for (int b = 0; b < g_SkeletonLinks; b++) {
                                            const Vector3& a = bonesScreen[g_Skeleton[b].from];
                                            const Vector3& c = bonesScreen[g_Skeleton[b].to];
                                            drawList->AddLine({ a.x, a.y }, { c.x, c.y }, skelColor, 1.5f);
                                        }
                                    }
                                    // bone-glow chams on top
                                    if (g_Config.espChams) {
                                        // brighten the core: same hue, near-white intensity
                                        ImVec4 base = isMate ? g_Config.colTeam : g_Config.colEnemy;
                                        ImVec4 corev((std::min)(1.0f, base.x + 0.5f),
                                                     (std::min)(1.0f, base.y + 0.5f),
                                                     (std::min)(1.0f, base.z + 0.5f),
                                                     (base.w * alpha) / 255.0f);
                                        ImU32 coreColor = ImGui::ColorConvertFloat4ToU32(corev);
                                        DrawChams(drawList, bonesScreen, baseColor, coreColor,
                                                  g_Config.chamsThickness, g_Config.chamsCore, g_Config.chamsJointRad);
                                    }
                                }
                            }
                        }

                        if (g_Config.espHeadDot) {
                            float radius = 4.0f;
                            drawList->AddCircleFilled({ screenHead.x, screenHead.y }, radius, headColor);
                            drawList->AddCircle({ screenHead.x, screenHead.y }, radius + 1.0f, ImColor(0, 0, 0, alpha / 2), 0, 1.5f);
                        }

                        if (g_Config.espHpText) {
                            char buf[16];
                            snprintf(buf, sizeof(buf), "%d", hp);
                            drawList->AddText({ screenHead.x - 8.0f, screenHead.y - 14.0f }, hpColor, buf);
                        }

                        if (g_Config.espName && hasName && nameListBase && nameListBase >= 0x10000) {
                            uintptr_t namePtr = mem.Read<uintptr_t>(nameListBase + 0x798 + (i * 0x4));
                            if (namePtr && namePtr >= 0x10000) {
                                char nameBuf[32] = {};
                                if (ReadProcessMemory(mem.GetHandle(), (LPCVOID)namePtr, nameBuf, sizeof(nameBuf) - 1, nullptr) && nameBuf[0]) {
                                    drawList->AddText({ screenHead.x - 30.0f, screenHead.y - 26.0f }, nameColor, nameBuf);
                                }
                            }
                        }

                        // distance text under feet
                        float dDisp = g_Config.distanceInMetres ? dist / 39.37f : dist;
                        const char* unit = g_Config.distanceInMetres ? "m" : "u";
                        char dbuf[24];
                        snprintf(dbuf, sizeof(dbuf), "%d%s", (int)dDisp, unit);
                        drawList->AddText({ screenHead.x - 12.0f, screenFeet.y + 2.0f }, nameColor, dbuf);

                        float height = std::abs(screenFeet.y - screenHead.y);
                        float width = height / 2.1f;

                        if (g_Config.espBox) {
                            DrawBox(drawList, { screenHead.x, screenHead.y }, { screenFeet.x, screenFeet.y },
                                    width, baseColor, g_Config.boxStyle, g_Config.boxThickness);
                        }
                        if (g_Config.espHealth) {
                            DrawHealthBar(screenHead.x - width / 2.0f, screenHead.y, height, hp, baseColor);
                        }
                        if (g_Config.espSnaplines) {
                            drawList->AddLine({ (float)overlay.GetWidth() / 2.0f, (float)overlay.GetHeight() },
                                              { screenFeet.x, screenFeet.y }, snapColor, 1.0f);
                        }
                    }
                }
            }
        }

        overlay.EndFrame();
        Sleep(1);
    }

    g_Config.Save();   // final autosave
    return 0;
}
