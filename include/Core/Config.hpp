#pragma once
// Config.hpp — persistent ESP configuration.
//
// holds every toggle + color the user can change at runtime, and serializes
// them to/from config.json sitting next to the exe. JSON is hand-rolled
// (no dependency) — the shape is flat enough that key:value parsing is enough,
// and we control both ends so we don't need a real JSON library.
//
// colors are ImVec4 (float r,g,b,a in 0..1) because that's what ImGui's
// ColorEdit4 consumes and what the draw list accepts. helpers convert to
// ImU32 (the packed uint32 the draw primitives actually take) on demand.

#include <Windows.h>
#include <string>
#include "imgui.h"

namespace Core {

struct Config {
    // ---- ESP master + per-feature toggles ----
    bool espEnabled   = true;
    bool espBox       = false;
    bool espHeadDot   = true;
    bool espHealth    = false;
    bool espTeam      = false;     // show teammates
    bool espSnaplines = false;
    bool espSkeleton  = true;
    bool espChams     = false;     // bone-glow chams (thick glowing body shape)
    bool espName      = false;
    bool espHpText    = true;
    bool bhopEnabled  = false;     // walls-only build: off + writes disabled
    bool menuOpen     = true;

    // ---- rendering options ----
    int  boxStyle      = 0;        // 0=outline, 1=filled, 2=corner
    float boxThickness = 1.5f;
    bool distanceInMetres = true;  // false = raw Hammer units
    float maxFadeDist   = 2000.0f; // beyond this, alpha floors at min

    // ---- chams tuning ----
    float chamsThickness = 10.0f;  // outer glow tube radius (px)
    float chamsCore      = 3.0f;   // bright inner core line thickness
    float chamsJointRad  = 5.0f;   // filled circle radius at major joints

    // ---- entity filter tuning ----
    // a player frozen in place for this many frames is treated as stale
    // (disconnected/dead-but-not-flagged) and skipped. at ~60fps overlay rate,
    // 180 frames = 3 seconds — long enough that real campers don't vanish.
    int   staleFrames    = 180;

    // ---- colors (float 0..1, alpha included) ----
    ImVec4 colEnemy     = ImVec4(1.00f, 0.00f, 0.00f, 1.0f); // red
    ImVec4 colTeam      = ImVec4(0.00f, 1.00f, 0.00f, 1.0f); // green
    ImVec4 colSkeleton  = ImVec4(1.00f, 1.00f, 1.00f, 0.85f);
    ImVec4 colHeadDot   = ImVec4(1.00f, 0.20f, 0.20f, 1.0f);
    ImVec4 colName      = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    ImVec4 colHpText    = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    ImVec4 colSnapline  = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);

    // ---- persistence ----
    // path defaults to "config.json" next to the exe; resolved at load time.
    std::string path = "config.json";

    void Load();   // no-op (defaults kept) if the file is missing/unreadable
    void Save() const;
};

// helper: pick the configured color for an entity, apply distance fade to alpha
inline ImU32 ColorFor(const Config& cfg, bool isTeammate, int alpha255) {
    const ImVec4& base = isTeammate ? cfg.colTeam : cfg.colEnemy;
    int a = (int)(base.w * alpha255);
    if (a < 0) a = 0; if (a > 255) a = 255;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(base.x, base.y, base.z, a / 255.0f));
}

} // namespace Core

extern Core::Config g_Config;
