#include "../include/Core/Memory.hpp"
#include "../include/Core/Config.hpp"
#include "../include/Game/Entity.hpp"
#include "../include/Overlay/Overlay.hpp"
#include "../include/Utils/Math.hpp"
#include "../include/Utils/anti_recoil.h"
#include <cstdint>
#include <cmath>
#include <cstring>
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
//
// boneValid[bn] = whether that bone projected onscreen. we draw PER-LINK and
// PER-JOINT, skipping any whose bones are offscreen — so a partial body still
// renders when one hand or the head crosses the screen edge. (the old
// all-or-nothing gate made chams flicker off whenever any single bone left
// the frustum.)
void DrawChams(ImDrawList* dl, const Vector3 bonesScreen[32], const bool boneValid[32],
               ImU32 skelColor, ImU32 coreColor,
               float thickness, float coreThick, float jointRad) {
    static const int majorJoints[] = { 14, 12, 11, 10, 0, 16, 29, 18, 31, 3, 7 };

    // outer glow tubes + bright cores, per link — skip links with an offscreen endpoint
    for (int b = 0; b < g_SkeletonLinks; b++) {
        int f = g_Skeleton[b].from, t = g_Skeleton[b].to;
        if (!boneValid[f] || !boneValid[t]) continue;
        const Vector3& a = bonesScreen[f];
        const Vector3& c = bonesScreen[t];
        dl->AddLine({ a.x, a.y }, { c.x, c.y }, skelColor, thickness);
        dl->AddLine({ a.x, a.y }, { c.x, c.y }, coreColor, coreThick);
    }
    // joint dots — only at joints that projected onscreen
    for (int j : majorJoints) {
        if (!boneValid[j]) continue;
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
            // ---- presets: one-click configs that set toggles + transparency ----
            ImGui::TextDisabled("presets");
            if (ImGui::Button("Clean (chams only)")) {
                g_Config.espEnabled = true;
                g_Config.espBox = false; g_Config.espHeadDot = false; g_Config.espHealth = false;
                g_Config.espSnaplines = false; g_Config.espSkeleton = false; g_Config.espChams = true;
                g_Config.espName = false; g_Config.espHpText = false; g_Config.espDistance = false;
                g_Config.alphaSkeleton = 0.5f; g_Config.alphaChams = 0.85f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Competitive")) {
                g_Config.espEnabled = true;
                g_Config.espBox = false; g_Config.espHeadDot = true; g_Config.espHealth = false;
                g_Config.espSnaplines = false; g_Config.espSkeleton = true; g_Config.espChams = false;
                g_Config.espName = false; g_Config.espHpText = true; g_Config.espDistance = false;
                g_Config.alphaSkeleton = 0.45f; g_Config.alphaChams = 0.85f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Walls+Chams")) {
                g_Config.espEnabled = true;
                g_Config.espBox = false; g_Config.espHeadDot = true; g_Config.espHealth = false;
                g_Config.espSnaplines = false; g_Config.espSkeleton = false; g_Config.espChams = true;
                g_Config.espName = false; g_Config.espHpText = true; g_Config.espDistance = false;
                g_Config.alphaSkeleton = 0.5f; g_Config.alphaChams = 0.7f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Everything")) {
                g_Config.espEnabled = true;
                g_Config.espBox = true; g_Config.espHeadDot = true; g_Config.espHealth = true;
                g_Config.espSnaplines = true; g_Config.espSkeleton = true; g_Config.espChams = true;
                g_Config.espName = true; g_Config.espHpText = true; g_Config.espDistance = true;
                g_Config.alphaSkeleton = 0.85f; g_Config.alphaChams = 0.85f;
            }
            ImGui::Separator();

            ImGui::Checkbox("Master ESP", &g_Config.espEnabled);
            if (g_Config.espEnabled) {
                ImGui::Separator();
                ImGui::Checkbox("Box",         &g_Config.espBox);
                ImGui::Checkbox("Head Dot",    &g_Config.espHeadDot);
                ImGui::Checkbox("Health Bar",  &g_Config.espHealth);
                ImGui::Checkbox("Snaplines",   &g_Config.espSnaplines);
                ImGui::Checkbox("Skeleton",    &g_Config.espSkeleton);
                ImGui::Checkbox("Chams",       &g_Config.espChams);
                ImGui::Checkbox("Name",        &g_Config.espName);
                ImGui::Checkbox("HP Text",     &g_Config.espHpText);
                ImGui::Checkbox("Distance",    &g_Config.espDistance);
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
            ImGui::TextDisabled("-- head dot --");
            ImGui::SliderFloat("head lift",     &g_Config.headLift,      0.0f, 0.30f, "%.2f");
            ImGui::SliderFloat("head dot size",  &g_Config.headDotRadius, 2.0f, 10.0f, "%.1f");

            ImGui::Separator();
            ImGui::TextDisabled("-- transparency --");
            ImGui::SliderFloat("skeleton alpha", &g_Config.alphaSkeleton, 0.05f, 1.0f, "%.2f");
            ImGui::SliderFloat("chams alpha",    &g_Config.alphaChams,    0.05f, 1.0f, "%.2f");

            ImGui::Separator();
            ImGui::TextDisabled("-- chams --");
            ImGui::SliderFloat("chams thickness", &g_Config.chamsThickness, 4.0f, 20.0f, "%.1f");
            ImGui::SliderFloat("chams core",      &g_Config.chamsCore,      1.0f, 8.0f,  "%.1f");
            ImGui::SliderFloat("chams joint",     &g_Config.chamsJointRad,  2.0f, 12.0f, "%.1f");

            ImGui::Separator();
            ImGui::TextDisabled("-- anti-recoil --");
            const char* modes[] = { "Pixel (SAD)", "Fixed (pull)" };
            ImGui::Combo("mode", &g_Config.recoilMode, modes, 2);
            ImGui::SliderFloat("sensitivity", &g_Config.recoilSensitivity, 0.1f, 2.0f, "%.2f");
            ImGui::SliderInt("max down/frame", &g_Config.recoilMaxComp, 1, 20);
            ImGui::SliderInt("fixed rate",     &g_Config.recoilFixedRate, 1, 10);
            ImGui::TextDisabled("live: +/- sensitivity, PgUp/PgDn max-down");

            ImGui::Separator();
            ImGui::TextDisabled("-- filter (pos+HP stale) --");
            ImGui::SliderInt("stale timeout (frames)", &g_Config.staleFrames, 30, 600);
            ImGui::SameLine(); ImGui::TextDisabled("(~%.0fs)", g_Config.staleFrames / 60.0f);

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
            ImGui::Checkbox("Anti-Recoil (pixel-based)", &g_Config.recoilEnabled);

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

    // load config first — resolve path next to the EXE, not CWD. CWD varies
    // (Explorer vs shell vs shortcut) so a relative "config.json" lands in
    // unpredictable places; the EXE folder is stable.
    {
        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH)) {
            std::string ep(exePath);
            size_t slash = ep.find_last_of("\\/");
            if (slash != std::string::npos) g_Config.path = ep.substr(0, slash + 1) + "config.json";
        }
    }
    g_Config.Load();

    // anti-recoil: pixel-based, no RPM — BitBlt crosshair strip + mouse pull
    ar::Config arCfg;
    arCfg.game_hwnd = nullptr; // set after we find the game window
    ar::AntiRecoil recoil(arCfg);

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

    // wire anti-recoil to the game window
    arCfg.game_hwnd = (void*)gameHwnd;
    recoil.SetConfig(arCfg);

    bool insertPressed = false;
    static uintptr_t g_BoneOff = 0x810;
    static uintptr_t g_LifeOff = 0xCF;
    static Vector3 g_LastPos[128] = {};
    static int g_StaleFrames[128] = {};
    static int g_LastHp[128] = {};
    static int g_FrameCounter = 0;

    while (overlay.IsOpen()) {
        overlay.BeginFrame();

        if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
            if (!insertPressed) {
                g_Config.menuOpen = !g_Config.menuOpen;
                insertPressed = true;
            }
        } else insertPressed = false;

        // sync overlay input capture with the ACTUAL menuOpen state every frame.
        // the old code only toggled on the INSERT keypress edge, so if menuOpen
        // flipped any other way (clicking the ImGui X, config load, END) the
        // overlay's WS_EX_TRANSPARENT flag went stale — cursor stuck or clicks
        // passing through to the game while the menu was up. cheap (a
        // SetWindowLongPtr) and correct regardless of how menuOpen changed.
        overlay.ToggleInput(g_Config.menuOpen);

        if (GetAsyncKeyState(VK_END) & 0x8000) {
            g_Config.Save();        // autosave on exit
            overlay.Stop();
        }

        overlay.UpdatePosition(gameHwnd);
        RenderMenu();
        g_FrameCounter++;

        // anti-recoil — sync config from g_Config every frame (so the +/- and
        // PgUp/PgDn keybinds + menu sliders take effect live), then update.
        if (g_Config.recoilEnabled) {
            arCfg.sensitivity      = g_Config.recoilSensitivity;
            arCfg.max_compensation = g_Config.recoilMaxComp;
            arCfg.fixed_rate       = g_Config.recoilFixedRate;
            arCfg.mode             = (g_Config.recoilMode == 1) ? ar::Mode::FIXED : ar::Mode::PIXEL;
            recoil.SetConfig(arCfg);
            recoil.Update();
        }

        // live keybinds: +/- adjusts pixel speed, PgUp/PgDn adjusts max-down.
        // edge-triggered so one tap = one step, not a runaway.
        {
            static bool prevPlus = false, prevMinus = false, prevPgUp = false, prevPgDn = false;
            bool curPlus  = (GetAsyncKeyState(VK_OEM_PLUS)  & 0x8000) != 0;
            bool curMinus = (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) != 0;
            bool curPgUp  = (GetAsyncKeyState(VK_PRIOR)    & 0x8000) != 0; // PgUp
            bool curPgDn  = (GetAsyncKeyState(VK_NEXT)     & 0x8000) != 0; // PgDn
            if (curPlus  && !prevPlus)  { g_Config.recoilSensitivity = (std::min)(2.0f, g_Config.recoilSensitivity + 0.05f); }
            if (curMinus && !prevMinus) { g_Config.recoilSensitivity = (std::max)(0.1f, g_Config.recoilSensitivity - 0.05f); }
            if (curPgUp  && !prevPgUp)  { g_Config.recoilMaxComp = (std::max)(1, g_Config.recoilMaxComp - 1); }   // less pull
            if (curPgDn  && !prevPgDn)  { g_Config.recoilMaxComp = (std::min)(20, g_Config.recoilMaxComp + 1); }  // more pull
            prevPlus = curPlus; prevMinus = curMinus; prevPgUp = curPgUp; prevPgDn = curPgDn;
        }

        bool f3Pressed = (GetAsyncKeyState(VK_F3) & 0x8000);

        uintptr_t localPlayerBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dwLocalPlayer);
        if (localPlayerBase) {
            Game::Entity localPlayer(localPlayerBase, mem);

            // skip ESP entirely while dead — sidesteps the whole spectate-target
            // problem and corpse-cleanup problem without needing dormant or stale
            // heuristics. when we respawn, ESP resumes with a clean slate.
            uint8_t myLife = mem.Read<uint8_t>(localPlayerBase + g_LifeOff);
            if (myLife != 0) goto skipEsp;

            if (g_Config.espEnabled) {
                Matrix4x4 viewMatrix = mem.Read<Matrix4x4>(engineBase + Game::Offsets::dw_ViewMatrix);
                int localTeam = localPlayer.GetTeam();
                Vector3 localPos = localPlayer.GetPosition();
                uintptr_t nameListBase = mem.Read<uintptr_t>(clientBase + 0x609D68);
                int screenW = overlay.GetWidth();
                int screenH = overlay.GetHeight();

                // ---- NO-FLICKER ARCHITECTURE (per swedz / maintained CS externals) ----
                // decouple READ from DRAW. the old loop interleaved ReadProcessMemory
                // with ImGui draw calls on the overlay thread — any RPM stall
                // (synchronous, variable latency) produced a half-drawn frame, which
                // presents as entities appearing/disappearing. flicker is NOT a filter
                // problem; it's a fetch/display architecture problem.
                //
                // pass 1 (read): walk entities, validate, read+project everything into
                //   a vector<RenderRecord> of pure data — no ImGui calls here.
                // pass 2 (draw): iterate the records and issue draw calls. RPM is done,
                //   so no stall can split a frame. a frame is either fully drawn or not.
                struct RenderRecord {
                    int hp; int entityTeam; bool isMate; int alpha;
                    float dist;
                    Vector3 screenFeet, screenHead;
                    Vector3 bonesScreen[32]; bool boneValid[32]; bool hasBones;
                    char name[32]; bool hasName;
                    bool headLifted; // head bone present (use lifted dot)
                    float headRadius; // range-scaled: project 4u above head, use on-screen delta
                };
                std::vector<RenderRecord> records;
                records.reserve(32);

                // ---- PASS 1: READ (all memory access here) ----
                for (int i = 1; i < 128; i++) {
                    uintptr_t entityBase = mem.Read<uintptr_t>(clientBase + Game::Offsets::dw_BaseEntity + (i * 0x10));
                    if (!entityBase || entityBase == localPlayerBase || entityBase < 0x10000) continue;

                    Game::Entity entity(entityBase, mem);
                    int hp = entity.GetHealth();
                    int entityTeam = entity.GetTeam();
                    Vector3 pos = entity.GetPosition();

                    // DORMANT (float compare): read as float vs local player.
                    // if they differ → entity dormant (server stopped updating).
                    // x86=0x60; x64 ~0x80-0xA0 — we'll scan to find working offset.
                    static uintptr_t g_DormantOff = 0x8C;
                    float myDormant = mem.Read<float>(localPlayerBase + g_DormantOff);
                    float entDormant = mem.Read<float>(entityBase + g_DormantOff);
                    if (entDormant != myDormant) continue;

                    uint8_t lifeState = mem.Read<uint8_t>(entityBase + g_LifeOff);
                    if (lifeState != 0) continue;
                    if (hp <= 0 || hp > 100) continue;
                    if (entityTeam < 2 || entityTeam > 3) continue;
                    if (std::abs(pos.x) < 1.0f && std::abs(pos.y) < 1.0f && std::abs(pos.z) < 1.0f) continue;
                    if (!g_Config.espTeam && entityTeam == localTeam) continue;

                    int modelIdx = mem.Read<int>(entityBase + 0xCC);
                    if (modelIdx <= 0) continue;
                    int moveType = mem.Read<int>(entityBase + Game::Offsets::m_MoveType);
                    if (moveType < 2 || moveType > 11) continue;

                    // name (cached once, not re-read at draw)
                    RenderRecord rec = {};
                    rec.hp = hp; rec.entityTeam = entityTeam; rec.isMate = (entityTeam == localTeam);
                    rec.hasName = false;
                    if (nameListBase && nameListBase >= 0x10000) {
                        uintptr_t np = mem.Read<uintptr_t>(nameListBase + 0x798 + (i * 0x4));
                        if (np && np >= 0x10000) {
                            if (ReadProcessMemory(mem.GetHandle(), (LPCVOID)np, rec.name, sizeof(rec.name) - 1, nullptr) && rec.name[0])
                                rec.hasName = true;
                        }
                    }
                    if (!rec.hasName) continue;

                    // position-stale + HP-stale: entity frozen AND HP unchanged
                    // for too long = dead/disconnected. dual-signal avoids false
                    // positives on campers (their HP changes on hit).
                    bool posSame = (std::abs(g_LastPos[i].x - pos.x) < 0.1f &&
                                    std::abs(g_LastPos[i].y - pos.y) < 0.1f &&
                                    std::abs(g_LastPos[i].z - pos.z) < 0.1f);
                    bool hpSame = (g_LastHp[i] == hp);
                    if (posSame && hpSame) {
                        g_StaleFrames[i]++;
                    } else {
                        g_StaleFrames[i] = 0;
                        g_LastPos[i] = pos;
                        g_LastHp[i] = hp;
                    }
                    if (g_StaleFrames[i] > g_Config.staleFrames) continue;

                    // distance alpha
                    float dx = pos.x - localPos.x, dy = pos.y - localPos.y, dz = pos.z - localPos.z;
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    int alpha = (int)(255.0f * (1.0f - (dist / g_Config.maxFadeDist)));
                    if (alpha < 30) alpha = 30; if (alpha > 255) alpha = 255;
                    rec.alpha = alpha;
                    rec.dist = dist;

                    // predict + head
                    Vector3 velocity = entity.GetVelocity();
                    Vector3 predictedPos = { pos.x + velocity.x * 0.03f, pos.y + velocity.y * 0.03f, pos.z + velocity.z * 0.03f };
                    Vector3 headPos;
                    uintptr_t bm = mem.Read<uintptr_t>(entityBase + g_BoneOff);
                    if (bm && bm >= 0x10000) headPos = mem.Read<Matrix3x4>(bm + 14 * 48).GetOrigin();
                    else headPos = predictedPos + mem.Read<Vector3>(entityBase + Game::Offsets::m_vecViewOffset);

                    // project feet + head
                    rec.headLifted = Utils::WorldToScreen(predictedPos, rec.screenFeet, viewMatrix, screenW, screenH) &&
                                     Utils::WorldToScreen(headPos, rec.screenHead, viewMatrix, screenW, screenH);
                    if (!rec.headLifted) continue; // offscreen — skip

                    // RANGE-SCALED HEAD RADIUS (pattern from arukenimon): project a point
                    // 4 world-units above the head bone and use the on-screen vertical delta
                    // as the circle radius. auto-scales with range — close enemy = big
                    // circle, far = small — instead of a fixed pixel size that's wrong at
                    // every distance but one. floor at headDotRadius so far targets still
                    // render a visible dot.
                    Vector3 aboveHead = { headPos.x, headPos.y, headPos.z + 4.0f };
                    Vector3 aboveHeadScreen;
                    if (Utils::WorldToScreen(aboveHead, aboveHeadScreen, viewMatrix, screenW, screenH)) {
                        float r = std::fabs(aboveHeadScreen.y - rec.screenHead.y);
                        rec.headRadius = (r > g_Config.headDotRadius) ? r : g_Config.headDotRadius;
                    } else {
                        rec.headRadius = g_Config.headDotRadius;
                    }

                    // bones (read + project, no draw)
                    rec.hasBones = false;
                    if ((g_Config.espSkeleton || g_Config.espChams) && bm && bm >= 0x10000) {
                        Vector3 bonesWorld[g_BoneCount];
                        bool allFail = true;
                        for (int bn = 0; bn < g_BoneCount; bn++) {
                            bonesWorld[bn] = mem.Read<Matrix3x4>(bm + bn * 48).GetOrigin();
                            rec.boneValid[bn] = Utils::WorldToScreen(bonesWorld[bn], rec.bonesScreen[bn], viewMatrix, screenW, screenH);
                            if (rec.boneValid[bn]) allFail = false;
                        }
                        rec.hasBones = !allFail;
                    }

                    if (f3Pressed) {
                        std::cout << "Idx: " << i << " | HP: " << hp << " | Team: " << entityTeam
                                  << " | Dist: " << (int)(g_Config.distanceInMetres ? dist / 39.37f : dist)
                                  << (g_Config.distanceInMetres ? "m" : "u") << std::endl;
                    }

                    records.push_back(rec);
                }

                // ---- PASS 2: DRAW (pure ImGui, zero RPM — frames can't split) ----
                auto drawList = ImGui::GetForegroundDrawList();
                for (const auto& rec : records) {
                    int alpha = rec.alpha;
                    ImU32 baseColor = Core::ColorFor(g_Config, rec.isMate, alpha);
                    ImU32 skelColor = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colSkeleton.x, g_Config.colSkeleton.y, g_Config.colSkeleton.z,
                               (g_Config.colSkeleton.w * g_Config.alphaSkeleton * alpha) / 255.0f));
                    ImU32 headColor = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colHeadDot.x, g_Config.colHeadDot.y, g_Config.colHeadDot.z,
                               (g_Config.colHeadDot.w * alpha) / 255.0f));
                    ImU32 nameColor = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colName.x, g_Config.colName.y, g_Config.colName.z,
                               (g_Config.colName.w * alpha) / 255.0f));
                    ImU32 hpColor   = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colHpText.x, g_Config.colHpText.y, g_Config.colHpText.z,
                               (g_Config.colHpText.w * alpha) / 255.0f));
                    ImU32 snapColor = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_Config.colSnapline.x, g_Config.colSnapline.y, g_Config.colSnapline.z,
                               (g_Config.colSnapline.w * alpha) / 255.0f));

                    const Vector3& screenFeet = rec.screenFeet;
                    const Vector3& screenHead = rec.screenHead;

                    // skeleton + chams (from cached projected bones)
                    if (rec.hasBones && (g_Config.espSkeleton || g_Config.espChams)) {
                        if (g_Config.espSkeleton) {
                            for (int b = 0; b < g_SkeletonLinks; b++) {
                                int f = g_Skeleton[b].from, t = g_Skeleton[b].to;
                                if (!rec.boneValid[f] || !rec.boneValid[t]) continue;
                                const Vector3& a = rec.bonesScreen[f];
                                const Vector3& c = rec.bonesScreen[t];
                                drawList->AddLine({ a.x, a.y }, { c.x, c.y }, skelColor, 1.5f);
                            }
                        }
                        if (g_Config.espChams) {
                            ImVec4 base = rec.isMate ? g_Config.colTeam : g_Config.colEnemy;
                            // apply the chams alpha multiplier on top of the fade alpha
                            base.w *= g_Config.alphaChams;
                            ImVec4 corev((std::min)(1.0f, base.x + 0.5f),
                                         (std::min)(1.0f, base.y + 0.5f),
                                         (std::min)(1.0f, base.z + 0.5f),
                                         (base.w * alpha) / 255.0f);
                            ImU32 coreColor = ImGui::ColorConvertFloat4ToU32(corev);
                            // chams tube color: same base hue, alpha scaled by the chams multiplier
                            ImU32 chamsBase = ImGui::ColorConvertFloat4ToU32(
                                ImVec4(base.x, base.y, base.z, (base.w * alpha) / 255.0f));
                            DrawChams(drawList, rec.bonesScreen, rec.boneValid, chamsBase, coreColor,
                                      g_Config.chamsThickness, g_Config.chamsCore, g_Config.chamsJointRad);
                        }
                    }

                    if (g_Config.espHeadDot) {
                        // the dot is lifted to the visual head center (bone 14 is the pivot)
                        // and sized to the actual head via the range-scaled radius computed
                        // in the read pass. close enemy = head-sized circle, far = small dot.
                        float bodyH = std::abs(screenFeet.y - screenHead.y);
                        float lift = bodyH * g_Config.headLift;
                        float hx = screenHead.x, hy = screenHead.y - lift;
                        float r = rec.headRadius;
                        drawList->AddCircleFilled({ hx, hy }, r, headColor);
                        drawList->AddCircle({ hx, hy }, r + 1.0f, ImColor(0, 0, 0, alpha / 2), 0, 1.5f);
                    }
                    if (g_Config.espHpText) {
                        char buf[16]; snprintf(buf, sizeof(buf), "%d", rec.hp);
                        drawList->AddText({ screenHead.x - 8.0f, screenHead.y - 14.0f }, hpColor, buf);
                    }
                    if (g_Config.espName && rec.hasName) {
                        drawList->AddText({ screenHead.x - 30.0f, screenHead.y - 26.0f }, nameColor, rec.name);
                    }
                    if (g_Config.espDistance) {
                        float dDisp = g_Config.distanceInMetres ? rec.dist / 39.37f : rec.dist;
                        const char* unit = g_Config.distanceInMetres ? "m" : "u";
                        char dbuf[24];
                        snprintf(dbuf, sizeof(dbuf), "%d%s", (int)dDisp, unit);
                        drawList->AddText({ screenHead.x - 12.0f, screenFeet.y + 2.0f }, nameColor, dbuf);
                    }
                    float height = std::abs(screenFeet.y - screenHead.y);
                    float width = height / 2.1f;
                    if (g_Config.espBox) {
                        DrawBox(drawList, { screenHead.x, screenHead.y }, { screenFeet.x, screenFeet.y },
                                width, baseColor, g_Config.boxStyle, g_Config.boxThickness);
                    }
                    if (g_Config.espHealth) {
                        DrawHealthBar(screenHead.x - width / 2.0f, screenHead.y, height, rec.hp, baseColor);
                    }
                    if (g_Config.espSnaplines) {
                        drawList->AddLine({ (float)screenW / 2.0f, (float)screenH },
                                          { screenFeet.x, screenFeet.y }, snapColor, 1.0f);
                    }
                }
            }
            skipEsp: ;
        }

        overlay.EndFrame();
        Sleep(1);
    }

    g_Config.Save();   // final autosave
    return 0;
}
