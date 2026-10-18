#include "auth.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cmath>
#include <string>
#include <fstream>
#include <filesystem>

namespace {
    bool g_authenticated = false;
    bool g_authFailed = false;
    char g_status[128] = "Enter your license key";
    char g_license[64] = "";
    char g_licenseDuration[128] = "Lifetime (Dev)";
    bool g_hwidLocked = true;
}

static std::string GetLicensePath() {
    const char* appdata = std::getenv("APPDATA");
    std::string base = appdata ? appdata : ".";
#ifdef UNBRANDED
    auto p = std::filesystem::path(base) / "Overlay";
#else
    auto p = std::filesystem::path(base) / "Takepeek-Wardogs";
#endif
    std::filesystem::create_directories(p);
    p /= "license.key";
    return p.string();
}

static void SaveLicenseToFile(const char* license) {
    try { std::ofstream ofs(GetLicensePath(), std::ios::trunc); if (ofs) ofs << license; } catch(...) {}
}

static void LoadLicenseFromFile(char* out, size_t sz) {
    try {
        std::ifstream ifs(GetLicensePath());
        if (ifs) { std::string s; std::getline(ifs, s); if (!s.empty()) strncpy_s(out, sz, s.c_str(), _TRUNCATE); }
    } catch(...) {}
}

void InitAuth() {
    LoadLicenseFromFile(g_license, sizeof(g_license));
    if (g_license[0] != '\0') { g_authenticated = true; sprintf_s(g_status, "License active (cached)"); }
    else sprintf_s(g_status, "Enter your license key");
}

bool IsAuthenticated() { return g_authenticated; }
const char* GetAuthStatus() { return g_status; }
const char* GetLicenseDuration() { return g_licenseDuration; }
bool IsLicenseHwidLocked() { return g_hwidLocked; }

// ============================================================================
// Brand colors — accent controlled by UNBRANDED macro
// ============================================================================
namespace WD {
    constexpr ImU32 Bg        = IM_COL32(22, 22, 28, 255);
    constexpr ImU32 Panel     = IM_COL32(18, 18, 24, 255);
    constexpr ImU32 Border    = IM_COL32(48, 48, 60, 255);
    constexpr ImU32 Accent    = ACCENT_COL(255);
    constexpr ImU32 AccentDim = ACCENT_COL(100);
    constexpr ImU32 TextH     = IM_COL32(240, 240, 244, 255);
    constexpr ImU32 TextN     = IM_COL32(180, 180, 190, 255);
    constexpr ImU32 TextD     = IM_COL32(100, 100, 112, 255);
    constexpr ImU32 Green     = IM_COL32(60, 200, 90, 255);
    constexpr ImU32 Red       = IM_COL32(220, 50, 50, 255);
    constexpr ImU32 Input     = IM_COL32(38, 38, 50, 255);
    constexpr ImU32 InputHov  = IM_COL32(44, 44, 58, 255);
    inline ImU32 AccentA(int a) { return ACCENT_COL(a); }
}

// ============================================================================
// Animated reticle logo
// ============================================================================
#ifndef UNBRANDED
static void DrawReticleAnimated(ImDrawList* dl, ImVec2 center, float size, float time) {
    float half = size * 0.5f, pad = size * 0.08f, arm = size * 0.22f;
    float thick = 2.4f, tickLen = size * 0.16f, gap = size * 0.05f;
    float phase = fmodf(time, 3.4f) / 3.4f;
    float cornerA = phase < 0.3f ? phase / 0.3f : 1.0f;
    ImU32 cornerCol = IM_COL32(220, 220, 228, (int)(cornerA * 255));
    float tickGrow = phase < 0.2f ? 0.f : (phase < 0.6f ? (phase - 0.2f) / 0.4f : 1.0f);
    ImU32 tickCol = IM_COL32(180, 180, 192, (int)(tickGrow * 255));
    float greenGrow = phase < 0.4f ? 0.f : (phase < 0.8f ? (phase - 0.4f) / 0.4f : 1.0f);
    ImU32 greenCol = ACCENT_COL( (int)(greenGrow * 255));
    float dotScale = phase < 0.5f ? 0.f : (phase < 0.7f ? (phase - 0.5f) / 0.2f : 1.0f);
    float L = center.x - half, T = center.y - half, R = center.x + half, B = center.y + half;
    dl->AddLine({L+pad,T+pad},{L+pad+arm,T+pad},cornerCol,thick); dl->AddLine({L+pad,T+pad},{L+pad,T+pad+arm},cornerCol,thick);
    dl->AddLine({R-pad,T+pad},{R-pad-arm,T+pad},cornerCol,thick); dl->AddLine({R-pad,T+pad},{R-pad,T+pad+arm},cornerCol,thick);
    dl->AddLine({L+pad,B-pad},{L+pad+arm,B-pad},cornerCol,thick); dl->AddLine({L+pad,B-pad},{L+pad,B-pad-arm},cornerCol,thick);
    dl->AddLine({R-pad,B-pad},{R-pad-arm,B-pad},cornerCol,thick); dl->AddLine({R-pad,B-pad},{R-pad,B-pad-arm},cornerCol,thick);
    float cx = center.x, cy = center.y, tl = tickLen * tickGrow;
    if (tl > 1.f) {
        dl->AddLine({cx,cy-gap-tl},{cx,cy-gap},tickCol,thick);
        dl->AddLine({cx,cy+gap},{cx,cy+gap+tl},tickCol,thick);
        dl->AddLine({cx-gap-tl,cy},{cx-gap,cy},tickCol,thick);
    }
    float rl = (tickLen * 1.6f) * greenGrow;
    if (rl > 1.f) dl->AddLine({cx+gap,cy},{cx+gap+rl,cy},greenCol,thick+0.5f);
    if (dotScale > 0.01f) { float ds = 3.5f*dotScale; dl->AddRectFilled({cx-ds,cy-ds},{cx+ds,cy+ds},greenCol,1.f); }
    float pulse = 0.5f + 0.5f * sinf(time * 2.0f);
    int glowA = (int)(12.0f + pulse * 18.0f);
    dl->AddCircle(center, half * 0.85f, WD::AccentA(glowA), 32, 1.2f);
}

static void DrawReticleStatic(ImDrawList* dl, ImVec2 center, float size) {
    float half = size * 0.5f, pad = size * 0.08f, arm = size * 0.22f;
    float thick = 2.0f, tickLen = size * 0.16f, gap = size * 0.05f;
    ImU32 cCol = IM_COL32(200, 200, 210, 255), tCol = IM_COL32(160, 160, 172, 255);
    float L = center.x - half, T = center.y - half, R = center.x + half, B = center.y + half;
    float cx = center.x, cy = center.y;
    dl->AddLine({L+pad,T+pad},{L+pad+arm,T+pad},cCol,thick); dl->AddLine({L+pad,T+pad},{L+pad,T+pad+arm},cCol,thick);
    dl->AddLine({R-pad,T+pad},{R-pad-arm,T+pad},cCol,thick); dl->AddLine({R-pad,T+pad},{R-pad,T+pad+arm},cCol,thick);
    dl->AddLine({L+pad,B-pad},{L+pad+arm,B-pad},cCol,thick); dl->AddLine({L+pad,B-pad},{L+pad,B-pad-arm},cCol,thick);
    dl->AddLine({R-pad,B-pad},{R-pad-arm,B-pad},cCol,thick); dl->AddLine({R-pad,B-pad},{R-pad,B-pad-arm},cCol,thick);
    dl->AddLine({cx,cy-gap-tickLen},{cx,cy-gap},tCol,thick);
    dl->AddLine({cx,cy+gap},{cx,cy+gap+tickLen},tCol,thick);
    dl->AddLine({cx-gap-tickLen,cy},{cx-gap,cy},tCol,thick);
    dl->AddLine({cx+gap,cy},{cx+gap+tickLen*1.6f,cy},WD::Accent,thick+0.5f);
    dl->AddRectFilled({cx-3,cy-3},{cx+3,cy+3},WD::Accent,1.f);
}
#endif // !UNBRANDED

void DrawTakePeekBrand(ImDrawList* dl, ImVec2 pos, float s, bool darkText) {
#ifndef UNBRANDED
    DrawReticleStatic(dl, {pos.x + 18*s, pos.y + 18*s}, 36*s);
    ImFont* bf = ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont();
    ImU32 wordCol = darkText ? IM_COL32(24, 24, 28, 255) : WD::TextH;
    dl->AddText(bf, 20.f*s, {pos.x+46*s,pos.y+6*s}, wordCol, "TAKEPEEK");
    float bx = pos.x + 168.f*s, by = pos.y + 8.f*s;
    dl->AddRect({bx,by},{bx+28.f*s,by+18.f*s}, WD::Accent, 3.f, 0, 1.5f);
    dl->AddText(ImGui::GetFont(), 11.f*s, {bx+5.f*s,by+2.5f*s}, WD::Accent, "WD");
#else
    (void)dl; (void)pos; (void)s; (void)darkText;
#endif
}

// ============================================================================
// Auth Window — dark split-panel design matching CS2
// ============================================================================
void DrawAuthWindow() {
    if (g_authenticated) return;

    static float animTime = 0.f;
    animTime += ImGui::GetIO().DeltaTime;

    ImFont* boldFont = ImGui::GetIO().Fonts->Fonts.Size > 1 ?
        ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont();

    const float W = 560.f, H = 360.f;

    ImGui::SetNextWindowSize({W, H}, ImGuiCond_Always);
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f, 0.086f, 0.11f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.19f, 0.19f, 0.24f, 1.f));

    ImGui::Begin("##auth", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // Drag
    if (ImGui::IsMouseClicked(0)) {
        ImVec2 mp = ImGui::GetMousePos();
        if (mp.y >= wp.y && mp.y <= wp.y + 50.f && mp.x >= wp.x && mp.x <= wp.x + W - 40.f) {
            ReleaseCapture();
            SendMessageW(GetAuthWindow(), WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
    }

    // Right panel — darker
    float splitX = 280.f;
    dl->AddRectFilled({wp.x + splitX, wp.y}, {wp.x + W, wp.y + H},
        IM_COL32(18, 18, 24, 255), 0.f);
    dl->AddLine({wp.x + splitX, wp.y + 20}, {wp.x + splitX, wp.y + H - 20},
        IM_COL32(50, 50, 64, 120), 1.f);

    // Right panel logo
    float logoCX = wp.x + splitX + (W - splitX) * 0.5f;
    float logoCY = wp.y + H * 0.45f;
#ifndef UNBRANDED
    DrawReticleAnimated(dl, {logoCX, logoCY}, 160.f, animTime);
    const char* brand = BRAND_NAME_UPPER;
    ImVec2 bts = boldFont->CalcTextSizeA(18.f, FLT_MAX, 0, brand);
    dl->AddText(boldFont, 18.f, {logoCX - bts.x/2, logoCY + 90.f}, WD::TextH, brand);
    float bx = logoCX + bts.x/2 + 6.f, by = logoCY + 92.f;
    dl->AddRect({bx, by}, {bx + 26, by + 16}, WD::Accent, 2.f, 0, 1.2f);
    dl->AddText(ImGui::GetFont(), 10.f, {bx + 5, by + 2}, WD::Accent, "WD");
#else
    const char* brand = BRAND_TITLE;
    ImVec2 bts = boldFont->CalcTextSizeA(18.f, FLT_MAX, 0, brand);
    dl->AddText(boldFont, 18.f, {logoCX - bts.x/2, logoCY + 60.f}, WD::TextH, brand);
#endif

    // Left panel content
    float px = wp.x + 28.f, py = wp.y;
    float inputW = splitX - 56.f;

    // Close button
    ImGui::SetCursorScreenPos({wp.x + splitX - 34.f, wp.y + 10.f});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.3f,0.4f,0.25f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f,0.3f,0.4f,0.4f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f,0.45f,0.5f,1.f));
    if (ImGui::Button("x##authclose", {22, 22}))
        PostMessage(GetAuthWindow(), WM_CLOSE, 0, 0);
    ImGui::PopStyleColor(4);

    // Header
#ifndef UNBRANDED
    DrawReticleAnimated(dl, {px + 10, py + 36}, 28.f, animTime);
    dl->AddText(boldFont, 15.f, {px + 30, py + 28}, WD::TextH, BRAND_NAME_UPPER);
    dl->AddText(ImGui::GetFont(), 11.f, {px + 30, py + 46}, WD::TextD, "WarDogs External");
#else
    dl->AddText(boldFont, 15.f, {px, py + 28}, WD::TextH, BRAND_TITLE);
    dl->AddText(ImGui::GetFont(), 11.f, {px, py + 46}, WD::TextD, "External Overlay");
#endif

    dl->AddLine({px, py + 72}, {px + inputW, py + 72}, IM_COL32(50, 50, 64, 100), 1.f);

    dl->AddText(ImGui::GetFont(), 11.f, {px, py + 88}, WD::TextD, "LICENSE KEY");

    // Input field — dark themed
    ImGui::SetCursorScreenPos({px, py + 106});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {12, 10});
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.2f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.17f, 0.17f, 0.23f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.19f, 0.19f, 0.25f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.92f, 0.95f, 1.f));
    ImGui::SetNextItemWidth(inputW);
    ImGui::InputText("##key", g_license, 64);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    // ACTIVATE button — green accent
    ImGui::SetCursorScreenPos({px, py + 160});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ACCENT_HOV_Rf, ACCENT_HOV_Gf, ACCENT_HOV_Bf, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ACCENT_ACT_Rf, ACCENT_ACT_Gf, ACCENT_ACT_Bf, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));

    if (ImGui::Button("ACTIVATE", {inputW, 38})) {
        if (g_license[0]=='\0') { g_authFailed=true; sprintf_s(g_status,"Enter a license key"); }
        else { g_authenticated=true; sprintf_s(g_status,"License active (dev)"); SaveLicenseToFile(g_license); }
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    // Status
    ImU32 statCol = g_authenticated ? WD::Green : (g_authFailed ? WD::Red : WD::TextD);
    dl->AddText(ImGui::GetFont(), 12.f, {px, py + 212}, statCol, g_status);

    // Bottom info
    dl->AddLine({px, py + H - 46}, {px + inputW, py + H - 46}, IM_COL32(50, 50, 64, 80), 1.f);
#ifdef UNBRANDED
    dl->AddText(ImGui::GetFont(), 11.f, {px, py + H - 34}, WD::TextD, "External Overlay");
#else
    dl->AddText(ImGui::GetFont(), 11.f, {px, py + H - 34}, WD::TextD, "WarDogs External | UE5");
#endif

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}
