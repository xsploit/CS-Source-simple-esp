// anti_recoil.cpp — old-school pixel-based recoil compensation.
//
// PIXEL mode pipeline (per Update() call):
//   1. fire gate: is the fire key held? if not, decay the smoother and return.
//   2. capture: BitBlt a strip_width x strip_height region centered on the
//      crosshair into a DIB section. convert the center column to grayscale
//      (top-down) -> cur[H].
//   3. diff: SAD-search the vertical offset between cur[] and prev[] over
//      [-search_range, +search_range]. the argmin is the detected motion.
//      optional sub-pixel: parabolic fit around the min.
//   4. sign: the world shifts DOWN as the view kicks UP, so cur[y] ≈ prev[y-d]
//      at d = kick. best_d = the upward kick in pixels.
//   5. humanize: scale = sensitivity * rand(0.82, 0.96) [under-compensate],
//      EMA-smooth, add ±1px jitter, clamp to [-max, +max], floor at 0 (don't
//      push the camera up — recoil only goes up).
//   6. compensate: SendInput a relative downward move (dy = compensation).
//
// FIXED mode: when fire key held, dy = fixed_rate (+ humanization jitter).
// the macro fallback — no capture, no diff, just a steady pull.
//
// humanization reasoning (the part that makes it look real):
//   - under-compensation (0.82-0.96x, randomized): a human never cancels
//     recoil perfectly. the residual climb is what makes it look manual.
//   - EMA smoothing: a human's pull isn't frame-perfect; it lags + curves.
//   - ±1px jitter: kills the perfectly-smooth curve that's a heuristic tell.
//   - per-frame cap: prevents the snap-correct that no human does.

#define NOMINMAX
#include "Utils/anti_recoil.h"

#include <windows.h>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

namespace ar {

// ---- tiny xorshift RNG (fast, no <random> overhead in a hot loop) ----
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9e3779b97f4a7c15ULL) {}
    uint32_t next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return (uint32_t)(s >> 32);
    }
    float frange(float lo, float hi) {
        return lo + (hi - lo) * (next() / 4294967296.0f);
    }
    int irange(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
};

// grayscale from a 24-bit BGR DIB pixel (bottom-up buffer).
static inline uint8_t lum(const uint8_t* p) {
    // BGR -> Y = 0.299R + 0.587G + 0.114B (use the BGR order from the DIB)
    return (uint8_t)(0.114f * p[0] + 0.587f * p[1] + 0.299f * p[2]);
}

struct AntiRecoil::Impl {
    Config cfg;
    bool   enabled = true;
    Rng    rng;

    // capture buffers (grayscale center column, top-down)
    std::vector<uint8_t> cur;
    std::vector<uint8_t> prev;
    bool                 have_prev = false;

    // DIB section for the capture (reused across frames)
    HDC      memDC   = nullptr;
    HBITMAP  dib     = nullptr;
    void*    bits    = nullptr;
    int      dibW   = 0;
    int      dibH   = 0;
    HDC      screenDC = nullptr;

    // smoother state
    float    smoothed = 0.0f;

    // diagnostics
    int      last_kick = 0;
    int      last_comp = 0;
    bool     firing    = false;

    explicit Impl(const Config& c) : cfg(c), rng(0xC0FFEEULL) {
        // resolve crosshair defaults to screen center
        if (cfg.crosshair_x < 0 || cfg.crosshair_y < 0) {
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            if (cfg.crosshair_x < 0) cfg.crosshair_x = sw / 2;
            if (cfg.crosshair_y < 0) cfg.crosshair_y = sh / 2;
        }
        cur.assign(cfg.strip_height, 0);
        prev.assign(cfg.strip_height, 0);
        setupDib();
    }

    ~Impl() {
        if (dib)    DeleteObject(dib);
        if (memDC)  DeleteDC(memDC);
        if (screenDC) ReleaseDC((HWND)cfg.game_hwnd, screenDC);
    }

    void setupDib() {
        screenDC = GetDC((HWND)cfg.game_hwnd);
        memDC    = CreateCompatibleDC(screenDC);
        dibW     = cfg.strip_width;
        dibH     = cfg.strip_height;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = dibW;
        bmi.bmiHeader.biHeight      = dibH;   // positive = bottom-up
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 24;
        bmi.bmiHeader.biCompression = BI_RGB;
        dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        SelectObject(memDC, dib);
    }

    // capture the strip into cur[] (grayscale center column, top-down).
    void capture() {
        int cx = cfg.crosshair_x;
        int cy = cfg.crosshair_y;
        int x0 = cx - dibW / 2;
        int y0 = cy - dibH / 2;
        if (!BitBlt(memDC, 0, 0, dibW, dibH, screenDC, x0, y0, SRCCOPY))
            return;  // capture failed — leave cur as-is
        // the DIB is 24-bit BGR, bottom-up, row stride padded to 4 bytes.
        int stride = ((dibW * 3 + 3) & ~3);
        uint8_t* base = (uint8_t*)bits;
        int centerCol = dibW / 2;
        for (int y = 0; y < dibH; y++) {
            // bottom-up: buffer row 0 = image bottom = screen y0 + dibH - 1.
            // we want top-down in cur[], so flip the row index.
            int bufRow = dibH - 1 - y;
            uint8_t* px = base + bufRow * stride + centerCol * 3;
            cur[y] = lum(px);
        }
    }

    // SAD search: find d that minimizes sum_y |cur[y] - prev[y - d]|.
    // returns the best integer d, and (if subpixel) refines it.
    float searchKick() {
        int H = dibH;
        int R = cfg.search_range;
        int bestD = 0;
        int bestSad = INT_MAX;
        // search d in [-R, +R]. d > 0 = world shifted down = view kicked up.
        for (int d = -R; d <= R; d++) {
            int sad = 0;
            for (int y = 0; y < H; y++) {
                int yp = y - d;            // prev index
                if (yp < 0 || yp >= H) continue;
                int diff = (int)cur[y] - (int)prev[yp];
                sad += diff < 0 ? -diff : diff;
            }
            if (sad < bestSad) { bestSad = sad; bestD = d; }
        }
        if (!cfg.subpixel || bestD <= -R || bestD >= R)
            return (float)bestD;
        // parabolic sub-pixel: fit y = a(d-b)^2 + c around the min.
        int sm = bestSad;
        int sL = INT_MAX, sR = INT_MAX;
        if (bestD - 1 >= -R) {
            int s = 0; for (int y = 0; y < H; y++) { int yp = y - (bestD-1); if (yp<0||yp>=H) continue; int df=(int)cur[y]-(int)prev[yp]; s += df<0?-df:df; } sL = s;
        }
        if (bestD + 1 <= R) {
            int s = 0; for (int y = 0; y < H; y++) { int yp = y - (bestD+1); if (yp<0||yp>=H) continue; int df=(int)cur[y]-(int)prev[yp]; s += df<0?-df:df; } sR = s;
        }
        float denom = (float)(sL - 2 * sm + sR);
        if (fabsf(denom) < 0.001f) return (float)bestD;
        float offset = 0.5f * (float)(sL - sR) / denom;
        return (float)bestD + offset;
    }

    void compensate(int dy) {
        INPUT inp = {};
        inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = MOUSEEVENTF_MOVE;  // relative move
        inp.mi.dx = 0;
        inp.mi.dy = dy;
        SendInput(1, &inp, sizeof(INPUT));
        last_comp = dy;
    }

    void update() {
        if (!enabled) return;
        firing = (GetAsyncKeyState(cfg.fire_key) & 0x8000) != 0;
        if (!firing) {
            // not firing — decay the smoother so the next burst starts clean.
            smoothed *= 0.5f;
            last_kick = 0;
            last_comp = 0;
            return;
        }

        if (cfg.mode == Mode::FIXED) {
            // blind pull: fixed_rate down, with humanization jitter.
            int dy = cfg.fixed_rate;
            if (cfg.humanize)
                dy += rng.irange(-1, 1);
            dy = (int)(dy * cfg.sensitivity);
            dy = (std::clamp)(dy, 0, cfg.max_compensation);
            compensate(dy);
            last_kick = dy;  // not a real "kick" but useful for the debug panel
            return;
        }

        // PIXEL mode: capture + diff + compensate.
        capture();

        if (!have_prev) {
            // first frame — just stash, no compensation.
            std::swap(cur, prev);
            have_prev = true;
            return;
        }

        float kick = searchKick();
        last_kick = (int)kick;

        // humanize: under-compensate (82-96% randomized), EMA-smooth, jitter.
        float scale = cfg.sensitivity;
        if (cfg.humanize)
            scale *= rng.frange(0.82f, 0.96f);
        float raw = kick * scale;
        smoothed = smoothed * (1.0f - cfg.smoothing) + raw * cfg.smoothing;

        int dy = (int)roundf(smoothed);
        if (cfg.humanize)
            dy += rng.irange(-1, 1);
        // recoil only goes up → compensation only goes down (dy >= 0).
        // floor at 0 so pixel noise near zero doesn't push the camera up.
        if (dy < 0) dy = 0;
        dy = (std::min)(dy, cfg.max_compensation);

        if (dy > 0) compensate(dy);
        else last_comp = 0;

        // roll the buffers: this frame's cur becomes next frame's prev.
        std::swap(cur, prev);
    }
};

// ---- public API ----
AntiRecoil::AntiRecoil(const Config& cfg) : _impl(new Impl(cfg)) {}
AntiRecoil::~AntiRecoil() { delete _impl; }
void AntiRecoil::SetEnabled(bool on) { _impl->enabled = on; }
void AntiRecoil::SetConfig(const Config& cfg) {
    _impl->cfg = cfg;
    // re-resolve crosshair defaults + re-alloc buffers if the strip size changed
    if (cfg.crosshair_x < 0 || cfg.crosshair_y < 0) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        _impl->cfg.crosshair_x = (cfg.crosshair_x < 0) ? sw/2 : cfg.crosshair_x;
        _impl->cfg.crosshair_y = (cfg.crosshair_y < 0) ? sh/2 : cfg.crosshair_y;
    }
    if ((int)_impl->cur.size() != cfg.strip_height) {
        _impl->cur.assign(cfg.strip_height, 0);
        _impl->prev.assign(cfg.strip_height, 0);
        _impl->have_prev = false;
    }
}
const Config& AntiRecoil::GetConfig() const { return _impl->cfg; }
void AntiRecoil::Update() { _impl->update(); }
int  AntiRecoil::LastKick() const { return _impl->last_kick; }
int  AntiRecoil::LastCompensation() const { return _impl->last_comp; }
bool AntiRecoil::IsFiring() const { return _impl->firing; }
bool AntiRecoil::IsEnabled() const { return _impl->enabled; }

} // namespace ar
