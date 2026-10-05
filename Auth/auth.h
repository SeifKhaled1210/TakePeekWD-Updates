#pragma once
#include <Windows.h>
#include "imgui.h"

#ifndef TAKEPEEK_VERSION
#define TAKEPEEK_VERSION "2.0.1"
#endif

// ============================================================================
// Brand macros — controlled by -DUNBRANDED=ON at build time
// ============================================================================
#ifdef UNBRANDED
#define BRAND_NAME       ""
#define BRAND_NAME_UPPER ""
#define BRAND_TITLE      "External Overlay"
#define BRAND_FULL       "External Overlay"
#define ACCENT_R 45
#define ACCENT_G 180
#define ACCENT_B 90
#define ACCENT_HOV_Rf 0.22f
#define ACCENT_HOV_Gf 0.78f
#define ACCENT_HOV_Bf 0.42f
#define ACCENT_ACT_Rf 0.12f
#define ACCENT_ACT_Gf 0.55f
#define ACCENT_ACT_Bf 0.28f
#else
#define BRAND_NAME       "TakePeek"
#define BRAND_NAME_UPPER "TAKEPEEK"
#define BRAND_TITLE      "TakePeek"
#define BRAND_FULL       "TakePeek WarDogs"
#define ACCENT_R 45
#define ACCENT_G 180
#define ACCENT_B 90
#define ACCENT_HOV_Rf 0.22f
#define ACCENT_HOV_Gf 0.78f
#define ACCENT_HOV_Bf 0.42f
#define ACCENT_ACT_Rf 0.12f
#define ACCENT_ACT_Gf 0.55f
#define ACCENT_ACT_Bf 0.28f
#endif

#define ACCENT_COL(a) IM_COL32(ACCENT_R, ACCENT_G, ACCENT_B, a)
#define ACCENT_Rf (ACCENT_R / 255.0f)
#define ACCENT_Gf (ACCENT_G / 255.0f)
#define ACCENT_Bf (ACCENT_B / 255.0f)

void InitAuth();
void DrawAuthWindow();
bool IsAuthenticated();
const char* GetAuthStatus();
const char* GetLicenseDuration();
bool IsLicenseHwidLocked();
HWND GetAuthWindow();
void DrawTakePeekBrand(ImDrawList* drawList, ImVec2 position, float scale = 1.0f, bool darkText = true);
