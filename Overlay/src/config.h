#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <ShlObj.h>
#include <cmath>

#define MAX_PROFILES 5
#define CONFIG_VERSION 19

struct WDConfig {
    int version = CONFIG_VERSION;
    char name[32] = "Default";

    // Aimbot
    bool aimbot = false;
    bool teamCheck = true;
    bool showFov = true;
    bool aimVisCheck = true;
    float fov = 5.f;
    float smooth = 4.f;
    int aimBone = 0;
    int aimKeyIdx = 0;
    bool aimHoldMode = true;
    bool aimLock = false;
    float aimMaxDistance = 200.f;
    bool aimPrediction = false;

    // Triggerbot
    bool triggerbot = false;
    int triggerKeyIdx = 4;
    int triggerDelay = 30;

    // Recoil / Spread / Sway
    bool noRecoil = false;
    bool noSpread = false;
    bool noSway = false;

    // ESP
    bool esp = true;
    bool espBoxes = true;
    bool espSkeleton = true;
    bool espHealth = true;
    bool espName = true;
    bool espDistance = true;
    bool espSnaplines = false;
    bool espHeadDot = false;
    bool espShowTeam = false;
    bool espVisCheck = false;
    bool espShowDowned = true;
    bool espShowDead = false;
    bool espHitmarker = true;
    bool watermark = true;
    bool radar = false;
    float espMaxDistance = 300.f;
    bool espOffScreen = false;
    bool espFaction = true;
    bool espWeapon = true;
    bool espViewDir = false;
    int espBoxStyle = 0;     // 0=full, 1=corners
    int espSnapOrigin = 0;   // 0=bottom, 1=center, 2=top
    bool espGlow = false;
    int espGlowIntensity = 3;
    float colGlow[4] = {1.f, 0.2f, 0.2f, 1.f};

    // World ESP
    bool espWorldItems = false;
    bool espMines = false;
    bool espEmplacements = true;
    bool espFOBs = true;
    bool espItemWeapons = true;
    bool espItemAmmo = true;
    bool espItemAttachments = true;
    bool espItemMedical = true;
    bool espItemGrenades = true;
    bool espItemOther = false;
    bool espBodybags = true;

    // Vehicle filters
    bool vehicleFilterLand = true;
    bool vehicleFilterAir = true;
    bool vehicleShowDead = false;
    bool vehicleShowEmpty = true;
    float vehicleMaxDistance = 2000.f;

    // Colors visible
    float colVisBox[4]  = {1.f, 1.f, 0.2f, 1.f};
    float colVisSkel[4] = {1.f, 0.9f, 0.1f, 1.f};
    float colVisName[4] = {1.f, 1.f, 1.f, 1.f};
    float colVisSnap[4] = {1.f, 1.f, 0.2f, 0.4f};

    // Colors hidden
    float colHidBox[4]  = {1.f, 0.2f, 0.2f, 1.f};
    float colHidSkel[4] = {0.8f, 0.15f, 0.15f, 1.f};
    float colHidName[4] = {0.8f, 0.8f, 0.8f, 1.f};
    float colHidSnap[4] = {1.f, 0.2f, 0.2f, 0.3f};

    // Colors team
    float colTeamBox[4]  = {0.2f, 0.5f, 1.f, 1.f};
    float colTeamSkel[4] = {0.15f, 0.4f, 0.8f, 1.f};
    float colTeamName[4] = {0.6f, 0.8f, 1.f, 1.f};

    // Vehicle ESP
    bool espVehicles = true;
    bool espVehicleOccupants = true;
    bool vehicleEnemyOnly = false;
    float colVehicle[4] = {0.9f, 0.65f, 0.2f, 0.7f};
    float colVehicleTeam[4] = {0.2f, 0.5f, 1.f, 0.5f};

    // Misc
    bool crosshair = false;
    bool fpsCounter = true;
    bool nightMode = false;
    float nightAlpha = 0.3f;
    bool fovChanger = false;
    int fovValue = 100;
    float radarSize = 162.f;
    float radarZoom = 1.5f;
    float radarPosX = -1.f;
    float radarPosY = -1.f;
    bool radarDraggable = true;

    void Validate() {
        auto clampF = [](float& v, float lo, float hi) { if (!std::isfinite(v)) v = lo; if (v < lo) v = lo; if (v > hi) v = hi; };
        auto clampI = [](int& v, int lo, int hi) { if (v < lo) v = lo; if (v > hi) v = hi; };
        clampF(fov, 1.f, 50.f);
        clampF(smooth, 1.f, 20.f);
        clampF(aimMaxDistance, 10.f, 1000.f);
        clampI(aimBone, 0, 3);
        clampI(aimKeyIdx, 0, 7);
        clampI(triggerKeyIdx, 0, 7);
        clampI(triggerDelay, 0, 200);
        clampF(espMaxDistance, 50.f, 2000.f);
        clampF(vehicleMaxDistance, 50.f, 3000.f);
        clampI(espBoxStyle, 0, 1);
        clampI(espSnapOrigin, 0, 2);
        clampI(espGlowIntensity, 1, 5);
        clampF(nightAlpha, 0.f, 1.f);
        clampI(fovValue, 30, 170);
        clampF(radarSize, 80.f, 300.f);
        clampF(radarZoom, 0.5f, 5.f);
        clampF(radarPosX, -1.f, 7680.f);
        clampF(radarPosY, -1.f, 4320.f);
        for (int i = 0; i < 4; i++) {
            clampF(colVisBox[i], 0.f, 1.f); clampF(colVisSkel[i], 0.f, 1.f);
            clampF(colVisName[i], 0.f, 1.f); clampF(colVisSnap[i], 0.f, 1.f);
            clampF(colHidBox[i], 0.f, 1.f); clampF(colHidSkel[i], 0.f, 1.f);
            clampF(colHidName[i], 0.f, 1.f); clampF(colHidSnap[i], 0.f, 1.f);
            clampF(colTeamBox[i], 0.f, 1.f); clampF(colTeamSkel[i], 0.f, 1.f);
            clampF(colTeamName[i], 0.f, 1.f); clampF(colVehicle[i], 0.f, 1.f);
            clampF(colVehicleTeam[i], 0.f, 1.f);
            clampF(colGlow[i], 0.f, 1.f);
        }
    }
};

class ConfigManager {
public:
    WDConfig profiles[MAX_PROFILES];
    int activeProfile = 0;
    char configDir[MAX_PATH] = {};

    void Init() {
        char appdata[MAX_PATH];
        SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
#ifdef UNBRANDED
        sprintf_s(configDir, "%s\\Overlay", appdata);
#else
        sprintf_s(configDir, "%s\\TakePeekWD", appdata);
#endif
        CreateDirectoryA(configDir, nullptr);
        for (int i = 0; i < MAX_PROFILES; i++) sprintf_s(profiles[i].name, "Profile %d", i+1);
        strcpy_s(profiles[0].name, "Default");
        for (int i = 0; i < MAX_PROFILES; i++) Load(i);
    }

    void Save(int idx) {
        if (idx < 0 || idx >= MAX_PROFILES) return;
        char path[MAX_PATH]; sprintf_s(path, "%s\\profile_%d.cfg", configDir, idx);
        FILE* f = nullptr; fopen_s(&f, path, "wb");
        if (f) { fwrite(&profiles[idx], sizeof(WDConfig), 1, f); fclose(f); }
    }

    void Load(int idx) {
        if (idx < 0 || idx >= MAX_PROFILES) return;
        char path[MAX_PATH]; sprintf_s(path, "%s\\profile_%d.cfg", configDir, idx);
        FILE* f = nullptr; fopen_s(&f, path, "rb");
        if (f) {
            WDConfig tmp;
            if (fread(&tmp, sizeof(WDConfig), 1, f) == 1 && tmp.version == CONFIG_VERSION) {
                tmp.name[31] = '\0';
                profiles[idx] = tmp;
                profiles[idx].Validate();
            }
            fclose(f);
        }
    }

    void Reset(int idx) {
        if (idx < 0 || idx >= MAX_PROFILES) return;
        char oldName[32];
        strcpy_s(oldName, profiles[idx].name);
        profiles[idx] = WDConfig{};
        strcpy_s(profiles[idx].name, oldName);
    }

    WDConfig& Active() {
        if (activeProfile < 0 || activeProfile >= MAX_PROFILES) activeProfile = 0;
        return profiles[activeProfile];
    }
};
