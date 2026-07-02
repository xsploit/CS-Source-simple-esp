// anti_recoil.h — old-school pixel-based recoil compensation.
//
// detects muzzle climb via a crosshair-region frame diff (BitBlt a thin
// vertical strip, SAD-search the vertical offset between frames), then
// pulls the mouse down to cancel the kick. no memory reads — pure screen
// + mouse. humanized so the compensation curve isn't a robotic 1:1 mirror.
//
// two modes:
//   PIXEL — adaptive. captures a strip at the crosshair, diffs consecutive
//           frames, finds the vertical motion via sum-of-absolute-differences,
//           compensates proportionally. the smart mode.
//   FIXED — blind pull. when the fire key is held, drags down at a fixed
//           rate. the macro fallback (use when the crosshair region is
//           unreadable — scope, fullscreen effect, etc.).
//
// wire into the overlay's render loop: call Update() every frame. it gates
// on the fire key (default LMB) + an enable toggle.

#pragma once
#include <cstdint>
#include <string>

namespace ar {

enum class Mode { PIXEL, FIXED };

struct Config {
    Mode       mode             = Mode::PIXEL;
    int        fire_key         = 0x01;   // VK_LBUTTON; the fire detection key
    float      sensitivity      = 1.0f;   // overall scale (1.0 = full, 0.8 = under)
    float      smoothing        = 0.4f;   // EMA alpha (0 = very smooth, 1 = reactive)
    int        strip_height     = 64;     // capture strip height (px)
    int        strip_width      = 3;      // capture strip width (px, odd for centering)
    int        search_range     = 20;     // max kick to search (px each direction)
    int        max_compensation = 8;      // per-frame downward cap (px)
    bool       subpixel         = true;   // parabolic sub-pixel refinement on the SAD min
    bool       humanize         = true;   // jitter + randomized under-compensation
    // fixed-pull mode (MODE_FIXED):
    int        fixed_rate       = 3;      // px/frame downward when fire key held
    // capture target:
    void*      game_hwnd        = nullptr;// HWND of the game window (nullptr = desktop DC)
    // crosshair location (screen coords). defaults are set in the ctor to
    // the screen center; override for games with an offset crosshair.
    int        crosshair_x      = -1;     // -1 = auto (screen center)
    int        crosshair_y      = -1;
};

class AntiRecoil {
public:
    explicit AntiRecoil(const Config& cfg = {});
    ~AntiRecoil();

    void SetEnabled(bool on);
    void SetConfig(const Config& cfg);
    const Config& GetConfig() const;

    // call every frame from the render loop. captures + diffs + compensates.
    // cheap: a 3x64 strip SAD over 41 candidates is ~8k ops, plus one BitBlt
    // of a tiny region (~0.3ms). safe to run at 60-144fps.
    void Update();

    // diagnostics (for an ImGui debug panel)
    int  LastKick() const;          // last detected upward kick (px)
    int  LastCompensation() const;  // last applied downward dy (px)
    bool IsFiring() const;          // fire key currently held?
    bool IsEnabled() const;

private:
    struct Impl;
    Impl* _impl;
};

} // namespace ar
