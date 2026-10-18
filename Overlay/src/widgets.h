#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "../../SDK/stealth.h"

namespace NxWidgets {

inline ImU32 GoldColor(float alpha = 1.f) { return IM_COL32(45, 180, 90, (int)(alpha * 255)); }
inline ImU32 GoldDim()    { return IM_COL32(30, 120, 60, 255); }
inline ImU32 TrackOff()   { return IM_COL32(44, 43, 53, 255); }
inline ImU32 TrackOn()    { return IM_COL32(45, 180, 90, 255); }
inline ImU32 KnobColor()  { return IM_COL32(240, 240, 245, 255); }
inline ImU32 TextActive() { return IM_COL32(255, 255, 255, 255); }
inline ImU32 TextDim()    { return IM_COL32(114, 113, 138, 255); }

static bool Toggle(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGui::PushID(label);
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID("toggle");
    const float height = ImGui::GetFrameHeight() * 0.7f;
    const float width = height * 1.9f;
    const float radius = height * 0.43f;
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);
    float totalW = width + g.Style.ItemInnerSpacing.x + labelSize.x;
    float totalH = ImMax(height, labelSize.y);
    ImRect bb(pos, ImVec2(pos.x + totalW, pos.y + totalH + g.Style.FramePadding.y));
    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !(*v);
    float storedAnim = ImGui::GetStateStorage()->GetFloat(id, *v ? 1.f : 0.f);
    float target = *v ? 1.f : 0.f;
    storedAnim += (target - storedAnim) * ImMin(g.IO.DeltaTime * 12.f, 1.f);
    ImGui::GetStateStorage()->SetFloat(id, storedAnim);
    float trackY = pos.y + (totalH - height) * 0.5f;
    ImVec2 trackMin(pos.x, trackY);
    ImVec2 trackMax(pos.x + width, trackY + height);
    ImVec4 offCol(0.176f, 0.173f, 0.216f, 1.f);
    ImVec4 onCol(0.176f, 0.706f, 0.353f, 1.f);
    ImVec4 curCol = ImLerp(offCol, onCol, storedAnim);
    if (hovered) { curCol.x += 0.05f; curCol.y += 0.05f; curCol.z += 0.05f; }
    window->DrawList->AddRectFilled(trackMin, trackMax, ImGui::ColorConvertFloat4ToU32(curCol), height * 0.5f);
    float knobX = trackMin.x + radius + 2.f + (width - radius * 2 - 4.f) * (storedAnim);
    float knobY = trackMin.y + height * 0.5f;
    window->DrawList->AddCircleFilled(ImVec2(knobX + 1, knobY + 1), radius, IM_COL32(0, 0, 0, 40), 16);
    window->DrawList->AddCircleFilled(ImVec2(knobX, knobY), radius, KnobColor(), 16);
    ImU32 textCol = *v ? TextActive() : TextDim();
    ImVec2 textPos(pos.x + width + g.Style.ItemInnerSpacing.x, pos.y + (totalH - labelSize.y) * 0.5f);
    window->DrawList->AddText(textPos, textCol, label);
    ImGui::PopID();
    return pressed;
}

static bool SliderFloat(const char* label, float* v, float vmin, float vmax, const char* fmt = "%.1f") {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGui::PushID(label);
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID("slider");
    float lineH = 4.f, dotR = 7.f, height = dotR * 2 + 4.f;
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 labelSize = ImGui::CalcTextSize(label);
    window->DrawList->AddText(pos, TextDim(), label);
    char valBuf[32]; snprintf(valBuf, sizeof(valBuf), fmt, *v);
    ImVec2 valSize = ImGui::CalcTextSize(valBuf);
    window->DrawList->AddText(ImVec2(pos.x + width - valSize.x, pos.y), GoldColor(), valBuf);
    pos.y += labelSize.y + 4.f;
    ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
    ImGui::ItemSize(ImVec2(width, labelSize.y + 4.f + height), 0);
    if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }
    bool hovered, held;
    ImGui::ButtonBehavior(bb, id, &hovered, &held);
    float range = (vmax - vmin);
    if (range < 1e-6f) range = 1.f;
    float t = (*v - vmin) / range;
    if (held) { t = ImClamp((g.IO.MousePos.x - bb.Min.x) / ImMax(width, 1.f), 0.f, 1.f); *v = vmin + t * range; }
    float lineY = pos.y + height * 0.5f, dotX = pos.x + t * width;
    window->DrawList->AddRectFilled(ImVec2(pos.x, lineY - lineH*0.5f), ImVec2(pos.x + width, lineY + lineH*0.5f), TrackOff(), lineH*0.5f);
    window->DrawList->AddRectFilled(ImVec2(pos.x, lineY - lineH*0.5f), ImVec2(dotX, lineY + lineH*0.5f), GoldColor(), lineH*0.5f);
    float handleR = (held || hovered) ? dotR + 1.f : dotR;
    window->DrawList->AddCircleFilled(ImVec2(dotX + 1, lineY + 1), handleR, IM_COL32(0, 0, 0, 50), 16);
    window->DrawList->AddCircleFilled(ImVec2(dotX, lineY), handleR, GoldColor(), 16);
    window->DrawList->AddCircleFilled(ImVec2(dotX, lineY), handleR - 2.5f, IM_COL32(30, 29, 37, 255), 16);
    ImGui::PopID();
    return held;
}

static bool SliderInt(const char* label, int* v, int vmin, int vmax, const char* fmt = "%d") {
    float fv = (float)*v;
    char fmtBuf[32]; snprintf(fmtBuf, sizeof(fmtBuf), fmt, *v);
    bool changed = SliderFloat(label, &fv, (float)vmin, (float)vmax, fmtBuf);
    *v = (int)fv;
    return changed;
}

static void Header(const char* text) {
    ImGui::Spacing();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + 3, pos.y + ImGui::GetTextLineHeight()), GoldColor(), 1.f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
    ImGui::TextColored(ImVec4(0.18f, 0.85f, 0.45f, 0.8f), "%s", text);
    ImGui::Spacing();
}

static const char* GetKeyName(int vk) {
    switch (vk) {
        case VK_LBUTTON:  return "Mouse 1"; case VK_RBUTTON:  return "Mouse 2";
        case VK_MBUTTON:  return "Mouse 3"; case VK_XBUTTON1: return "Mouse 4";
        case VK_XBUTTON2: return "Mouse 5"; case VK_BACK:     return "Backspace";
        case VK_TAB:      return "Tab";     case VK_RETURN:   return "Enter";
        case VK_SHIFT:    return "Shift";   case VK_CONTROL:  return "Ctrl";
        case VK_MENU:     return "Alt";     case VK_CAPITAL:  return "Caps Lock";
        case VK_ESCAPE:   return "Escape";  case VK_SPACE:    return "Space";
        case VK_PRIOR:    return "Page Up"; case VK_NEXT:     return "Page Down";
        case VK_END:      return "End";     case VK_HOME:     return "Home";
        case VK_LEFT:     return "Left";    case VK_UP:       return "Up";
        case VK_RIGHT:    return "Right";   case VK_DOWN:     return "Down";
        case VK_INSERT:   return "Insert";  case VK_DELETE:   return "Delete";
        case VK_LSHIFT:   return "L-Shift"; case VK_RSHIFT:   return "R-Shift";
        case VK_LCONTROL: return "L-Ctrl";  case VK_RCONTROL: return "R-Ctrl";
        case VK_LMENU:    return "L-Alt";   case VK_RMENU:    return "R-Alt";
        case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3";
        case VK_F4: return "F4"; case VK_F5: return "F5"; case VK_F6: return "F6";
        case VK_F7: return "F7"; case VK_F8: return "F8"; case VK_F9: return "F9";
        case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
        case VK_NUMPAD0: return "Num 0"; case VK_NUMPAD1: return "Num 1";
        case VK_NUMPAD2: return "Num 2"; case VK_NUMPAD3: return "Num 3";
        case VK_NUMPAD4: return "Num 4"; case VK_NUMPAD5: return "Num 5";
        case VK_NUMPAD6: return "Num 6"; case VK_NUMPAD7: return "Num 7";
        case VK_NUMPAD8: return "Num 8"; case VK_NUMPAD9: return "Num 9";
        default: {
            if (vk >= 0x30 && vk <= 0x39) { static char buf[2]; buf[0] = (char)vk; buf[1] = 0; return buf; }
            if (vk >= 0x41 && vk <= 0x5A) { static char buf[2]; buf[0] = (char)vk; buf[1] = 0; return buf; }
            static char hexBuf[8]; snprintf(hexBuf, sizeof(hexBuf), "0x%02X", vk); return hexBuf;
        }
    }
}

static bool Keybind(const char* label, int* key) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGui::PushID(label);
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID("keybind");
    static ImGuiID activeKeybind = 0;
    static bool waitingRelease = false;
    bool isListening = (activeKeybind == id);
    float btnW = 110.f, totalW = ImGui::GetContentRegionAvail().x;
    ImVec2 pos = window->DC.CursorPos;
    float h = ImGui::GetFrameHeight();
    ImRect bb(pos, ImVec2(pos.x + totalW, pos.y + h));
    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) { ImGui::PopID(); return false; }
    ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);
    window->DrawList->AddText(ImVec2(pos.x, pos.y + (h - labelSize.y) * 0.5f), TextDim(), label);
    float btnX = pos.x + totalW - btnW;
    ImRect btnBB(ImVec2(btnX, pos.y), ImVec2(btnX + btnW, pos.y + h));
    bool hovered = ImGui::IsMouseHoveringRect(btnBB.Min, btnBB.Max);
    if (hovered && ImGui::IsMouseClicked(0) && !isListening) { activeKeybind = id; waitingRelease = true; isListening = true; }
    bool changed = false;
    if (isListening) {
        if (waitingRelease) {
            bool anyHeld = false;
            for (int vk = 1; vk < 255; vk++) { if ((DynAPI::pAsyncKeyState ? DynAPI::pAsyncKeyState(vk) : 0) & 0x8000) { anyHeld = true; break; } }
            if (!anyHeld) waitingRelease = false;
        } else {
            for (int vk = 1; vk < 255; vk++) {
                if (vk == VK_INSERT) continue;
                if ((DynAPI::pAsyncKeyState ? DynAPI::pAsyncKeyState(vk) : 0) & 0x8000) {
                    if (vk == VK_ESCAPE) { activeKeybind = 0; break; }
                    *key = vk; activeKeybind = 0; changed = true; break;
                }
            }
        }
    }
    ImU32 bgCol;
    if (isListening) bgCol = IM_COL32(45, 180, 90, 100);
    else if (hovered) bgCol = IM_COL32(60, 58, 75, 255);
    else              bgCol = IM_COL32(45, 44, 55, 255);
    window->DrawList->AddRectFilled(btnBB.Min, btnBB.Max, bgCol, 4.f);
    window->DrawList->AddRect(btnBB.Min, btnBB.Max, isListening ? GoldColor(0.8f) : IM_COL32(70, 68, 90, 255), 4.f);
    const char* btnText;
    if (isListening && waitingRelease) btnText = "[ ... ]";
    else if (isListening)              btnText = "[ press key ]";
    else                               btnText = GetKeyName(*key);
    ImVec2 ts = ImGui::CalcTextSize(btnText);
    window->DrawList->AddText(ImVec2(btnX + (btnW - ts.x) * 0.5f, pos.y + (h - ts.y) * 0.5f), isListening ? GoldColor() : TextActive(), btnText);
    ImGui::PopID();
    return changed;
}

} // namespace NxWidgets
