#include "../../include/Core/Config.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

// tiny JSON serializer/parser — flat object of known keys. not a general JSON
// parser; we control both ends and the shape is fixed. ImVec4 written as
// [r,g,b,a] arrays. unknown keys on load are ignored (forward-compat).

namespace Core {

// ---- small helpers ----
static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

static std::string VecToJson(const ImVec4& v) {
    char buf[80];
    snprintf(buf, sizeof(buf), "[%.4f, %.4f, %.4f, %.4f]", v.x, v.y, v.z, v.w);
    return buf;
}

static bool ParseVec(const std::string& s, ImVec4& out) {
    // expects "[r, g, b, a]"
    if (s.size() < 9 || s.front() != '[' || s.back() != ']') return false;
    std::string inner = s.substr(1, s.size() - 2);
    float f[4] = {0, 0, 0, 0};
    int idx = 0;
    std::stringstream ss(inner);
    std::string tok;
    while (std::getline(ss, tok, ',') && idx < 4) {
        f[idx++] = (float)atof(Trim(tok).c_str());
    }
    if (idx != 4) return false;
    out = ImVec4(f[0], f[1], f[2], f[3]);
    return true;
}

// ---- Save ----
void Config::Save() const {
    std::ofstream f(path);
    if (!f) return;
    f << "{\n";
    f << "  \"espEnabled\": "        << (espEnabled ? "true" : "false")        << ",\n";
    f << "  \"espBox\": "            << (espBox ? "true" : "false")            << ",\n";
    f << "  \"espHeadDot\": "        << (espHeadDot ? "true" : "false")        << ",\n";
    f << "  \"espHealth\": "         << (espHealth ? "true" : "false")         << ",\n";
    f << "  \"espTeam\": "           << (espTeam ? "true" : "false")           << ",\n";
    f << "  \"espSnaplines\": "      << (espSnaplines ? "true" : "false")      << ",\n";
    f << "  \"espSkeleton\": "       << (espSkeleton ? "true" : "false")       << ",\n";
    f << "  \"espName\": "           << (espName ? "true" : "false")           << ",\n";
    f << "  \"espHpText\": "         << (espHpText ? "true" : "false")         << ",\n";
    f << "  \"espDistance\": "       << (espDistance ? "true" : "false")       << ",\n";
    f << "  \"bhopEnabled\": "       << (bhopEnabled ? "true" : "false")       << ",\n";
    f << "  \"menuOpen\": "          << (menuOpen ? "true" : "false")          << ",\n";
    f << "  \"boxStyle\": "          << boxStyle                              << ",\n";
    f << "  \"boxThickness\": "      << boxThickness                          << ",\n";
    f << "  \"distanceInMetres\": "  << (distanceInMetres ? "true" : "false")  << ",\n";
    f << "  \"maxFadeDist\": "       << maxFadeDist                           << ",\n";
    f << "  \"chamsThickness\": "    << chamsThickness                        << ",\n";
    f << "  \"chamsCore\": "         << chamsCore                             << ",\n";
    f << "  \"chamsJointRad\": "     << chamsJointRad                         << ",\n";
    f << "  \"headLift\": "          << headLift                              << ",\n";
    f << "  \"headDotRadius\": "     << headDotRadius                         << ",\n";
    f << "  \"staleFrames\": "       << staleFrames                           << ",\n";
    f << "  \"colEnemy\": "          << VecToJson(colEnemy)                   << ",\n";
    f << "  \"colTeam\": "           << VecToJson(colTeam)                    << ",\n";
    f << "  \"colSkeleton\": "       << VecToJson(colSkeleton)                << ",\n";
    f << "  \"colHeadDot\": "        << VecToJson(colHeadDot)                 << ",\n";
    f << "  \"colName\": "           << VecToJson(colName)                    << ",\n";
    f << "  \"colHpText\": "         << VecToJson(colHpText)                  << ",\n";
    f << "  \"colSnapline\": "       << VecToJson(colSnapline)                << "\n";
    f << "}\n";
}

// ---- Load ----
// tolerant: missing keys keep defaults, malformed lines are skipped.
void Config::Load() {
    std::ifstream f(path);
    if (!f) return;            // first run — defaults are fine

    std::string line;
    while (std::getline(f, line)) {
        std::string s = Trim(line);
        if (s.empty() || s == "{" || s == "}") continue;
        // strip trailing comma
        if (!s.empty() && s.back() == ',') s.pop_back();

        // split on first ':'
        size_t colon = s.find(':');
        if (colon == std::string::npos) continue;
        std::string key = Trim(s.substr(0, colon));
        std::string val = Trim(s.substr(colon + 1));

        // strip quotes from key
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
            key = key.substr(1, key.size() - 2);

        // bool
        if (val == "true" || val == "false") {
            bool b = (val == "true");
            if      (key == "espEnabled")        espEnabled = b;
            else if (key == "espBox")            espBox = b;
            else if (key == "espHeadDot")        espHeadDot = b;
            else if (key == "espHealth")         espHealth = b;
            else if (key == "espTeam")           espTeam = b;
            else if (key == "espSnaplines")      espSnaplines = b;
            else if (key == "espSkeleton")       espSkeleton = b;
            else if (key == "espName")           espName = b;
            else if (key == "espHpText")         espHpText = b;
            else if (key == "espDistance")       espDistance = b;
            else if (key == "bhopEnabled")       bhopEnabled = b;
            else if (key == "menuOpen")          menuOpen = b;
            else if (key == "distanceInMetres")  distanceInMetres = b;
            continue;
        }

        // ImVec4 (array form)
        if (!val.empty() && val.front() == '[') {
            ImVec4 v;
            if (!ParseVec(val, v)) continue;
            if      (key == "colEnemy")     colEnemy = v;
            else if (key == "colTeam")      colTeam = v;
            else if (key == "colSkeleton")  colSkeleton = v;
            else if (key == "colHeadDot")   colHeadDot = v;
            else if (key == "colName")      colName = v;
            else if (key == "colHpText")    colHpText = v;
            else if (key == "colSnapline")  colSnapline = v;
            continue;
        }

        // numeric
        if (key == "boxStyle")     boxStyle     = atoi(val.c_str());
        else if (key == "boxThickness")  boxThickness = (float)atof(val.c_str());
        else if (key == "maxFadeDist")   maxFadeDist  = (float)atof(val.c_str());
        else if (key == "chamsThickness") chamsThickness = (float)atof(val.c_str());
        else if (key == "chamsCore")      chamsCore      = (float)atof(val.c_str());
        else if (key == "chamsJointRad")  chamsJointRad  = (float)atof(val.c_str());
        else if (key == "headLift")       headLift       = (float)atof(val.c_str());
        else if (key == "headDotRadius")  headDotRadius  = (float)atof(val.c_str());
        else if (key == "staleFrames")    staleFrames    = atoi(val.c_str());
    }
}

} // namespace Core
