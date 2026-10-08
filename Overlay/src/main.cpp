// TakePeek WD — WarDogs External Overlay (UE5 LWC)
// Offsets sourced from community reversal (UC thread, Aug 2026)
// Process: WardogsClient-Win64-Shipping.exe | Window class: UnrealWindow
// Engine: UE 5.7.4 | Anti-cheat: Elytra (Embark Studios)
// Class chain: APawn -> ABHMoverPawn -> AWDMoverCharacter -> AWDMoverPlayerCharacter
//
// NOTE: GWorld and other globals shift with every game patch.
// Structural offsets (gameInstance, persistentLevel, etc.) are stable.
// Update GWorld/GNames/GObjects after each patch using pattern scan or manual find.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#include <TlHelp32.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "config.h"
#include "soldier_texture.h"
#include "widgets.h"
#include "auth.h"
#include "../../SDK/stealth.h"
#include "../../SDK/memory.h"

namespace Logger { static inline void Log(const char*, const char*, ...) {} }

#include "../../kdmapper/include/gdrv_mapper.h"
#include "../../kdmapper/driver_loader.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winhttp.lib")

// TAKEPEEK_VERSION defined in auth.h

#define UPDATE_HOST L"raw.githubusercontent.com"
#define UPDATE_PATH L"/SeifKhaled1210/TakePeekWD-Updates/main/version.json"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// ============================================================================
// UE5 WarDogs Offsets — from community reversal
// Globals (GWorld etc.) change per patch — update after each game update
// Structural offsets are stable across patches
// ============================================================================
namespace O {
    // --- Globals (patch-dependent — last updated Sep 30 2026) ---
    // Globals WILL change with the next game patch — pattern scan runs at startup as fallback.
    constexpr uintptr_t GWorld   = 0x0D0E0E98;
    constexpr uintptr_t GNames   = 0x0CE81F40;
    constexpr uintptr_t GObjects = 0x0CF54760;
    constexpr uintptr_t GEngine  = 0x0D0E4150;

    // --- Engine functions (RVAs from game base) ---
    constexpr uintptr_t StaticFindObject = 0x18846E0;
    constexpr uintptr_t FNameToString    = 0x1684AD0;
    constexpr uintptr_t GetBoneMatrix    = 0x1F33FE0;
    constexpr uintptr_t ProcessEvent     = 0x1850CE0;
    constexpr uintptr_t FreeObjectName   = 0x1538E90;

    // --- UEngine ---
    constexpr uintptr_t GEngine_GameViewport     = 0xC88; // UGameViewportClient*
    constexpr uintptr_t GameViewport_World       = 0x78;  // UWorld* (alt GWorld path)

    // --- UWorld ---
    constexpr uintptr_t UWorld_PersistentLevel    = 0x30;
    constexpr uintptr_t UWorld_StreamingLevels    = 0x90;  // TArray<ULevelStreaming*>
    constexpr uintptr_t UWorld_TimeSeconds        = 0x840; // float — game time
    constexpr uintptr_t UWorld_DeltaTimeSeconds   = 0x864; // float
    constexpr uintptr_t UWorld_GameState           = 0x1B0;
    constexpr uintptr_t UWorld_Levels              = 0x1C8;
    constexpr uintptr_t UWorld_OwningGameInstance  = 0x228;

    // --- AGameStateBase / UWDGameStateSession ---
    constexpr uintptr_t GS_PlayerArray = 0x2D0; // TArray<APlayerState*>
    constexpr uintptr_t GS_CachedServerFPS       = 0x444; // float — server tick rate
    constexpr uintptr_t GS_bServerCheatsEnabled   = 0x5A0; // bool
    constexpr uintptr_t GS_bAdminModeEnabled      = 0x5A1; // bool

    // --- ULevel ---
    constexpr uintptr_t ULevel_Actors     = 0xA0;   // TArray<AActor*>: Data at +0x0, Num at +0x8

    // --- ULevelStreaming ---
    constexpr uintptr_t ULevelStreaming_LoadedLevel = 0x190; // ULevel*

    // --- UGameInstance ---
    constexpr uintptr_t GameInstance_LocalPlayers = 0x38;  // TArray<ULocalPlayer*>
    constexpr uintptr_t GameInstanceAlt           = 0x12C8; // Alt GameInstance path via GEngine

    // --- ULocalPlayer (inherits UPlayer) ---
    constexpr uintptr_t UPlayer_PlayerController = 0x30;

    // --- APlayerController ---
    constexpr uintptr_t PC_Pawn          = 0x360;  // AcknowledgedPawn
    constexpr uintptr_t PC_CameraManager = 0x370;
    constexpr uintptr_t PC_ControlRotation = 0x330;

    // --- APlayerCameraManager ---
    constexpr uintptr_t CameraManager_ViewTarget = 0x350; // TViewTarget[0] — live camera (UC post #623)
    constexpr uintptr_t CameraManager_CachePrivate = 0x1560; // FCameraCacheEntry (stale cache per UC)
    constexpr uintptr_t CameraManager_LastFrameCache = 0x1E40; // LastFrameCameraCachePrivate

    // --- FCameraCacheEntry ---
    constexpr uintptr_t CameraCache_POV = 0x10;  // FMinimalViewInfo

    // --- FMinimalViewInfo (UE5 LWC — doubles!) ---
    constexpr uintptr_t POV_Location    = 0x00;  // FVector (3 x double = 0x18)
    constexpr uintptr_t POV_Rotation    = 0x18;  // FRotator (3 x double = 0x18)
    constexpr uintptr_t POV_FOV         = 0x30;  // float
    constexpr uintptr_t POV_FirstPersonFOV = 0x38; // float — viewmodel/weapon FOV
    constexpr uintptr_t POV_AspectRatio = 0x5C;  // float

    // --- AActor ---
    constexpr uintptr_t AActor_RootComponent = 0x1C0;
    constexpr uintptr_t AActor_Instigator    = 0x1A8;
    constexpr uintptr_t AActor_ObjId         = 0x18;
    constexpr uintptr_t AActor_HiddenByte    = 0x61;  // bHidden flag byte
    constexpr uintptr_t AActor_DestroyedByte = 0x65;  // bActorIsBeingDestroyed flag byte
    constexpr uintptr_t AActor_ReplicatedMovement = 0xD8; // FRepMovement — Location (3 doubles) is first field

    // --- APawn ---
    constexpr uintptr_t APawn_PlayerState = 0x2D8;
    constexpr uintptr_t APawn_Controller  = 0x2E8;

    // --- APlayerState (base UPlayerState, SDK confirmed Sep 30 2026) ---
    constexpr uintptr_t PS_PlayerId          = 0x2BC;  // int32
    constexpr uintptr_t PS_CompressedPing    = 0x2C0;  // uint8
    constexpr uintptr_t PS_bIsABot           = 0x2C2;  // bool (bitfield byte — bIsABot)
    constexpr uintptr_t PS_PawnPrivate       = 0x330;
    constexpr uintptr_t PS_PlayerNamePrivate = 0x350;  // FString (SDK: 0x308 is SavedNetworkAddress, 0x350 is PlayerNamePrivate)

    // --- USceneComponent ---
    constexpr uintptr_t Scene_RelativeLocation = 0x168; // FVector (3 x double)
    constexpr uintptr_t Scene_ComponentVelocity = 0x1B0; // FVector — component velocity
    constexpr uintptr_t Scene_ComponentToWorld = 0x1D0; // FTransform (double, 0x60)
    constexpr uintptr_t Scene_AttachChildren   = 0x108; // TArray<USceneComponent*>

    // --- ACharacter (SDK confirmed) ---
    constexpr uintptr_t Char_Mesh              = 0x338; // USkeletalMeshComponent*
    constexpr uintptr_t Char_CharacterMovement = 0x340; // UCharacterMovementComponent*
    constexpr uintptr_t Char_bIsCrouched       = 0x460; // bool (bitfield byte)

    // --- UCharacterMovementComponent (SDK confirmed) ---
    constexpr uintptr_t CMC_Velocity           = 0x0F8; // FVector (inherited UMovementComponent)
    constexpr uintptr_t CMC_MovementMode       = 0x261; // uint8 EMovementMode
    constexpr uintptr_t CMC_MaxWalkSpeed       = 0x2A8; // float
    constexpr uintptr_t CMC_MaxWalkSpeedCrouched = 0x2AC; // float

    // --- ABHMoverPawn (parent of AWDMoverCharacter, deob confirmed) ---
    constexpr uintptr_t BHPawn_MoverComponent  = 0x378; // UBHPawnMoverComponent* (Mover plugin, NOT UCharacterMovementComponent)

    // --- UWDCharacterAnimInstance (stance booleans, deob confirmed) ---
    constexpr uintptr_t AnimInst_SprintOrTacSprint  = 0xC3A; // bool — bSprintOrTacSprint
    constexpr uintptr_t AnimInst_StanceProne        = 0xC44; // bool — IsActiveStanceProne
    constexpr uintptr_t AnimInst_StanceCrouch       = 0xC45; // bool — IsActiveStanceCrouch
    constexpr uintptr_t AnimInst_StanceStand        = 0xC46; // bool — IsActiveStanceStand

    // --- AWDMoverCharacter (game-specific, Sep 14 SDK) ---
    constexpr uintptr_t WDChar_CharacterMesh    = 0x390; // UBHSkeletalMeshComponentBudgeted* (in parent ABHMoverPawn)
    constexpr uintptr_t WDChar_MeshFallback1    = 0x578; // Fallback mesh (UC post #631)
    // 0xB98 = FirstPersonBodyMesh, 0xBA0 = adjacent FP data — NOT valid for third-person ESP
    constexpr uintptr_t WDChar_VitalityComponent = 0x738; // WDCharacterVitalityComponent*
    constexpr uintptr_t WDChar_InventoryComponent = 0x748; // UWDPlayerInventoryComponent*
    constexpr uintptr_t WDChar_VehicleOperator   = 0x758; // UWDVehicleOperatorComponent*
    constexpr uintptr_t WDChar_CameraManagerRef  = 0xB48; // APlayerCameraManager* (direct ref)

    // --- UWDVitalityComponent (new SDK confirmed) ---
    // 0x130 = BaseHealth (float) — NOT current health! It's the initial/max base value.
    // Actual current health comes from GetCurrentHealth() which sums GAS attributes.
    constexpr uintptr_t Vitality_BaseHealth          = 0x130; // float (initial base, NOT current)
    constexpr uintptr_t Vitality_MaxHealth           = 0x134; // float
    constexpr uintptr_t Vitality_RegenDelay          = 0x138; // float
    constexpr uintptr_t Vitality_HealthAttr          = 0x140; // GAS attribute object ptr (in MISSED region)
    constexpr uintptr_t AttrObj_CurrentValue         = 0x20;  // f64 current value (FBHGameplayAttributeRepData +0x20)

    // --- GAS attribute iteration (UBHGameplayEffectComponent) ---
    constexpr uintptr_t GASComp_AttrContainer    = 0x3D8; // FBHGameplayAttributeRepContainer
    constexpr uintptr_t AttrContainer_Array      = 0x118; // TArray<FBHGameplayAttributeRepData> within container
    constexpr uintptr_t AttrRepData_Tag          = 0x0C;  // FGameplayTag within rep data entry
    constexpr uintptr_t AttrRepData_BaseValue    = 0x18;  // double
    constexpr uintptr_t AttrRepData_CurrentValue = 0x20;  // double
    constexpr int       AttrRepData_Stride       = 0x38;  // sizeof(FBHGameplayAttributeRepData)

    // --- Faction / Team / Squad ---
    constexpr uintptr_t PS_FactionComponent = 0x548; // WDFactionComponent* (on PlayerState)
    constexpr uintptr_t Faction_Tag         = 0x100; // FGameplayTag (initial/design-time)
    constexpr uintptr_t Faction_TagRep      = 0x110; // FGameplayTag (replicated runtime — prefer this for remote players)
    constexpr uintptr_t Faction_Object      = 0x128; // AWDFaction* — resolved faction actor
    constexpr uintptr_t PS_SquadComponent   = 0x550; // WDSquadComponent* (on PlayerState)
    constexpr uintptr_t PS_VitalityStateTag = 0x61C; // FGameplayTag — alive/downed/dead on PlayerState (+0x58 shift)
    constexpr uintptr_t PS_MatchStats       = 0x580; // TArray<FWDMatchStat> — K/D/etc (+0x58 shift)

    // --- AWDFaction (faction actor data) ---
    constexpr uintptr_t Faction_Score       = 0x2B8; // int32 — faction score
    constexpr uintptr_t Faction_PlayerCount = 0x2BC; // int32
    constexpr uintptr_t Faction_Data        = 0x2C8; // UWDFactionData*

    // --- UWDFactionData (data asset — singleton per faction) ---
    constexpr uintptr_t FactionData_Tag     = 0x30;  // FGameplayTag
    constexpr uintptr_t FactionData_TeamId  = 0x108; // uint8 GenericTeamId

    // --- AWDGameStateSession ---
    constexpr uintptr_t GSSession_ServerName    = 0x408; // FString
    constexpr uintptr_t GSSession_Factions      = 0x448; // TArray<AWDFaction*>
    constexpr uintptr_t GSSession_MatchState    = 0x520; // UBHMatchStateComponent*
    constexpr uintptr_t Squad_Id            = 0x140; // 16-byte SquadId (lo at +0x140, hi at +0x148)

    // --- USkinnedMeshComponent ---
    constexpr uintptr_t Skinned_ComponentSpaceTransforms = 0x620; // TArray<FTransform> BoneTransforms0
    constexpr uintptr_t Skinned_ComponentSpaceTransforms1 = 0x630; // TArray<FTransform> BoneTransforms1 (double-buffered)
    constexpr uintptr_t Skinned_BoneTransformsAlt0 = 0xA08; // Alt bone array candidate (UC post #638)
    constexpr uintptr_t Skinned_BoneTransformsAlt1 = 0xA18; // Alt bone array candidate (UC post #638)
    constexpr uintptr_t Skinned_SkinnedAsset = 0x5B0;
    constexpr uintptr_t Skinned_SkinnedAssetAlt = 0x5A8;
    constexpr uintptr_t Skinned_LastSubmitTime  = 0x358; // float — last frame submitted for render
    constexpr uintptr_t Skinned_LastRenderTime = 0x35C; // float — last frame bone was drawn; for vis check
    constexpr uintptr_t Skinned_LastRenderTimeOnScreen = 0x360; // float — screen-only render time (excludes shadow passes)
    constexpr uintptr_t Skinned_VisTickOption  = 0x814; // uint8 — VisibilityBasedAnimTickOption
    constexpr uintptr_t Skinned_RenderStateBits = 0x818; // bRecentlyRendered flag byte
    constexpr uintptr_t Skinned_AnimScriptInst = 0x958; // UAnimInstance*

    // --- AWDMoverCharacter extended (new SDK + Sep 13 dump) ---
    constexpr uintptr_t WDChar_GameplayEffectComp = 0x730; // UBHGameplayEffectComponent*
    constexpr uintptr_t WDChar_WeaponBehavior  = 0x770; // UWDWeaponBehaviorComponent*
    constexpr uintptr_t WDChar_Invincible      = 0x7F3; // bool (last bool before VitalityState)
    constexpr uintptr_t WDChar_DeathState      = 0x7F4; // FWDCharacterVitalityState (0x28 bytes)
    // FWDCharacterVitalityState layout (0x28, SDK):
    //   +0x00 = InstigatingPlayerController (TWeakObjectPtr, 8)
    //   +0x08 = VitalityTag (FGameplayTag, 8)
    //   +0x10 = CurrentFallToCriticalTime (float)
    //   +0x14 = obfuscated float (alt: replicated current health)
    //   +0x18 = CurrentGiveUpTime (float) (alt: replicated max health)
    //   +0x1C = EWDBleedoutState (uint8 enum)
    //   +0x1D = EBHPlayerStance (uint8)
    //   +0x1E = bool
    //   +0x1F = bool
    constexpr uintptr_t DeathState_BleedoutState = 0x1C;
    constexpr uintptr_t DeathState_GiveUpTime    = 0x18;
    constexpr uintptr_t DeathState_FallToCrit    = 0x10;
    constexpr uintptr_t WDChar_AimingAlpha     = 0x81D; // char — OnRep_AimingAlpha (0x81C is a different bool)

    // --- UAnimInstance (aim rotation from AnimScriptInstance) ---
    constexpr uintptr_t AnimInst_AimRotation   = 0x6A0; // FRotator — CommonAimingData+AimRotation
    constexpr uintptr_t AnimInst_SwayEnabled   = 0xFC0; // bool — bWeaponSwayEnabled

    // --- Weapon / Recoil (UC page 22, confirmed chain) ---
    // No recoil: pawn → 0x770 (WeaponBehavior) → 0x440 (StatsData) → 0xBB0 (ViewKickMagnitudeMultiplier)
    constexpr uintptr_t WeaponBehavior_StatsData  = 0x440; // UWDWeaponStatsData*
    constexpr uintptr_t WeaponStats_ViewKick      = 0xBB0; // float — write 0.0f for no recoil
    constexpr uintptr_t WeaponBehavior_bAiming     = 0x3E0; // bool — is currently ADS
    constexpr uintptr_t WeaponBehavior_ADSValue   = 0x3E4; // float 0.0-1.0 — ADS transition state
    constexpr uintptr_t WeaponOwner_AttributeSet  = 0x3F8; // FAttributeSet — spread/recoil attrs

    // --- No-Sway (UC page 22-23, confirmed WBH chain) ---
    constexpr uintptr_t WeaponBehavior_SwayMode      = 0xE0;  // uint8 — set to 2 to disable sway
    constexpr uintptr_t WeaponBehavior_SwayEnergyX    = 0x260; // float — horizontal sway energy
    constexpr uintptr_t WeaponBehavior_SwayEnergyY    = 0x264; // float — vertical sway energy
    constexpr uintptr_t WeaponBehavior_SwayEnergyZ    = 0x268; // float
    constexpr uintptr_t WeaponBehavior_SwayEnergyW    = 0x26C; // float
    constexpr uintptr_t WeaponBehavior_SwayLiveX      = 0xBD8; // float — live sway offset X
    constexpr uintptr_t WeaponBehavior_SwayLiveY      = 0xBE0; // float — live sway offset Y
    constexpr uintptr_t WeaponBehavior_SwayLiveZ      = 0xBE8; // float
    constexpr uintptr_t WeaponBehavior_SwayLiveW      = 0xBF0; // float

    // --- Enhanced No-Recoil (UC page 22, pattern recoil chain) ---
    constexpr uintptr_t WeaponBehavior_PatternArray   = 0x380; // TArray<FRecoilPatternEntry> (stride 0x40, yaw at +0x08, pitch at +0x10)
    constexpr uintptr_t WeaponBehavior_ShotIndex      = 0x390; // int32 — current shot in pattern, pin to 0
    constexpr uintptr_t WeaponStats_RecoilYawMinMax    = 0xCE8; // FVector2D — RecoilPatternYawMinMax
    constexpr uintptr_t WeaponStats_RecoilPitchMax    = 0xCF0; // float — RecoilPatternPitchMax
    constexpr uintptr_t WeaponStats_PostPatternRandomH = 0xCF8; // float — post-pattern random horizontal
    constexpr uintptr_t WeaponStats_PostPatternRandomV = 0xD08; // float — post-pattern random vertical
    constexpr uintptr_t WeaponStats_ItemTag           = 0x30;  // FGameplayTag — weapon identifier
    constexpr uintptr_t WeaponStats_InitialBulletSpeed = 0xDD8; // float — InitialBulletSpeed
    constexpr uintptr_t WeaponStats_IronSightsAimTime = 0x18C; // float — IronSightsAimTime

    // --- Admin / Spectator Detection (UC page 25) ---
    constexpr uintptr_t PS_IsDeveloper = 0x619; // bool on PlayerState (+0x58 shift)
    constexpr uintptr_t PS_IsAdmin     = 0x61A; // bool on PlayerState (+0x58 shift)
    constexpr uintptr_t PC_SpectatorPawn = 0x6C8; // ASpectatorPawn* on PlayerController

    // --- Vehicle Damage Model (UC page 26) ---
    constexpr uintptr_t Vehicle_DamageModel = 0x788; // UBHVehicleDamageModelComponent*

    // --- USceneComponent attachment ---
    constexpr uintptr_t Scene_AttachParent = 0xF0; // USceneComponent* — parent component if attached (SDK 0xF0)

    // --- Bullet Velocity (UC page 23) ---
    // Walk: pawn inventory → held item → WDItemExtension_RangedWeapon → +0x118 = WDWeaponComponent
    constexpr uintptr_t WeaponComp_SpeedModifier  = 0x114; // float — velocity multiplier
    constexpr uintptr_t WeaponComp_BaseSpeed      = 0xEE0; // float — base projectile speed (cm/s)

    // --- Ammo / Magazine (UC page 24) ---
    constexpr uintptr_t MagData_MagazineTag       = 0x00;  // FGameplayTag
    constexpr uintptr_t MagData_MagazineSize      = 0x08;  // int32
    constexpr uintptr_t MagData_BaseAmmoTag       = 0x0C;  // FGameplayTag
    constexpr uintptr_t MagData_Stride            = 0x30;  // sizeof(FWDMagazineData)

    // --- Static classes (RVAs from module base) ---
    constexpr uintptr_t WDMoverPlayerChar_Static  = 0xD197920;
    constexpr uintptr_t ProjectileSubsystem_Static = 0xD19A218;
    constexpr uintptr_t Server_FireProjectile     = 0x6B0F000; // UWDWeaponComponent::Server_FireProjectile

    // --- UWDProjectileSubsystem instance data ---
    // Sparse at +0x108, instance stride 0x320:
    //   +0x08 = location, +0x20 = velocity, +0xC8 = status (2=active)
    //   +0xE8 = UWDProjectileData*, data+0x30 = caliber tag, data+0x38+0x1FC = speed
    constexpr uintptr_t ProjSub_Sparse            = 0x108;
    constexpr int       ProjSub_InstanceStride     = 0x320;
    constexpr uintptr_t ProjInst_Location          = 0x08;
    constexpr uintptr_t ProjInst_Velocity          = 0x20;
    constexpr uintptr_t ProjInst_Status            = 0xC8;  // 2 = active
    constexpr uintptr_t ProjInst_Data              = 0xE8;  // UWDProjectileData*
    constexpr uintptr_t ProjData_CaliberTag        = 0x30;
    constexpr uintptr_t ProjData_TypicalSpeed      = 0x234; // 0x38 + 0x1FC

    // --- Vehicle ---
    constexpr uintptr_t Vehicle_VehicleVariant = 0x3A8; // UModularVehicleVariant*
    constexpr uintptr_t Vehicle_SeatComponent  = 0x748; // UBHVehicleSeatComponent*
    constexpr uintptr_t Vehicle_Mesh           = 0x750; // USkeletalMeshComponentVehicle*
    constexpr uintptr_t Vehicle_Config         = 0xA28; // UBHBaseVehicleConfigDataAsset*
    constexpr uintptr_t VehicleVariant_Id      = 0x34;  // FGameplayTag
    constexpr uintptr_t SeatComp_Occupants     = 0x108; // TArray<UBHVehicleOperatorComponent*>
    constexpr uintptr_t VehicleOp_CurrentSeat  = 0x198;
    constexpr uintptr_t Vehicle_Destroyed      = 0x830; // bIsVehicleDestroyed (deobfuscated)
    constexpr uintptr_t VehicleFaction_Stationary = 0x0AC8;
    constexpr uintptr_t VehicleFaction_Airplane   = 0x0BA8;
    constexpr uintptr_t VehicleFaction_Rotary     = 0x0B88;
    constexpr uintptr_t VehicleFaction_Tracked    = 0x0BE8;
    constexpr uintptr_t VehicleFaction_Wheeled    = 0x0BD8;
    constexpr uintptr_t Vehicle_EngineRunning  = 0x841; // bIsEngineRunning (deobfuscated)
    constexpr uintptr_t Vehicle_HornActive     = 0x8D8; // bIsHornActive (deobfuscated)

    // --- View direction (AController — SDK confirmed at 0x330) ---
    constexpr uintptr_t Controller_ControlRotation = 0x330; // FRotator (3 doubles: pitch, yaw, roll)

    // --- Remote player view direction (UBHMoverPawn — SDK confirmed) ---
    constexpr uintptr_t BHPawn_SmoothedRemoteViewPitch = 0x3A8; // float — replicated pitch for remote players
    constexpr uintptr_t BHPawn_RemoteViewYaw           = 0x3D0; // uint16 — compressed replicated yaw

    // --- Player Clan Tag (UWDPlayerState — SDK confirmed) ---
    constexpr uintptr_t PS_PlayerClanTag = 0x408; // FString — player's clan tag

    // --- Inventory / Equipment ---
    constexpr uintptr_t WDChar_InventoryComp = 0x748; // UWDPlayerInventoryComponent*
    constexpr uintptr_t InvComp_HeldItem     = 0x8C8; // ABHItem* — currently held weapon/item
    constexpr uintptr_t InvComp_ActiveItem   = 0xA78; // ABHItem* — active weapon fallback
    constexpr uintptr_t BHItem_ItemId        = 0x328; // FGameplayTag — weapon/item identifier
    constexpr uintptr_t InvComp_ArmorArray   = 0x818; // TArray<FWDReplicatedArmorDurability>
    constexpr int       ArmorEntry_Stride    = 0x14;  // sizeof(FWDReplicatedArmorDurability)
    constexpr uintptr_t ArmorEntry_Durability = 0x8;  // float — current durability

    // --- BHInventorySubsystem (item name resolution) ---
    constexpr uintptr_t BHInvSubsys_UClass       = 0xD17EA88; // UClass* static
    constexpr uintptr_t InvSubsys_ItemDefMap     = 0x38;  // TMap — ItemDefinitions
    constexpr uintptr_t ItemDef_ItemId           = 0x30;  // FGameplayTag
    constexpr uintptr_t ItemDef_DisplayName      = 0x58;  // FText

    // --- AWDProjectile (new SDK) ---
    constexpr uintptr_t Projectile_SpawnLocation   = 0x338; // FVector — GetSpawnLocation()
    constexpr uintptr_t Projectile_InitialVelocity = 0x350; // FVector — GetSpawnVelocity()
    constexpr uintptr_t Projectile_NetId           = 0x310; // uint32_t
    // FWDReplicatedProjectileData (replicated spawn info):
    constexpr uintptr_t RepProj_SpawnLocation = 0x28; // FVector_NetQuantize
    constexpr uintptr_t RepProj_SpawnDirection = 0x40; // FVector_NetQuantizeNormal
    constexpr uintptr_t RepProj_SpawnVelocity  = 0x5C; // float
    constexpr uintptr_t RepProj_NetId          = 0x58; // int32_t

    // --- Dropped Items / Lootables ---
    constexpr uintptr_t DroppedItem_ItemTag = 0x33C; // FGameplayTag
    constexpr uintptr_t Lootable_Hidden     = 0x300; // bool — bIsHiddenInGame
    constexpr uintptr_t Lootable_SpawnData  = 0x308; // SpawnedItemData

    // --- Placeables / Mines (AWDPlaceable / AWDLandminePlaceable) ---
    constexpr uintptr_t Placeable_EntityId = 0x368; // FGameplayTag
    constexpr uintptr_t LandMine_FactionComp = 0x3E0; // UWDFactionComponent*
    constexpr uintptr_t Explosive_FactionComp = 0x308; // UWDFactionComponent*

    // UE5 LWC FTransform: Quat(4x f64)=0x20, Translation(3x f64 + pad)=0x20, Scale(3x f64 + pad)=0x20 → 0x60 total
    constexpr int FTRANSFORM_STRIDE = 0x60;

    // --- UObject (for class name resolution) ---
    constexpr uintptr_t UObject_ClassPrivate = 0x10;
    constexpr uintptr_t UObject_NamePrivate  = 0x18; // FName (ComparisonIndex at +0x0)

    // --- FName pool layout (GNames → FNameEntryAllocator) ---
    constexpr uintptr_t GNames_PoolOffset   = 0x00;  // chunk table start (NamePoolBlocks)
    constexpr int FNAME_BLOCK_BITS          = 16;
    constexpr int FNAME_ENTRY_STRIDE        = 8;
    constexpr int FNAME_HEADER_OFF          = 0x08;   // u16 header within entry
    constexpr int FNAME_KIND_OFF            = 0x0A;   // u8 kind within entry
    constexpr int FNAME_DATA_OFF            = 0x0C;   // string bytes start
    constexpr int FNAME_HEADER_SHIFT        = 6;      // len = header >> 6
    constexpr int FNAME_KIND_FIXED          = 2;      // kind==2 → fixed 22 chars
    constexpr int FNAME_KIND_FIXED_LEN      = 22;

    // --- RefSkeleton (embedded in USkeletalMesh / USkinnedAsset) ---
    constexpr uintptr_t SkinnedAsset_RefSkeleton = 0x2C0;
    constexpr int BONE_INFO_STRIDE = 0x0C; // sizeof(FMeshBoneInfo) = FName(8) + ParentIndex(4)

    // --- Pattern scan signatures for auto-resolving globals after patches ---
    // GWorld: mov rax,[rip+disp32] at sig offset 18, instruction length 7
    // 48 8B 41 ?? 48 85 C0 74 ?? 48 8B 40 ?? 48 85 C0 75 ?? 48 8B 05 ?? ?? ?? ?? C3
    // GNames: lea r8,[rip+disp32] at sig offset 0, instruction length 7
    // 4C 8D 05 ?? ?? ?? ?? EB ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ??
}

// ============================================================================
// Math types — UE5 LWC uses doubles
// ============================================================================
struct DVec3 {
    double x, y, z;
    DVec3 operator-(const DVec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    DVec3 operator+(const DVec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    DVec3 operator*(const DVec3& o) const { return {x*o.x, y*o.y, z*o.z}; }
    DVec3 operator*(double s) const { return {x*s, y*s, z*s}; }
    double length() const { return sqrt(x*x + y*y + z*z); }
    double dot(const DVec3& o) const { return x*o.x + y*o.y + z*o.z; }
};
struct DQuat { double x, y, z, w; };
struct DRotator { double pitch, yaw, roll; };
struct Vec2 { float x, y; };

struct DTransform {
    DQuat rotation;      // 0x00, 32 bytes
    DVec3 translation;   // 0x20, 24 bytes
    double _pad0;        // 0x38, UE5 pads FVector3d to 32 bytes
    DVec3 scale3D;       // 0x40, 24 bytes
    double _pad1;        // 0x58, UE5 pads FVector3d to 32 bytes

    bool Valid() const {
        double n = rotation.x*rotation.x + rotation.y*rotation.y + rotation.z*rotation.z + rotation.w*rotation.w;
        return std::isfinite(n) && n > 0.25 && n < 4.0
            && std::isfinite(translation.x) && std::isfinite(translation.y) && std::isfinite(translation.z)
            && std::isfinite(scale3D.x) && std::isfinite(scale3D.y) && std::isfinite(scale3D.z)
            && scale3D.x > 0.001 && scale3D.y > 0.001 && scale3D.z > 0.001;
    }
};
static_assert(sizeof(DTransform) == 0x60, "DTransform must be 0x60");

struct DTransformPacked {
    DQuat rotation;      // 0x00, 32 bytes
    DVec3 translation;   // 0x20, 24 bytes
    DVec3 scale3D;       // 0x38, 24 bytes
};
static_assert(sizeof(DTransformPacked) == 0x50, "DTransformPacked must be 0x50");

struct FTransformF {
    float rotX, rotY, rotZ, rotW;  // 0x00, 16 bytes
    float tX, tY, tZ, _tw;        // 0x10, 16 bytes
    float sX, sY, sZ, _sw;        // 0x20, 16 bytes
};
static_assert(sizeof(FTransformF) == 0x30, "FTransformF must be 0x30");

// ============================================================================
// Globals
// ============================================================================
static HWND g_gameWnd = nullptr;
static HWND g_overlayWnd = nullptr;

// BYOVD state
static GdrvMapper::VulnDriverState g_vulnState = {};
static bool g_byovdActive = false;
static DWORD64 g_ntoskrnlBase = 0;

// Convenience aliases into Memory namespace
static auto& g_pid = Memory::g_pid;
static auto& g_base = Memory::g_baseAddress;
static auto& g_proc = Memory::g_proc;

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swapchain = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static ID3D11ShaderResourceView* g_soldierSRV = nullptr;

static bool g_running = true;
static bool g_menuOpen = false;
static int g_menuTab = 0;
static int g_screenW = 0, g_screenH = 0;
static ConfigManager g_config;

// Runtime-resolved global offsets (pattern scanner updates these if defaults fail)
static uintptr_t g_gworldOff = O::GWorld;
static uintptr_t g_gnamesOff = O::GNames;

// UWorld::Levels TArray offset — auto-discovered at runtime
static uintptr_t g_levelsOffset = 0;
static bool g_levelsDiscovered = false;
static std::chrono::steady_clock::time_point g_levelsRetryTime = {};

// GameState → PlayerArray approach (primary player source)
static uintptr_t g_gameState = 0;
static uintptr_t g_playerArrayOffset = 0;
static bool g_gameStateScanDone = false;

// Debug HUD (F3 toggle)
static bool g_showDebugHud = false;

// ============================================================================
// Process / Memory helpers
// ============================================================================
template<typename T>
T Read(uintptr_t addr) {
    return Memory::Read<T>(addr);
}

template<typename T>
bool Write(uintptr_t addr, const T& val) {
    return Memory::Write<T>(addr, val);
}

inline bool IsValidPtr(uintptr_t p) {
    return p > 0x10000000 && p < 0x7F0000000000;
}

template<typename T>
bool SafeWrite(uintptr_t addr, const T& val) {
    if (!IsValidPtr(addr)) return false;
    return Memory::Write<T>(addr, val);
}

bool ReadRaw(uintptr_t addr, void* buf, size_t sz) {
    return Memory::ReadBuffer(addr, buf, sz);
}

static DWORD FindProcess(const wchar_t* name) {
    if (!DynAPI::pSnapshot || !DynAPI::pP32First || !DynAPI::pP32Next) return 0;
    HANDLE snap = DynAPI::pSnapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (DynAPI::pP32First(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; } } while (DynAPI::pP32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// Find process via NtQuerySystemInformation — bypasses CreateToolhelp32Snapshot hooks
static DWORD FindProcessNtQuery(const wchar_t* name) {
    typedef struct _MY_UNICODE_STRING { USHORT Length; USHORT MaximumLength; PWSTR Buffer; } MY_UNICODE_STRING;
    typedef struct _MY_SYSTEM_PROCESS_INFO {
        ULONG NextEntryOffset; ULONG NumberOfThreads; BYTE Reserved1[48];
        MY_UNICODE_STRING ImageName; LONG BasePriority; HANDLE UniqueProcessId;
    } MY_SYSTEM_PROCESS_INFO;
    typedef NTSTATUS(NTAPI* fnNtQSI)(ULONG, PVOID, ULONG, PULONG);
    static fnNtQSI pNtQSI = nullptr;
    if (!pNtQSI) pNtQSI = (fnNtQSI)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation");
    if (!pNtQSI) return 0;

    ULONG bufSize = 2 * 1024 * 1024;
    auto buffer = (BYTE*)VirtualAlloc(nullptr, bufSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buffer) return 0;

    ULONG needed = 0;
    NTSTATUS status = pNtQSI(5 /*SystemProcessInformation*/, buffer, bufSize, &needed);
    if (status < 0) { VirtualFree(buffer, 0, MEM_RELEASE); return 0; }

    DWORD pid = 0;
    auto ptr = buffer;
    while (true) {
        auto* entry = (MY_SYSTEM_PROCESS_INFO*)ptr;
        if (entry->ImageName.Buffer && entry->ImageName.Length > 0) {
            if (_wcsicmp(entry->ImageName.Buffer, name) == 0) {
                pid = (DWORD)(ULONG_PTR)entry->UniqueProcessId;
                break;
            }
        }
        if (entry->NextEntryOffset == 0) break;
        ptr += entry->NextEntryOffset;
    }
    VirtualFree(buffer, 0, MEM_RELEASE);
    return pid;
}

// Find game by its UnrealWindow class — bypasses anti-cheat process hiding
static DWORD FindProcessByWindow() {
    struct FindData { DWORD pid; HWND hwnd; };
    FindData fd{0, nullptr};
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        if (!IsWindowVisible(hwnd)) return TRUE;
        char cls[64]; GetClassNameA(hwnd, cls, 64);
        if (strcmp(cls, "UnrealWindow") == 0) {
            auto* d = (FindData*)lp;
            GetWindowThreadProcessId(hwnd, &d->pid);
            d->hwnd = hwnd;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&fd);
    return fd.pid;
}

static DWORD SafeFindProcess(const wchar_t* name, int* crashMask) {
    DWORD pid = 0;
    int crashes = 0;
    __try { pid = FindProcess(name); } __except(EXCEPTION_EXECUTE_HANDLER) { crashes |= 1; }
    if (!pid) { __try { pid = FindProcessByWindow(); } __except(EXCEPTION_EXECUTE_HANDLER) { crashes |= 2; } }
    if (!pid) { __try { pid = FindProcessNtQuery(name); } __except(EXCEPTION_EXECUTE_HANDLER) { crashes |= 4; } }
    if (crashMask) *crashMask = crashes;
    return pid;
}

static uintptr_t GetModuleBase(DWORD pid, const wchar_t* name, std::string* diagOut = nullptr) {
    if (!DynAPI::pSnapshot || !DynAPI::pM32First || !DynAPI::pM32Next) {
        if (diagOut) *diagOut = "DynAPI snapshot/module ptrs NULL";
        return 0;
    }
    for (int attempt = 0; attempt < 3; attempt++) {
        HANDLE snap = DynAPI::pSnapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (diagOut) {
                char buf[128]; sprintf_s(buf, "Snapshot failed attempt %d err=%lu", attempt+1, err);
                *diagOut = buf;
            }
            if (err == ERROR_BAD_LENGTH) { Sleep(500); continue; }
            return 0;
        }
        MODULEENTRY32W me{}; me.dwSize = sizeof(me);
        uintptr_t base = 0;
        int modCount = 0;
        if (DynAPI::pM32First(snap, &me)) {
            do {
                modCount++;
                if (_wcsicmp(me.szModule, name) == 0) { base = (uintptr_t)me.modBaseAddr; break; }
            } while (DynAPI::pM32Next(snap, &me));
        }
        CloseHandle(snap);
        if (base) return base;
        if (diagOut) {
            char buf[128]; sprintf_s(buf, "Module not found in %d modules (attempt %d)", modCount, attempt+1);
            *diagOut = buf;
        }
        Sleep(1000);
    }
    return 0;
}

static uintptr_t GetModuleBasePEB(HANDLE proc, std::string* diagOut = nullptr) {
    typedef NTSTATUS(NTAPI* fnNtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static fnNtQIP pNtQIP = nullptr;
    if (!pNtQIP) pNtQIP = (fnNtQIP)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    if (!pNtQIP) { if (diagOut) *diagOut = "NtQueryInformationProcess not found"; return 0; }
    struct { ULONG_PTR Reserved1; PVOID PebBaseAddress; ULONG_PTR Reserved2[4]; } pbi{};
    ULONG retLen = 0;
    NTSTATUS st = pNtQIP(proc, 0, &pbi, sizeof(pbi), &retLen);
    if (st < 0) {
        if (diagOut) { char buf[64]; sprintf_s(buf, "NtQIP failed NTSTATUS=0x%08X", (unsigned)st); *diagOut = buf; }
        return 0;
    }
    if (!pbi.PebBaseAddress) { if (diagOut) *diagOut = "PEB address is NULL"; return 0; }
    uintptr_t imageBase = 0;
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(proc, (BYTE*)pbi.PebBaseAddress + 0x10, &imageBase, sizeof(imageBase), &bytesRead)) {
        if (diagOut) { char buf[64]; sprintf_s(buf, "RPM PEB+0x10 failed err=%lu", GetLastError()); *diagOut = buf; }
        return 0;
    }
    if (!imageBase && diagOut) *diagOut = "ImageBaseAddress read as 0";
    return imageBase;
}

static HWND FindGameWindow() {
    struct FindData { DWORD pid; HWND result; };
    FindData fd{g_pid, nullptr};
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* d = (FindData*)lp;
        DWORD wp; GetWindowThreadProcessId(hwnd, &wp);
        if (wp == d->pid && IsWindowVisible(hwnd)) {
            char cls[64]; GetClassNameA(hwnd, cls, 64);
            if (strcmp(cls, XS("UnrealWindow").c_str()) == 0) { d->result = hwnd; return FALSE; }
        }
        return TRUE;
    }, (LPARAM)&fd);
    return fd.result;
}

// ============================================================================
// UE5 LWC helpers (double-precision transforms)
// ============================================================================
static DVec3 QuatRotateVector(const DQuat& q, const DVec3& v) {
    double x2 = q.x*2.0, y2 = q.y*2.0, z2 = q.z*2.0;
    double xx = q.x*x2, yy = q.y*y2, zz = q.z*z2;
    double xy = q.x*y2, xz = q.x*z2, yz = q.y*z2;
    double wx = q.w*x2, wy = q.w*y2, wz = q.w*z2;
    return {
        (1.0 - yy - zz)*v.x + (xy - wz)*v.y + (xz + wy)*v.z,
        (xy + wz)*v.x + (1.0 - xx - zz)*v.y + (yz - wx)*v.z,
        (xz - wy)*v.x + (yz + wx)*v.y + (1.0 - xx - yy)*v.z
    };
}

static DVec3 TransformBoneToWorld(const DTransform& bone, const DTransform& c2w) {
    DVec3 scaled = bone.translation * c2w.scale3D;
    DVec3 rotated = QuatRotateVector(c2w.rotation, scaled);
    return rotated + c2w.translation;
}

static DVec3 TransformBonePackedToWorld(const DTransformPacked& bone, const DTransform& c2w) {
    DVec3 scaled = bone.translation * c2w.scale3D;
    DVec3 rotated = QuatRotateVector(c2w.rotation, scaled);
    return rotated + c2w.translation;
}

static DVec3 TransformBoneFToWorld(const FTransformF& bone, const DTransform& c2w) {
    DVec3 bt = {(double)bone.tX, (double)bone.tY, (double)bone.tZ};
    DVec3 scaled = bt * c2w.scale3D;
    DVec3 rotated = QuatRotateVector(c2w.rotation, scaled);
    return rotated + c2w.translation;
}

static int g_boneStride = 0;

// ============================================================================
// World-to-screen using camera POV (UE5 rotation matrix, double precision)
// ============================================================================
struct CameraData {
    DVec3 location;
    DRotator rotation;
    float fov;
    float aspectRatio;
    float timeSeconds;
    bool valid;
};
static CameraData g_camera{};
static CameraData s_workerCam{};

static constexpr double PI = 3.14159265358979323846;
static constexpr double DEG2RAD = PI / 180.0;

static void UpdateCameraView() {
    if (!g_base) return;
    uintptr_t gworld = Read<uintptr_t>(g_base + g_gworldOff);
    if (!gworld || gworld < 0x10000000 || gworld >= 0x7FFFFFFFFFFF) {
        uintptr_t eng = Read<uintptr_t>(g_base + O::GEngine);
        if (eng && eng > 0x10000000 && eng < 0x7FFFFFFFFFFF) {
            uintptr_t vp = Read<uintptr_t>(eng + O::GEngine_GameViewport);
            if (vp && vp > 0x10000000 && vp < 0x7FFFFFFFFFFF)
                gworld = Read<uintptr_t>(vp + O::GameViewport_World);
        }
    }
    if (!gworld || gworld < 0x10000000 || gworld >= 0x7FFFFFFFFFFF) { g_camera.valid = false; return; }
    uintptr_t gameInstance = Read<uintptr_t>(gworld + O::UWorld_OwningGameInstance);
    if (!gameInstance) { g_camera.valid = false; return; }
    uintptr_t localPlayers = Read<uintptr_t>(gameInstance + O::GameInstance_LocalPlayers);
    if (!localPlayers) { g_camera.valid = false; return; }
    uintptr_t localPlayer = Read<uintptr_t>(localPlayers);
    if (!localPlayer) { g_camera.valid = false; return; }
    uintptr_t playerController = Read<uintptr_t>(localPlayer + O::UPlayer_PlayerController);
    if (!playerController) { g_camera.valid = false; return; }
    uintptr_t camMgr = Read<uintptr_t>(playerController + O::PC_CameraManager);
    if (!camMgr) { g_camera.valid = false; return; }

    g_camera.timeSeconds = Read<float>(gworld + O::UWorld_TimeSeconds);

    // Match worker thread camera source order: CameraCachePrivate → LastFrameCache → ViewTarget → PendingViewTarget
    uintptr_t povBase = 0;
    DVec3 loc{};
    float fov = 0.f;
    for (uintptr_t camOff : {(uintptr_t)0x1560, (uintptr_t)0x1E40, (uintptr_t)0x350, (uintptr_t)0xC40}) {
        uintptr_t pb = camMgr + camOff + O::CameraCache_POV;
        DVec3 tl = Read<DVec3>(pb + O::POV_Location);
        float tf = Read<float>(pb + O::POV_FOV);
        if (tf > 1.f && tf < 170.f && (tl.x != 0 || tl.y != 0 || tl.z != 0)) {
            povBase = pb;
            loc = tl;
            fov = tf;
            break;
        }
    }
    if (!povBase) { g_camera.valid = false; return; }

    g_camera.location = loc;
    g_camera.rotation = Read<DRotator>(povBase + O::POV_Rotation);
    g_camera.fov = fov;
    g_camera.aspectRatio = Read<float>(povBase + O::POV_AspectRatio);
    if (g_camera.fov < 1.f || g_camera.fov > 170.f) g_camera.fov = 90.f;
    if (g_camera.aspectRatio <= 0.f) g_camera.aspectRatio = 16.f / 9.f;
    g_camera.valid = true;
}

static bool WorldToScreen(const DVec3& world, Vec2& screen) {
    if (!g_camera.valid) return false;
    DVec3 delta = world - g_camera.location;
    double dist = delta.length();
    if (dist <= 0.0 || dist > 10000000.0) return false;

    double sp = sin(g_camera.rotation.pitch * DEG2RAD);
    double cp = cos(g_camera.rotation.pitch * DEG2RAD);
    double sy = sin(g_camera.rotation.yaw * DEG2RAD);
    double cy = cos(g_camera.rotation.yaw * DEG2RAD);
    double sr = sin(g_camera.rotation.roll * DEG2RAD);
    double cr = cos(g_camera.rotation.roll * DEG2RAD);

    // Rotation matrix axes
    DVec3 axisX = {cp*cy, cp*sy, sp};
    DVec3 axisY = {sr*sp*cy - cr*sy, sr*sp*sy + cr*cy, -sr*cp};
    DVec3 axisZ = {-(cr*sp*cy + sr*sy), cy*sr - cr*sp*sy, cr*cp};

    double transformed_x = delta.dot(axisY);
    double transformed_y = delta.dot(axisZ);
    double transformed_z = delta.dot(axisX);
    if (transformed_z < 1.0) return false;

    double fovRad = (double)g_camera.fov * DEG2RAD;
    double fovTan = tan(fovRad * 0.5);
    if (fovTan <= 0.0) return false;

    double cx = (double)g_screenW * 0.5;
    double cy2 = (double)g_screenH * 0.5;

    screen.x = (float)(cx + (transformed_x / fovTan) * cx / transformed_z);
    screen.y = (float)(cy2 - (transformed_y / fovTan) * cx / transformed_z);

    if (screen.x != screen.x || screen.y != screen.y) return false;
    if (screen.x < -(float)g_screenW || screen.x > (float)g_screenW * 2.f) return false;
    if (screen.y < -(float)g_screenH || screen.y > (float)g_screenH * 2.f) return false;
    return true;
}

// ============================================================================
// Bone indices — resolved dynamically per actor class
// ============================================================================
struct BoneIndices {
    int head = -1, neck = -1;
    int spine3 = -1, spine2 = -1, spine1 = -1, pelvis = -1;
    int lUpperArm = -1, lForearm = -1, lHand = -1;
    int rUpperArm = -1, rForearm = -1, rHand = -1;
    int lThigh = -1, lCalf = -1, lFoot = -1;
    int rThigh = -1, rCalf = -1, rFoot = -1;
    int lClavicle = -1, rClavicle = -1;
    int lToe = -1, rToe = -1;
    std::vector<int32_t> parentChain;
    bool valid = false;

    int ToSlot(int slot) const {
        const int* map[] = {
            &head, &neck, &spine3, &spine2, &spine1, &pelvis,
            &lUpperArm, &lForearm, &lHand, &rUpperArm, &rForearm, &rHand,
            &lThigh, &lCalf, &lFoot, &rThigh, &rCalf, &rFoot,
            &lClavicle, &rClavicle, &lToe, &rToe
        };
        return (slot >= 0 && slot < 22) ? *map[slot] : -1;
    }
};

// ============================================================================
// Player data
// ============================================================================
struct PlayerData {
    uintptr_t pawn;
    uintptr_t mesh;
    uintptr_t rootComp;
    bool isAttached;
    DVec3 position;
    DVec3 headPos;
    Vec2 screenHead;
    Vec2 screenFeet;
    float health;
    float maxHealth;
    bool isTeammate;
    bool isValid;
    bool isDowned;
    bool isDead;
    bool isInvincible;
    bool isADS;
    bool healthValid;
    uint8_t bleedoutState;
    float giveUpTime;
    uint32_t factionId;
    std::string name;
    std::string clanTag;
    DVec3 bones[22];
    int boneCount;
    float distance;
    bool visible;
    bool aimVisible;
    BoneIndices resolvedBones;
    std::string factionName;
    std::string weaponName;
    DRotator viewRotation;
    bool hasViewDir;
    bool isAdmin;
    bool isDeveloper;
    bool isBot;
    uint8_t ping;
    bool isInVehicle;
    DVec3 velocity;
    uint8_t stance; // 0=Prone, 1=Crouch, 2=Stand, 3=unknown
    bool isSprinting;
    bool isTacSprinting;
};

struct VehicleData {
    uintptr_t actor;
    DVec3 position;
    Vec2 screenPos;
    std::string typeName;
    int occupantCount;
    float distance;
    bool isValid;
    bool isDestroyed;
    bool engineRunning;
    bool isAir;
    bool isTeamVehicle;
    bool isNeutral;
    bool hasLocalPlayer;
    float speed;
    DVec3 velocity;
    std::vector<std::string> occupantNames;
};

struct WorldItemData {
    DVec3 position;
    Vec2 screenPos;
    std::string name;
    float distance;
    bool isValid;
    int type;    // 0=droppedItem, 1=mine, 2=emplacement, 3=FOB
    int subType; // for type==0: 0=weapon, 1=ammo, 2=attachment, 3=medical, 4=grenade, 5=other
};

static std::vector<PlayerData> g_players; // render-thread only — written from snapshot
static std::vector<VehicleData> g_vehicles;
static std::vector<WorldItemData> g_worldItems;
static uintptr_t g_localPawn = 0;
static uint32_t g_localFactionId = 0;
static std::string g_localFactionName;
static uintptr_t g_localFactionObj = 0;
static uintptr_t g_localFactionData = 0;
static uint8_t g_localTeamId = 0xFF;
static uint64_t g_localSquadLo = 0, g_localSquadHi = 0;
static bool g_localSquadded = false;
static float g_localBulletSpeed = 0.f;
static bool g_localIsADS = false;

// Worker-thread player list — never touched by render thread
static std::vector<PlayerData> s_workerPlayers;
static std::vector<VehicleData> s_workerVehicles;
static std::vector<WorldItemData> s_workerWorldItems;

// Hitmarker state
static std::atomic<float> g_hitmarkerTimer{0.f};
static std::chrono::steady_clock::time_point g_lastFrameTime = std::chrono::steady_clock::now();
static float g_deltaTime = 0.f;

struct PrevHealthEntry { uintptr_t pawn; float health; };
static std::vector<PrevHealthEntry> s_workerPrevHealth;
static int s_lastDeadCount = 0;

// ============================================================================
// Entity cache — background thread scans entities, render reads snapshot
// ============================================================================
namespace EntityCache {
    struct Snapshot {
        std::vector<PlayerData> players;
        std::vector<VehicleData> vehicles;
        std::vector<WorldItemData> worldItems;
        uintptr_t localPawn = 0;
        uint64_t localFaction = 0;
        uintptr_t localFactionObj = 0;
        uintptr_t localFactionData = 0;
        std::chrono::steady_clock::time_point timestamp;
    };

    inline std::atomic<bool> g_running{false};
    static std::shared_ptr<Snapshot> g_snapshot;
    static std::mutex g_mutex;
    static std::thread g_thread;

    static std::shared_ptr<Snapshot> GetSnapshot() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_snapshot ? g_snapshot : std::make_shared<Snapshot>();
    }

    // Forward declaration — implemented after all helper functions
    static void WorkerThread();

    static void Start() {
        g_running = true;
        g_thread = std::thread(WorkerThread);
    }

    static void Stop() {
        g_running = false;
        if (g_thread.joinable()) g_thread.join();
    }
}

// ============================================================================
// FString reader (UE5 — UTF-16)
// ============================================================================
static std::string ReadFString(uintptr_t addr) {
    uintptr_t data = Read<uintptr_t>(addr);
    int len = Read<int>(addr + 8);
    if (!data || data < 0x10000 || data > 0x7FFFFFFFFFFF || len <= 0 || len > 128) return "";
    std::wstring ws(len, L'\0');
    ReadRaw(data, ws.data(), len * 2);
    std::string result;
    result.reserve(len);
    for (int i = 0; i < len && ws[i]; i++) {
        wchar_t c = ws[i];
        if (c >= 0x20 && c < 0x7F) result += (char)c;
        else if (c >= 0x7F) result += '?';
        else break;
    }
    return result;
}

// ============================================================================
// FName resolution via GNames pool — tries 3 pool layouts for cross-build compat
// ============================================================================
static std::unordered_map<uint32_t, std::string> s_fnameCache;
static auto s_fnameCacheTime = std::chrono::steady_clock::now();

static std::string ResolveFName(int32_t comparisonIndex) {
    if (comparisonIndex <= 0) return "";
    uint32_t id = (uint32_t)comparisonIndex;
    if ((id >> 16) > 8191) return "";

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - s_fnameCacheTime).count() > 60) {
        s_fnameCache.clear();
        s_fnameCacheTime = now;
    }
    auto it = s_fnameCache.find(id);
    if (it != s_fnameCache.end()) return it->second;

    uintptr_t pool = g_base + g_gnamesOff;

    struct Layout { int table, stride, header, data; };
    for (auto layout : {Layout{0,2,0,2}, Layout{0,1,0,2}, Layout{0x10,2,0,2}, Layout{0x10,8,8,0xC}}) {
        uintptr_t block = Read<uintptr_t>(pool + layout.table + (uint64_t)(id >> 16) * 8);
        if (!block || block < 0x10000000 || block >= 0x7FFFFFFFFFFF) continue;

        uintptr_t entry = block + (uint64_t)(id & 0xFFFF) * layout.stride;
        uint16_t hdr = Read<uint16_t>(entry + layout.header);
        int len = hdr >> 6;
        if (len <= 0 || len > 512) continue;

        std::string candidate;
        if (hdr & 1) {
            wchar_t wbuf[513] = {};
            ReadRaw(entry + layout.data, wbuf, len * 2);
            candidate.resize(len);
            for (int i = 0; i < len; i++) candidate[i] = (char)(wbuf[i] & 0xFF);
        } else {
            char buf[513] = {};
            ReadRaw(entry + layout.data, buf, len);
            candidate.assign(buf, len);
        }

        if (candidate.empty()) continue;
        bool valid = true;
        for (unsigned char c : candidate) {
            if (c < 32 || c == 127) { valid = false; break; }
        }
        if (!valid) continue;
        s_fnameCache[id] = candidate;
        return candidate;
    }
    return "";
}

static std::string ResolveFactionName(uint32_t factionId) {
    if (!factionId) return "";
    std::string tagStr = ResolveFName((int32_t)factionId);
    if (tagStr.empty() || tagStr == "None") return "";
    size_t dot = tagStr.rfind('.');
    if (dot != std::string::npos) return tagStr.substr(dot + 1);
    return tagStr;
}

static ImU32 GetFactionColor(const std::string& faction) {
    if (faction.find("Lonestar") != std::string::npos || faction.find("lonestar") != std::string::npos)
        return IM_COL32(255, 200, 50, 255);  // gold/yellow
    if (faction.find("Valkyra") != std::string::npos || faction.find("valkyra") != std::string::npos)
        return IM_COL32(100, 160, 255, 255);  // blue
    if (faction.find("Manticore") != std::string::npos || faction.find("manticore") != std::string::npos)
        return IM_COL32(220, 60, 60, 255);    // red
    return IM_COL32(200, 200, 200, 255);       // unknown faction — gray
}

// ============================================================================
// Dynamic bone cache — resolves bone indices by name from SkinnedAsset
// Different actor classes can have different bone layouts.
// ============================================================================
static std::unordered_map<uintptr_t, BoneIndices> g_boneCache;
static std::chrono::steady_clock::time_point g_boneCacheTime = std::chrono::steady_clock::now();
static std::unordered_set<uintptr_t> g_dummyClasses;
static std::unordered_set<uintptr_t> g_playerClasses;
static std::unordered_set<uintptr_t> g_vehicleClasses;
static std::unordered_set<uintptr_t> g_nonVehicleClasses;

static void WriteStartupLog(const char* stage, const char* detail = nullptr);

static BoneIndices ResolveBones(uintptr_t skinnedAsset) {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - g_boneCacheTime).count() > 30) {
        g_boneCache.clear();
        g_boneCacheTime = now;
    }
    auto it = g_boneCache.find(skinnedAsset);
    if (it != g_boneCache.end()) return it->second;

    BoneIndices result;
    if (!skinnedAsset) { g_boneCache[skinnedAsset] = result; return result; }

    uintptr_t refSkel = skinnedAsset + O::SkinnedAsset_RefSkeleton;
    uintptr_t boneData = Read<uintptr_t>(refSkel + 0x20);
    int32_t boneCount = Read<int32_t>(refSkel + 0x28);

    int32_t boneCap = Read<int32_t>(refSkel + 0x2C);
    if (!boneData || boneData < 0x10000000 || boneData >= 0x7FFFFFFFFFFF ||
        boneCount < 16 || boneCount > 1024 || boneCap < boneCount) {
        g_boneCache[skinnedAsset] = result;
        return result;
    }

    struct BoneInfo { uint32_t name, number; int32_t parent; };
    static_assert(sizeof(BoneInfo) == 12);
    std::vector<BoneInfo> infos(boneCount);
    ReadRaw(boneData, infos.data(), boneCount * sizeof(BoneInfo));

    result.parentChain.resize(boneCount);
    for (int i = 0; i < boneCount; i++) {
        if (infos[i].parent >= i || infos[i].parent < -1) {
            g_boneCache[skinnedAsset] = BoneIndices{};
            return BoneIndices{};
        }
        result.parentChain[i] = infos[i].parent;
        std::string name = ResolveFName((int32_t)infos[i].name);
        if (name.empty()) continue;

        std::string lower = name;
        for (auto& c : lower) { if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); if (c == '-' || c == ' ') c = '_'; }

        if (lower == "head" || lower == "head_01" || lower == "head_m" || lower == "ik_head" || lower == "b_head" || lower == "head_end" || lower == "bip01_head" || lower == "def_head")
            { if (result.head < 0) result.head = i; }
        else if (lower == "neck_01" || lower == "neck01" || lower == "neck" || lower == "neck_02" || lower == "bip01_neck" || lower == "def_neck")
            { if (result.neck < 0) result.neck = i; }
        else if (lower == "spine_05" || lower == "spine_04" || lower == "spine_03" || lower == "spine03" || lower == "spine3" || lower == "spine_3" || lower == "bip01_spine3")
            { if (result.spine3 < 0) result.spine3 = i; }
        else if (lower == "spine_02" || lower == "spine2" || lower == "spine_2" || lower == "bip01_spine2")
            { if (result.spine2 < 0) result.spine2 = i; }
        else if (lower == "spine_01" || lower == "spine01" || lower == "spine1" || lower == "spine_1" || lower == "spine" || lower == "bip01_spine1")
            { if (result.spine1 < 0) result.spine1 = i; }
        else if (lower == "pelvis" || lower == "hips" || lower == "hip" || lower == "pelvis_01" || lower == "bip01_pelvis" || lower == "root")
            { if (result.pelvis < 0) result.pelvis = i; }
        else if (lower == "upperarm_l" || lower == "upper_arm_l" || lower == "l_upperarm" || lower == "leftshoulder" || lower == "def_upperarm_l")
            { if (result.lUpperArm < 0) result.lUpperArm = i; }
        else if (lower == "lowerarm_l" || lower == "forearm_l" || lower == "l_lowerarm" || lower == "leftforearm" || lower == "def_lowerarm_l")
            { if (result.lForearm < 0) result.lForearm = i; }
        else if (lower == "hand_l" || lower == "handleft" || lower == "l_hand" || lower == "lefthand" || lower == "def_hand_l")
            { if (result.lHand < 0) result.lHand = i; }
        else if (lower == "upperarm_r" || lower == "upper_arm_r" || lower == "r_upperarm" || lower == "rightshoulder" || lower == "def_upperarm_r")
            { if (result.rUpperArm < 0) result.rUpperArm = i; }
        else if (lower == "lowerarm_r" || lower == "forearm_r" || lower == "r_lowerarm" || lower == "rightforearm" || lower == "def_lowerarm_r")
            { if (result.rForearm < 0) result.rForearm = i; }
        else if (lower == "hand_r" || lower == "handright" || lower == "r_hand" || lower == "righthand" || lower == "def_hand_r")
            { if (result.rHand < 0) result.rHand = i; }
        else if (lower == "thigh_l" || lower == "thighleft" || lower == "l_thigh" || lower == "leftupleg" || lower == "def_thigh_l")
            { if (result.lThigh < 0) result.lThigh = i; }
        else if (lower == "calf_l" || lower == "calfleft" || lower == "l_calf" || lower == "leftleg" || lower == "def_calf_l")
            { if (result.lCalf < 0) result.lCalf = i; }
        else if (lower == "foot_l" || lower == "footleft" || lower == "l_foot" || lower == "leftfoot" || lower == "def_foot_l")
            { if (result.lFoot < 0) result.lFoot = i; }
        else if (lower == "thigh_r" || lower == "thighright" || lower == "r_thigh" || lower == "rightupleg" || lower == "def_thigh_r")
            { if (result.rThigh < 0) result.rThigh = i; }
        else if (lower == "calf_r" || lower == "calfright" || lower == "r_calf" || lower == "rightleg" || lower == "def_calf_r")
            { if (result.rCalf < 0) result.rCalf = i; }
        else if (lower == "foot_r" || lower == "footright" || lower == "r_foot" || lower == "rightfoot" || lower == "def_foot_r")
            { if (result.rFoot < 0) result.rFoot = i; }
        else if (lower == "clavicle_l" || lower == "clavicleleft" || lower == "l_clavicle" || lower == "def_clavicle_l")
            { if (result.lClavicle < 0) result.lClavicle = i; }
        else if (lower == "clavicle_r" || lower == "clavicleright" || lower == "r_clavicle" || lower == "def_clavicle_r")
            { if (result.rClavicle < 0) result.rClavicle = i; }
        else if (lower == "ball_l" || lower == "l_ball" || lower == "def_ball_l" || lower == "toe_l")
            { if (result.lToe < 0) result.lToe = i; }
        else if (lower == "ball_r" || lower == "r_ball" || lower == "def_ball_r" || lower == "toe_r")
            { if (result.rToe < 0) result.rToe = i; }
    }

    result.valid = (result.head >= 0 && result.pelvis >= 0);

    // w00t3d tip: if any resolved index exceeds boneCount, discard entire mapping
    auto checkIdx = [&](int idx) { return idx < 0 || idx < boneCount; };
    if (!(checkIdx(result.head) && checkIdx(result.neck) && checkIdx(result.spine3) &&
          checkIdx(result.spine2) && checkIdx(result.spine1) && checkIdx(result.pelvis) &&
          checkIdx(result.lUpperArm) && checkIdx(result.lForearm) && checkIdx(result.lHand) &&
          checkIdx(result.rUpperArm) && checkIdx(result.rForearm) && checkIdx(result.rHand) &&
          checkIdx(result.lThigh) && checkIdx(result.lCalf) && checkIdx(result.lFoot) &&
          checkIdx(result.rThigh) && checkIdx(result.rCalf) && checkIdx(result.rFoot) &&
          checkIdx(result.lClavicle) && checkIdx(result.rClavicle) &&
          checkIdx(result.lToe) && checkIdx(result.rToe))) {
        result = BoneIndices{};
    }

    static bool s_boneDumped = false;
    if (!s_boneDumped) {
        s_boneDumped = true;
        char msg[256];
        sprintf_s(msg, "boneCount=%d valid=%d head=%d neck=%d pelvis=%d lThigh=%d", boneCount, result.valid ? 1 : 0,
            result.head, result.neck, result.pelvis, result.lThigh);
        WriteStartupLog("BoneResolve", msg);
        int dumpMax = (boneCount < 20) ? boneCount : 20;
        for (int d = 0; d < dumpMax; d++) {
            int32_t ni = Read<int32_t>(boneData + (uintptr_t)d * O::BONE_INFO_STRIDE);
            int32_t pi = Read<int32_t>(boneData + (uintptr_t)d * O::BONE_INFO_STRIDE + 0x8);
            std::string bn = ResolveFName(ni);
            char bm[128];
            sprintf_s(bm, "  [%d] %s (parent=%d)", d, bn.c_str(), pi);
            WriteStartupLog("BoneDump", bm);
        }
    }

    g_boneCache[skinnedAsset] = result;
    return result;
}

// ============================================================================
// Actor class filter — identifies shooting range dummies by class name
// ============================================================================
static bool IsActorDummy(uintptr_t actor) {
    uintptr_t actorClass = Read<uintptr_t>(actor + O::UObject_ClassPrivate);
    if (!actorClass) return true;

    if (g_dummyClasses.count(actorClass)) return true;
    if (g_playerClasses.count(actorClass)) return false;

    int32_t classNameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
    std::string className = ResolveFName(classNameIdx);

    if (className.empty()) return true;

    if (className.find("TargetDummy") != std::string::npos ||
        className.find("ShootingRange") != std::string::npos ||
        className.find("Emplacement") != std::string::npos ||
        className.find("Deployable") != std::string::npos ||
        className.find("Projectile") != std::string::npos ||
        className.find("DroppedItem") != std::string::npos ||
        className.find("Pickup") != std::string::npos ||
        className.find("Buildable") != std::string::npos ||
        className.find("Sandbag") != std::string::npos ||
        className.find("HESCO") != std::string::npos ||
        className.find("Hesco") != std::string::npos ||
        className.find("RazorWire") != std::string::npos ||
        className.find("Fortification") != std::string::npos ||
        className.find("Structure") != std::string::npos ||
        className.find("Bunker") != std::string::npos ||
        className.find("Tower") != std::string::npos ||
        className.find("FOB") != std::string::npos ||
        className.find("ForwardOperating") != std::string::npos ||
        className.find("ForwardBase") != std::string::npos ||
        className.find("HAB") != std::string::npos ||
        className.find("SpawnPoint") != std::string::npos ||
        className.find("Placeable") != std::string::npos ||
        className.find("Inventory") != std::string::npos ||
        className.find("Container") != std::string::npos ||
        className.find("Mortar") != std::string::npos ||
        className.find("Radio") != std::string::npos) {
        g_dummyClasses.insert(actorClass);
        return true;
    }

    bool isPlayerClass = (className.find("Mover") != std::string::npos ||
                          className.find("Character") != std::string::npos);
    if (!isPlayerClass) {
        g_dummyClasses.insert(actorClass);
        return true;
    }

    g_playerClasses.insert(actorClass);
    return false;
}

static bool IsVehicleActor(uintptr_t actor, std::string& outName) {
    uintptr_t actorClass = Read<uintptr_t>(actor + O::UObject_ClassPrivate);
    if (!actorClass) return false;
    if (g_nonVehicleClasses.count(actorClass)) return false;
    if (g_vehicleClasses.count(actorClass)) {
        int32_t nameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
        outName = ResolveFName(nameIdx);
        return true;
    }

    int32_t nameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
    std::string className = ResolveFName(nameIdx);

    if (className.find("GC_") == 0 ||
        className.find("MDestroyed") == 0 ||
        className.find("PostDestroyed") != std::string::npos ||
        className.find("HIDZ") != std::string::npos ||
        className.find("DustKickup") != std::string::npos ||
        className.find("Moving_WHL") != std::string::npos ||
        className.find("Niagara") != std::string::npos ||
        className.find("Particle") != std::string::npos ||
        className.find("Decal") != std::string::npos ||
        className.find("Carcass") != std::string::npos ||
        className.find("NavData") != std::string::npos ||
        className.find("AbstractNav") != std::string::npos ||
        className.find("Spectator") != std::string::npos) {
        g_nonVehicleClasses.insert(actorClass);
        return false;
    }

    bool nameMatch = className.find("Pawn") != std::string::npos ||
                     className.find("WDWheeledVehicle") != std::string::npos ||
                     className.find("WDRotary") != std::string::npos ||
                     className.find("WDStationary") != std::string::npos ||
                     className.find("BHBaseVehicle") != std::string::npos ||
                     className.find("Vehicle") != std::string::npos ||
                     className.find("Truck") != std::string::npos ||
                     className.find("APC") != std::string::npos ||
                     className.find("Helicopter") != std::string::npos ||
                     className.find("Heli") != std::string::npos ||
                     className.find("Rotary") != std::string::npos ||
                     className.find("Boat") != std::string::npos ||
                     className.find("Tank") != std::string::npos ||
                     className.find("Aircraft") != std::string::npos ||
                     className.find("Chinook") != std::string::npos ||
                     className.find("Blackhawk") != std::string::npos ||
                     className.find("Transport") != std::string::npos ||
                     className.find("Aviation") != std::string::npos ||
                     className.find("Airplane") != std::string::npos ||
                     className.find("ROT_") != std::string::npos;

    if (nameMatch) {
        g_vehicleClasses.insert(actorClass);
        outName = className;
        return true;
    }

    if (className.find("MoverPlayer") != std::string::npos ||
        className.find("MoverCharacter") != std::string::npos ||
        className.find("MoverPawn") != std::string::npos ||
        className.find("PlayerCharacter") != std::string::npos ||
        className.find("Soldier") != std::string::npos ||
        className.find("Infantry") != std::string::npos) {
        g_nonVehicleClasses.insert(actorClass);
        return false;
    }

    // Walk superclass chain — catches ALL blueprint vehicles (they inherit BHBaseVehiclePawn)
    constexpr uintptr_t USTRUCT_SUPER = 0x40;
    uintptr_t superClass = Read<uintptr_t>(actorClass + USTRUCT_SUPER);
    for (int depth = 0; depth < 8 && superClass && superClass > 0x10000000 && superClass < 0x7FFFFFFFFFFF; depth++) {
        int32_t superNameIdx = Read<int32_t>(superClass + O::UObject_NamePrivate);
        std::string superName = ResolveFName(superNameIdx);
        if (superName.find("BHBaseVehicle") != std::string::npos ||
            superName.find("ModularVehicle") != std::string::npos ||
            superName.find("WheeledVehicle") != std::string::npos) {
            g_vehicleClasses.insert(actorClass);
            outName = className;
            return true;
        }
        if (superName == "Actor" || superName == "Pawn" || superName == "Object") break;
        superClass = Read<uintptr_t>(superClass + USTRUCT_SUPER);
    }

    // Seat component fallback — don't cache negative results at class level (instance-specific)
    uintptr_t seatComp = Read<uintptr_t>(actor + O::Vehicle_SeatComponent);
    if (!seatComp || seatComp < 0x10000000 || seatComp >= 0x7FFFFFFFFFFF) {
        g_nonVehicleClasses.insert(actorClass);
        return false;
    }
    int32_t seatOccCount = Read<int32_t>(seatComp + O::SeatComp_Occupants + 8);
    if (seatOccCount < 0 || seatOccCount > 30) {
        g_nonVehicleClasses.insert(actorClass);
        return false;
    }

    g_vehicleClasses.insert(actorClass);
    outName = className;
    return true;
}

static bool IsVehicleAir(const std::string& name) {
    return name.find("Helicopter") != std::string::npos ||
           name.find("Heli") != std::string::npos ||
           name.find("Rotary") != std::string::npos ||
           name.find("Aircraft") != std::string::npos ||
           name.find("Plane") != std::string::npos ||
           name.find("Chinook") != std::string::npos ||
           name.find("Blackhawk") != std::string::npos ||
           name.find("Aviation") != std::string::npos ||
           name.find("Airplane") != std::string::npos ||
           name.find("ROT_") != std::string::npos;
}

static std::string ResolveVehicleName(const std::string& className) {
    struct VehicleNameEntry { const char* pattern; const char* display; };
    static const VehicleNameEntry MAP[] = {
        {"Bobcat",      "Bobcat"},
        {"Kodiak",      "Kodiak"},
        {"Humvee",      "Humvee"},
        {"HMMWV",       "Humvee"},
        {"L2A6",        "Leopard 2A6"},
        {"Leopard",     "Leopard 2A6"},
        {"Abrams",      "M1 Abrams"},
        {"M1A2",        "M1 Abrams"},
        {"T90",         "T-90"},
        {"T72",         "T-72"},
        {"BMP",         "BMP"},
        {"BTR",         "BTR"},
        {"LAV",         "LAV-25"},
        {"Stryker",     "Stryker"},
        {"Bradley",     "M2 Bradley"},
        {"BlackHawk",   "Black Hawk"},
        {"Blackhawk",   "Black Hawk"},
        {"Apache",      "AH-64 Apache"},
        {"AH64",        "AH-64 Apache"},
        {"Chinook",     "CH-47 Chinook"},
        {"LittleBird",  "MH-6 Little Bird"},
        {"MH6",         "MH-6 Little Bird"},
        {"Hind",        "Mi-24 Hind"},
        {"Mi24",        "Mi-24 Hind"},
        {"Havoc",       "Mi-28 Havoc"},
        {"Mi28",        "Mi-28 Havoc"},
        {"Ka52",        "Ka-52"},
        {"Osprey",      "V-22 Osprey"},
        {"UH60",        "UH-60 Black Hawk"},
        {"Mi8",         "Mi-8 Hip"},
        {"Mi17",        "Mi-17 Hip"},
        {"Huey",        "UH-1 Huey"},
        {"UH1",         "UH-1 Huey"},
        {"Viper",       "AH-1Z Viper"},
        {"AH1",         "AH-1Z Viper"},
        {"Cobra",       "AH-1 Cobra"},
        {"Merlin",      "AW101 Merlin"},
        {"Puma",        "SA 330 Puma"},
        {"Scout",       "Scout Heli"},
        {"Helicopter",  "Helicopter"},
        {"Heli",        "Helicopter"},
        {"MRAP",        "MRAP"},
        {"Tigr",        "Tigr"},
        {"BRDM",        "BRDM-2"},
        {"Mortar",      "Mortar"},
        {"Technical",   "Technical"},
        {"Pickup",      "Technical"},
        {"Truck",       "Truck"},
        {"Logistics",   "Logistics"},
        {"Transport",   "Transport"},
        {"SPAA",        "SPAA"},
        {"ZSU",         "ZSU-23-4"},
        {"Tunguska",    "Tunguska"},
        {"MTLB",        "MT-LB"},
        {"M113",        "M113"},
        {"Warrior",     "FV510 Warrior"},
        {"Cougar",      "Cougar"},
        {"JLTV",        "JLTV Oshkosh"},
        {"Bike",        "Dirt Bike"},
        {"ATV",         "ATV"},
        {"Quad",        "ATV"},
        {"Boat",        "Boat"},
        {"RHIB",        "RHIB"},
        {"WDRotary",    "Helicopter"},
        {"RotaryVehicle","Helicopter"},
        {"WheeledVehiclePawn","Vehicle"},
        {"StationaryVehicle","Emplacement"},
        {"WDStationary","Emplacement"},
    };
    for (auto& e : MAP)
        if (className.find(e.pattern) != std::string::npos)
            return e.display;
    return className;
}

static std::unordered_set<uintptr_t> g_droppedItemClasses;
static std::unordered_set<uintptr_t> g_mineClasses;
static std::unordered_set<uintptr_t> g_emplacementClasses;
static std::unordered_set<uintptr_t> g_fobClasses;
static std::unordered_set<uintptr_t> g_bodybagClasses;
static std::unordered_set<uintptr_t> g_nonWorldClasses;

static int IsWorldItemActor(uintptr_t actor, std::string& outName) {
    uintptr_t actorClass = Read<uintptr_t>(actor + O::UObject_ClassPrivate);
    if (!actorClass) return -1;
    if (g_nonWorldClasses.count(actorClass)) return -1;
    if (g_droppedItemClasses.count(actorClass)) {
        int32_t nameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
        outName = ResolveFName(nameIdx);
        return 0;
    }
    if (g_mineClasses.count(actorClass)) {
        int32_t nameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
        outName = ResolveFName(nameIdx);
        return 1;
    }
    if (g_emplacementClasses.count(actorClass)) {
        int32_t nameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
        outName = ResolveFName(nameIdx);
        return 2;
    }
    if (g_fobClasses.count(actorClass)) {
        int32_t nameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
        outName = ResolveFName(nameIdx);
        return 3;
    }
    if (g_bodybagClasses.count(actorClass)) {
        outName = "Death Loot";
        return 4;
    }

    int32_t nameIdx = Read<int32_t>(actorClass + O::UObject_NamePrivate);
    std::string className = ResolveFName(nameIdx);

    if (className.find("DroppedItem") != std::string::npos) {
        g_droppedItemClasses.insert(actorClass);
        outName = className;
        return 0;
    }
    if (className.find("Landmine") != std::string::npos ||
        className.find("Claymore") != std::string::npos ||
        className.find("_Mine") != std::string::npos ||
        (className.find("Explosive") != std::string::npos &&
         className.find("Barrel") == std::string::npos &&
         className.find("Projectile") == std::string::npos &&
         className.find("Effect") == std::string::npos)) {
        g_mineClasses.insert(actorClass);
        outName = className;
        return 1;
    }
    if (className.find("Emplacement") != std::string::npos ||
        className.find("Mortar") != std::string::npos ||
        className.find("HMG") != std::string::npos ||
        className.find("TOW") != std::string::npos ||
        className.find("Placeable") != std::string::npos) {
        g_emplacementClasses.insert(actorClass);
        outName = className;
        return 2;
    }
    if (className.find("FOB") != std::string::npos ||
        className.find("ForwardOperating") != std::string::npos ||
        className.find("ForwardBase") != std::string::npos ||
        className.find("HAB") != std::string::npos ||
        className.find("SpawnPoint") != std::string::npos) {
        g_fobClasses.insert(actorClass);
        outName = className;
        return 3;
    }

    if (className.find("PlayerInventoryContainer") != std::string::npos ||
        className.find("DeathContainer") != std::string::npos ||
        className.find("DroppedInventoryContainer") != std::string::npos ||
        className.find("PhysicalContainer") != std::string::npos ||
        className.find("DeathBag") != std::string::npos ||
        className.find("BodyBag") != std::string::npos ||
        className.find("Bodybag") != std::string::npos ||
        className.find("DeadBody") != std::string::npos ||
        className.find("Corpse") != std::string::npos ||
        className.find("DeathLoot") != std::string::npos ||
        className.find("LootBag") != std::string::npos ||
        className.find("DroppedLoot") != std::string::npos) {
        g_bodybagClasses.insert(actorClass);
        outName = "Death Loot";
        return 4;
    }

    g_nonWorldClasses.insert(actorClass);
    return -1;
}

static int ClassifyDroppedItem(const std::string& tag) {
    auto has = [&](const char* s) {
        for (size_t i = 0; i < tag.size(); i++) {
            size_t j = 0;
            while (s[j] && i + j < tag.size() && (tag[i+j] == s[j] || (tag[i+j] >= 'A' && tag[i+j] <= 'Z' && tag[i+j]+32 == s[j]) || (tag[i+j] >= 'a' && tag[i+j] <= 'z' && tag[i+j]-32 == s[j]))) j++;
            if (!s[j]) return true;
        }
        return false;
    };
    if (has("Rifle") || has("Pistol") || has("Shotgun") || has("Sniper") || has("SMG") ||
        has("LMG") || has("Launcher") || has("Weapon") || has("Carbine") || has("DMR") ||
        has("Marksman") || has("Machinegun") || has("MachineGun") || has("RPG") || has("AT4") ||
        has("MAAWS") || has("Javelin") || has("Gun")) return 0;
    if (has("Ammo") || has("Magazine") || has("Mag") || has("Round") || has("Munition")) return 1;
    if (has("Scope") || has("Sight") || has("Grip") || has("Muzzle") || has("Suppressor") ||
        has("Attachment") || has("Optic") || has("Bipod") || has("Foregrip") || has("Barrel") ||
        has("Stock") || has("Laser") || has("Flashlight")) return 2;
    if (has("Medkit") || has("Bandage") || has("Medical") || has("Health") || has("FirstAid") ||
        has("Heal") || has("Tourniquet")) return 3;
    if (has("Grenade") || has("Smoke") || has("Flash") || has("Frag") || has("Molotov") ||
        has("Throwable") || has("C4") || has("TNT") || has("IED")) return 4;
    return 5;
}

// ============================================================================
// Startup log — writes timestamped messages for diagnosing issues
// ============================================================================
static void WriteStartupLog(const char* stage, const char* detail) {
    char logPath[MAX_PATH];
    GetModuleFileNameA(nullptr, logPath, MAX_PATH);
    std::string lp(logPath);
#ifdef UNBRANDED
    lp = lp.substr(0, lp.find_last_of("\\/") + 1) + "Overlay_Startup.log";
#else
    lp = lp.substr(0, lp.find_last_of("\\/") + 1) + "TakePeek_Startup.log";
#endif
    std::ofstream lf(lp, std::ios::app);
    if (lf.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char tb[64]; ctime_s(tb, sizeof(tb), &t);
        tb[strlen(tb)-1] = '\0';
        lf << "[" << tb << "] " << stage;
        if (detail) lf << " — " << detail;
        lf << std::endl;
    }
}

// ============================================================================
// Pattern scanner — auto-resolve globals after game patches
// ============================================================================
static uintptr_t PatternScan(uintptr_t start, size_t size,
                              const uint8_t* pattern, const char* mask, size_t patLen) {
    const size_t CHUNK = 0x10000;
    std::vector<uint8_t> buf(CHUNK + patLen);

    for (size_t off = 0; off + patLen <= size; off += CHUNK) {
        size_t readSz = CHUNK + patLen;
        if (off + readSz > size) readSz = size - off;
        if (readSz < patLen) break;
        if (!ReadRaw(start + off, buf.data(), readSz)) continue;

        for (size_t i = 0; i + patLen <= readSz; i++) {
            bool found = true;
            for (size_t j = 0; j < patLen; j++) {
                if (mask[j] == 'x' && buf[i + j] != pattern[j]) { found = false; break; }
            }
            if (found) return start + off + i;
        }
    }
    return 0;
}

static bool AutoResolveGlobals() {
    // SDK offsets are authoritative — skip scanning entirely
    WriteStartupLog("AutoResolve", "disabled — using SDK offsets");
    return false;

    int32_t peOff = Read<int32_t>(g_base + 0x3C);
    uint32_t imageSize = Read<uint32_t>(g_base + peOff + 0x50);
    if (imageSize == 0 || imageSize > 0x20000000) imageSize = 0x10000000;

    bool updated = false;

    auto IsHeapPtr = [&](uintptr_t p) -> bool {
        if (p == 0 || p < 0x10000 || p >= 0x7FFFFFFFFFFF) return false;
        if (p >= g_base && p < g_base + imageSize) return false;
        return true;
    };
    auto IsModulePtr = [&](uintptr_t p) -> bool {
        return (p >= g_base && p < g_base + imageSize);
    };

    auto ValidateGWorld = [&](uintptr_t val) -> bool {
        if (!IsHeapPtr(val)) return false;
        uintptr_t vtable = Read<uintptr_t>(val);
        if (!IsModulePtr(vtable)) return false;
        uintptr_t level = Read<uintptr_t>(val + 0x30);
        if (!IsHeapPtr(level)) return false;
        uintptr_t gameInst = Read<uintptr_t>(val + 0x228);
        if (!IsHeapPtr(gameInst)) return false;
        uintptr_t actors = Read<uintptr_t>(level + O::ULevel_Actors);
        if (!IsHeapPtr(actors)) return false;
        int32_t actorCnt = Read<int32_t>(level + O::ULevel_Actors + 8);
        if (actorCnt <= 0 || actorCnt > 200000) return false;
        uintptr_t gs = Read<uintptr_t>(val + O::UWorld_GameState);
        if (gs != 0 && !IsHeapPtr(gs)) return false;
        return true;
    };

    // Check if the default (SDK) offset already works — skip scan if so
    {
        uintptr_t val = Read<uintptr_t>(g_base + g_gworldOff);
        if (ValidateGWorld(val)) {
            WriteStartupLog("AutoResolve", "default GWorld offset is valid, skipping scan");
            return false;
        }
    }

    // --- GWorld via RIP-relative scan: find ALL mov reg,[rip+disp] that resolve to .data section ---
    // Scan for 48 8B 05/0D/15/1D/25/2D/35/3D XX XX XX XX (mov r64, [rip+disp32])
    // Filter: resolved address must land in the upper portion of the image (.data/.rdata area)
    // Then validate the pointer chain
    {
        WriteStartupLog("RIP-scan", "scanning .text for mov reg,[rip+disp] resolving near expected GWorld");
        uintptr_t dataStart = imageSize / 2;
        size_t textSize = imageSize < 0x8000000 ? imageSize : 0x8000000;
        int totalChecked = 0;
        int nearExpected = 0;

        const size_t CHUNK = 0x10000;
        std::vector<uint8_t> buf(CHUNK);

        for (uintptr_t off = 0x1000; off < textSize - 7; off += CHUNK) {
            size_t readLen = (off + CHUNK <= textSize) ? CHUNK : (textSize - off);
            ReadRaw(g_base + off, buf.data(), readLen);

            for (size_t i = 0; i + 7 <= readLen; i++) {
                if (buf[i] != 0x48) continue;
                if (buf[i+1] != 0x8B) continue;
                uint8_t modrm = buf[i+2];
                if ((modrm & 0xC7) != 0x05) continue;

                int32_t disp;
                memcpy(&disp, &buf[i+3], 4);
                uintptr_t instrAddr = off + i;
                uintptr_t resolved = instrAddr + 7 + disp;
                if (resolved < dataStart || resolved >= imageSize) continue;

                uintptr_t expectedOff = (uintptr_t)O::GWorld;
                int64_t delta = (int64_t)resolved - (int64_t)expectedOff;
                if (delta < 0) delta = -delta;
                if (delta > 0x200000) continue;

                totalChecked++;
                uintptr_t val = Read<uintptr_t>(g_base + resolved);
                if (!ValidateGWorld(val)) continue;

                nearExpected++;
                char msg[256];
                sprintf_s(msg, "FOUND at .text+0x%llX -> RVA=0x%llX val=0x%llX",
                    (unsigned long long)instrAddr, (unsigned long long)resolved, (unsigned long long)val);
                WriteStartupLog("RIP-scan", msg);
                g_gworldOff = resolved;
                updated = true;
                break;
            }
            if (updated) break;
        }
        char summary[128];
        sprintf_s(summary, "checked %d refs near expected offset, %d valid", totalChecked, nearExpected);
        WriteStartupLog("RIP-scan", summary);
    }

    // --- Neighborhood scan: search ±512KB around expected GWorld offset ---
    if (!updated) {
        WriteStartupLog("Neighborhood scan", "searching ±512KB around expected GWorld offset");
        const size_t SCAN_RANGE = 0x80000;
        uintptr_t scanStart = (g_gworldOff > SCAN_RANGE) ? (g_gworldOff - SCAN_RANGE) : 0;
        uintptr_t scanEnd = g_gworldOff + SCAN_RANGE;
        if (scanEnd > imageSize) scanEnd = imageSize;
        int candidates = 0;
        for (uintptr_t off = scanStart; off < scanEnd; off += 8) {
            uintptr_t val = Read<uintptr_t>(g_base + off);
            if (!ValidateGWorld(val)) continue;
            uintptr_t level = Read<uintptr_t>(val + 0x30);
            char msg[128];
            sprintf_s(msg, "CANDIDATE at RVA=0x%llX val=0x%llX level=0x%llX", (unsigned long long)off, (unsigned long long)val, (unsigned long long)level);
            WriteStartupLog("Neighborhood", msg);
            if (!updated) { g_gworldOff = off; updated = true; }
            candidates++;
            if (candidates >= 5) break;
        }
        if (!candidates) WriteStartupLog("Neighborhood scan", "no valid UWorld pointer found in range");
    }

    // --- Full .data section scan: brute-force every 8-byte slot in upper half of image ---
    if (!updated) {
        WriteStartupLog("Full data scan", "scanning entire .data section for UWorld pointer");
        uintptr_t dataStart = imageSize / 2;
        if (dataStart < 0x800000) dataStart = 0x800000;
        const size_t CHUNK = 0x10000;
        std::vector<uint8_t> buf(CHUNK);
        int candidates = 0;

        for (uintptr_t off = dataStart; off + 8 <= imageSize && !updated; off += CHUNK) {
            size_t readLen = CHUNK;
            if (off + readLen > imageSize) readLen = imageSize - off;
            if (readLen < 8) break;
            if (!ReadRaw(g_base + off, buf.data(), readLen)) continue;

            for (size_t i = 0; i + 8 <= readLen; i += 8) {
                uintptr_t val;
                memcpy(&val, &buf[i], 8);
                if (!IsHeapPtr(val)) continue;
                if (!ValidateGWorld(val)) continue;

                uintptr_t level = Read<uintptr_t>(val + 0x30);
                uintptr_t gi = Read<uintptr_t>(val + 0x228);
                int32_t actCnt = Read<int32_t>(level + O::ULevel_Actors + 8);
                char msg[256];
                sprintf_s(msg, "FOUND at RVA=0x%llX val=0x%llX level=0x%llX gi=0x%llX actors=%d",
                    (unsigned long long)(off + i), (unsigned long long)val,
                    (unsigned long long)level, (unsigned long long)gi, actCnt);
                WriteStartupLog("Full data scan", msg);

                g_gworldOff = off + i;
                updated = true;
                candidates++;
                break;
            }
        }
        if (!candidates) WriteStartupLog("Full data scan", "no valid UWorld pointer found in .data");
    }

    // --- GNames validation helper ---
    auto ValidateGNames = [&](uintptr_t rva) -> bool {
        uintptr_t pool = g_base + rva;
        struct Layout { int table, stride, header, data; };
        for (auto layout : {Layout{0,2,0,2}, Layout{0x10,8,8,0xC}, Layout{0x10,2,0,2}, Layout{0,1,0,2}}) {
            uintptr_t block0 = Read<uintptr_t>(pool + layout.table);
            if (!block0 || block0 < 0x10000000 || block0 >= 0x7FFFFFFFFFFF) continue;
            if (block0 >= g_base && block0 < g_base + imageSize) continue;
            uint16_t hdr = Read<uint16_t>(block0 + layout.header);
            int len = hdr >> 6;
            if (len < 4 || len > 8) continue;
            char buf[16] = {};
            ReadRaw(block0 + layout.data, buf, len);
            if (strncmp(buf, "None", 4) == 0) return true;
        }
        return false;
    };

    // --- GNames signature ---
    bool gnamesFound = false;
    {
        static const uint8_t gnSig[] = {
            0x4C, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00,
            0xEB, 0x00,
            0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00,
            0xE8, 0x00, 0x00, 0x00, 0x00
        };
        static const char gnMask[] = "xxx????x?xxx????x????";

        uintptr_t gnAddr = PatternScan(g_base, imageSize, gnSig, gnMask, sizeof(gnSig));
        if (gnAddr) {
            int32_t disp = Read<int32_t>(gnAddr + 3);
            uintptr_t resolved = gnAddr + 7 + disp;
            if (resolved > g_base && resolved < g_base + imageSize) {
                uintptr_t rva = resolved - g_base;
                if (ValidateGNames(rva)) {
                    g_gnamesOff = rva;
                    gnamesFound = true;
                    WriteStartupLog("GNames found via pattern", (std::string("RVA=0x") + ([&]{ char b[32]; sprintf_s(b, "%llX", (unsigned long long)g_gnamesOff); return std::string(b); })()).c_str());
                }
            }
        }
        if (!gnamesFound) WriteStartupLog("GNames pattern scan", "primary pattern not found or failed validation");
    }

    // --- GNames: validate current default ---
    if (!gnamesFound && ValidateGNames(g_gnamesOff)) {
        gnamesFound = true;
        WriteStartupLog("GNames default offset valid", (std::string("RVA=0x") + ([&]{ char b[32]; sprintf_s(b, "%llX", (unsigned long long)g_gnamesOff); return std::string(b); })()).c_str());
    }

    // --- GNames: LEA-based RIP scan (.text) — finds any lea r64,[rip+disp] resolving near expected GNames ---
    if (!gnamesFound) {
        WriteStartupLog("GNames RIP-scan", "scanning .text for lea r64,[rip+disp] near expected GNames");
        size_t textSize = imageSize < 0x8000000 ? imageSize : 0x8000000;
        uintptr_t dataStart = imageSize / 2;
        const size_t CHUNK = 0x10000;
        std::vector<uint8_t> buf2(CHUNK);

        for (uintptr_t off = 0x1000; off < textSize - 7 && !gnamesFound; off += CHUNK) {
            size_t readLen = (off + CHUNK <= textSize) ? CHUNK : (textSize - off);
            ReadRaw(g_base + off, buf2.data(), readLen);

            for (size_t i = 0; i + 7 <= readLen && !gnamesFound; i++) {
                uint8_t b0 = buf2[i], b1 = buf2[i+1], b2 = buf2[i+2];
                bool isLea = false;
                if (b0 == 0x4C && b1 == 0x8D && (b2 & 0xC7) == 0x05) isLea = true;
                if (b0 == 0x48 && b1 == 0x8D && (b2 & 0xC7) == 0x05) isLea = true;
                if (!isLea) continue;

                int32_t disp;
                memcpy(&disp, &buf2[i+3], 4);
                uintptr_t instrAddr = off + i;
                uintptr_t resolved = instrAddr + 7 + disp;
                if (resolved < dataStart || resolved >= imageSize) continue;

                uintptr_t expectedOff = (uintptr_t)O::GNames;
                int64_t delta = (int64_t)resolved - (int64_t)expectedOff;
                if (delta < 0) delta = -delta;
                if (delta > 0x200000) continue;

                if (ValidateGNames(resolved)) {
                    g_gnamesOff = resolved;
                    gnamesFound = true;
                    char msg[128];
                    sprintf_s(msg, "FOUND at .text+0x%llX -> RVA=0x%llX", (unsigned long long)instrAddr, (unsigned long long)resolved);
                    WriteStartupLog("GNames RIP-scan", msg);
                }
            }
        }
        if (!gnamesFound) WriteStartupLog("GNames RIP-scan", "no valid GNames found via LEA scan");
    }

    // --- GNames: neighborhood scan ±512KB ---
    if (!gnamesFound) {
        WriteStartupLog("GNames neighborhood", "searching ±512KB around expected offset");
        const size_t SCAN_RANGE = 0x80000;
        uintptr_t expectedGN = (uintptr_t)O::GNames;
        uintptr_t scanStart = (expectedGN > SCAN_RANGE) ? (expectedGN - SCAN_RANGE) : 0;
        uintptr_t scanEnd = expectedGN + SCAN_RANGE;
        if (scanEnd > imageSize) scanEnd = imageSize;
        for (uintptr_t off = scanStart; off < scanEnd; off += 0x10) {
            if (ValidateGNames(off)) {
                g_gnamesOff = off;
                gnamesFound = true;
                char msg[128];
                sprintf_s(msg, "FOUND at RVA=0x%llX", (unsigned long long)off);
                WriteStartupLog("GNames neighborhood", msg);
                break;
            }
        }
        if (!gnamesFound) WriteStartupLog("GNames neighborhood", "no valid GNames in range");
    }

    return updated;
}
// ============================================================================
// Mini SDK Dumper — external offset discovery via kernel reads
// ============================================================================
static std::atomic<bool> g_dumpRunning{false};
static std::atomic<bool> g_dumpDone{false};

static void RunOffsetDumper() {
    if (g_dumpRunning.exchange(true)) return;
    g_dumpDone = false;

    char dumpPath[MAX_PATH], desktop[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_DESKTOP, nullptr, 0, desktop);
    sprintf_s(dumpPath, "%s\\TakePeek_OffsetDump.txt", desktop);

    std::ofstream f(dumpPath);
    if (!f.is_open()) { g_dumpRunning = false; return; }

    time_t t = time(nullptr); char tb[64]; ctime_s(tb, sizeof(tb), &t); tb[strlen(tb)-1] = '\0';
    f << "=== TakePeek WarDogs — Offset Dump ===" << std::endl;
    f << "Time: " << tb << std::endl;
    f << "Base: 0x" << std::hex << g_base << std::dec << std::endl;
    f << "GWorld RVA: 0x" << std::hex << g_gworldOff << std::endl;
    f << "GNames RVA: 0x" << g_gnamesOff << std::endl;
    f << "GObjects RVA: 0x" << O::GObjects << std::dec << std::endl << std::endl;

    // UE 5.6.1 reflection layout
    const int GOBJECTS_PREFIX = 0x10;
    const int FUOBJECTITEM_SIZE = 0x18;
    const int ELEMENTS_PER_CHUNK = 0x10000;
    const int USTRUCT_SUPER = 0x40;
    const int USTRUCT_CHILDPROPS = 0x50;
    const int USTRUCT_PROPSIZE = 0x58;
    const int FFIELD_NEXT = 0x18;
    const int FFIELD_NAME = 0x20;
    const int FPROP_OFFSET = 0x44;
    const int FPROP_ELEMSIZE = 0x34;

    // Probe GObjects prefix
    uintptr_t chunksPtr = 0; int32_t numElements = 0;
    for (int prefix : {0x10, 0x00, 0x08}) {
        uintptr_t cp = Read<uintptr_t>(g_base + O::GObjects + prefix);
        int32_t ne = Read<int32_t>(g_base + O::GObjects + prefix + 0x14);
        if (cp > 0x10000 && cp < 0x7FFFFFFFFFFF && ne > 1000 && ne < 2000000) {
            chunksPtr = cp; numElements = ne;
            f << "GObjects: prefix=0x" << std::hex << prefix << " chunks=0x" << cp
              << " count=" << std::dec << ne << std::endl;
            break;
        }
    }
    if (!chunksPtr) {
        f << "ERROR: Could not locate GObjects array" << std::endl;
        f.close(); g_dumpRunning = false; return;
    }

    // Target classes to dump
    static const char* targets[] = {
        "World", "GameInstance", "PlayerController", "PlayerCameraManager",
        "Pawn", "PlayerState", "SceneComponent", "SkinnedMeshComponent",
        "SkeletalMeshComponent", "Actor", "GameStateBase", "WDMoverCharacter",
        "BHMoverPawn", "BHBaseVehiclePawn", "WDCharacterVitalityComponent",
        "Character", "Controller", "Level", "Player", "LocalPlayer",
        "WDFactionComponent", "WDSquadComponent", "BHVehicleSeatComponent",
        "WDMoverPawn", "BHSkeletalMeshComponentBudgeted",
        nullptr
    };

    // Find the "Class" meta-class FName index for fast filtering
    uintptr_t classMetaClass = 0;

    // Cache chunk pointers to reduce reads
    const int MAX_CHUNKS = 128;
    uintptr_t chunkCache[MAX_CHUNKS]{};
    int numChunks = (numElements + ELEMENTS_PER_CHUNK - 1) / ELEMENTS_PER_CHUNK;
    if (numChunks > MAX_CHUNKS) numChunks = MAX_CHUNKS;
    for (int c = 0; c < numChunks; c++)
        chunkCache[c] = Read<uintptr_t>(chunksPtr + c * 8);

    int classesFound = 0;
    f << std::endl << "Scanning " << numElements << " objects..." << std::endl;

    for (int i = 0; i < numElements && i < 800000; i++) {
        int chunkIdx = i / ELEMENTS_PER_CHUNK;
        int itemIdx = i % ELEMENTS_PER_CHUNK;
        if (chunkIdx >= numChunks) break;
        uintptr_t chunk = chunkCache[chunkIdx];
        if (!chunk) continue;

        uintptr_t obj = Read<uintptr_t>(chunk + (uintptr_t)itemIdx * FUOBJECTITEM_SIZE);
        if (!obj || obj < 0x10000) continue;

        uintptr_t objClass = Read<uintptr_t>(obj + 0x10);
        if (!objClass || objClass < 0x10000) continue;

        // First time: discover UClass metaclass
        if (!classMetaClass) {
            int32_t clsNameIdx = Read<int32_t>(objClass + 0x18);
            std::string clsName = ResolveFName(clsNameIdx);
            if (clsName == "Class") classMetaClass = objClass;
            else continue;
        }
        if (objClass != classMetaClass) continue;

        // This is a UClass — read its name
        int32_t nameIdx = Read<int32_t>(obj + 0x18);
        std::string name = ResolveFName(nameIdx);
        if (name.empty()) continue;

        bool isTarget = false;
        for (int t = 0; targets[t]; t++) {
            if (name == targets[t]) { isTarget = true; break; }
        }
        if (!isTarget) continue;

        // Get full class path (Outer chain)
        uintptr_t outer = Read<uintptr_t>(obj + 0x20);
        std::string outerName;
        if (outer) {
            int32_t outerNameIdx = Read<int32_t>(outer + 0x18);
            outerName = ResolveFName(outerNameIdx);
        }

        int32_t classSize = Read<int32_t>(obj + USTRUCT_PROPSIZE);
        f << std::endl << "========== " << name;
        if (!outerName.empty()) f << " (in " << outerName << ")";
        f << " | size=0x" << std::hex << classSize << std::dec << " ==========" << std::endl;

        // SuperStruct chain
        uintptr_t super = Read<uintptr_t>(obj + USTRUCT_SUPER);
        if (super) {
            int32_t superNameIdx = Read<int32_t>(super + 0x18);
            f << "  Inherits: " << ResolveFName(superNameIdx);
            uintptr_t ss2 = Read<uintptr_t>(super + USTRUCT_SUPER);
            if (ss2) {
                int32_t ss2Idx = Read<int32_t>(ss2 + 0x18);
                f << " -> " << ResolveFName(ss2Idx);
            }
            f << std::endl;
        }

        // Walk ChildProperties (own properties only)
        uintptr_t prop = Read<uintptr_t>(obj + USTRUCT_CHILDPROPS);
        int propCount = 0;
        while (prop && prop > 0x10000 && prop < 0x7FFFFFFFFFFF && propCount < 500) {
            int32_t propNameIdx = Read<int32_t>(prop + FFIELD_NAME);
            int32_t offset = Read<int32_t>(prop + FPROP_OFFSET);
            int32_t elemSize = Read<int32_t>(prop + FPROP_ELEMSIZE);

            std::string propName = ResolveFName(propNameIdx);
            f << "  0x" << std::hex << std::setfill('0') << std::setw(4) << offset
              << std::setfill(' ') << " [" << std::setw(3) << std::dec << elemSize << "b] "
              << propName << std::endl;

            prop = Read<uintptr_t>(prop + FFIELD_NEXT);
            propCount++;
        }
        f << "  (" << propCount << " own properties)" << std::endl;

        // Walk inherited properties from SuperStruct
        uintptr_t superWalk = Read<uintptr_t>(obj + USTRUCT_SUPER);
        int inheritDepth = 0;
        while (superWalk && superWalk > 0x10000 && inheritDepth < 10) {
            int32_t sNameIdx = Read<int32_t>(superWalk + 0x18);
            std::string sName = ResolveFName(sNameIdx);
            uintptr_t sProp = Read<uintptr_t>(superWalk + USTRUCT_CHILDPROPS);
            int sPropCount = 0;
            while (sProp && sProp > 0x10000 && sProp < 0x7FFFFFFFFFFF && sPropCount < 500) {
                sProp = Read<uintptr_t>(sProp + FFIELD_NEXT);
                sPropCount++;
            }
            if (sPropCount > 0)
                f << "  + " << sPropCount << " inherited from " << sName << std::endl;
            superWalk = Read<uintptr_t>(superWalk + USTRUCT_SUPER);
            inheritDepth++;
        }

        classesFound++;
    }

    f << std::endl << "=== DONE — " << classesFound << " classes dumped ===" << std::endl;
    f.close();
    g_dumpDone = true;
    g_dumpRunning = false;
}

// ============================================================================
// Health reader — GAS attribute chain is primary, BaseHealth is fallback
// New SDK confirms: 0x130 = BaseHealth (initial value, NOT current).
// Actual current health requires GetCurrentHealth() which sums GAS attributes.
// We read the attribute pointer at 0x140 (MISSED region in SDK) -> +0x20 f64.
// UC page 23 confirms: pawn+0x738 -> +0x140 -> +0x20 for health.
// ============================================================================
static bool ReadHealth(uintptr_t pawn, float& hp, float& maxHp) {
    hp = 0.f; maxHp = 0.f;
    uintptr_t vitality = Read<uintptr_t>(pawn + O::WDChar_VitalityComponent);
    if (!vitality) return false;

    maxHp = Read<float>(vitality + O::Vitality_MaxHealth);
    if (!std::isfinite(maxHp) || maxHp <= 0.f || maxHp > 999.f) return false;

    float currentHp = 0.f;
    uintptr_t healthAttr = Read<uintptr_t>(vitality + O::Vitality_HealthAttr);
    if (healthAttr) {
        double attrVal = Read<double>(healthAttr + O::AttrObj_CurrentValue);
        if (std::isfinite(attrVal) && attrVal >= 0.0 && attrVal <= 999.0)
            currentHp = (float)attrVal;
    }
    if (currentHp <= 0.f)
        currentHp = Read<float>(vitality + O::Vitality_BaseHealth);

    if (currentHp <= 0.f) {
        float inlineHp = Read<float>(pawn + 0x808);
        if (std::isfinite(inlineHp) && inlineHp > 0.f && inlineHp <= 999.f)
            currentHp = inlineHp;
    }

    if (!std::isfinite(currentHp) || currentHp < 0.f || currentHp > 999.f) return false;

    hp = std::clamp(currentHp, 0.f, maxHp);
    return true;
}

// ============================================================================
// Bone reading — ComponentSpaceTransforms + ComponentToWorld
// ============================================================================
static uintptr_t g_c2wOffset = 0;

static DTransform ReadC2W(uintptr_t mesh) {
    if (g_c2wOffset) {
        DTransform d = Read<DTransform>(mesh + g_c2wOffset);
        return d;
    }
    for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200,
                           (uintptr_t)0x210, (uintptr_t)0x220, (uintptr_t)0x230, (uintptr_t)0x240}) {
        DTransform d = Read<DTransform>(mesh + off);
        if (fabs(d.rotation.w) >= 0.01 && fabs(d.rotation.w) <= 1.01 &&
            (d.translation.x != 0.0 || d.translation.y != 0.0) &&
            fabs(d.translation.x) < 1e9 && fabs(d.translation.y) < 1e9) {
            g_c2wOffset = off;
            char buf[128];
            sprintf_s(buf, "C2W offset discovered: 0x%llX", (unsigned long long)off);
            WriteStartupLog("C2W", buf);
            return d;
        }
    }
    FTransformF f = Read<FTransformF>(mesh + O::Scene_ComponentToWorld);
    DTransform d;
    d.rotation = {(double)f.rotX, (double)f.rotY, (double)f.rotZ, (double)f.rotW};
    d.translation = {(double)f.tX, (double)f.tY, (double)f.tZ};
    d._pad0 = 0;
    d.scale3D = {(double)f.sX, (double)f.sY, (double)f.sZ};
    d._pad1 = 0;
    return d;
}

static bool ValidateC2W(const DTransform& c2w) {
    if (fabs(c2w.rotation.w) < 0.01 || fabs(c2w.rotation.w) > 1.01) return false;
    if (c2w.translation.x == 0.0 && c2w.translation.y == 0.0 && c2w.translation.z == 0.0) return false;
    if (fabs(c2w.translation.x) > 1e9 || fabs(c2w.translation.y) > 1e9 || fabs(c2w.translation.z) > 1e9) return false;
    return true;
}

static bool ValidateBoneBuffer(uintptr_t boneArray, int stride) {
    if (stride == 0x30) {
        FTransformF root = Read<FTransformF>(boneArray);
        if (fabsf(root.rotW) < 0.01f || fabsf(root.rotW) > 1.01f) return false;
        if (fabsf(root.tX) > 500.f || fabsf(root.tY) > 500.f || fabsf(root.tZ) > 500.f) return false;
        return true;
    } else if (stride == 0x50) {
        DTransformPacked root = Read<DTransformPacked>(boneArray);
        if (fabs(root.rotation.w) < 0.01 || fabs(root.rotation.w) > 1.01) return false;
        if (fabs(root.translation.x) > 500.0 || fabs(root.translation.y) > 500.0 || fabs(root.translation.z) > 500.0) return false;
        return true;
    } else {
        DTransform root = Read<DTransform>(boneArray);
        if (fabs(root.rotation.w) < 0.01 || fabs(root.rotation.w) > 1.01) return false;
        if (fabs(root.translation.x) > 500.0 || fabs(root.translation.y) > 500.0 || fabs(root.translation.z) > 500.0) return false;
        return true;
    }
}

static uintptr_t g_discoveredBoneOffset = 0;

static bool TestBoneArray(uintptr_t arrData, int32_t count, int stride) {
    if (!arrData || arrData < 0x10000000 || arrData >= 0x7FFFFFFFFFFF) return false;
    if (count < 10 || count > 500) return false;
    if (stride == 0x60) {
        DTransform b0 = Read<DTransform>(arrData);
        if (fabs(b0.rotation.w) < 0.01 || fabs(b0.rotation.w) > 1.01) return false;
        if (fabs(b0.translation.x) > 500.0 || fabs(b0.translation.y) > 500.0 || fabs(b0.translation.z) > 500.0) return false;
        if (count > 1) {
            DTransform b1 = Read<DTransform>(arrData + 0x60);
            if (fabs(b1.rotation.w) < 0.01 || fabs(b1.rotation.w) > 1.01) return false;
        }
        return true;
    } else if (stride == 0x50) {
        DTransformPacked b0 = Read<DTransformPacked>(arrData);
        if (fabs(b0.rotation.w) < 0.01 || fabs(b0.rotation.w) > 1.01) return false;
        if (fabs(b0.translation.x) > 500.0 || fabs(b0.translation.y) > 500.0 || fabs(b0.translation.z) > 500.0) return false;
        if (count > 1) {
            DTransformPacked b1 = Read<DTransformPacked>(arrData + 0x50);
            if (fabs(b1.rotation.w) < 0.01 || fabs(b1.rotation.w) > 1.01) return false;
        }
        return true;
    } else {
        FTransformF b0 = Read<FTransformF>(arrData);
        if (fabsf(b0.rotW) < 0.01f || fabsf(b0.rotW) > 1.01f) return false;
        if (fabsf(b0.tX) > 500.f || fabsf(b0.tY) > 500.f || fabsf(b0.tZ) > 500.f) return false;
        if (count > 1) {
            FTransformF b1 = Read<FTransformF>(arrData + 0x30);
            if (fabsf(b1.rotW) < 0.01f || fabsf(b1.rotW) > 1.01f) return false;
        }
        return true;
    }
}

static bool DiscoverBoneArrayOffset(uintptr_t mesh) {
    static bool s_scanLogged = false;
    static const uintptr_t knownOffsets[] = {
        O::Skinned_BoneTransformsAlt1,  // 0xA18 — primary in reference source
        O::Skinned_ComponentSpaceTransforms1, // 0x630
        O::Skinned_ComponentSpaceTransforms,  // 0x620
        O::Skinned_BoneTransformsAlt0,  // 0xA08
        0x640, 0x650, 0x660, 0x670, 0x680,
    };
    for (auto off : knownOffsets) {
        uintptr_t arrData = Read<uintptr_t>(mesh + off);
        int32_t count = Read<int32_t>(mesh + off + 0x8);
        if (count < 10 || count > 500) continue;
        if (!arrData || arrData < 0x10000000 || arrData >= 0x7FFFFFFFFFFF) continue;
        if (TestBoneArray(arrData, count, 0x60)) {
            g_discoveredBoneOffset = off;
            g_boneStride = 0x60;
            char buf[256];
            sprintf_s(buf, "found bones at mesh+0x%llX count=%d stride=0x60 (known offset)", (unsigned long long)off, count);
            WriteStartupLog("BoneScan", buf);
            return true;
        }
    }
    for (uintptr_t off = 0x600; off <= 0xB00; off += 0x8) {
        uintptr_t arrData = Read<uintptr_t>(mesh + off);
        int32_t count = Read<int32_t>(mesh + off + 0x8);
        if (count < 10 || count > 500) continue;
        if (!arrData || arrData < 0x10000000 || arrData >= 0x7FFFFFFFFFFF) continue;
        if (TestBoneArray(arrData, count, 0x60)) {
            g_discoveredBoneOffset = off;
            g_boneStride = 0x60;
            char buf[256];
            sprintf_s(buf, "found bones at mesh+0x%llX count=%d stride=0x60 (scan)", (unsigned long long)off, count);
            WriteStartupLog("BoneScan", buf);
            return true;
        }
    }
    if (!s_scanLogged) {
        s_scanLogged = true;
        WriteStartupLog("BoneScan", "no valid bone array found in mesh 0x600-0xB00");
    }
    return false;
}

static DVec3 GetBoneWorldPos(uintptr_t mesh, int boneIdx) {
    if (!mesh || boneIdx < 0) return {};

    if (g_discoveredBoneOffset == 0) {
        if (!DiscoverBoneArrayOffset(mesh)) return {};
    }

    uintptr_t boneArray = Read<uintptr_t>(mesh + g_discoveredBoneOffset);
    int32_t boneCount = Read<int32_t>(mesh + g_discoveredBoneOffset + 0x8);
    if (!boneArray || boneCount <= 0 || boneIdx >= boneCount) {
        uintptr_t altOff = (g_discoveredBoneOffset == 0x620) ? 0x630 :
                           (g_discoveredBoneOffset == 0x630) ? 0x620 :
                           g_discoveredBoneOffset + 0x10;
        boneArray = Read<uintptr_t>(mesh + altOff);
        boneCount = Read<int32_t>(mesh + altOff + 0x8);
        if (!boneArray || boneCount <= 0 || boneIdx >= boneCount) return {};
    }

    DTransform c2w = ReadC2W(mesh);
    if (!ValidateC2W(c2w)) return {};

    if (!ValidateBoneBuffer(boneArray, g_boneStride)) {
        uintptr_t altOff = (g_discoveredBoneOffset == 0x620) ? 0x630 :
                           (g_discoveredBoneOffset == 0x630) ? 0x620 :
                           g_discoveredBoneOffset + 0x10;
        uintptr_t altArray = Read<uintptr_t>(mesh + altOff);
        int32_t altCount = Read<int32_t>(mesh + altOff + 0x8);
        if (altArray && altCount > boneIdx && ValidateBoneBuffer(altArray, g_boneStride))
            boneArray = altArray;
    }

    if (g_boneStride == 0x30) {
        FTransformF bone = Read<FTransformF>(boneArray + boneIdx * 0x30);
        return TransformBoneFToWorld(bone, c2w);
    } else if (g_boneStride == 0x50) {
        DTransformPacked bone = Read<DTransformPacked>(boneArray + boneIdx * 0x50);
        return TransformBonePackedToWorld(bone, c2w);
    } else {
        DTransform bone = Read<DTransform>(boneArray + boneIdx * 0x60);
        return TransformBoneToWorld(bone, c2w);
    }
}

static DVec3 ApplyTransform(const DTransform& t, DVec3 p) {
    DVec3 scaled = p * t.scale3D;
    DVec3 rotated = QuatRotateVector(t.rotation, scaled);
    return rotated + t.translation;
}

// Reference-matched skeleton reader: tries multiple bone array offsets and strides,
// batch reads, validates C2W proximity and per-bone distance, spine check.
static bool ReadSkeleton(uintptr_t mesh, const BoneIndices& bi, DVec3 playerPos, DVec3* outBones) {
    if (!mesh) return false;

    DTransform c2w{};
    bool c2wOk = false;

    if (g_c2wOffset) {
        c2w = Read<DTransform>(mesh + g_c2wOffset);
        if (c2w.Valid()) {
            double dx = c2w.translation.x - playerPos.x, dy = c2w.translation.y - playerPos.y, dz = c2w.translation.z - playerPos.z;
            c2wOk = (sqrt(dx*dx + dy*dy + dz*dz) <= 500.0);
        }
    }
    if (!c2wOk) {
        for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200,
                               (uintptr_t)0x210, (uintptr_t)0x220, (uintptr_t)0x230, (uintptr_t)0x240}) {
            DTransform t = Read<DTransform>(mesh + off);
            if (!t.Valid()) continue;
            double dx = t.translation.x - playerPos.x, dy = t.translation.y - playerPos.y, dz = t.translation.z - playerPos.z;
            if (sqrt(dx*dx + dy*dy + dz*dz) > 500.0) continue;
            c2w = t; c2wOk = true;
            if (!g_c2wOffset) {
                g_c2wOffset = off;
                char buf[128];
                sprintf_s(buf, "ReadSkeleton discovered C2W at mesh+0x%llX", (unsigned long long)off);
                WriteStartupLog("C2W", buf);
            }
            break;
        }
    }
    if (!c2wOk) {
        for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200}) {
            FTransformF tf = Read<FTransformF>(mesh + off);
            if (fabsf(tf.rotW) < 0.01f || fabsf(tf.rotW) > 1.01f) continue;
            DVec3 tl = {(double)tf.tX, (double)tf.tY, (double)tf.tZ};
            if (tl.x == 0.0 && tl.y == 0.0) continue;
            double dx = tl.x - playerPos.x, dy = tl.y - playerPos.y, dz = tl.z - playerPos.z;
            if (sqrt(dx*dx + dy*dy + dz*dz) > 500.0) continue;
            c2w.rotation = {(double)tf.rotX, (double)tf.rotY, (double)tf.rotZ, (double)tf.rotW};
            c2w.translation = tl;
            c2w._pad0 = 0;
            c2w.scale3D = {(double)tf.sX, (double)tf.sY, (double)tf.sZ};
            c2w._pad1 = 0;
            c2wOk = true;
            break;
        }
    }
    if (!c2wOk) return false;

    int stride = g_boneStride ? g_boneStride : 0;

    for (uintptr_t boneOff : {(uintptr_t)O::Skinned_BoneTransformsAlt1, (uintptr_t)O::Skinned_ComponentSpaceTransforms1,
                               (uintptr_t)O::Skinned_ComponentSpaceTransforms, (uintptr_t)O::Skinned_BoneTransformsAlt0,
                               (uintptr_t)0x640, (uintptr_t)0x650, (uintptr_t)0x660}) {
        bool isLocal = (boneOff == O::Skinned_BoneTransformsAlt0);
        if (isLocal && bi.parentChain.empty()) continue;

        uintptr_t arrData = Read<uintptr_t>(mesh + boneOff);
        int32_t arrCount = Read<int32_t>(mesh + boneOff + 0x8);
        int32_t arrCap = Read<int32_t>(mesh + boneOff + 0xC);
        if (!arrData || arrData < 0x10000000 || arrData >= 0x7FFFFFFFFFFF) continue;
        if (arrCount < 10 || arrCount > 1024 || arrCap < arrCount) continue;
        if (!bi.valid && arrCount < 149) continue;

        static const int stridesToTry[] = {0x60, 0x30, 0x50};
        for (int tryStride : stridesToTry) {
            if (stride && tryStride != stride && g_discoveredBoneOffset) continue;

            DVec3 bones[22]{};
            bool boneOk[22]{};

            if (tryStride == 0x60) {
                std::vector<DTransform> transforms(arrCount);
                ReadRaw(arrData, transforms.data(), arrCount * sizeof(DTransform));

                for (int slot = 0; slot < 22; slot++) {
                    int idx = bi.ToSlot(slot);
                    if (idx < 0 || idx >= arrCount || !transforms[idx].Valid()) continue;
                    DVec3 point = transforms[idx].translation;
                    bool ok = true;
                    if (isLocal) {
                        for (int parent = bi.parentChain[idx]; parent >= 0 && parent < arrCount; parent = bi.parentChain[parent]) {
                            if (!transforms[parent].Valid()) { ok = false; break; }
                            point = ApplyTransform(transforms[parent], point);
                        }
                    }
                    point = ApplyTransform(c2w, point);
                    if (!ok || !std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) continue;
                    double bdx = point.x - playerPos.x, bdy = point.y - playerPos.y, bdz = point.z - playerPos.z;
                    if (sqrt(bdx*bdx + bdy*bdy + bdz*bdz) > 400.0) continue;
                    bones[slot] = point;
                    boneOk[slot] = true;
                }
            } else if (tryStride == 0x30) {
                int readSize = arrCount * (int)sizeof(FTransformF);
                std::vector<FTransformF> fBones(arrCount);
                ReadRaw(arrData, fBones.data(), readSize);

                for (int slot = 0; slot < 22; slot++) {
                    int idx = bi.ToSlot(slot);
                    if (idx < 0 || idx >= arrCount) continue;
                    FTransformF& fb = fBones[idx];
                    if (fabsf(fb.rotW) < 0.01f || fabsf(fb.rotW) > 1.01f) continue;
                    DVec3 point = {(double)fb.tX, (double)fb.tY, (double)fb.tZ};
                    point = ApplyTransform(c2w, point);
                    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) continue;
                    double bdx = point.x - playerPos.x, bdy = point.y - playerPos.y, bdz = point.z - playerPos.z;
                    if (sqrt(bdx*bdx + bdy*bdy + bdz*bdz) > 400.0) continue;
                    bones[slot] = point;
                    boneOk[slot] = true;
                }
            } else {
                std::vector<DTransformPacked> pBones(arrCount);
                ReadRaw(arrData, pBones.data(), arrCount * (int)sizeof(DTransformPacked));

                for (int slot = 0; slot < 22; slot++) {
                    int idx = bi.ToSlot(slot);
                    if (idx < 0 || idx >= arrCount) continue;
                    DTransformPacked& pb = pBones[idx];
                    if (fabs(pb.rotation.w) < 0.01 || fabs(pb.rotation.w) > 1.01) continue;
                    DVec3 point = pb.translation;
                    point = ApplyTransform(c2w, point);
                    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) continue;
                    double bdx = point.x - playerPos.x, bdy = point.y - playerPos.y, bdz = point.z - playerPos.z;
                    if (sqrt(bdx*bdx + bdy*bdy + bdz*bdz) > 400.0) continue;
                    bones[slot] = point;
                    boneOk[slot] = true;
                }
            }

            if (boneOk[0] && boneOk[1] && boneOk[2] && boneOk[5]) {
                double sdx = bones[0].x - bones[5].x, sdy = bones[0].y - bones[5].y, sdz = bones[0].z - bones[5].z;
                double spine = sqrt(sdx*sdx + sdy*sdy + sdz*sdz);
                if (spine >= 15.0 && spine <= 200.0) {
                    memcpy(outBones, bones, 22 * sizeof(DVec3));
                    if (!g_discoveredBoneOffset) {
                        g_discoveredBoneOffset = boneOff;
                        g_boneStride = tryStride;
                        char buf[256];
                        sprintf_s(buf, "ReadSkeleton discovered bones at mesh+0x%llX stride=0x%X count=%d",
                            (unsigned long long)boneOff, tryStride, arrCount);
                        WriteStartupLog("BoneScan", buf);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

// ============================================================================
// Diagnostics — comprehensive dump for testers
// ============================================================================
static void HexDump(std::ofstream& f, uintptr_t addr, int bytes, const char* label) {
    f << "  [HEX " << label << " @ 0x" << std::hex << addr << "]" << std::dec << std::endl;
    const int perLine = 16;
    for (int off = 0; off < bytes; off += perLine) {
        f << "    " << std::hex << std::setfill('0') << std::setw(4) << off << ": ";
        uint8_t line[16]{};
        for (int b = 0; b < perLine && (off + b) < bytes; b++)
            line[b] = Read<uint8_t>(addr + off + b);
        for (int b = 0; b < perLine && (off + b) < bytes; b++)
            f << std::setw(2) << (int)line[b] << " ";
        f << " | ";
        for (int b = 0; b < perLine && (off + b) < bytes; b++)
            f << (char)(line[b] >= 0x20 && line[b] < 0x7F ? line[b] : '.');
        f << std::dec << std::setfill(' ') << std::endl;
    }
}

static void DiscoverLevelsOffset(uintptr_t gworld, uintptr_t persistentLevel);

static void RunDiagnostics() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string dir(path);
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
#ifdef UNBRANDED
    dir += "Overlay_Diag.txt";
#else
    dir += "TakePeek_Diag.txt";
#endif

    std::ofstream f(dir);
    if (!f.is_open()) return;

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char timeBuf[64];
    ctime_s(timeBuf, sizeof(timeBuf), &t);

#ifdef UNBRANDED
    f << "=== Overlay Full Diagnostics ===" << std::endl;
#else
    f << "=== TakePeek WarDogs Full Diagnostics ===" << std::endl;
#endif
    f << "Time: " << timeBuf;
    f << "Build: CL-496049" << std::endl;
    f << std::endl;

    // --- Connection ---
    f << "[CONNECTION]" << std::endl;
    f << "  Read mode:            " << (Memory::g_usePhysRW ? "PHYSICAL R/W (BYOVD)" : "USERMODE (OpenProcess)") << std::endl;
    f << "  PID:                  " << g_pid << std::endl;
    f << "  Base address:         0x" << std::hex << g_base << std::dec << std::endl;
    f << "  Game window:          " << (g_gameWnd ? "FOUND" : "NOT FOUND") << std::endl;
    f << "  Process handle:       0x" << std::hex << (uintptr_t)g_proc << std::dec << std::endl;
    f << std::endl;

    // --- All configured offsets ---
    f << "[OFFSET TABLE]" << std::endl;
    f << "  GWorld                = 0x" << std::hex << g_gworldOff << " (default: 0x" << O::GWorld << ")" << std::endl;
    f << "  GNames                = 0x" << g_gnamesOff << " (default: 0x" << O::GNames << ")" << std::endl;
    f << "  GObjects              = 0x" << O::GObjects << std::endl;
    f << "  UWorld_PersistentLevel = 0x" << O::UWorld_PersistentLevel << std::endl;
    f << "  UWorld_OwningGameInstance = 0x" << O::UWorld_OwningGameInstance << std::endl;
    f << "  ULevel_Actors          = 0x" << O::ULevel_Actors << std::endl;
    f << "  GameInstance_LocalPlayers = 0x" << O::GameInstance_LocalPlayers << std::endl;
    f << "  UPlayer_PlayerController = 0x" << O::UPlayer_PlayerController << std::endl;
    f << "  PC_Pawn                = 0x" << O::PC_Pawn << std::endl;
    f << "  PC_CameraManager       = 0x" << O::PC_CameraManager << std::endl;
    f << "  CameraManager_Cache    = 0x" << O::CameraManager_CachePrivate << std::endl;
    f << "  CameraManager_LastFr   = 0x" << O::CameraManager_LastFrameCache << std::endl;
    f << "  CameraCache_POV        = 0x" << O::CameraCache_POV << std::endl;
    f << "  POV_Location           = 0x" << O::POV_Location << std::endl;
    f << "  POV_Rotation           = 0x" << O::POV_Rotation << std::endl;
    f << "  POV_FOV                = 0x" << O::POV_FOV << std::endl;
    f << "  POV_AspectRatio        = 0x" << O::POV_AspectRatio << std::endl;
    f << "  AActor_RootComponent   = 0x" << O::AActor_RootComponent << std::endl;
    f << "  APawn_PlayerState      = 0x" << O::APawn_PlayerState << std::endl;
    f << "  PS_PlayerNamePrivate   = 0x" << O::PS_PlayerNamePrivate << std::endl;
    f << "  PS_FactionComponent    = 0x" << O::PS_FactionComponent << std::endl;
    f << "  PS_SquadComponent      = 0x" << O::PS_SquadComponent << std::endl;
    f << "  Faction_Tag            = 0x" << O::Faction_Tag << std::endl;
    f << "  Squad_Id               = 0x" << O::Squad_Id << std::endl;
    f << "  WDChar_CharacterMesh   = 0x" << O::WDChar_CharacterMesh << std::endl;
    f << "  WDChar_VitalityComp    = 0x" << O::WDChar_VitalityComponent << std::endl;
    f << "  Vitality_MaxHealth     = 0x" << O::Vitality_MaxHealth << std::endl;
    f << "  Vitality_BaseHealth = 0x" << O::Vitality_BaseHealth << std::endl;
    f << "  Skinned_BoneTransforms = 0x" << O::Skinned_ComponentSpaceTransforms << " (alt: 0x" << O::Skinned_BoneTransformsAlt0 << ", 0x" << O::Skinned_BoneTransformsAlt1 << ")" << std::endl;
    f << "  Skinned_SkinnedAsset   = 0x" << O::Skinned_SkinnedAsset << " (alt: 0x" << O::Skinned_SkinnedAssetAlt << ")" << std::endl;
    f << "  CameraManager_ViewTarget = 0x" << O::CameraManager_ViewTarget << std::endl;
    f << "  Scene_RelativeLocation = 0x" << O::Scene_RelativeLocation << std::endl;
    f << "  Scene_ComponentToWorld = 0x" << O::Scene_ComponentToWorld << std::endl;
    f << "  SkinnedAsset_RefSkel   = 0x" << O::SkinnedAsset_RefSkeleton << std::endl;
    f << std::dec << std::endl;

    bool patternUsed = (g_gworldOff != O::GWorld || g_gnamesOff != O::GNames);
    f << "  Pattern scan:          " << (patternUsed ? "ACTIVE (offsets differ from defaults)" : "NOT USED (using defaults)") << std::endl;
    f << std::endl;

    // --- Raw pointer chain with hex values ---
    f << "[UWORLD CHAIN — raw pointers]" << std::endl;
    uintptr_t gworld = Read<uintptr_t>(g_base + g_gworldOff);
    f << "  base+GWorld            -> 0x" << std::hex << gworld << (gworld ? "" : " *** NULL ***") << std::dec << std::endl;
    if (!gworld) { f << "  !!! GWorld is NULL — offsets are stale !!!" << std::endl; f.close(); return; }

    uintptr_t gameInstance = Read<uintptr_t>(gworld + O::UWorld_OwningGameInstance);
    uintptr_t level        = Read<uintptr_t>(gworld + O::UWorld_PersistentLevel);
    f << "  GWorld+0x" << std::hex << O::UWorld_OwningGameInstance << " (GameInst) -> 0x" << gameInstance << std::endl;
    f << "  GWorld+0x" << O::UWorld_PersistentLevel << " (Level)    -> 0x" << level << std::dec << std::endl;

    uintptr_t localPlayers = 0, localPlayer = 0, playerController = 0, localPawn = 0;
    if (gameInstance) {
        localPlayers = Read<uintptr_t>(gameInstance + O::GameInstance_LocalPlayers);
        f << "  GameInst+0x" << std::hex << O::GameInstance_LocalPlayers << " (LPs)   -> 0x" << localPlayers << std::dec << std::endl;
        if (localPlayers) {
            localPlayer = Read<uintptr_t>(localPlayers);
            f << "  LocalPlayers[0]        -> 0x" << std::hex << localPlayer << std::dec << std::endl;
            if (localPlayer) {
                playerController = Read<uintptr_t>(localPlayer + O::UPlayer_PlayerController);
                f << "  LP+0x" << std::hex << O::UPlayer_PlayerController << " (PC)          -> 0x" << playerController << std::dec << std::endl;
            }
        }
    }
    if (playerController) {
        localPawn = Read<uintptr_t>(playerController + O::PC_Pawn);
        uintptr_t camMgr = Read<uintptr_t>(playerController + O::PC_CameraManager);
        f << "  PC+0x" << std::hex << O::PC_Pawn << " (Pawn)        -> 0x" << localPawn << std::endl;
        f << "  PC+0x" << O::PC_CameraManager << " (CamMgr)    -> 0x" << camMgr << std::dec << std::endl;
    }
    f << std::endl;

    // --- Hex dump of GWorld region ---
    if (gworld) HexDump(f, gworld, 0x240, "UWorld");

    // --- Camera deep dump ---
    f << "[CAMERA]" << std::endl;
    if (playerController) {
        uintptr_t camMgr = Read<uintptr_t>(playerController + O::PC_CameraManager);
        if (camMgr) {
            uintptr_t povBase = camMgr + O::CameraManager_CachePrivate + O::CameraCache_POV;
            DVec3 loc = Read<DVec3>(povBase + O::POV_Location);
            DRotator rot = Read<DRotator>(povBase + O::POV_Rotation);
            float fov = Read<float>(povBase + O::POV_FOV);
            float aspect = Read<float>(povBase + O::POV_AspectRatio);

            bool locOk = std::isfinite(loc.x) && (loc.x != 0 || loc.y != 0 || loc.z != 0);
            bool rotOk = std::isfinite(rot.pitch) && std::isfinite(rot.yaw);
            bool fovOk = fov > 1.f && fov < 170.f;

            f << "  CameraManager:        0x" << std::hex << camMgr << std::dec << std::endl;
            f << "  POV base:             0x" << std::hex << povBase << std::dec << std::endl;
            f << "  Location:             " << (locOk ? "OK" : "ZERO/INVALID") << " (" << loc.x << ", " << loc.y << ", " << loc.z << ")" << std::endl;
            f << "  Rotation:             " << (rotOk ? "OK" : "INVALID") << " (P:" << rot.pitch << " Y:" << rot.yaw << " R:" << rot.roll << ")" << std::endl;
            f << "  FOV:                  " << (fovOk ? "OK" : "OUT OF RANGE") << " (" << fov << ")" << std::endl;
            f << "  AspectRatio:          " << aspect << (aspect > 0.f ? "" : " *** INVALID ***") << std::endl;
            HexDump(f, povBase, 0x70, "FMinimalViewInfo");
        } else {
            f << "  CameraManager:        NULL" << std::endl;
        }
    } else {
        f << "  Skipped — no PlayerController" << std::endl;
    }
    f << std::endl;

    // --- GNames pool dump ---
    f << "[GNAMES POOL]" << std::endl;
    uintptr_t pool = g_base + g_gnamesOff;
    f << "  Pool address:         0x" << std::hex << pool << std::dec << std::endl;
    uintptr_t block0 = Read<uintptr_t>(pool + O::GNames_PoolOffset);
    f << "  Block[0]:             0x" << std::hex << block0 << std::dec << (block0 ? "" : " *** NULL ***") << std::endl;
    if (block0) {
        for (int idx : {0, 1, 2, 3, 5, 10, 50, 100, 500, 1000}) {
            std::string nm = ResolveFName(idx);
            f << "  FName[" << idx << "]: " << (nm.empty() ? "(empty)" : nm) << std::endl;
        }
        HexDump(f, pool, 0x40, "GNames header");
        HexDump(f, block0, 0x80, "Block[0] first entries");
    }
    f << std::endl;

    // --- Levels discovery ---
    f << "[LEVELS]" << std::endl;
    if (gworld) {
        DiscoverLevelsOffset(gworld, level);
        f << "  Levels offset:        " << (g_levelsDiscovered ? "0x" : "NOT FOUND") << std::endl;
        if (g_levelsDiscovered) {
            f << "  UWorld+0x" << std::hex << g_levelsOffset << std::dec << std::endl;
            uintptr_t ld = Read<uintptr_t>(gworld + g_levelsOffset);
            int32_t lc = Read<int32_t>(gworld + g_levelsOffset + 8);
            f << "  Level count:          " << lc << std::endl;
            for (int i = 0; i < lc && i < 20; i++) {
                uintptr_t lvl = Read<uintptr_t>(ld + i * 8);
                int32_t ac = 0;
                if (lvl) ac = Read<int32_t>(lvl + O::ULevel_Actors + 8);
                f << "    [" << i << "] 0x" << std::hex << lvl << std::dec
                  << " actors=" << ac << (lvl == level ? " (PersistentLevel)" : "") << std::endl;
            }
        }
    }
    f << std::endl;

    // --- GameState → PlayerArray ---
    f << "[GAMESTATE]" << std::endl;
    if (gworld) {
        uintptr_t gs = Read<uintptr_t>(gworld + O::UWorld_GameState);
        f << "  UWorld+0x1B0:         0x" << std::hex << gs << std::dec << std::endl;
        if (gs && gs > 0x10000000 && gs < 0x7FFFFFFFFFFF) {
            uintptr_t gsCls = Read<uintptr_t>(gs + O::UObject_ClassPrivate);
            if (gsCls) {
                int32_t gsNi = Read<int32_t>(gsCls + O::UObject_NamePrivate);
                f << "  Class:                " << ResolveFName(gsNi) << std::endl;
            }
            uintptr_t paData = Read<uintptr_t>(gs + O::GS_PlayerArray);
            int32_t paCount = Read<int32_t>(gs + O::GS_PlayerArray + 8);
            int32_t paCap = Read<int32_t>(gs + O::GS_PlayerArray + 12);
            f << "  PlayerArray(+0x2D0): data=0x" << std::hex << paData << " cnt=" << std::dec << paCount << " cap=" << paCap << std::endl;
            if (paData && paCount > 0 && paCount <= 200) {
                int show = paCount < 5 ? paCount : 5;
                for (int i = 0; i < show; i++) {
                    uintptr_t ps = Read<uintptr_t>(paData + i * 8);
                    uintptr_t pw = (ps > 0x10000000) ? Read<uintptr_t>(ps + O::PS_PawnPrivate) : 0;
                    std::string nm = (ps > 0x10000000) ? ReadFString(ps + O::PS_PlayerNamePrivate) : "";
                    f << "    [" << i << "] PS=0x" << std::hex << ps << " Pawn=0x" << pw << std::dec << " name=" << nm << std::endl;
                }
            }
        }
    }
    f << "  g_gameState:          0x" << std::hex << g_gameState << std::dec << std::endl;
    f << "  g_localFactionId:       0x" << std::hex << g_localFactionId << std::dec << std::endl;
    f << "  g_localSquad:         lo=0x" << std::hex << g_localSquadLo << " hi=0x" << g_localSquadHi << std::dec
      << (g_localSquadded ? " (active)" : " (none)") << std::endl;
    f << std::endl;

    // --- Full actor scan with class listing ---
    f << "[ACTOR SCAN — full dump]" << std::endl;
    if (level) {
        uintptr_t actorArray = Read<uintptr_t>(level + O::ULevel_Actors);
        int actorCount = Read<int>(level + O::ULevel_Actors + 0x8);
        f << "  Actor array:          0x" << std::hex << actorArray << std::dec << std::endl;
        f << "  Actor count:          " << actorCount << " (PersistentLevel only)" << std::endl;

        if (actorArray && actorCount > 0) {
            int cap = (actorCount > 300) ? 300 : actorCount;
            int withMesh = 0, withHealth = 0, withPS = 0, withFaction = 0, withBones = 0;
            int dummies = 0, totalActors = 0;
            uintptr_t firstValidPawn = 0;
            std::string firstClassName;
            std::unordered_map<std::string, int> classCounts;

            f << std::endl << "  --- Per-actor class listing (first " << cap << ") ---" << std::endl;
            for (int i = 0; i < cap; i++) {
                uintptr_t actor = Read<uintptr_t>(actorArray + i * sizeof(uintptr_t));
                if (!actor) continue;
                totalActors++;

                uintptr_t classPtr = Read<uintptr_t>(actor + O::UObject_ClassPrivate);
                std::string className = "(unknown)";
                if (classPtr) {
                    int32_t nameIdx = Read<int32_t>(classPtr + O::UObject_NamePrivate);
                    className = ResolveFName(nameIdx);
                    if (className.empty()) className = "(unresolved)";
                }
                classCounts[className]++;

                uintptr_t mesh = Read<uintptr_t>(actor + O::WDChar_CharacterMesh);
                if (!mesh) continue;
                withMesh++;

                bool isDummy = IsActorDummy(actor);
                if (isDummy) { dummies++; continue; }

                float hp = 0.f, mhp = 0.f;
                bool hasHealth = ReadHealth(actor, hp, mhp);
                if (hasHealth) withHealth++;

                uintptr_t ps = Read<uintptr_t>(actor + O::APawn_PlayerState);
                if (ps) {
                    withPS++;
                    uintptr_t fc = Read<uintptr_t>(ps + O::PS_FactionComponent);
                    if (fc) withFaction++;
                }

                uintptr_t sa = Read<uintptr_t>(mesh + O::Skinned_SkinnedAsset);
                if (sa) {
                    BoneIndices bi = ResolveBones(sa);
                    if (bi.valid) withBones++;
                }

                if (!firstValidPawn && ps) {
                    firstValidPawn = actor;
                    firstClassName = className;
                }

                // Log every pawn with mesh
                std::string pName;
                if (ps) pName = ReadFString(ps + O::PS_PlayerNamePrivate);
                f << "    [" << i << "] 0x" << std::hex << actor << std::dec
                  << " class=" << className
                  << " mesh=0x" << std::hex << mesh << std::dec
                  << " hp=" << (hasHealth ? std::to_string((int)hp) + "/" + std::to_string((int)mhp) : "N/A")
                  << (ps ? " PS=OK" : " PS=NULL")
                  << (pName.empty() ? "" : " name=" + pName)
                  << std::endl;
            }

            f << std::endl << "  --- Class summary ---" << std::endl;
            for (auto& [cls, cnt] : classCounts) {
                f << "    " << cls << ": " << cnt << std::endl;
            }

            f << std::endl << "  --- Counts ---" << std::endl;
            f << "  Total actors scanned: " << totalActors << std::endl;
            f << "  With mesh (pawns):    " << withMesh << std::endl;
            f << "  Shooting dummies:     " << dummies << std::endl;
            f << "  With health:          " << withHealth << (withHealth == 0 && withMesh > 0 ? " *** BROKEN ***" : "") << std::endl;
            f << "  With PlayerState:     " << withPS << (withPS == 0 && withMesh > 0 ? " *** BROKEN ***" : "") << std::endl;
            f << "  With FactionComp:     " << withFaction << std::endl;
            f << "  With resolved bones:  " << withBones << (withBones == 0 && withMesh > 0 ? " *** BROKEN ***" : "") << std::endl;

            // --- First pawn deep dive with full hex dumps ---
            if (firstValidPawn) {
                f << std::endl << "  --- First pawn deep dive ---" << std::endl;
                f << "  Address:              0x" << std::hex << firstValidPawn << std::dec << std::endl;
                f << "  Class:                " << firstClassName << std::endl;

                uintptr_t fvMesh = Read<uintptr_t>(firstValidPawn + O::WDChar_CharacterMesh);
                uintptr_t fvRoot = Read<uintptr_t>(firstValidPawn + O::AActor_RootComponent);

                if (fvRoot) {
                    DVec3 pos = Read<DVec3>(fvRoot + O::Scene_RelativeLocation);
                    f << "  RootComponent:        0x" << std::hex << fvRoot << std::dec << std::endl;
                    f << "  Position:             (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
                }

                uintptr_t fvVitality = Read<uintptr_t>(firstValidPawn + O::WDChar_VitalityComponent);
                f << "  VitalityComponent:    0x" << std::hex << fvVitality << std::dec << std::endl;
                if (fvVitality) {
                    float curHp = Read<float>(fvVitality + O::Vitality_BaseHealth);
                    float maxHp = Read<float>(fvVitality + O::Vitality_MaxHealth);
                    f << "  Vitality CurrentHP:   " << curHp << std::endl;
                    f << "  Vitality MaxHealth:   " << maxHp << std::endl;
                    HexDump(f, fvVitality + 0x130, 0x30, "Vitality health region");
                }

                float hp = 0.f, mhp = 0.f;
                bool hOk = ReadHealth(firstValidPawn, hp, mhp);
                f << "  ReadHealth result:    " << (hOk ? "OK" : "FAILED") << " (hp=" << hp << " max=" << mhp << ")" << std::endl;

                if (fvMesh) {
                    f << "  CharacterMesh:        0x" << std::hex << fvMesh << std::dec << std::endl;
                    uintptr_t fvSA = Read<uintptr_t>(fvMesh + O::Skinned_SkinnedAsset);
                    f << "  SkinnedAsset:         0x" << std::hex << fvSA << std::dec << std::endl;

                    uintptr_t dbgBoneOff2 = g_discoveredBoneOffset ? g_discoveredBoneOffset : O::Skinned_ComponentSpaceTransforms;
                    uintptr_t boneArray = Read<uintptr_t>(fvMesh + dbgBoneOff2);
                    int32_t boneCount = Read<int32_t>(fvMesh + dbgBoneOff2 + 0x8);
                    f << "  BoneArray:            0x" << std::hex << boneArray << std::dec << " (count: " << boneCount << ") offset=0x" << std::hex << dbgBoneOff2 << std::dec << std::endl;

                    if (fvSA) {
                        BoneIndices bi = ResolveBones(fvSA);
                        f << "  Bone resolution:      " << (bi.valid ? "OK" : "FAILED") << std::endl;
                        if (bi.valid) {
                            f << "    head=" << bi.head << " neck=" << bi.neck << " pelvis=" << bi.pelvis << std::endl;
                            f << "    lHand=" << bi.lHand << " rHand=" << bi.rHand << std::endl;
                            f << "    lFoot=" << bi.lFoot << " rFoot=" << bi.rFoot << std::endl;
                        }

                        // Dump all bone names from RefSkeleton
                        uintptr_t refSkel = fvSA + O::SkinnedAsset_RefSkeleton;
                        uintptr_t boneInfoArr = Read<uintptr_t>(refSkel + 0x20);
                        int32_t boneInfoCount = Read<int32_t>(refSkel + 0x28);
                        if (boneInfoArr && boneInfoCount > 0 && boneInfoCount < 500) {
                            f << std::endl << "  --- Full bone list (" << boneInfoCount << " bones) ---" << std::endl;
                            for (int b = 0; b < boneInfoCount; b++) {
                                int32_t nameIdx = Read<int32_t>(boneInfoArr + b * O::BONE_INFO_STRIDE);
                                int32_t parentIdx = Read<int32_t>(boneInfoArr + b * O::BONE_INFO_STRIDE + 0x8);
                                std::string boneName = ResolveFName(nameIdx);
                                f << "    [" << b << "] " << (boneName.empty() ? "(unresolved)" : boneName) << " parent=" << parentIdx << std::endl;
                            }
                        }
                    }

                    if (boneArray && boneCount > 0) {
                        f << std::endl << "  --- Bone transform probe (first 3 bones) ---" << std::endl;
                        f << "  BoneStride detected:  0x" << std::hex << g_boneStride << std::dec << std::endl;
                        DTransform c2w = ReadC2W(fvMesh);
                        f << "  C2W translation:      (" << c2w.translation.x << ", " << c2w.translation.y << ", " << c2w.translation.z << ")" << std::endl;
                        for (int b = 0; b < 3 && b < boneCount; b++) {
                            FTransformF bf = Read<FTransformF>(boneArray + b * 0x30);
                            DTransformPacked bp = Read<DTransformPacked>(boneArray + b * 0x50);
                            DTransform bd = Read<DTransform>(boneArray + b * 0x60);
                            f << "    bone[" << b << "] as float(0x30):  rot=(" << bf.rotX << "," << bf.rotY << "," << bf.rotZ << "," << bf.rotW
                              << ") t=(" << bf.tX << "," << bf.tY << "," << bf.tZ << ")" << std::endl;
                            f << "    bone[" << b << "] as packed(0x50): rot=(" << bp.rotation.x << "," << bp.rotation.y << "," << bp.rotation.z << "," << bp.rotation.w
                              << ") t=(" << bp.translation.x << "," << bp.translation.y << "," << bp.translation.z << ")" << std::endl;
                            f << "    bone[" << b << "] as padded(0x60): rot=(" << bd.rotation.x << "," << bd.rotation.y << "," << bd.rotation.z << "," << bd.rotation.w
                              << ") t=(" << bd.translation.x << "," << bd.translation.y << "," << bd.translation.z << ")" << std::endl;
                        }
                        HexDump(f, boneArray, 0xA0, "BoneArray first 0xA0 bytes (raw)");
                    }

                    HexDump(f, fvMesh + 0x5A0, 0x40, "SkinnedMeshComp around SkinnedAsset");
                    HexDump(f, fvMesh + 0x610, 0x20, "SkinnedMeshComp around BoneTransforms");
                }

                uintptr_t fvPS = Read<uintptr_t>(firstValidPawn + O::APawn_PlayerState);
                if (fvPS) {
                    std::string name = ReadFString(fvPS + O::PS_PlayerNamePrivate);
                    f << "  PlayerState:          0x" << std::hex << fvPS << std::dec << std::endl;
                    f << "  Player name:          " << (name.empty() ? "(empty)" : name) << std::endl;

                    uintptr_t fvFC = Read<uintptr_t>(fvPS + O::PS_FactionComponent);
                    if (fvFC) {
                        uint64_t ft = Read<uint64_t>(fvFC + O::Faction_Tag);
                        f << "  FactionComp:          0x" << std::hex << fvFC << std::endl;
                        f << "  Faction tag:          0x" << ft << std::dec << std::endl;
                        HexDump(f, fvFC + 0x100, 0x20, "FactionComp tag region");
                    }
                    uintptr_t fvSC = Read<uintptr_t>(fvPS + O::PS_SquadComponent);
                    if (fvSC) {
                        uint64_t sid = Read<uint64_t>(fvSC + O::Squad_Id);
                        f << "  SquadComp:            0x" << std::hex << fvSC << std::endl;
                        f << "  Squad ID:             0x" << sid << std::dec << std::endl;
                    }

                    HexDump(f, fvPS + 0x4E0, 0x30, "PlayerState faction/squad region");
                }

                HexDump(f, firstValidPawn + 0x380, 0x40, "Pawn mesh+vitality region");
            }
        }
    } else {
        f << "  Skipped — no PersistentLevel" << std::endl;
    }
    f << std::endl;

    // --- Local pawn offset probe ---
    f << "[LOCAL PAWN OFFSET PROBE]" << std::endl;
    if (localPawn) {
        f << "  LocalPawn:            0x" << std::hex << localPawn << std::dec << std::endl;
        uintptr_t lpClass = Read<uintptr_t>(localPawn + O::UObject_ClassPrivate);
        if (lpClass) {
            int32_t lpNameIdx = Read<int32_t>(lpClass + O::UObject_NamePrivate);
            f << "  Class:                " << ResolveFName(lpNameIdx) << std::endl;
        }
        uintptr_t lpPS = Read<uintptr_t>(localPawn + O::APawn_PlayerState);
        f << "  PlayerState:          0x" << std::hex << lpPS << std::dec << std::endl;
        if (lpPS) {
            uintptr_t lpFC = Read<uintptr_t>(lpPS + O::PS_FactionComponent);
            f << "  FactionComp:          0x" << std::hex << lpFC << std::dec << std::endl;
            if (lpFC) {
                uint64_t lpFT = Read<uint64_t>(lpFC + O::Faction_Tag);
                f << "  Local faction tag:    0x" << std::hex << lpFT << std::dec << std::endl;
            }
        }
        f << "  g_localFactionId:       0x" << std::hex << g_localFactionId << std::dec << std::endl;
        HexDump(f, localPawn + 0x2D0, 0x30, "LocalPawn PlayerState region");
        HexDump(f, localPawn + 0x380, 0x30, "LocalPawn mesh region");
        HexDump(f, localPawn + 0x730, 0x20, "LocalPawn vitality region");
    } else {
        f << "  Not spawned yet" << std::endl;
    }
    f << std::endl;

    // --- Summary ---
    f << "[SUMMARY]" << std::endl;
    bool allGood = gworld && level && playerController;
    f << "  Core chain:           " << (allGood ? "OK" : "BROKEN") << std::endl;
    f << "  GWorld:               " << (gworld ? "OK" : "NULL") << std::endl;
    f << "  Level:                " << (level ? "OK" : "NULL") << std::endl;
    f << "  PlayerController:     " << (playerController ? "OK" : "NULL") << std::endl;
    f << "  LocalPawn:            " << (localPawn ? "OK" : "NULL") << std::endl;
    f << "  Report saved to:      " << dir << std::endl;
    f << "============================================" << std::endl;

    f.close();
}

// ============================================================================
// Read UE5 game state
// ============================================================================
static void UpdateCamera() {
    s_workerCam.valid = false;
    g_localPawn = 0;
    if (!g_base) return;

    static bool s_camChainLogged = false;

    uintptr_t gworld = Read<uintptr_t>(g_base + g_gworldOff);
    if (!gworld || gworld < 0x10000000 || gworld >= 0x7FFFFFFFFFFF) {
        uintptr_t eng = Read<uintptr_t>(g_base + O::GEngine);
        if (eng && eng > 0x10000000 && eng < 0x7FFFFFFFFFFF) {
            uintptr_t vp = Read<uintptr_t>(eng + O::GEngine_GameViewport);
            if (vp && vp > 0x10000000 && vp < 0x7FFFFFFFFFFF)
                gworld = Read<uintptr_t>(vp + O::GameViewport_World);
        }
    }
    if (!gworld || gworld < 0x10000000 || gworld >= 0x7FFFFFFFFFFF) return;

    // Try multiple GameInstance sources (matches DMA reference Controller() fallback)
    uintptr_t playerController = 0;
    uintptr_t instances[3] = { Read<uintptr_t>(gworld + O::UWorld_OwningGameInstance), 0, 0 };
    uintptr_t engine = Read<uintptr_t>(g_base + O::GEngine);
    if (engine && engine > 0x10000000 && engine < 0x7FFFFFFFFFFF) {
        instances[1] = Read<uintptr_t>(engine + O::UWorld_OwningGameInstance);
        instances[2] = Read<uintptr_t>(engine + O::GameInstanceAlt);
    }
    for (auto gi : instances) {
        if (!gi || gi < 0x10000000 || gi >= 0x7FFFFFFFFFFF) continue;
        uintptr_t lps = Read<uintptr_t>(gi + O::GameInstance_LocalPlayers);
        if (!lps) continue;
        uintptr_t lp = Read<uintptr_t>(lps);
        if (!lp || lp < 0x10000000 || lp >= 0x7FFFFFFFFFFF) continue;
        uintptr_t pc = Read<uintptr_t>(lp + O::UPlayer_PlayerController);
        if (pc && pc > 0x10000000 && pc < 0x7FFFFFFFFFFF) { playerController = pc; break; }
    }
    if (!playerController) { if (!s_camChainLogged) { WriteStartupLog("CamChain", "playerController=NULL (all GI paths)"); } return; }

    g_localPawn = Read<uintptr_t>(playerController + O::PC_Pawn);
    if (!g_localPawn || g_localPawn < 0x10000000 || g_localPawn >= 0x7FFFFFFFFFFF)
        g_localPawn = Read<uintptr_t>(playerController + 0x2F8);
    if (!g_localPawn || g_localPawn < 0x10000000 || g_localPawn >= 0x7FFFFFFFFFFF)
        g_localPawn = Read<uintptr_t>(playerController + 0x308);
    if (!g_localPawn || g_localPawn < 0x10000000 || g_localPawn >= 0x7FFFFFFFFFFF)
        g_localPawn = 0;

    uintptr_t camMgr = Read<uintptr_t>(playerController + O::PC_CameraManager);
    if (!camMgr) { if (!s_camChainLogged) { WriteStartupLog("CamChain", "cameraManager=NULL"); } return; }

    s_workerCam.timeSeconds = Read<float>(gworld + O::UWorld_TimeSeconds);

    // Try 4 camera cache offsets (matching DMA reference order)
    bool camFound = false;
    for (uintptr_t camOff : {(uintptr_t)0x1560, (uintptr_t)0x1E40, (uintptr_t)0x350, (uintptr_t)0xC40}) {
        uintptr_t povBase = camMgr + camOff + O::CameraCache_POV;
        DVec3 loc = Read<DVec3>(povBase + O::POV_Location);
        DRotator rot = Read<DRotator>(povBase + O::POV_Rotation);
        float fov = Read<float>(povBase + O::POV_FOV);
        if (fov > 1.f && fov < 170.f && (loc.x != 0.0 || loc.y != 0.0 || loc.z != 0.0) &&
            fabs(loc.x) < 1e9 && fabs(loc.y) < 1e9 && fabs(rot.pitch) <= 90.0) {
            s_workerCam.location = loc;
            s_workerCam.rotation = rot;
            s_workerCam.fov = fov;
            s_workerCam.aspectRatio = Read<float>(povBase + O::POV_AspectRatio);
            if (s_workerCam.aspectRatio <= 0.f) s_workerCam.aspectRatio = 16.f / 9.f;
            s_workerCam.valid = true;
            camFound = true;
            break;
        }
    }
    if (!camFound) {
        s_workerCam.fov = 90.f;
        s_workerCam.aspectRatio = 16.f / 9.f;
    }

    if (!s_camChainLogged) {
        s_camChainLogged = true;
        char buf[256];
        sprintf_s(buf, "OK cam=(%.0f,%.0f,%.0f) rot=(%.1f,%.1f) fov=%.1f pawn=0x%llX",
            s_workerCam.location.x, s_workerCam.location.y, s_workerCam.location.z,
            s_workerCam.rotation.pitch, s_workerCam.rotation.yaw, s_workerCam.fov,
            (unsigned long long)g_localPawn);
        WriteStartupLog("CamChain", buf);
    }

    g_localIsADS = false;
    if (g_localPawn) {
        g_localIsADS = Read<uint8_t>(g_localPawn + O::WDChar_AimingAlpha) != 0;
        uintptr_t localPS = Read<uintptr_t>(g_localPawn + O::APawn_PlayerState);
        if (localPS) {
            uintptr_t factionComp = Read<uintptr_t>(localPS + O::PS_FactionComponent);
            if (factionComp && factionComp > 0x10000000 && factionComp < 0x7F0000000000) {
                uintptr_t readObj = Read<uintptr_t>(factionComp + O::Faction_Object);
                if (readObj && readObj > 0x10000000 && readObj < 0x7F0000000000) {
                    g_localFactionObj = readObj;
                    uintptr_t fData = Read<uintptr_t>(readObj + O::Faction_Data);
                    if (fData && fData > 0x10000000 && fData < 0x7F0000000000) {
                        g_localFactionData = fData;
                        uint8_t tid = Read<uint8_t>(fData + O::FactionData_TeamId);
                        if (tid <= 32) g_localTeamId = tid;
                        uint32_t dataTag = Read<uint32_t>(fData + O::FactionData_Tag);
                        if (dataTag) g_localFactionId = dataTag;
                    }
                }
                if (!g_localFactionId) {
                    uint32_t compTag = Read<uint32_t>(factionComp + O::Faction_TagRep);
                    if (compTag) g_localFactionId = compTag;
                }
                if (!g_localFactionId) {
                    uint32_t altTag = Read<uint32_t>(factionComp + O::Faction_Tag);
                    if (altTag) g_localFactionId = altTag;
                }
                if (g_localFactionId) {
                    std::string resolved = ResolveFactionName(g_localFactionId);
                    if (!resolved.empty()) g_localFactionName = resolved;
                    else g_localFactionId = 0;
                }

                static bool s_factionProbed = false;
                if (!s_factionProbed && (g_localFactionObj || g_localFactionData)) {
                    s_factionProbed = true;
                    char buf[512];
                    sprintf_s(buf, "factionComp=0x%llX fObj=0x%llX fData=0x%llX fId=0x%X teamId=%d name=%s",
                        (unsigned long long)factionComp, (unsigned long long)g_localFactionObj,
                        (unsigned long long)g_localFactionData, g_localFactionId, g_localTeamId,
                        g_localFactionName.c_str());
                    WriteStartupLog("FactionProbe", buf);
                }
            }

            uintptr_t squadComp = Read<uintptr_t>(localPS + O::PS_SquadComponent);
            if (squadComp && squadComp > 0x10000000 && squadComp < 0x7FFFFFFFFFFF) {
                g_localSquadLo = Read<uint64_t>(squadComp + O::Squad_Id);
                g_localSquadHi = Read<uint64_t>(squadComp + O::Squad_Id + 8);
                g_localSquadded = (g_localSquadLo | g_localSquadHi) != 0;
            }
        }

        g_localBulletSpeed = 0.f;
        uintptr_t weapBehav = Read<uintptr_t>(g_localPawn + O::WDChar_WeaponBehavior);
        if (weapBehav && weapBehav > 0x10000000 && weapBehav < 0x7FFFFFFFFFFF) {
            uintptr_t statsData = Read<uintptr_t>(weapBehav + O::WeaponBehavior_StatsData);
            if (statsData && statsData > 0x10000000 && statsData < 0x7FFFFFFFFFFF) {
                float spd = Read<float>(statsData + O::WeaponStats_InitialBulletSpeed);
                if (spd > 1000.f && spd < 200000.f)
                    g_localBulletSpeed = spd;
            }
        }
    }

    // FOV changer — write desired FOV to all camera locations (skip when ADS so scope works)
    auto& cfgCam = g_config.Active();
    if (cfgCam.fovChanger && s_workerCam.valid && IsValidPtr(camMgr) && !g_localIsADS) {
        float desiredFov = (float)cfgCam.fovValue;
        uintptr_t fovAddr = camMgr + O::CameraManager_CachePrivate + O::CameraCache_POV + O::POV_FOV;
        uintptr_t fovAddrLF = camMgr + O::CameraManager_LastFrameCache + O::CameraCache_POV + O::POV_FOV;
        uintptr_t fovAddrVT = camMgr + O::CameraManager_ViewTarget + O::CameraCache_POV + O::POV_FOV;
        SafeWrite<float>(fovAddr, desiredFov);
        SafeWrite<float>(fovAddrLF, desiredFov);
        SafeWrite<float>(fovAddrVT, desiredFov);
        s_workerCam.fov = desiredFov;
    }
}

// Hardcoded bone fallbacks — used only when dynamic resolution fails.
// Indices kept low (< 50) so they work on reduced-LOD meshes.
// These match a typical UE5 mannequin-derived skeleton.
static const BoneIndices FALLBACK_BONES = [] {
    BoneIndices b;
    b.head = 9; b.neck = 7;
    b.spine3 = 6; b.spine2 = 4; b.spine1 = 2; b.pelvis = 1;
    b.lUpperArm = 11; b.lForearm = 12; b.lHand = 22;
    b.rUpperArm = 65; b.rForearm = 66; b.rHand = 76;
    b.lThigh = 145; b.lCalf = 146; b.lFoot = 147;
    b.rThigh = 122; b.rCalf = 123; b.rFoot = 124;
    b.valid = true;
    return b;
}();

// Auto-discover UWorld::Levels TArray offset
// Strategy: scan UWorld for TArrays whose entries look like ULevel objects (have actor arrays at +0xA0)
static void DiscoverLevelsOffset(uintptr_t gworld, uintptr_t persistentLevel) {
    if (g_levelsDiscovered) return;
    g_levelsDiscovered = true; // mark attempted — prevents per-frame spamming

    int bestOff = 0, bestScore = 0, bestCount = 0;

    for (int off = 0x38; off <= 0x300; off += 8) {
        uintptr_t arrData = Read<uintptr_t>(gworld + off);
        if (arrData < 0x10000000 || arrData >= 0x7FFFFFFFFFFF) continue;
        int32_t count = Read<int32_t>(gworld + off + 8);
        if (count < 1 || count > 1000) continue;
        int32_t cap = Read<int32_t>(gworld + off + 12);
        if (cap < count || cap > 2000) continue;

        int checkLimit = count < 20 ? count : 20;
        bool foundPL = false;
        int validLevels = 0;

        for (int i = 0; i < checkLimit; i++) {
            uintptr_t entry = Read<uintptr_t>(arrData + i * 8);
            if (entry == persistentLevel) foundPL = true;
            if (entry < 0x10000000 || entry >= 0x7FFFFFFFFFFF) continue;
            uintptr_t actArr = Read<uintptr_t>(entry + O::ULevel_Actors);
            int32_t actCnt = Read<int32_t>(entry + O::ULevel_Actors + 8);
            if (actArr > 0x10000000 && actArr < 0x7FFFFFFFFFFF && actCnt >= 0 && actCnt < 50000)
                validLevels++;
        }

        if (validLevels == 0) continue;

        char buf[256];
        sprintf_s(buf, "UW+0x%X cnt=%d cap=%d plMatch=%s validLvl=%d/%d",
            off, count, cap, foundPL ? "YES" : "no", validLevels, checkLimit);
        WriteStartupLog("LevelsProbe", buf);

        // Score: PL match is highest priority, then valid level ratio
        int score = (foundPL ? 10000 : 0) + validLevels * 100 / checkLimit + count;
        if (score > bestScore) {
            bestScore = score;
            bestOff = off;
            bestCount = count;
        }
    }

    if (bestOff) {
        g_levelsOffset = bestOff;
        char buf[128];
        sprintf_s(buf, "UWorld+0x%X count=%d score=%d", bestOff, bestCount, bestScore);
        WriteStartupLog("LevelsFound", buf);
    } else {
        WriteStartupLog("LevelsNotFound", "no valid candidate in 0x38-0x300");
    }
}

static void FindGameState(uintptr_t gworld) {
    g_gameState = 0;
    g_playerArrayOffset = 0;
    g_gameStateScanDone = true;

    uintptr_t gs = Read<uintptr_t>(gworld + O::UWorld_GameState);
    if (!gs || gs < 0x10000000 || gs >= 0x7FFFFFFFFFFF) {
        WriteStartupLog("GameState", "UWorld+0x1B0 is NULL — world not ready");
        return;
    }

    // Validate it's actually a GameState by checking PlayerArray
    uintptr_t arrData = Read<uintptr_t>(gs + O::GS_PlayerArray);
    int32_t arrCount = Read<int32_t>(gs + O::GS_PlayerArray + 8);

    if (arrData > 0x10000000 && arrData < 0x7FFFFFFFFFFF && arrCount >= 0 && arrCount <= 200) {
        g_gameState = gs;
        g_playerArrayOffset = O::GS_PlayerArray;
        char buf[128];
        sprintf_s(buf, "gs=0x%llX playerArray=%d", (unsigned long long)gs, arrCount);
        WriteStartupLog("GameStateFound", buf);
    } else {
        char buf[128];
        sprintf_s(buf, "gs=0x%llX arrData=0x%llX cnt=%d — invalid", (unsigned long long)gs, (unsigned long long)arrData, arrCount);
        WriteStartupLog("GameState", buf);
    }
}

static void UpdatePlayers() {
    s_workerPlayers.clear();
    if (!g_base) return;

    uintptr_t gworld = Read<uintptr_t>(g_base + g_gworldOff);
    if (!gworld || gworld < 0x10000000 || gworld >= 0x7FFFFFFFFFFF) {
        uintptr_t engine = Read<uintptr_t>(g_base + O::GEngine);
        if (engine && engine > 0x10000000 && engine < 0x7FFFFFFFFFFF) {
            uintptr_t viewport = Read<uintptr_t>(engine + O::GEngine_GameViewport);
            if (viewport && viewport > 0x10000000 && viewport < 0x7FFFFFFFFFFF)
                gworld = Read<uintptr_t>(viewport + O::GameViewport_World);
        }
    }
    if (!gworld || gworld < 0x10000000 || gworld >= 0x7FFFFFFFFFFF) return;
    uintptr_t persistLevel = Read<uintptr_t>(gworld + O::UWorld_PersistentLevel);
    if (!persistLevel) return;

    auto& cfg = g_config.Active();

    // Velocity tracking for position interpolation
    static std::unordered_map<uintptr_t, DVec3> s_prevPositions;
    static std::unordered_map<uintptr_t, DVec3> s_prevVehPositions;
    static std::chrono::steady_clock::time_point s_prevTickTime = std::chrono::steady_clock::now();
    auto tickNow = std::chrono::steady_clock::now();
    float tickDt = std::chrono::duration<float>(tickNow - s_prevTickTime).count();

    static std::unordered_map<uintptr_t, bool> s_factionCache;
    static std::unordered_map<uintptr_t, bool> s_persistentDeath;
    struct DeadCache { PlayerData data; std::chrono::steady_clock::time_point deathTime; };
    static std::unordered_map<uintptr_t, DeadCache> s_deadPlayerCache;
    static std::unordered_map<uintptr_t, std::chrono::steady_clock::time_point> s_lastVisibleTime;
    static uint32_t s_deadTagId = 0;
    static uint32_t s_dbnoTagId = 0;
    static uint32_t s_aliveTagId = 0;
    static uint8_t s_prevLocalTeamId = 0xFF;
    static uint32_t s_prevLocalFaction = 0;
    static auto s_lastCacheClear = std::chrono::steady_clock::now();

    {
        auto now = std::chrono::steady_clock::now();
        bool localChanged = (g_localTeamId != s_prevLocalTeamId && g_localTeamId != 0xFF) ||
                            (g_localFactionId != s_prevLocalFaction && g_localFactionId != 0);
        bool periodicRefresh = std::chrono::duration_cast<std::chrono::seconds>(now - s_lastCacheClear).count() >= 5;
        if (localChanged || periodicRefresh) {
            s_factionCache.clear();
            s_lastCacheClear = now;
        }
        s_prevLocalTeamId = g_localTeamId;
        s_prevLocalFaction = g_localFactionId;
    }

    // Detect level transitions
    static uintptr_t s_lastLevel = 0;
    static auto s_lastStatusLog = std::chrono::steady_clock::now();
    static bool s_pipelineLogged = false;

    if (persistLevel != s_lastLevel) {
        s_lastLevel = persistLevel;
        g_boneStride = 0;
        g_discoveredBoneOffset = 0;
        g_c2wOffset = 0;
        g_gameStateScanDone = false;
        g_gameState = 0;
        g_playerArrayOffset = 0;
        s_pipelineLogged = false;
        g_boneCache.clear();
        s_fnameCache.clear();
        g_dummyClasses.clear();
        g_playerClasses.clear();
        g_vehicleClasses.clear();
        g_nonVehicleClasses.clear();
        g_droppedItemClasses.clear();
        g_mineClasses.clear();
        g_emplacementClasses.clear();
        g_fobClasses.clear();
        g_nonWorldClasses.clear();
        s_workerPrevHealth.clear();
        s_prevPositions.clear();
        s_prevVehPositions.clear();
        s_factionCache.clear();
        s_persistentDeath.clear();
        s_deadPlayerCache.clear();
        s_lastVisibleTime.clear();
        s_deadTagId = 0;
        s_dbnoTagId = 0;
        s_aliveTagId = 0;
        s_prevLocalTeamId = 0xFF;
        s_prevLocalFaction = 0;
        g_localFactionId = 0;
        g_localFactionName.clear();
        g_localFactionObj = 0;
        g_localFactionData = 0;
        g_localTeamId = 0xFF;
        g_localSquadLo = 0;
        g_localSquadHi = 0;
        g_localSquadded = false;

        char hdr[256];
        sprintf_s(hdr, "LEVEL CHANGED: level=0x%llX gworld=0x%llX",
            (unsigned long long)persistLevel, (unsigned long long)gworld);
        WriteStartupLog("LevelChange", hdr);
    }

    // Find GameState via UWorld+0x1B0
    if (!g_gameStateScanDone) {
        FindGameState(gworld);
    }

    // Retry every 3s if GameState not ready (world still initializing)
    if (g_gameStateScanDone && !g_gameState) {
        static auto s_gsRetryTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::steady_clock::now() - s_gsRetryTime;
        if (std::chrono::duration<float>(elapsed).count() > 3.f) {
            s_gsRetryTime = std::chrono::steady_clock::now();
            g_gameStateScanDone = false;
        }
    }

    // PRIMARY: Use GameState -> PlayerArray -> PawnPrivate
    if (g_gameState && g_playerArrayOffset) {
        uintptr_t arrData = Read<uintptr_t>(g_gameState + g_playerArrayOffset);
        int32_t arrCount = Read<int32_t>(g_gameState + g_playerArrayOffset + 8);
        if (arrData && arrCount > 0 && arrCount <= 200) {

        int dbgTotal = arrCount, dbgValidPS = 0, dbgWithPawn = 0, dbgNotLocal = 0;
        int dbgValidMesh = 0, dbgDead = 0, dbgAfterTeam = 0, dbgOnScreen = 0;
        int dbgLogged = 0;

        for (int i = 0; i < arrCount; i++) {
            uintptr_t playerState = Read<uintptr_t>(arrData + i * 8);
            if (!playerState || playerState < 0x10000000 || playerState >= 0x7FFFFFFFFFFF) continue;
            dbgValidPS++;

            uintptr_t actor = Read<uintptr_t>(playerState + O::PS_PawnPrivate);
            if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;
            dbgWithPawn++;
            if (actor == g_localPawn) continue;
            dbgNotLocal++;

            uintptr_t mesh = Read<uintptr_t>(actor + O::WDChar_CharacterMesh);
            auto meshValid = [](uintptr_t m, uintptr_t base) {
                return m && m >= 0x10000000 && m < 0x7FFFFFFFFFFF && !(m >= base && m < base + 0x10000000);
            };
            if (!meshValid(mesh, g_base)) {
                mesh = Read<uintptr_t>(actor + O::WDChar_MeshFallback1);
                if (!meshValid(mesh, g_base)) continue;
            }
            dbgValidMesh++;

            PlayerData p{};
            p.pawn = actor;
            p.mesh = mesh;

            p.healthValid = ReadHealth(actor, p.health, p.maxHealth);
            if (!p.healthValid) { p.health = 0.f; p.maxHealth = 100.f; }

            p.isInvincible = Read<uint8_t>(actor + O::WDChar_Invincible) != 0;
            p.bleedoutState = Read<uint8_t>(actor + O::WDChar_DeathState + O::DeathState_BleedoutState);
            p.giveUpTime = Read<float>(actor + O::WDChar_DeathState + O::DeathState_GiveUpTime);
            p.isDead = (p.bleedoutState >= 2) || (p.healthValid && p.health <= 0.f);
            p.isDowned = !p.isDead && (p.bleedoutState >= 1);
            if (!p.isDead && !p.isDowned) {
                uint32_t vTag = Read<uint32_t>(playerState + O::PS_VitalityStateTag);
                if (!vTag) {
                    uint32_t inlineTag = Read<uint32_t>(actor + O::WDChar_DeathState + 0x08);
                    if (inlineTag) vTag = inlineTag;
                }
                if (vTag) {
                    if (s_deadTagId && vTag == s_deadTagId) p.isDead = true;
                    else if (s_dbnoTagId && vTag == s_dbnoTagId) p.isDowned = true;
                    else if (vTag != s_aliveTagId) {
                        std::string tagName = ResolveFName((int32_t)vTag);
                        if (tagName.find("Dead") != std::string::npos) { s_deadTagId = vTag; p.isDead = true; }
                        else if (tagName.find("DBNO") != std::string::npos || tagName.find("Down") != std::string::npos || tagName.find("Critical") != std::string::npos) { s_dbnoTagId = vTag; p.isDowned = true; }
                        else s_aliveTagId = vTag;
                    }
                }
            }
            if (!p.isDead && !p.healthValid) p.isDead = true;
            if (p.isDead) s_persistentDeath[actor] = true;
            else if (s_persistentDeath.count(actor) && s_persistentDeath[actor]) {
                if (p.healthValid && p.health > 0.01f && p.bleedoutState <= 1) s_persistentDeath[actor] = false;
                else p.isDead = true;
            }
            if (p.isDead) dbgDead++;
            p.isADS = Read<uint8_t>(actor + O::WDChar_AimingAlpha) != 0;

            p.stance = 2;
            p.isSprinting = false;
            p.isTacSprinting = false;
            if (mesh) {
                uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                    if (Read<uint8_t>(animInst + O::AnimInst_StanceProne)) p.stance = 0;
                    else if (Read<uint8_t>(animInst + O::AnimInst_StanceCrouch)) p.stance = 1;
                    else p.stance = 2;
                    p.isSprinting = Read<uint8_t>(animInst + O::AnimInst_SprintOrTacSprint) != 0;
                }
            }

            // Admin / Developer / Bot detection — always read, never filtered
            p.isDeveloper = Read<uint8_t>(playerState + O::PS_IsDeveloper) != 0;
            p.isAdmin = Read<uint8_t>(playerState + O::PS_IsAdmin) != 0;
            p.isBot = (Read<uint8_t>(playerState + O::PS_bIsABot) & 0x08) != 0;
            p.ping = Read<uint8_t>(playerState + O::PS_CompressedPing);

            uintptr_t factionComp = Read<uintptr_t>(playerState + O::PS_FactionComponent);
            uintptr_t playerFactionData = 0;
            int playerTeam = -1;
            bool validFC = factionComp && factionComp > 0x10000000 && factionComp < 0x7F0000000000;
            if (validFC) {
                uintptr_t playerFactionObj = Read<uintptr_t>(factionComp + O::Faction_Object);
                if (playerFactionObj && playerFactionObj > 0x10000000 && playerFactionObj < 0x7F0000000000) {
                    playerFactionData = Read<uintptr_t>(playerFactionObj + O::Faction_Data);
                    if (playerFactionData && (playerFactionData < 0x10000000 || playerFactionData > 0x7F0000000000))
                        playerFactionData = 0;
                }
                if (playerFactionData) {
                    uint8_t tid = Read<uint8_t>(playerFactionData + O::FactionData_TeamId);
                    if (tid <= 32) playerTeam = tid;
                    uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                    if (dataTag) p.factionId = dataTag;
                }
                if (!p.factionId) {
                    uint32_t compTag = Read<uint32_t>(factionComp + O::Faction_TagRep);
                    if (compTag) p.factionId = compTag;
                }
                if (!p.factionId) {
                    uint32_t altTag = Read<uint32_t>(factionComp + O::Faction_Tag);
                    if (altTag) p.factionId = altTag;
                }
                if (p.factionId) {
                    std::string chk = ResolveFName((int32_t)p.factionId);
                    if (chk.empty() || chk == "None") p.factionId = 0;
                }
            }

            // Relation cascade (matches DMA reference order): squad → factionData → factionId → factionName → teamId
            int relation = 0; // 0=unknown, 1=friendly, -1=enemy
            std::string playerFName;

            uintptr_t squadComp = Read<uintptr_t>(playerState + O::PS_SquadComponent);
            if (squadComp && squadComp > 0x10000000 && squadComp < 0x7FFFFFFFFFFF && g_localSquadded) {
                uint64_t sqLo = Read<uint64_t>(squadComp + O::Squad_Id);
                uint64_t sqHi = Read<uint64_t>(squadComp + O::Squad_Id + 8);
                if ((sqLo | sqHi) != 0 && sqLo == g_localSquadLo && sqHi == g_localSquadHi)
                    relation = 1;
            }
            if (relation == 0 && g_localFactionData && playerFactionData)
                relation = (playerFactionData == g_localFactionData) ? 1 : -1;
            if (relation == 0 && g_localFactionId && p.factionId)
                relation = (g_localFactionId == p.factionId) ? 1 : -1;
            if (relation == 0 && !g_localFactionName.empty() && p.factionId) {
                playerFName = ResolveFactionName(p.factionId);
                if (!playerFName.empty())
                    relation = (g_localFactionName == playerFName) ? 1 : -1;
            }
            if (relation == 0 && g_localTeamId != 0xFF && playerTeam >= 0)
                relation = (g_localTeamId == (uint8_t)playerTeam) ? 1 : -1;

            bool factionMatch = (relation == 1);
            if (relation != 0) {
                s_factionCache[actor] = factionMatch;
            }

            static bool s_factionDebugLogged = false;
            if (!s_factionDebugLogged && dbgValidMesh >= 3) {
                s_factionDebugLogged = true;
                char fdbg[512];
                sprintf_s(fdbg, "FactionDbg: rel=%d lData=0x%llX pData=0x%llX lName=%s pName=%s lFid=0x%X pFid=0x%X lTid=%d pTid=%d fc=0x%llX fObj=0x%llX",
                    relation, (unsigned long long)g_localFactionData, (unsigned long long)playerFactionData,
                    g_localFactionName.c_str(), playerFName.c_str(),
                    g_localFactionId, p.factionId, g_localTeamId, playerTeam,
                    (unsigned long long)factionComp, (unsigned long long)(validFC ? Read<uintptr_t>(factionComp + O::Faction_Object) : 0));
                WriteStartupLog("FactionDbg", fdbg);
            }

            uintptr_t invComp = Read<uintptr_t>(actor + O::WDChar_InventoryComp);
            if (invComp) {
                uintptr_t heldItem = Read<uintptr_t>(invComp + O::InvComp_HeldItem);
                if (!heldItem || heldItem < 0x10000000 || heldItem >= 0x7FFFFFFFFFFF)
                    heldItem = Read<uintptr_t>(invComp + O::InvComp_ActiveItem);
                if (heldItem && heldItem > 0x10000000 && heldItem < 0x7FFFFFFFFFFF) {
                    int32_t weaponTagIdx = Read<int32_t>(heldItem + O::BHItem_ItemId);
                    std::string tagStr = ResolveFName(weaponTagIdx);
                    if (!tagStr.empty()) {
                        size_t dot = tagStr.rfind('.');
                        p.weaponName = (dot != std::string::npos) ? tagStr.substr(dot + 1) : tagStr;
                    }
                }
            }

            p.isTeammate = factionMatch;
            dbgAfterTeam++;

            p.name = ReadFString(playerState + O::PS_PlayerNamePrivate);
            p.clanTag = ReadFString(playerState + O::PS_PlayerClanTag);
            p.factionName = ResolveFactionName(p.factionId);
            if (p.factionName.empty() && playerFactionData) {
                uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                if (dataTag) {
                    p.factionId = dataTag;
                    p.factionName = ResolveFactionName(dataTag);
                }
            }

            p.hasViewDir = false;
            uintptr_t controller = Read<uintptr_t>(actor + O::APawn_Controller);
            if (controller && controller > 0x10000000 && controller < 0x7FFFFFFFFFFF) {
                p.viewRotation = Read<DRotator>(controller + O::Controller_ControlRotation);
                p.hasViewDir = true;
            }
            if (!p.hasViewDir) {
                uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                    DRotator aim = Read<DRotator>(animInst + O::AnimInst_AimRotation);
                    if (fabs(aim.pitch) < 180.0 && fabs(aim.yaw) <= 360.0) {
                        p.viewRotation = aim;
                        p.hasViewDir = true;
                    }
                }
            }
            if (!p.hasViewDir) {
                float rvPitch = Read<float>(actor + O::BHPawn_SmoothedRemoteViewPitch);
                uint16_t rvYawComp = Read<uint16_t>(actor + O::BHPawn_RemoteViewYaw);
                if (fabs(rvPitch) < 180.f && rvYawComp != 0) {
                    p.viewRotation.pitch = (double)rvPitch;
                    p.viewRotation.yaw = (double)rvYawComp * (360.0 / 65536.0);
                    p.viewRotation.roll = 0.0;
                    p.hasViewDir = true;
                }
            }

            uintptr_t skinnedAsset = Read<uintptr_t>(mesh + O::Skinned_SkinnedAsset);
            if (!skinnedAsset)
                skinnedAsset = Read<uintptr_t>(mesh + O::Skinned_SkinnedAssetAlt);
            BoneIndices bi = skinnedAsset ? ResolveBones(skinnedAsset) : BoneIndices{};
            bool dynamicBones = bi.valid;
            if (!bi.valid) bi = FALLBACK_BONES;

            uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
            p.rootComp = rootComp;
            p.isAttached = false;
            if (rootComp) {
                bool gotPos = false;
                if (g_c2wOffset) {
                    DTransform t = Read<DTransform>(rootComp + g_c2wOffset);
                    if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                        (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                        fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                        p.position = t.translation;
                        gotPos = true;
                    }
                }
                if (!gotPos) {
                    for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                        DTransform t = Read<DTransform>(rootComp + off);
                        if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                            (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                            fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                            p.position = t.translation;
                            gotPos = true;
                            if (!g_c2wOffset) g_c2wOffset = off;
                            break;
                        }
                    }
                }
                if (!gotPos) p.position = Read<DVec3>(rootComp + O::Scene_RelativeLocation);
                uintptr_t attachParent = Read<uintptr_t>(rootComp + O::Scene_AttachParent);
                p.isAttached = (attachParent && attachParent > 0x10000000 && attachParent < 0x7FFFFFFFFFFF);
            }

            p.distance = (float)((p.position - s_workerCam.location).length() / 100.0);

            {
                p.visible = true;
                p.aimVisible = true;
                float lrtScreen = Read<float>(mesh + O::Skinned_LastRenderTimeOnScreen);
                uint8_t renderBits = Read<uint8_t>(mesh + O::Skinned_RenderStateBits);
                bool recentlyRendered = (renderBits & 0x80) != 0;
                if (s_workerCam.timeSeconds > 1.f && lrtScreen > 1.f && lrtScreen <= s_workerCam.timeSeconds) {
                    bool rendered = ((double)s_workerCam.timeSeconds - (double)lrtScreen) <= 0.15;
                    p.visible = rendered || recentlyRendered;
                } else {
                    p.visible = recentlyRendered;
                }
                p.aimVisible = recentlyRendered;
                if (p.visible || p.aimVisible) s_lastVisibleTime[actor] = tickNow;
                else if (s_lastVisibleTime.count(actor)) {
                    float age = std::chrono::duration<float>(tickNow - s_lastVisibleTime[actor]).count();
                    if (age <= 0.3f) p.aimVisible = true;
                }
            }

            dbgOnScreen++;

            p.boneCount = 22;
            if (!ReadSkeleton(mesh, bi, p.position, p.bones)) {
                p.headPos = p.position;
                p.headPos.z += 80.0;
                for (int b = 0; b < 22; b++) p.bones[b] = {};
                p.bones[0] = p.headPos;
            } else {
                p.headPos = p.bones[0];
            }

            p.resolvedBones = bi;
            p.velocity = {};
            if (tickDt > 0.001f && tickDt < 2.f) {
                auto it = s_prevPositions.find(p.pawn);
                if (it != s_prevPositions.end()) {
                    p.velocity = (p.position - it->second) * (1.0 / tickDt);
                }
            }
            s_prevPositions[p.pawn] = p.position;
            p.isValid = true;
            if (p.isDead) {
                s_deadPlayerCache[actor] = {p, tickNow};
                if (!cfg.espShowDead) continue;
            }
            s_workerPlayers.push_back(p);
        }

        {
            static auto s_lastPipeLog = std::chrono::steady_clock::now();
            float pipeSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - s_lastPipeLog).count();
            if ((!s_pipelineLogged && arrCount >= 5) || pipeSecs > 30.f) {
                s_pipelineLogged = true;
                s_lastPipeLog = std::chrono::steady_clock::now();
                int noMeshCount = dbgNotLocal - dbgValidMesh;
                int teamFiltered = dbgValidMesh - dbgDead - dbgAfterTeam;
                char pipe[384];
                sprintf_s(pipe, "total=%d validPS=%d withPawn=%d notLocal=%d validMesh=%d(noMesh=%d) dead=%d afterTeam=%d(teamFiltered=%d) added=%zu",
                    dbgTotal, dbgValidPS, dbgWithPawn, dbgNotLocal, dbgValidMesh, noMeshCount, dbgDead, dbgAfterTeam, teamFiltered, s_workerPlayers.size());
                WriteStartupLog("PlayerPipe", pipe);
            }
        }
        s_lastDeadCount = dbgDead;
        } // end arrData valid check
    }

    // SECONDARY: Scan level actor list for player pawns NOT in PlayerArray.
    // UE5 net relevancy excludes enemy PlayerStates from GameState->PlayerArray,
    // so enemies only appear as pawns in the persistent level's actor list.
    {
        std::unordered_set<uintptr_t> knownPawns;
        knownPawns.insert(g_localPawn);
        for (auto& p : s_workerPlayers)
            if (p.isValid) knownPawns.insert(p.pawn);

        uintptr_t actorArray = Read<uintptr_t>(persistLevel + O::ULevel_Actors);
        int32_t actorCount = Read<int32_t>(persistLevel + O::ULevel_Actors + 8);
        if (actorArray && actorCount > 0 && actorCount < 50000) {
            int actorScanFound = 0;
            auto meshValid = [](uintptr_t m, uintptr_t base) {
                return m && m >= 0x10000000 && m < 0x7FFFFFFFFFFF && !(m >= base && m < base + 0x10000000);
            };

            for (int i = 0; i < actorCount; i++) {
                uintptr_t actor = Read<uintptr_t>(actorArray + i * 8);
                if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;
                if (knownPawns.count(actor)) continue;

                uintptr_t mesh = Read<uintptr_t>(actor + O::WDChar_CharacterMesh);
                if (!meshValid(mesh, g_base)) {
                    mesh = Read<uintptr_t>(actor + O::WDChar_MeshFallback1);
                    if (!meshValid(mesh, g_base)) continue;
                }

                uintptr_t playerState = Read<uintptr_t>(actor + O::APawn_PlayerState);
                bool hasPS = (playerState && playerState > 0x10000000 && playerState < 0x7FFFFFFFFFFF);

                if (IsActorDummy(actor)) continue;
                std::string vehName;
                if (IsVehicleActor(actor, vehName)) continue;
                std::string wiName;
                if (IsWorldItemActor(actor, wiName) >= 0) continue;

                PlayerData p{};
                p.pawn = actor;
                p.mesh = mesh;

                p.healthValid = ReadHealth(actor, p.health, p.maxHealth);
                if (!p.healthValid) { p.health = 0.f; p.maxHealth = 100.f; }

                p.isInvincible = Read<uint8_t>(actor + O::WDChar_Invincible) != 0;
                p.bleedoutState = Read<uint8_t>(actor + O::WDChar_DeathState + O::DeathState_BleedoutState);
                p.giveUpTime = Read<float>(actor + O::WDChar_DeathState + O::DeathState_GiveUpTime);
                p.isDead = (p.bleedoutState >= 2) || (p.healthValid && p.health <= 0.f);
                p.isDowned = !p.isDead && (p.bleedoutState >= 1);
                if (!p.isDead && !p.isDowned && hasPS) {
                    uint32_t vTag = Read<uint32_t>(playerState + O::PS_VitalityStateTag);
                    if (!vTag) {
                        uint32_t inlineTag = Read<uint32_t>(actor + O::WDChar_DeathState + 0x08);
                        if (inlineTag) vTag = inlineTag;
                    }
                    if (vTag) {
                        if (s_deadTagId && vTag == s_deadTagId) p.isDead = true;
                        else if (s_dbnoTagId && vTag == s_dbnoTagId) p.isDowned = true;
                        else if (vTag != s_aliveTagId) {
                            std::string tagName = ResolveFName((int32_t)vTag);
                            if (tagName.find("Dead") != std::string::npos) { s_deadTagId = vTag; p.isDead = true; }
                            else if (tagName.find("DBNO") != std::string::npos || tagName.find("Down") != std::string::npos || tagName.find("Critical") != std::string::npos) { s_dbnoTagId = vTag; p.isDowned = true; }
                            else s_aliveTagId = vTag;
                        }
                    }
                }
                if (!p.isDead && !p.healthValid) p.isDead = true;
                if (p.isDead) s_persistentDeath[actor] = true;
                else if (s_persistentDeath.count(actor) && s_persistentDeath[actor]) {
                    if (p.healthValid && p.health > 0.01f && p.bleedoutState <= 1) s_persistentDeath[actor] = false;
                    else p.isDead = true;
                }
                p.isADS = Read<uint8_t>(actor + O::WDChar_AimingAlpha) != 0;

                p.stance = 2;
                p.isSprinting = false;
                p.isTacSprinting = false;
                if (mesh) {
                    uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                    if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                        if (Read<uint8_t>(animInst + O::AnimInst_StanceProne)) p.stance = 0;
                        else if (Read<uint8_t>(animInst + O::AnimInst_StanceCrouch)) p.stance = 1;
                        else p.stance = 2;
                        p.isSprinting = Read<uint8_t>(animInst + O::AnimInst_SprintOrTacSprint) != 0;
                    }
                }

                bool factionMatch = false;
                bool isSquadmate = false;
                if (hasPS) {
                    p.isDeveloper = Read<uint8_t>(playerState + O::PS_IsDeveloper) != 0;
                    p.isAdmin = Read<uint8_t>(playerState + O::PS_IsAdmin) != 0;
                    p.isBot = (Read<uint8_t>(playerState + O::PS_bIsABot) & 0x08) != 0;
                    p.ping = Read<uint8_t>(playerState + O::PS_CompressedPing);

                    uintptr_t factionComp = Read<uintptr_t>(playerState + O::PS_FactionComponent);
                    uintptr_t playerFactionData = 0;
                    int playerTeam2 = -1;
                    bool validFC2 = factionComp && factionComp > 0x10000000 && factionComp < 0x7F0000000000;
                    if (validFC2) {
                        uintptr_t playerFactionObj = Read<uintptr_t>(factionComp + O::Faction_Object);
                        if (playerFactionObj && playerFactionObj > 0x10000000 && playerFactionObj < 0x7F0000000000) {
                            playerFactionData = Read<uintptr_t>(playerFactionObj + O::Faction_Data);
                            if (playerFactionData && (playerFactionData < 0x10000000 || playerFactionData > 0x7F0000000000))
                                playerFactionData = 0;
                        }
                        if (playerFactionData) {
                            uint8_t tid = Read<uint8_t>(playerFactionData + O::FactionData_TeamId);
                            if (tid <= 32) playerTeam2 = tid;
                            uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                            if (dataTag) p.factionId = dataTag;
                        }
                        if (!p.factionId) {
                            uint32_t compTag = Read<uint32_t>(factionComp + O::Faction_TagRep);
                            if (compTag) p.factionId = compTag;
                        }
                        if (!p.factionId) {
                            uint32_t altTag = Read<uint32_t>(factionComp + O::Faction_Tag);
                            if (altTag) p.factionId = altTag;
                        }
                        if (p.factionId) {
                            std::string chk = ResolveFName((int32_t)p.factionId);
                            if (chk.empty() || chk == "None") p.factionId = 0;
                        }
                    }

                    int relation2 = 0;
                    uintptr_t squadComp = Read<uintptr_t>(playerState + O::PS_SquadComponent);
                    if (squadComp && squadComp > 0x10000000 && squadComp < 0x7FFFFFFFFFFF && g_localSquadded) {
                        uint64_t sqLo = Read<uint64_t>(squadComp + O::Squad_Id);
                        uint64_t sqHi = Read<uint64_t>(squadComp + O::Squad_Id + 8);
                        if ((sqLo | sqHi) != 0 && sqLo == g_localSquadLo && sqHi == g_localSquadHi)
                            relation2 = 1;
                    }
                    if (relation2 == 0 && g_localFactionData && playerFactionData)
                        relation2 = (playerFactionData == g_localFactionData) ? 1 : -1;
                    if (relation2 == 0 && g_localFactionId && p.factionId)
                        relation2 = (g_localFactionId == p.factionId) ? 1 : -1;
                    if (relation2 == 0 && !g_localFactionName.empty() && p.factionId) {
                        std::string pFName = ResolveFactionName(p.factionId);
                        if (!pFName.empty())
                            relation2 = (g_localFactionName == pFName) ? 1 : -1;
                    }
                    if (relation2 == 0 && g_localTeamId != 0xFF && playerTeam2 >= 0)
                        relation2 = (g_localTeamId == (uint8_t)playerTeam2) ? 1 : -1;

                    factionMatch = (relation2 == 1);
                    if (relation2 != 0) {
                        s_factionCache[actor] = factionMatch;
                    }

                    p.name = ReadFString(playerState + O::PS_PlayerNamePrivate);
                    p.clanTag = ReadFString(playerState + O::PS_PlayerClanTag);
                    p.factionName = ResolveFactionName(p.factionId);
                    if (p.factionName.empty() && playerFactionData) {
                        uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                        if (dataTag) {
                            p.factionId = dataTag;
                            p.factionName = ResolveFactionName(dataTag);
                        }
                    }
                }

                uintptr_t invComp = Read<uintptr_t>(actor + O::WDChar_InventoryComp);
                if (invComp) {
                    uintptr_t heldItem = Read<uintptr_t>(invComp + O::InvComp_HeldItem);
                    if (!heldItem || heldItem < 0x10000000 || heldItem >= 0x7FFFFFFFFFFF)
                        heldItem = Read<uintptr_t>(invComp + O::InvComp_ActiveItem);
                    if (heldItem && heldItem > 0x10000000 && heldItem < 0x7FFFFFFFFFFF) {
                        int32_t weaponTagIdx = Read<int32_t>(heldItem + O::BHItem_ItemId);
                        std::string tagStr = ResolveFName(weaponTagIdx);
                        if (!tagStr.empty()) {
                            size_t dot = tagStr.rfind('.');
                            p.weaponName = (dot != std::string::npos) ? tagStr.substr(dot + 1) : tagStr;
                        }
                    }
                }

                p.isTeammate = factionMatch;

                p.hasViewDir = false;
                uintptr_t controller = Read<uintptr_t>(actor + O::APawn_Controller);
                if (controller && controller > 0x10000000 && controller < 0x7FFFFFFFFFFF) {
                    p.viewRotation = Read<DRotator>(controller + O::Controller_ControlRotation);
                    p.hasViewDir = true;
                }
                if (!p.hasViewDir) {
                    uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                    if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                        DRotator aim = Read<DRotator>(animInst + O::AnimInst_AimRotation);
                        if (fabs(aim.pitch) < 180.0 && fabs(aim.yaw) <= 360.0) {
                            p.viewRotation = aim;
                            p.hasViewDir = true;
                        }
                    }
                }
                if (!p.hasViewDir) {
                    float rvPitch = Read<float>(actor + O::BHPawn_SmoothedRemoteViewPitch);
                    uint16_t rvYawComp = Read<uint16_t>(actor + O::BHPawn_RemoteViewYaw);
                    if (fabs(rvPitch) < 180.f && rvYawComp != 0) {
                        p.viewRotation.pitch = (double)rvPitch;
                        p.viewRotation.yaw = (double)rvYawComp * (360.0 / 65536.0);
                        p.viewRotation.roll = 0.0;
                        p.hasViewDir = true;
                    }
                }

                uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
                p.rootComp = rootComp;
                p.isAttached = false;
                if (rootComp) {
                    bool gotPos = false;
                    for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                        DTransform t = Read<DTransform>(rootComp + off);
                        if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                            (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                            fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                            p.position = t.translation;
                            gotPos = true;
                            break;
                        }
                    }
                    if (!gotPos) p.position = Read<DVec3>(rootComp + O::Scene_RelativeLocation);
                    uintptr_t attachParent = Read<uintptr_t>(rootComp + O::Scene_AttachParent);
                    p.isAttached = (attachParent && attachParent > 0x10000000 && attachParent < 0x7FFFFFFFFFFF);
                }
                if (p.position.x == 0.0 && p.position.y == 0.0 && p.position.z == 0.0) {
                    DVec3 repLoc;
                    repLoc.x = Read<double>(actor + O::AActor_ReplicatedMovement);
                    repLoc.y = Read<double>(actor + O::AActor_ReplicatedMovement + 8);
                    repLoc.z = Read<double>(actor + O::AActor_ReplicatedMovement + 16);
                    if (fabs(repLoc.x) > 1.0 || fabs(repLoc.y) > 1.0)
                        p.position = repLoc;
                }
                if (p.position.x == 0.0 && p.position.y == 0.0 && p.position.z == 0.0) continue;

                DTransform meshC2W = ReadC2W(mesh);
                uintptr_t skinnedAsset = Read<uintptr_t>(mesh + O::Skinned_SkinnedAsset);
                if (!skinnedAsset)
                    skinnedAsset = Read<uintptr_t>(mesh + O::Skinned_SkinnedAssetAlt);
                BoneIndices bi = skinnedAsset ? ResolveBones(skinnedAsset) : BoneIndices{};
                if (!bi.valid) bi = FALLBACK_BONES;

                p.distance = (float)((p.position - s_workerCam.location).length() / 100.0);

                {
                    p.visible = true;
                    p.aimVisible = true;
                    float lrtScreen = Read<float>(mesh + O::Skinned_LastRenderTimeOnScreen);
                    uint8_t renderBits = Read<uint8_t>(mesh + O::Skinned_RenderStateBits);
                    bool recentlyRendered = (renderBits & 0x80) != 0;
                    if (s_workerCam.timeSeconds > 1.f && lrtScreen > 1.f && lrtScreen <= s_workerCam.timeSeconds) {
                        bool rendered = ((double)s_workerCam.timeSeconds - (double)lrtScreen) <= 0.15;
                        p.visible = rendered || recentlyRendered;
                    } else {
                        p.visible = recentlyRendered;
                    }
                    p.aimVisible = recentlyRendered;
                    if (p.visible || p.aimVisible) s_lastVisibleTime[actor] = tickNow;
                    else if (s_lastVisibleTime.count(actor)) {
                        float age = std::chrono::duration<float>(tickNow - s_lastVisibleTime[actor]).count();
                        if (age <= 0.3f) p.aimVisible = true;
                    }
                }

                p.boneCount = 22;
                if (!ReadSkeleton(mesh, bi, p.position, p.bones)) {
                    p.headPos = p.position;
                    p.headPos.z += 160.0;
                    for (int b = 0; b < 22; b++) p.bones[b] = {};
                    p.bones[0] = p.headPos;
                } else {
                    p.headPos = p.bones[0];
                }

                p.resolvedBones = bi;
                p.velocity = {};
                if (tickDt > 0.001f && tickDt < 2.f) {
                    auto it = s_prevPositions.find(p.pawn);
                    if (it != s_prevPositions.end())
                        p.velocity = (p.position - it->second) * (1.0 / tickDt);
                }
                s_prevPositions[p.pawn] = p.position;

                p.isValid = true;
                if (p.isDead) {
                    s_deadPlayerCache[actor] = {p, tickNow};
                    if (!cfg.espShowDead) continue;
                }
                s_workerPlayers.push_back(p);
                actorScanFound++;
            }

            static auto s_lastActorScanLog = std::chrono::steady_clock::now();
            float asSecs = std::chrono::duration<float>(std::chrono::steady_clock::now() - s_lastActorScanLog).count();
            if (actorScanFound > 0 && asSecs > 10.f) {
                s_lastActorScanLog = std::chrono::steady_clock::now();
                char buf[192];
                sprintf_s(buf, "ActorList found %d extra players (not in PlayerArray), total now %zu",
                    actorScanFound, s_workerPlayers.size());
                WriteStartupLog("ActorScan", buf);
            }
        }
    }

    // STREAMING LEVELS: scan ULevelStreaming entries for additional player pawns
    {
        uintptr_t slData = Read<uintptr_t>(gworld + O::UWorld_StreamingLevels);
        int32_t slCount = Read<int32_t>(gworld + O::UWorld_StreamingLevels + 8);
        if (slData && slData > 0x10000000 && slData < 0x7FFFFFFFFFFF && slCount > 0 && slCount <= 200) {
            std::unordered_set<uintptr_t> knownPawns2;
            knownPawns2.insert(g_localPawn);
            for (auto& p : s_workerPlayers)
                if (p.isValid) knownPawns2.insert(p.pawn);

            auto meshValid = [](uintptr_t m, uintptr_t base) {
                return m && m >= 0x10000000 && m < 0x7FFFFFFFFFFF && !(m >= base && m < base + 0x10000000);
            };
            int slFound = 0;
            for (int si = 0; si < slCount; si++) {
                uintptr_t streaming = Read<uintptr_t>(slData + si * 8);
                if (!streaming || streaming < 0x10000000 || streaming >= 0x7FFFFFFFFFFF) continue;
                uintptr_t loadedLevel = Read<uintptr_t>(streaming + O::ULevelStreaming_LoadedLevel);
                if (!loadedLevel || loadedLevel < 0x10000000 || loadedLevel >= 0x7FFFFFFFFFFF) continue;
                if (loadedLevel == persistLevel) continue;

                uintptr_t slActors = Read<uintptr_t>(loadedLevel + O::ULevel_Actors);
                int32_t slActCount = Read<int32_t>(loadedLevel + O::ULevel_Actors + 8);
                if (!slActors || slActCount <= 0 || slActCount >= 50000) continue;

                for (int ai = 0; ai < slActCount; ai++) {
                    uintptr_t actor = Read<uintptr_t>(slActors + ai * 8);
                    if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;
                    if (knownPawns2.count(actor)) continue;

                    uintptr_t mesh = Read<uintptr_t>(actor + O::WDChar_CharacterMesh);
                    if (!meshValid(mesh, g_base)) {
                        mesh = Read<uintptr_t>(actor + O::WDChar_MeshFallback1);
                        if (!meshValid(mesh, g_base)) continue;
                    }

                    if (IsActorDummy(actor)) continue;
                    std::string slVehName;
                    if (IsVehicleActor(actor, slVehName)) continue;
                    std::string slWiName;
                    if (IsWorldItemActor(actor, slWiName) >= 0) continue;

                    uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
                    bool validRC = rootComp && rootComp >= 0x10000000 && rootComp < 0x7FFFFFFFFFFF;

                    DVec3 pos{};
                    if (validRC) {
                        bool gotPos = false;
                        for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                            DTransform t = Read<DTransform>(rootComp + off);
                            if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                                (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                                fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                                pos = t.translation;
                                gotPos = true;
                                break;
                            }
                        }
                        if (!gotPos) {
                            pos.x = Read<double>(rootComp + O::Scene_RelativeLocation);
                            pos.y = Read<double>(rootComp + O::Scene_RelativeLocation + 8);
                            pos.z = Read<double>(rootComp + O::Scene_RelativeLocation + 16);
                        }
                    }
                    if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0) {
                        DVec3 repLoc;
                        repLoc.x = Read<double>(actor + O::AActor_ReplicatedMovement);
                        repLoc.y = Read<double>(actor + O::AActor_ReplicatedMovement + 8);
                        repLoc.z = Read<double>(actor + O::AActor_ReplicatedMovement + 16);
                        if (fabs(repLoc.x) > 1.0 || fabs(repLoc.y) > 1.0)
                            pos = repLoc;
                    }
                    if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0) continue;

                    uintptr_t attachParent = validRC ? Read<uintptr_t>(rootComp + O::Scene_AttachParent) : 0;

                    PlayerData p{};
                    p.pawn = actor;
                    p.mesh = mesh;
                    p.rootComp = rootComp;
                    p.position = pos;
                    p.isAttached = (attachParent && attachParent > 0x10000000 && attachParent < 0x7FFFFFFFFFFF);
                    p.distance = (float)((pos - s_workerCam.location).length() / 100.0);
                    if (p.distance > 800.f) continue;

                    p.healthValid = ReadHealth(actor, p.health, p.maxHealth);
                    if (!p.healthValid) { p.health = 0.f; p.maxHealth = 100.f; }

                    p.isInvincible = Read<uint8_t>(actor + O::WDChar_Invincible) != 0;
                    p.bleedoutState = Read<uint8_t>(actor + O::WDChar_DeathState + O::DeathState_BleedoutState);
                    p.giveUpTime = Read<float>(actor + O::WDChar_DeathState + O::DeathState_GiveUpTime);
                    p.isDead = (p.bleedoutState >= 2) || (p.healthValid && p.health <= 0.f);
                    p.isDowned = !p.isDead && (p.bleedoutState >= 1);

                    uintptr_t playerState = Read<uintptr_t>(actor + O::APawn_PlayerState);
                    bool hasPS = (playerState && playerState > 0x10000000 && playerState < 0x7FFFFFFFFFFF);

                    if (!p.isDead && !p.isDowned && hasPS) {
                        uint32_t vTag = Read<uint32_t>(playerState + O::PS_VitalityStateTag);
                        if (!vTag) {
                            uint32_t inlineTag = Read<uint32_t>(actor + O::WDChar_DeathState + 0x08);
                            if (inlineTag) vTag = inlineTag;
                        }
                        if (vTag) {
                            if (s_deadTagId && vTag == s_deadTagId) { p.isDead = true; s_persistentDeath[actor] = true; }
                            else if (s_dbnoTagId && vTag == s_dbnoTagId) p.isDowned = true;
                            else if (vTag != s_aliveTagId) {
                                std::string tagName = ResolveFName((int32_t)vTag);
                                if (tagName.find("Dead") != std::string::npos) { s_deadTagId = vTag; p.isDead = true; s_persistentDeath[actor] = true; }
                                else if (tagName.find("DBNO") != std::string::npos || tagName.find("Down") != std::string::npos || tagName.find("Critical") != std::string::npos) { s_dbnoTagId = vTag; p.isDowned = true; }
                                else s_aliveTagId = vTag;
                            }
                        }
                    }

                    if (!p.isDead && !p.healthValid) p.isDead = true;
                    if (p.isDead) s_persistentDeath[actor] = true;
                    else if (s_persistentDeath.count(actor) && s_persistentDeath[actor]) {
                        if (p.healthValid && p.health > 0.01f && p.bleedoutState <= 1) s_persistentDeath[actor] = false;
                        else p.isDead = true;
                    }
                    p.isADS = Read<uint8_t>(actor + O::WDChar_AimingAlpha) != 0;

                    p.stance = 2;
                    p.isSprinting = false;
                    p.isTacSprinting = false;
                    if (mesh) {
                        uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                        if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                            if (Read<uint8_t>(animInst + O::AnimInst_StanceProne)) p.stance = 0;
                            else if (Read<uint8_t>(animInst + O::AnimInst_StanceCrouch)) p.stance = 1;
                            else p.stance = 2;
                            p.isSprinting = Read<uint8_t>(animInst + O::AnimInst_SprintOrTacSprint) != 0;
                        }
                    }

                    if (hasPS) {
                        p.isDeveloper = Read<uint8_t>(playerState + O::PS_IsDeveloper) != 0;
                        p.isAdmin = Read<uint8_t>(playerState + O::PS_IsAdmin) != 0;
                        p.isBot = (Read<uint8_t>(playerState + O::PS_bIsABot) & 0x08) != 0;
                        p.ping = Read<uint8_t>(playerState + O::PS_CompressedPing);

                        bool factionMatch = false;
                        uintptr_t factionComp = Read<uintptr_t>(playerState + O::PS_FactionComponent);
                        uintptr_t playerFactionData = 0;
                        int playerTeamSL = -1;
                        bool validFC = factionComp && factionComp > 0x10000000 && factionComp < 0x7F0000000000;
                        if (validFC) {
                            uintptr_t playerFactionObj = Read<uintptr_t>(factionComp + O::Faction_Object);
                            if (playerFactionObj && playerFactionObj > 0x10000000 && playerFactionObj < 0x7F0000000000) {
                                playerFactionData = Read<uintptr_t>(playerFactionObj + O::Faction_Data);
                                if (playerFactionData && (playerFactionData < 0x10000000 || playerFactionData > 0x7F0000000000))
                                    playerFactionData = 0;
                            }
                            if (playerFactionData) {
                                uint8_t tid = Read<uint8_t>(playerFactionData + O::FactionData_TeamId);
                                if (tid <= 32) playerTeamSL = tid;
                                uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                                if (dataTag) p.factionId = dataTag;
                            }
                            if (!p.factionId) {
                                uint32_t compTag = Read<uint32_t>(factionComp + O::Faction_TagRep);
                                if (compTag) p.factionId = compTag;
                            }
                            if (!p.factionId) {
                                uint32_t altTag = Read<uint32_t>(factionComp + O::Faction_Tag);
                                if (altTag) p.factionId = altTag;
                            }
                        }

                        int relation = 0;
                        uintptr_t squadCompSL = Read<uintptr_t>(playerState + O::PS_SquadComponent);
                        if (squadCompSL && squadCompSL > 0x10000000 && squadCompSL < 0x7FFFFFFFFFFF && g_localSquadded) {
                            uint64_t sqLo = Read<uint64_t>(squadCompSL + O::Squad_Id);
                            uint64_t sqHi = Read<uint64_t>(squadCompSL + O::Squad_Id + 8);
                            if ((sqLo | sqHi) != 0 && sqLo == g_localSquadLo && sqHi == g_localSquadHi)
                                relation = 1;
                        }
                        if (relation == 0 && g_localFactionData && playerFactionData)
                            relation = (playerFactionData == g_localFactionData) ? 1 : -1;
                        if (relation == 0 && g_localFactionId && p.factionId)
                            relation = (g_localFactionId == p.factionId) ? 1 : -1;
                        if (relation == 0 && !g_localFactionName.empty() && p.factionId) {
                            std::string pFN = ResolveFactionName(p.factionId);
                            if (!pFN.empty())
                                relation = (g_localFactionName == pFN) ? 1 : -1;
                        }
                        if (relation == 0 && g_localTeamId != 0xFF && playerTeamSL >= 0)
                            relation = (g_localTeamId == (uint8_t)playerTeamSL) ? 1 : -1;

                        factionMatch = (relation == 1);
                        if (relation != 0) {
                            s_factionCache[actor] = factionMatch;
                        }

                        p.isTeammate = factionMatch;
                        p.name = ReadFString(playerState + O::PS_PlayerNamePrivate);
                        p.clanTag = ReadFString(playerState + O::PS_PlayerClanTag);
                        p.factionName = ResolveFactionName(p.factionId);
                        if (p.factionName.empty() && playerFactionData) {
                            uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                            if (dataTag) {
                                p.factionId = dataTag;
                                p.factionName = ResolveFactionName(dataTag);
                            }
                        }
                    }

                    uintptr_t invComp = Read<uintptr_t>(actor + O::WDChar_InventoryComp);
                    if (invComp) {
                        uintptr_t heldItem = Read<uintptr_t>(invComp + O::InvComp_HeldItem);
                        if (!heldItem || heldItem < 0x10000000 || heldItem >= 0x7FFFFFFFFFFF)
                            heldItem = Read<uintptr_t>(invComp + O::InvComp_ActiveItem);
                        if (heldItem && heldItem > 0x10000000 && heldItem < 0x7FFFFFFFFFFF) {
                            int32_t weaponTagIdx = Read<int32_t>(heldItem + O::BHItem_ItemId);
                            std::string tagStr = ResolveFName(weaponTagIdx);
                            if (!tagStr.empty()) {
                                size_t dot = tagStr.rfind('.');
                                p.weaponName = (dot != std::string::npos) ? tagStr.substr(dot + 1) : tagStr;
                            }
                        }
                    }

                    p.hasViewDir = false;
                    uintptr_t controller = Read<uintptr_t>(actor + O::APawn_Controller);
                    if (controller && controller > 0x10000000 && controller < 0x7FFFFFFFFFFF) {
                        p.viewRotation = Read<DRotator>(controller + O::Controller_ControlRotation);
                        p.hasViewDir = true;
                    }
                    if (!p.hasViewDir) {
                        uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                        if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                            DRotator aim = Read<DRotator>(animInst + O::AnimInst_AimRotation);
                            if (fabs(aim.pitch) < 180.0 && fabs(aim.yaw) <= 360.0) {
                                p.viewRotation = aim;
                                p.hasViewDir = true;
                            }
                        }
                    }
                    if (!p.hasViewDir) {
                        float rvPitch = Read<float>(actor + O::BHPawn_SmoothedRemoteViewPitch);
                        uint16_t rvYawComp = Read<uint16_t>(actor + O::BHPawn_RemoteViewYaw);
                        if (fabs(rvPitch) < 180.f && rvYawComp != 0) {
                            p.viewRotation.pitch = (double)rvPitch;
                            p.viewRotation.yaw = (double)rvYawComp * (360.0 / 65536.0);
                            p.viewRotation.roll = 0.0;
                            p.hasViewDir = true;
                        }
                    }

                    {
                        p.visible = true;
                        p.aimVisible = true;
                        float lrtScreen = Read<float>(mesh + O::Skinned_LastRenderTimeOnScreen);
                        uint8_t renderBits = Read<uint8_t>(mesh + O::Skinned_RenderStateBits);
                        bool recentlyRendered = (renderBits & 0x80) != 0;
                        if (s_workerCam.timeSeconds > 1.f && lrtScreen > 1.f && lrtScreen <= s_workerCam.timeSeconds) {
                            bool rendered = ((double)s_workerCam.timeSeconds - (double)lrtScreen) <= 0.15;
                            p.visible = rendered || recentlyRendered;
                        } else {
                            p.visible = recentlyRendered;
                        }
                        p.aimVisible = recentlyRendered;
                        if (p.visible || p.aimVisible) s_lastVisibleTime[actor] = tickNow;
                        else if (s_lastVisibleTime.count(actor)) {
                            float age = std::chrono::duration<float>(tickNow - s_lastVisibleTime[actor]).count();
                            if (age <= 0.3f) p.aimVisible = true;
                        }
                    }

                    uintptr_t sa = Read<uintptr_t>(mesh + O::Skinned_SkinnedAsset);
                    if (!sa) sa = Read<uintptr_t>(mesh + O::Skinned_SkinnedAssetAlt);
                    BoneIndices bi = sa ? ResolveBones(sa) : BoneIndices{};
                    if (!bi.valid) bi = FALLBACK_BONES;

                    p.boneCount = 22;
                    if (!ReadSkeleton(mesh, bi, p.position, p.bones)) {
                        p.headPos = p.position;
                        p.headPos.z += 160.0;
                        for (int b = 0; b < 22; b++) p.bones[b] = {};
                        p.bones[0] = p.headPos;
                    } else {
                        p.headPos = p.bones[0];
                    }
                    p.resolvedBones = bi;
                    p.velocity = {};
                    if (tickDt > 0.001f && tickDt < 2.f) {
                        auto it = s_prevPositions.find(p.pawn);
                        if (it != s_prevPositions.end())
                            p.velocity = (p.position - it->second) * (1.0 / tickDt);
                    }
                    s_prevPositions[p.pawn] = p.position;
                    p.isValid = true;
                    if (p.isDead) {
                        s_deadPlayerCache[actor] = {p, tickNow};
                        if (!cfg.espShowDead) { knownPawns2.insert(actor); continue; }
                    }
                    knownPawns2.insert(actor);
                    s_workerPlayers.push_back(p);
                    slFound++;
                }
            }
            if (slFound > 0) {
                static auto s_lastSlLog = std::chrono::steady_clock::now();
                if (std::chrono::duration<float>(std::chrono::steady_clock::now() - s_lastSlLog).count() > 15.f) {
                    s_lastSlLog = std::chrono::steady_clock::now();
                    char buf[128];
                    sprintf_s(buf, "StreamingLevels found %d extra players", slFound);
                    WriteStartupLog("StreamingScan", buf);
                }
            }
        }
    }

    // UWORLD::LEVELS: scan World Partition sub-levels for player pawns
    {
        uintptr_t lvlOff = g_levelsOffset ? g_levelsOffset : O::UWorld_Levels;
        uintptr_t lvlData = Read<uintptr_t>(gworld + lvlOff);
        int32_t lvlCount = Read<int32_t>(gworld + lvlOff + 8);
        if (lvlData && lvlData > 0x10000000 && lvlData < 0x7FFFFFFFFFFF && lvlCount > 0 && lvlCount <= 500) {
            std::unordered_set<uintptr_t> knownPawns3;
            knownPawns3.insert(g_localPawn);
            for (auto& pp : s_workerPlayers)
                if (pp.isValid) knownPawns3.insert(pp.pawn);

            auto meshValid = [](uintptr_t m, uintptr_t base) {
                return m && m >= 0x10000000 && m < 0x7FFFFFFFFFFF && !(m >= base && m < base + 0x10000000);
            };
            int wpFound = 0;

            for (int li = 0; li < lvlCount; li++) {
                uintptr_t level = Read<uintptr_t>(lvlData + li * 8);
                if (!level || level < 0x10000000 || level >= 0x7FFFFFFFFFFF) continue;
                if (level == persistLevel) continue;

                uintptr_t wpActors = Read<uintptr_t>(level + O::ULevel_Actors);
                int32_t wpActCount = Read<int32_t>(level + O::ULevel_Actors + 8);
                if (!wpActors || wpActCount <= 0 || wpActCount >= 50000) continue;

                for (int ai = 0; ai < wpActCount; ai++) {
                    uintptr_t actor = Read<uintptr_t>(wpActors + ai * 8);
                    if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;
                    if (knownPawns3.count(actor)) continue;

                    uintptr_t mesh = Read<uintptr_t>(actor + O::WDChar_CharacterMesh);
                    if (!meshValid(mesh, g_base)) {
                        mesh = Read<uintptr_t>(actor + O::WDChar_MeshFallback1);
                        if (!meshValid(mesh, g_base)) continue;
                    }

                    if (IsActorDummy(actor)) continue;
                    std::string wpVehName;
                    if (IsVehicleActor(actor, wpVehName)) continue;
                    std::string wpWiName;
                    if (IsWorldItemActor(actor, wpWiName) >= 0) continue;

                    uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
                    bool validRC = rootComp && rootComp >= 0x10000000 && rootComp < 0x7FFFFFFFFFFF;

                    DVec3 pos{};
                    if (validRC) {
                        bool gotPos = false;
                        for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                            DTransform t = Read<DTransform>(rootComp + off);
                            if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                                (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                                fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                                pos = t.translation;
                                gotPos = true;
                                break;
                            }
                        }
                        if (!gotPos) {
                            pos.x = Read<double>(rootComp + O::Scene_RelativeLocation);
                            pos.y = Read<double>(rootComp + O::Scene_RelativeLocation + 8);
                            pos.z = Read<double>(rootComp + O::Scene_RelativeLocation + 16);
                        }
                    }
                    if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0) {
                        DVec3 repLoc;
                        repLoc.x = Read<double>(actor + O::AActor_ReplicatedMovement);
                        repLoc.y = Read<double>(actor + O::AActor_ReplicatedMovement + 8);
                        repLoc.z = Read<double>(actor + O::AActor_ReplicatedMovement + 16);
                        if (fabs(repLoc.x) > 1.0 || fabs(repLoc.y) > 1.0)
                            pos = repLoc;
                    }
                    if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0) continue;

                    uintptr_t attachParent = validRC ? Read<uintptr_t>(rootComp + O::Scene_AttachParent) : 0;

                    PlayerData p{};
                    p.pawn = actor;
                    p.mesh = mesh;
                    p.rootComp = rootComp;
                    p.position = pos;
                    p.isAttached = (attachParent && attachParent > 0x10000000 && attachParent < 0x7FFFFFFFFFFF);
                    p.distance = (float)((pos - s_workerCam.location).length() / 100.0);
                    if (p.distance > 800.f) continue;

                    p.healthValid = ReadHealth(actor, p.health, p.maxHealth);
                    if (!p.healthValid) { p.health = 0.f; p.maxHealth = 100.f; }

                    p.isInvincible = Read<uint8_t>(actor + O::WDChar_Invincible) != 0;
                    p.bleedoutState = Read<uint8_t>(actor + O::WDChar_DeathState + O::DeathState_BleedoutState);
                    p.giveUpTime = Read<float>(actor + O::WDChar_DeathState + O::DeathState_GiveUpTime);
                    p.isDead = (p.bleedoutState >= 2) || (p.healthValid && p.health <= 0.f);
                    p.isDowned = !p.isDead && (p.bleedoutState >= 1);

                    uintptr_t playerState = Read<uintptr_t>(actor + O::APawn_PlayerState);
                    bool hasPS = (playerState && playerState > 0x10000000 && playerState < 0x7FFFFFFFFFFF);

                    if (!p.isDead && !p.isDowned && hasPS) {
                        uint32_t vTag = Read<uint32_t>(playerState + O::PS_VitalityStateTag);
                        if (!vTag) {
                            uint32_t inlineTag = Read<uint32_t>(actor + O::WDChar_DeathState + 0x08);
                            if (inlineTag) vTag = inlineTag;
                        }
                        if (vTag) {
                            if (s_deadTagId && vTag == s_deadTagId) { p.isDead = true; s_persistentDeath[actor] = true; }
                            else if (s_dbnoTagId && vTag == s_dbnoTagId) p.isDowned = true;
                            else if (vTag != s_aliveTagId) {
                                std::string tagName = ResolveFName((int32_t)vTag);
                                if (tagName.find("Dead") != std::string::npos) { s_deadTagId = vTag; p.isDead = true; s_persistentDeath[actor] = true; }
                                else if (tagName.find("DBNO") != std::string::npos || tagName.find("Down") != std::string::npos || tagName.find("Critical") != std::string::npos) { s_dbnoTagId = vTag; p.isDowned = true; }
                                else s_aliveTagId = vTag;
                            }
                        }
                    }

                    if (!p.isDead && !p.healthValid) p.isDead = true;
                    if (p.isDead) s_persistentDeath[actor] = true;
                    else if (s_persistentDeath.count(actor) && s_persistentDeath[actor]) {
                        if (p.healthValid && p.health > 0.01f && p.bleedoutState <= 1) s_persistentDeath[actor] = false;
                        else p.isDead = true;
                    }
                    p.isADS = Read<uint8_t>(actor + O::WDChar_AimingAlpha) != 0;

                    p.stance = 2;
                    p.isSprinting = false;
                    p.isTacSprinting = false;
                    if (mesh) {
                        uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                        if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                            if (Read<uint8_t>(animInst + O::AnimInst_StanceProne)) p.stance = 0;
                            else if (Read<uint8_t>(animInst + O::AnimInst_StanceCrouch)) p.stance = 1;
                            else p.stance = 2;
                            p.isSprinting = Read<uint8_t>(animInst + O::AnimInst_SprintOrTacSprint) != 0;
                        }
                    }

                    if (hasPS) {
                        p.isDeveloper = Read<uint8_t>(playerState + O::PS_IsDeveloper) != 0;
                        p.isAdmin = Read<uint8_t>(playerState + O::PS_IsAdmin) != 0;
                        p.isBot = (Read<uint8_t>(playerState + O::PS_bIsABot) & 0x08) != 0;
                        p.ping = Read<uint8_t>(playerState + O::PS_CompressedPing);

                        bool factionMatch = false;
                        uintptr_t factionComp = Read<uintptr_t>(playerState + O::PS_FactionComponent);
                        uintptr_t playerFactionData = 0;
                        int playerTeamWP = -1;
                        bool validFC = factionComp && factionComp > 0x10000000 && factionComp < 0x7F0000000000;
                        if (validFC) {
                            uintptr_t playerFactionObj = Read<uintptr_t>(factionComp + O::Faction_Object);
                            if (playerFactionObj && playerFactionObj > 0x10000000 && playerFactionObj < 0x7F0000000000) {
                                playerFactionData = Read<uintptr_t>(playerFactionObj + O::Faction_Data);
                                if (playerFactionData && (playerFactionData < 0x10000000 || playerFactionData > 0x7F0000000000))
                                    playerFactionData = 0;
                            }
                            if (playerFactionData) {
                                uint8_t tid = Read<uint8_t>(playerFactionData + O::FactionData_TeamId);
                                if (tid <= 32) playerTeamWP = tid;
                                uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                                if (dataTag) p.factionId = dataTag;
                            }
                            if (!p.factionId) {
                                uint32_t compTag = Read<uint32_t>(factionComp + O::Faction_TagRep);
                                if (compTag) p.factionId = compTag;
                            }
                            if (!p.factionId) {
                                uint32_t altTag = Read<uint32_t>(factionComp + O::Faction_Tag);
                                if (altTag) p.factionId = altTag;
                            }
                        }

                        int relation = 0;
                        uintptr_t squadCompWP = Read<uintptr_t>(playerState + O::PS_SquadComponent);
                        if (squadCompWP && squadCompWP > 0x10000000 && squadCompWP < 0x7FFFFFFFFFFF && g_localSquadded) {
                            uint64_t sqLo = Read<uint64_t>(squadCompWP + O::Squad_Id);
                            uint64_t sqHi = Read<uint64_t>(squadCompWP + O::Squad_Id + 8);
                            if ((sqLo | sqHi) != 0 && sqLo == g_localSquadLo && sqHi == g_localSquadHi)
                                relation = 1;
                        }
                        if (relation == 0 && g_localFactionData && playerFactionData)
                            relation = (playerFactionData == g_localFactionData) ? 1 : -1;
                        if (relation == 0 && g_localFactionId && p.factionId)
                            relation = (g_localFactionId == p.factionId) ? 1 : -1;
                        if (relation == 0 && !g_localFactionName.empty() && p.factionId) {
                            std::string pFN = ResolveFactionName(p.factionId);
                            if (!pFN.empty())
                                relation = (g_localFactionName == pFN) ? 1 : -1;
                        }
                        if (relation == 0 && g_localTeamId != 0xFF && playerTeamWP >= 0)
                            relation = (g_localTeamId == (uint8_t)playerTeamWP) ? 1 : -1;

                        factionMatch = (relation == 1);
                        if (relation != 0) {
                            s_factionCache[actor] = factionMatch;
                        }

                        p.isTeammate = factionMatch;
                        p.name = ReadFString(playerState + O::PS_PlayerNamePrivate);
                        p.clanTag = ReadFString(playerState + O::PS_PlayerClanTag);
                        p.factionName = ResolveFactionName(p.factionId);
                        if (p.factionName.empty() && playerFactionData) {
                            uint32_t dataTag = Read<uint32_t>(playerFactionData + O::FactionData_Tag);
                            if (dataTag) {
                                p.factionId = dataTag;
                                p.factionName = ResolveFactionName(dataTag);
                            }
                        }
                    }

                    uintptr_t invComp = Read<uintptr_t>(actor + O::WDChar_InventoryComp);
                    if (invComp) {
                        uintptr_t heldItem = Read<uintptr_t>(invComp + O::InvComp_HeldItem);
                        if (!heldItem || heldItem < 0x10000000 || heldItem >= 0x7FFFFFFFFFFF)
                            heldItem = Read<uintptr_t>(invComp + O::InvComp_ActiveItem);
                        if (heldItem && heldItem > 0x10000000 && heldItem < 0x7FFFFFFFFFFF) {
                            int32_t weaponTagIdx = Read<int32_t>(heldItem + O::BHItem_ItemId);
                            std::string tagStr = ResolveFName(weaponTagIdx);
                            if (!tagStr.empty()) {
                                size_t dot = tagStr.rfind('.');
                                p.weaponName = (dot != std::string::npos) ? tagStr.substr(dot + 1) : tagStr;
                            }
                        }
                    }

                    p.hasViewDir = false;
                    uintptr_t controller = Read<uintptr_t>(actor + O::APawn_Controller);
                    if (controller && controller > 0x10000000 && controller < 0x7FFFFFFFFFFF) {
                        p.viewRotation = Read<DRotator>(controller + O::Controller_ControlRotation);
                        p.hasViewDir = true;
                    }
                    if (!p.hasViewDir) {
                        uintptr_t animInst = Read<uintptr_t>(mesh + O::Skinned_AnimScriptInst);
                        if (animInst && animInst > 0x10000000 && animInst < 0x7FFFFFFFFFFF) {
                            DRotator aim = Read<DRotator>(animInst + O::AnimInst_AimRotation);
                            if (fabs(aim.pitch) < 180.0 && fabs(aim.yaw) <= 360.0) {
                                p.viewRotation = aim;
                                p.hasViewDir = true;
                            }
                        }
                    }
                    if (!p.hasViewDir) {
                        float rvPitch = Read<float>(actor + O::BHPawn_SmoothedRemoteViewPitch);
                        uint16_t rvYawComp = Read<uint16_t>(actor + O::BHPawn_RemoteViewYaw);
                        if (fabs(rvPitch) < 180.f && rvYawComp != 0) {
                            p.viewRotation.pitch = (double)rvPitch;
                            p.viewRotation.yaw = (double)rvYawComp * (360.0 / 65536.0);
                            p.viewRotation.roll = 0.0;
                            p.hasViewDir = true;
                        }
                    }

                    {
                        p.visible = true;
                        p.aimVisible = true;
                        float lrtScreen = Read<float>(mesh + O::Skinned_LastRenderTimeOnScreen);
                        uint8_t renderBits = Read<uint8_t>(mesh + O::Skinned_RenderStateBits);
                        bool recentlyRendered = (renderBits & 0x80) != 0;
                        if (s_workerCam.timeSeconds > 1.f && lrtScreen > 1.f && lrtScreen <= s_workerCam.timeSeconds) {
                            bool rendered = ((double)s_workerCam.timeSeconds - (double)lrtScreen) <= 0.15;
                            p.visible = rendered || recentlyRendered;
                        } else {
                            p.visible = recentlyRendered;
                        }
                        p.aimVisible = recentlyRendered;
                        if (p.visible || p.aimVisible) s_lastVisibleTime[actor] = tickNow;
                        else if (s_lastVisibleTime.count(actor)) {
                            float age = std::chrono::duration<float>(tickNow - s_lastVisibleTime[actor]).count();
                            if (age <= 0.3f) p.aimVisible = true;
                        }
                    }

                    uintptr_t sa = Read<uintptr_t>(mesh + O::Skinned_SkinnedAsset);
                    if (!sa) sa = Read<uintptr_t>(mesh + O::Skinned_SkinnedAssetAlt);
                    BoneIndices bi = sa ? ResolveBones(sa) : BoneIndices{};
                    if (!bi.valid) bi = FALLBACK_BONES;

                    p.boneCount = 22;
                    if (!ReadSkeleton(mesh, bi, p.position, p.bones)) {
                        p.headPos = p.position;
                        p.headPos.z += 160.0;
                        for (int b = 0; b < 22; b++) p.bones[b] = {};
                        p.bones[0] = p.headPos;
                    } else {
                        p.headPos = p.bones[0];
                    }
                    p.resolvedBones = bi;
                    p.velocity = {};
                    if (tickDt > 0.001f && tickDt < 2.f) {
                        auto it = s_prevPositions.find(p.pawn);
                        if (it != s_prevPositions.end())
                            p.velocity = (p.position - it->second) * (1.0 / tickDt);
                    }
                    s_prevPositions[p.pawn] = p.position;
                    p.isValid = true;
                    if (p.isDead) {
                        s_deadPlayerCache[actor] = {p, tickNow};
                        if (!cfg.espShowDead) { knownPawns3.insert(actor); continue; }
                    }
                    knownPawns3.insert(actor);
                    s_workerPlayers.push_back(p);
                    wpFound++;
                }
            }
            if (wpFound > 0) {
                static auto s_lastWpLog = std::chrono::steady_clock::now();
                if (std::chrono::duration<float>(std::chrono::steady_clock::now() - s_lastWpLog).count() > 15.f) {
                    s_lastWpLog = std::chrono::steady_clock::now();
                    char buf[128];
                    sprintf_s(buf, "WorldPartition found %d extra players", wpFound);
                    WriteStartupLog("WorldPartitionScan", buf);
                }
            }
        }
    }

    // Deduplicate players found by multiple scan loops
    {
        std::unordered_set<uintptr_t> seenPawns;
        auto end = std::remove_if(s_workerPlayers.begin(), s_workerPlayers.end(),
            [&seenPawns](const PlayerData& p) {
                if (!p.isValid || !p.pawn) return false;
                if (seenPawns.count(p.pawn)) return true;
                seenPawns.insert(p.pawn);
                return false;
            });
        s_workerPlayers.erase(end, s_workerPlayers.end());
    }

    // Inject cached dead players whose pawns were destroyed (not found by any scan)
    if (cfg.espShowDead) {
        std::unordered_set<uintptr_t> livePawns;
        for (auto& p : s_workerPlayers)
            if (p.isValid) livePawns.insert(p.pawn);

        for (auto it = s_deadPlayerCache.begin(); it != s_deadPlayerCache.end(); ) {
            float age = std::chrono::duration<float>(tickNow - it->second.deathTime).count();
            if (age > 120.f) { it = s_deadPlayerCache.erase(it); continue; }
            if (!livePawns.count(it->first)) {
                PlayerData dp = it->second.data;
                dp.isDead = true;
                dp.visible = false;
                dp.aimVisible = false;
                s_workerPlayers.push_back(dp);
            }
            ++it;
        }
    } else {
        for (auto it = s_deadPlayerCache.begin(); it != s_deadPlayerCache.end(); ) {
            float age = std::chrono::duration<float>(tickNow - it->second.deathTime).count();
            if (age > 120.f) it = s_deadPlayerCache.erase(it);
            else ++it;
        }
    }

    // Build set of teammate pawns for vehicle team detection
    std::unordered_set<uintptr_t> teammatePawns;
    teammatePawns.insert(g_localPawn);
    for (auto& p : s_workerPlayers) {
        if (p.isValid && p.isTeammate) teammatePawns.insert(p.pawn);
    }

    // Vehicle scan — iterate level actors looking for vehicle pawns
    s_workerVehicles.clear();
    std::unordered_set<uintptr_t> vehicleOccupantPawns;
    if (cfg.espVehicles) {
        uintptr_t actorArray = Read<uintptr_t>(persistLevel + O::ULevel_Actors);
        int32_t actorCount = Read<int32_t>(persistLevel + O::ULevel_Actors + 8);
        if (actorArray && actorCount > 0 && actorCount < 50000) {
            for (int i = 0; i < actorCount; i++) {
                uintptr_t actor = Read<uintptr_t>(actorArray + i * 8);
                if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;

                std::string className;
                if (!IsVehicleActor(actor, className)) continue;

                uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
                if (!rootComp) continue;

                VehicleData v{};
                v.actor = actor;
                {
                    bool gotVPos = false;
                    for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                        DTransform t = Read<DTransform>(rootComp + off);
                        if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                            (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                            fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                            v.position = t.translation;
                            gotVPos = true;
                            break;
                        }
                    }
                    if (!gotVPos) v.position = Read<DVec3>(rootComp + O::Scene_RelativeLocation);
                }
                if (v.position.x == 0.0 && v.position.y == 0.0 && v.position.z == 0.0) {
                    DVec3 repLoc;
                    repLoc.x = Read<double>(actor + O::AActor_ReplicatedMovement);
                    repLoc.y = Read<double>(actor + O::AActor_ReplicatedMovement + 8);
                    repLoc.z = Read<double>(actor + O::AActor_ReplicatedMovement + 16);
                    if (fabs(repLoc.x) > 1.0 || fabs(repLoc.y) > 1.0)
                        v.position = repLoc;
                }
                if (v.position.x == 0.0 && v.position.y == 0.0 && v.position.z == 0.0) continue;
                v.distance = (float)((v.position - s_workerCam.location).length() / 100.0);
                if (v.distance > cfg.vehicleMaxDistance) continue;

                uintptr_t seatComp = Read<uintptr_t>(actor + O::Vehicle_SeatComponent);
                v.occupantCount = 0;

                v.isDestroyed = Read<uint8_t>(actor + O::Vehicle_Destroyed) != 0;
                v.engineRunning = Read<uint8_t>(actor + O::Vehicle_EngineRunning) != 0;

                // Check if local player or teammates are in this vehicle
                v.hasLocalPlayer = false;
                v.isTeamVehicle = false;
                if (seatComp) {
                    uintptr_t occArray = Read<uintptr_t>(seatComp + O::SeatComp_Occupants);
                    int32_t occArrCount = Read<int32_t>(seatComp + O::SeatComp_Occupants + 8);
                    if (occArray && occArrCount > 0 && occArrCount < 30) {
                        for (int oi = 0; oi < occArrCount; oi++) {
                            uintptr_t opComp = Read<uintptr_t>(occArray + oi * 8);
                            if (!opComp || opComp < 0x10000000) continue;
                            uintptr_t outerPawn = Read<uintptr_t>(opComp + 0x20);
                            if (!outerPawn || outerPawn < 0x10000000) continue;
                            v.occupantCount++;
                            vehicleOccupantPawns.insert(outerPawn);
                            if (!v.isTeamVehicle) {
                                if (outerPawn == g_localPawn) {
                                    v.hasLocalPlayer = true;
                                    v.isTeamVehicle = true;
                                } else if (teammatePawns.count(outerPawn)) {
                                    v.isTeamVehicle = true;
                                }
                            }
                            if (cfg.espVehicleOccupants && outerPawn != g_localPawn) {
                                uintptr_t occPS = Read<uintptr_t>(outerPawn + O::APawn_PlayerState);
                                if (occPS) {
                                    std::string occName = ReadFString(occPS + O::PS_PlayerNamePrivate);
                                    if (!occName.empty()) v.occupantNames.push_back(occName);
                                }
                            }
                        }
                    }
                    // Fallback: if occupant check didn't identify team, try faction directly
                    if (!v.isTeamVehicle && (g_localFactionData || g_localFactionObj) && occArray && occArrCount > 0 && occArrCount < 30) {
                        for (int oi = 0; oi < occArrCount && !v.isTeamVehicle; oi++) {
                            uintptr_t opComp = Read<uintptr_t>(occArray + oi * 8);
                            if (!opComp || opComp < 0x10000000) continue;
                            uintptr_t outerPawn = Read<uintptr_t>(opComp + 0x20);
                            if (!outerPawn || outerPawn < 0x10000000 || outerPawn >= 0x7FFFFFFFFFFF) continue;
                            uintptr_t occPS = Read<uintptr_t>(outerPawn + O::APawn_PlayerState);
                            if (!occPS) continue;
                            uintptr_t occFC = Read<uintptr_t>(occPS + O::PS_FactionComponent);
                            if (!occFC || occFC < 0x10000000 || occFC >= 0x7F0000000000) continue;
                            uintptr_t occFObj = Read<uintptr_t>(occFC + O::Faction_Object);
                            if (occFObj && occFObj > 0x10000000 && occFObj < 0x7F0000000000) {
                                if (occFObj == g_localFactionObj) { v.isTeamVehicle = true; break; }
                                uintptr_t occFData = Read<uintptr_t>(occFObj + O::Faction_Data);
                                if (g_localFactionData && occFData == g_localFactionData) { v.isTeamVehicle = true; break; }
                            }
                        }
                    }
                }
                v.isNeutral = false;
                if (!v.isTeamVehicle && (g_localFactionId || g_localTeamId != 0xFF)) {
                    uintptr_t vehFcOff = O::VehicleFaction_Stationary;
                    if (className.find("Wheeled") != std::string::npos || className.find("WHL") != std::string::npos ||
                        className.find("Truck") != std::string::npos || className.find("APC") != std::string::npos)
                        vehFcOff = O::VehicleFaction_Wheeled;
                    else if (className.find("Tracked") != std::string::npos || className.find("TNK") != std::string::npos ||
                             className.find("Tank") != std::string::npos || className.find("SPW") != std::string::npos)
                        vehFcOff = O::VehicleFaction_Tracked;
                    else if (className.find("Rotary") != std::string::npos || className.find("ROT") != std::string::npos ||
                             className.find("Helicopter") != std::string::npos || className.find("Heli") != std::string::npos ||
                             className.find("Chinook") != std::string::npos || className.find("Blackhawk") != std::string::npos)
                        vehFcOff = O::VehicleFaction_Rotary;
                    else if (className.find("Airplane") != std::string::npos || className.find("AIR") != std::string::npos ||
                             className.find("Aircraft") != std::string::npos)
                        vehFcOff = O::VehicleFaction_Airplane;

                    bool anyFactionFound = false;
                    uintptr_t vfc = Read<uintptr_t>(actor + vehFcOff);
                    if (vfc && vfc > 0x10000000 && vfc < 0x7F0000000000) {
                        uintptr_t vfObj = Read<uintptr_t>(vfc + O::Faction_Object);
                        if (vfObj && vfObj > 0x10000000 && vfObj < 0x7F0000000000) {
                            uintptr_t vfData = Read<uintptr_t>(vfObj + O::Faction_Data);
                            if (vfData && vfData > 0x10000000 && vfData < 0x7F0000000000) {
                                anyFactionFound = true;
                                if (g_localFactionData && vfData == g_localFactionData) v.isTeamVehicle = true;
                                else {
                                    uint32_t vfId = Read<uint32_t>(vfData + O::FactionData_Tag);
                                    if (vfId && g_localFactionId && vfId == g_localFactionId) v.isTeamVehicle = true;
                                    else {
                                        uint8_t vTid = Read<uint8_t>(vfData + O::FactionData_TeamId);
                                        if (vTid <= 32 && g_localTeamId != 0xFF && vTid == g_localTeamId) v.isTeamVehicle = true;
                                    }
                                }
                            }
                        }
                        if (!anyFactionFound) {
                            uint32_t vTagRep = Read<uint32_t>(vfc + O::Faction_TagRep);
                            if (!vTagRep) vTagRep = Read<uint32_t>(vfc + O::Faction_Tag);
                            if (vTagRep) {
                                anyFactionFound = true;
                                if (g_localFactionId && vTagRep == g_localFactionId) v.isTeamVehicle = true;
                            }
                        }
                    }
                    if (!anyFactionFound && v.occupantCount == 0)
                        v.isNeutral = true;
                }

                // Clean up class name and resolve to friendly name
                size_t bp = className.find("_BP");
                if (bp != std::string::npos) className = className.substr(0, bp);
                size_t c = className.find("_C");
                if (c != std::string::npos) className = className.substr(0, c);
                v.typeName = ResolveVehicleName(className);
                v.isAir = IsVehicleAir(className);
                v.velocity = {};
                v.speed = 0.f;
                if (tickDt > 0.001f && tickDt < 2.f) {
                    auto it = s_prevVehPositions.find(v.actor);
                    if (it != s_prevVehPositions.end()) {
                        v.velocity = (v.position - it->second) * (1.0 / tickDt);
                        v.speed = (float)(sqrt(v.velocity.x*v.velocity.x + v.velocity.y*v.velocity.y) / 100.0);
                    }
                }
                s_prevVehPositions[v.actor] = v.position;
                if (v.hasLocalPlayer) { v.isValid = false; continue; }
                v.isValid = true;
                s_workerVehicles.push_back(v);
            }
        }

        // Streaming level vehicle scan
        std::unordered_set<uintptr_t> knownVehicles;
        for (auto& vv : s_workerVehicles) knownVehicles.insert(vv.actor);
        uintptr_t slVData = Read<uintptr_t>(gworld + O::UWorld_StreamingLevels);
        int32_t slVCount = Read<int32_t>(gworld + O::UWorld_StreamingLevels + 8);
        if (slVData && slVData > 0x10000000 && slVData < 0x7FFFFFFFFFFF && slVCount > 0 && slVCount <= 200) {

            for (int si = 0; si < slVCount; si++) {
                uintptr_t streaming = Read<uintptr_t>(slVData + si * 8);
                if (!streaming || streaming < 0x10000000 || streaming >= 0x7FFFFFFFFFFF) continue;
                uintptr_t loadedLevel = Read<uintptr_t>(streaming + O::ULevelStreaming_LoadedLevel);
                if (!loadedLevel || loadedLevel < 0x10000000 || loadedLevel >= 0x7FFFFFFFFFFF) continue;
                if (loadedLevel == persistLevel) continue;

                uintptr_t slActors = Read<uintptr_t>(loadedLevel + O::ULevel_Actors);
                int32_t slActCount = Read<int32_t>(loadedLevel + O::ULevel_Actors + 8);
                if (!slActors || slActCount <= 0 || slActCount >= 50000) continue;

                for (int ai = 0; ai < slActCount; ai++) {
                    uintptr_t actor = Read<uintptr_t>(slActors + ai * 8);
                    if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;
                    if (knownVehicles.count(actor)) continue;

                    std::string className;
                    if (!IsVehicleActor(actor, className)) continue;

                    uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
                    if (!rootComp) continue;

                    VehicleData v{};
                    v.actor = actor;
                    {
                        bool gotVPos = false;
                        for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                            DTransform t = Read<DTransform>(rootComp + off);
                            if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                                (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                                fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                                v.position = t.translation;
                                gotVPos = true;
                                break;
                            }
                        }
                        if (!gotVPos) v.position = Read<DVec3>(rootComp + O::Scene_RelativeLocation);
                    }
                    if (v.position.x == 0.0 && v.position.y == 0.0 && v.position.z == 0.0) {
                        DVec3 repLoc;
                        repLoc.x = Read<double>(actor + O::AActor_ReplicatedMovement);
                        repLoc.y = Read<double>(actor + O::AActor_ReplicatedMovement + 8);
                        repLoc.z = Read<double>(actor + O::AActor_ReplicatedMovement + 16);
                        if (fabs(repLoc.x) > 1.0 || fabs(repLoc.y) > 1.0)
                            v.position = repLoc;
                    }
                    if (v.position.x == 0.0 && v.position.y == 0.0 && v.position.z == 0.0) continue;
                    v.distance = (float)((v.position - s_workerCam.location).length() / 100.0);
                    if (v.distance > cfg.vehicleMaxDistance) continue;

                    uintptr_t seatComp = Read<uintptr_t>(actor + O::Vehicle_SeatComponent);
                    v.occupantCount = 0;
                    v.isDestroyed = Read<uint8_t>(actor + O::Vehicle_Destroyed) != 0;
                    v.engineRunning = Read<uint8_t>(actor + O::Vehicle_EngineRunning) != 0;
                    v.hasLocalPlayer = false;
                    v.isTeamVehicle = false;
                    v.isNeutral = false;

                    if (seatComp) {
                        uintptr_t occArray = Read<uintptr_t>(seatComp + O::SeatComp_Occupants);
                        int32_t occArrCount = Read<int32_t>(seatComp + O::SeatComp_Occupants + 8);
                        if (occArray && occArrCount > 0 && occArrCount < 30) {
                            for (int oi = 0; oi < occArrCount; oi++) {
                                uintptr_t opComp = Read<uintptr_t>(occArray + oi * 8);
                                if (!opComp || opComp < 0x10000000) continue;
                                uintptr_t outerPawn = Read<uintptr_t>(opComp + 0x20);
                                if (!outerPawn || outerPawn < 0x10000000) continue;
                                v.occupantCount++;
                                vehicleOccupantPawns.insert(outerPawn);
                                if (!v.isTeamVehicle) {
                                    if (outerPawn == g_localPawn) {
                                        v.hasLocalPlayer = true;
                                        v.isTeamVehicle = true;
                                    } else if (teammatePawns.count(outerPawn)) {
                                        v.isTeamVehicle = true;
                                    }
                                }
                                if (cfg.espVehicleOccupants && outerPawn != g_localPawn) {
                                    uintptr_t occPS = Read<uintptr_t>(outerPawn + O::APawn_PlayerState);
                                    if (occPS) {
                                        std::string occName = ReadFString(occPS + O::PS_PlayerNamePrivate);
                                        if (!occName.empty()) v.occupantNames.push_back(occName);
                                    }
                                }
                            }
                        }
                    }
                    if (!v.isTeamVehicle && (g_localFactionId || g_localTeamId != 0xFF)) {
                        uintptr_t vehFcOff = O::VehicleFaction_Stationary;
                        if (className.find("Wheeled") != std::string::npos || className.find("WHL") != std::string::npos ||
                            className.find("Truck") != std::string::npos || className.find("APC") != std::string::npos)
                            vehFcOff = O::VehicleFaction_Wheeled;
                        else if (className.find("Tracked") != std::string::npos || className.find("TNK") != std::string::npos ||
                                 className.find("Tank") != std::string::npos || className.find("SPW") != std::string::npos)
                            vehFcOff = O::VehicleFaction_Tracked;
                        else if (className.find("Rotary") != std::string::npos || className.find("ROT") != std::string::npos ||
                                 className.find("Helicopter") != std::string::npos || className.find("Heli") != std::string::npos ||
                                 className.find("Chinook") != std::string::npos || className.find("Blackhawk") != std::string::npos)
                            vehFcOff = O::VehicleFaction_Rotary;
                        else if (className.find("Airplane") != std::string::npos || className.find("AIR") != std::string::npos ||
                                 className.find("Aircraft") != std::string::npos)
                            vehFcOff = O::VehicleFaction_Airplane;

                        bool anyFactionFound = false;
                        uintptr_t vfc = Read<uintptr_t>(actor + vehFcOff);
                        if (vfc && vfc > 0x10000000 && vfc < 0x7F0000000000) {
                            uintptr_t vfObj = Read<uintptr_t>(vfc + O::Faction_Object);
                            if (vfObj && vfObj > 0x10000000 && vfObj < 0x7F0000000000) {
                                uintptr_t vfData = Read<uintptr_t>(vfObj + O::Faction_Data);
                                if (vfData && vfData > 0x10000000 && vfData < 0x7F0000000000) {
                                    anyFactionFound = true;
                                    if (g_localFactionData && vfData == g_localFactionData) v.isTeamVehicle = true;
                                    else {
                                        uint32_t vfId = Read<uint32_t>(vfData + O::FactionData_Tag);
                                        if (vfId && g_localFactionId && vfId == g_localFactionId) v.isTeamVehicle = true;
                                        else {
                                            uint8_t vTid = Read<uint8_t>(vfData + O::FactionData_TeamId);
                                            if (vTid <= 32 && g_localTeamId != 0xFF && vTid == g_localTeamId) v.isTeamVehicle = true;
                                        }
                                    }
                                }
                            }
                            if (!anyFactionFound) {
                                uint32_t vTagRep = Read<uint32_t>(vfc + O::Faction_TagRep);
                                if (!vTagRep) vTagRep = Read<uint32_t>(vfc + O::Faction_Tag);
                                if (vTagRep) {
                                    anyFactionFound = true;
                                    if (g_localFactionId && vTagRep == g_localFactionId) v.isTeamVehicle = true;
                                }
                            }
                        }
                        if (!anyFactionFound && v.occupantCount == 0)
                            v.isNeutral = true;
                    }

                    size_t bp = className.find("_BP");
                    if (bp != std::string::npos) className = className.substr(0, bp);
                    size_t c = className.find("_C");
                    if (c != std::string::npos) className = className.substr(0, c);
                    v.typeName = ResolveVehicleName(className);
                    v.isAir = IsVehicleAir(className);
                    v.velocity = {};
                    v.speed = 0.f;
                    if (tickDt > 0.001f && tickDt < 2.f) {
                        auto it = s_prevVehPositions.find(v.actor);
                        if (it != s_prevVehPositions.end()) {
                            v.velocity = (v.position - it->second) * (1.0 / tickDt);
                            v.speed = (float)(sqrt(v.velocity.x*v.velocity.x + v.velocity.y*v.velocity.y) / 100.0);
                        }
                    }
                    s_prevVehPositions[v.actor] = v.position;
                    if (v.hasLocalPlayer) { v.isValid = false; continue; }
                    v.isValid = true;
                    knownVehicles.insert(actor);
                    s_workerVehicles.push_back(v);
                }
            }
        }

        // UWorld::Levels scan — catches vehicles in World Partition sub-levels
        uintptr_t lvlOff = g_levelsOffset ? g_levelsOffset : O::UWorld_Levels;
        uintptr_t lvlData = Read<uintptr_t>(gworld + lvlOff);
        int32_t lvlCount = Read<int32_t>(gworld + lvlOff + 8);
        if (lvlData && lvlData > 0x10000000 && lvlData < 0x7FFFFFFFFFFF && lvlCount > 0 && lvlCount <= 500) {
            for (int li = 0; li < lvlCount; li++) {
                uintptr_t level = Read<uintptr_t>(lvlData + li * 8);
                if (!level || level < 0x10000000 || level >= 0x7FFFFFFFFFFF) continue;
                if (level == persistLevel) continue;
                uintptr_t slActors = Read<uintptr_t>(level + O::ULevel_Actors);
                int32_t slActCount = Read<int32_t>(level + O::ULevel_Actors + 8);
                if (!slActors || slActCount <= 0 || slActCount >= 50000) continue;
                for (int ai = 0; ai < slActCount; ai++) {
                    uintptr_t actor = Read<uintptr_t>(slActors + ai * 8);
                    if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;
                    if (knownVehicles.count(actor)) continue;
                    std::string className;
                    if (!IsVehicleActor(actor, className)) continue;
                    uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
                    if (!rootComp) continue;
                    VehicleData v{};
                    v.actor = actor;
                    {
                        bool gotVPos = false;
                        for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                            DTransform t = Read<DTransform>(rootComp + off);
                            if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                                (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                                fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                                v.position = t.translation;
                                gotVPos = true;
                                break;
                            }
                        }
                        if (!gotVPos) v.position = Read<DVec3>(rootComp + O::Scene_RelativeLocation);
                    }
                    if (v.position.x == 0.0 && v.position.y == 0.0 && v.position.z == 0.0) continue;
                    v.distance = (float)((v.position - s_workerCam.location).length() / 100.0);
                    if (v.distance > cfg.vehicleMaxDistance) continue;
                    uintptr_t seatComp = Read<uintptr_t>(actor + O::Vehicle_SeatComponent);
                    v.occupantCount = 0;
                    v.isDestroyed = Read<uint8_t>(actor + O::Vehicle_Destroyed) != 0;
                    v.engineRunning = Read<uint8_t>(actor + O::Vehicle_EngineRunning) != 0;
                    v.hasLocalPlayer = false;
                    v.isTeamVehicle = false;
                    v.isNeutral = false;
                    if (seatComp) {
                        uintptr_t occArray = Read<uintptr_t>(seatComp + O::SeatComp_Occupants);
                        int32_t occArrCount = Read<int32_t>(seatComp + O::SeatComp_Occupants + 8);
                        if (occArray && occArrCount > 0 && occArrCount < 30) {
                            for (int oi = 0; oi < occArrCount; oi++) {
                                uintptr_t opComp = Read<uintptr_t>(occArray + oi * 8);
                                if (!opComp || opComp < 0x10000000) continue;
                                uintptr_t outerPawn = Read<uintptr_t>(opComp + 0x20);
                                if (!outerPawn || outerPawn < 0x10000000) continue;
                                v.occupantCount++;
                                vehicleOccupantPawns.insert(outerPawn);
                                if (!v.isTeamVehicle) {
                                    if (outerPawn == g_localPawn) { v.hasLocalPlayer = true; v.isTeamVehicle = true; }
                                    else if (teammatePawns.count(outerPawn)) v.isTeamVehicle = true;
                                }
                            }
                        }
                    }
                    size_t bp = className.find("_BP"); if (bp != std::string::npos) className = className.substr(0, bp);
                    size_t c = className.find("_C"); if (c != std::string::npos) className = className.substr(0, c);
                    v.typeName = ResolveVehicleName(className);
                    v.isAir = IsVehicleAir(className);
                    v.velocity = {}; v.speed = 0.f;
                    if (v.hasLocalPlayer) continue;
                    v.isValid = true;
                    knownVehicles.insert(actor);
                    s_workerVehicles.push_back(v);
                }
            }
        }
    }

    // Mark players as in-vehicle by cross-referencing with vehicle occupant pawns
    if (!vehicleOccupantPawns.empty()) {
        for (auto& p : s_workerPlayers) {
            if (p.isValid && vehicleOccupantPawns.count(p.pawn))
                p.isInVehicle = true;
        }
    }

    // World item scan — dropped items, mines, explosives
    s_workerWorldItems.clear();
    if (cfg.espWorldItems || cfg.espMines || cfg.espEmplacements || cfg.espFOBs || cfg.espBodybags) {
        uintptr_t actorArray = Read<uintptr_t>(persistLevel + O::ULevel_Actors);
        int32_t actorCount = Read<int32_t>(persistLevel + O::ULevel_Actors + 8);
        if (actorArray && actorCount > 0 && actorCount < 50000) {
            for (int i = 0; i < actorCount; i++) {
                uintptr_t actor = Read<uintptr_t>(actorArray + i * 8);
                if (!actor || actor < 0x10000000 || actor >= 0x7FFFFFFFFFFF) continue;

                std::string className;
                int itemType = IsWorldItemActor(actor, className);
                if (itemType < 0) continue;
                if (itemType == 0 && !cfg.espWorldItems) continue;
                if (itemType == 1 && !cfg.espMines) continue;
                if (itemType == 2 && !cfg.espEmplacements) continue;
                if (itemType == 3 && !cfg.espFOBs) continue;
                if (itemType == 4 && !cfg.espBodybags) continue;

                uintptr_t rootComp = Read<uintptr_t>(actor + O::AActor_RootComponent);
                if (!rootComp) continue;

                WorldItemData w{};
                w.position = Read<DVec3>(rootComp + O::Scene_RelativeLocation);
                w.distance = (float)((w.position - s_workerCam.location).length() / 100.0);
                float maxDist = (itemType >= 2) ? cfg.vehicleMaxDistance : cfg.espMaxDistance;
                if (w.distance > maxDist) continue;

                // Resolve item name from FGameplayTag
                if (itemType == 4) {
                    w.name = "Bodybag";
                    int32_t tagIdx = Read<int32_t>(actor + O::DroppedItem_ItemTag);
                    if (tagIdx) {
                        std::string tagStr = ResolveFName(tagIdx);
                        if (!tagStr.empty() && tagStr != "None") {
                            size_t dot = tagStr.rfind('.');
                            std::string itemName = (dot != std::string::npos) ? tagStr.substr(dot + 1) : tagStr;
                            if (!itemName.empty()) w.name = "Bodybag + " + itemName;
                        }
                    }
                } else if (itemType == 0) {
                    int32_t tagIdx = Read<int32_t>(actor + O::DroppedItem_ItemTag);
                    std::string tagStr = ResolveFName(tagIdx);
                    size_t dot = tagStr.rfind('.');
                    w.name = (dot != std::string::npos) ? tagStr.substr(dot + 1) : tagStr;
                } else {
                    int32_t tagIdx = Read<int32_t>(actor + O::Placeable_EntityId);
                    std::string tagStr = ResolveFName(tagIdx);
                    size_t dot = tagStr.rfind('.');
                    w.name = (dot != std::string::npos) ? tagStr.substr(dot + 1) : tagStr;
                }
                if (w.name.empty()) {
                    size_t bp = className.find("_BP");
                    if (bp != std::string::npos) className = className.substr(0, bp);
                    size_t c = className.find("_C");
                    if (c != std::string::npos) className = className.substr(0, c);
                    w.name = className;
                }

                w.type = itemType;
                w.subType = (itemType == 0) ? ClassifyDroppedItem(w.name) : 0;

                if (itemType == 0) {
                    if (w.subType == 0 && !cfg.espItemWeapons) continue;
                    if (w.subType == 1 && !cfg.espItemAmmo) continue;
                    if (w.subType == 2 && !cfg.espItemAttachments) continue;
                    if (w.subType == 3 && !cfg.espItemMedical) continue;
                    if (w.subType == 4 && !cfg.espItemGrenades) continue;
                    if (w.subType == 5 && !cfg.espItemOther) continue;
                }

                w.isValid = true;
                s_workerWorldItems.push_back(w);
            }
        }
    }

    // Hitmarker detection
    if (cfg.espHitmarker) {
        for (auto& p : s_workerPlayers) {
            if (!p.isValid || p.isTeammate) continue;
            for (auto& prev : s_workerPrevHealth) {
                if (prev.pawn == p.pawn && p.health < prev.health) {
                    g_hitmarkerTimer = 0.25f;
                    break;
                }
            }
        }
        s_workerPrevHealth.clear();
        for (auto& p : s_workerPlayers) {
            if (p.isValid && !p.isTeammate)
                s_workerPrevHealth.push_back({p.pawn, p.health});
        }
    }

    // Periodic status log every 15 seconds (AFTER iteration so count is accurate)
    auto now = std::chrono::steady_clock::now();
    float secsSinceLog = std::chrono::duration<float>(now - s_lastStatusLog).count();
    if (secsSinceLog > 10.f) {
        s_lastStatusLog = now;
        int paCnt = 0;
        if (g_gameState && g_playerArrayOffset)
            paCnt = Read<int32_t>(g_gameState + g_playerArrayOffset + 8);
        int enemies = 0, teammates = 0;
        for (auto& p : s_workerPlayers) { if (p.isTeammate) teammates++; else enemies++; }
        int teamVehs = 0, enemyVehs = 0;
        for (auto& v : s_workerVehicles) { if (v.isTeamVehicle) teamVehs++; else enemyVehs++; }
        char status[512];
        sprintf_s(status, "PA(%d) shown=%zu(E:%d T:%d dead=%d) vehs=%zu(T:%d E:%d) cam=%s lPawn=0x%llX lFTag=0x%llX lFObj=0x%llX lFData=0x%llX showTeam=%d",
            paCnt, s_workerPlayers.size(), enemies, teammates, s_lastDeadCount,
            s_workerVehicles.size(), teamVehs, enemyVehs,
            s_workerCam.valid ? "OK" : "NO",
            (unsigned long long)g_localPawn,
            (unsigned long long)g_localFactionId,
            (unsigned long long)g_localFactionObj,
            (unsigned long long)g_localFactionData,
            cfg.espShowTeam ? 1 : 0);
        WriteStartupLog("Status", status);

        // Log first 3 players' faction data for debugging
        int facDbg = 0;
        for (auto& p : s_workerPlayers) {
            if (!p.isValid || facDbg >= 3) break;
            facDbg++;
            char facBuf[256];
            sprintf_s(facBuf, "  player=%s dist=%.0f fTag=0x%llX isTeam=%d bleed=%d vis=%d hp=%.0f",
                p.name.c_str(), p.distance, (unsigned long long)p.factionId,
                p.isTeammate ? 1 : 0, p.bleedoutState, p.visible ? 1 : 0, p.health);
            WriteStartupLog("Status", facBuf);
        }
    }

    // Prune velocity maps — remove entries for entities no longer present
    if (s_prevPositions.size() > 256) {
        std::unordered_set<uintptr_t> activePawns;
        for (auto& p : s_workerPlayers) activePawns.insert(p.pawn);
        for (auto it = s_prevPositions.begin(); it != s_prevPositions.end();) {
            if (activePawns.find(it->first) == activePawns.end()) it = s_prevPositions.erase(it);
            else ++it;
        }
    }
    if (s_prevVehPositions.size() > 256) {
        std::unordered_set<uintptr_t> activeVehs;
        for (auto& v : s_workerVehicles) activeVehs.insert(v.actor);
        for (auto it = s_prevVehPositions.begin(); it != s_prevVehPositions.end();) {
            if (activeVehs.find(it->first) == activeVehs.end()) it = s_prevVehPositions.erase(it);
            else ++it;
        }
    }

    s_prevTickTime = tickNow;
}

// ============================================================================
// Entity cache worker — scans entities in background thread
// ============================================================================
static void ApplyNoRecoil() {
    auto& cfg = g_config.Active();
    if (!cfg.noRecoil || !s_workerCam.valid || !IsValidPtr(g_localPawn)) return;
    uintptr_t weapBehav = Read<uintptr_t>(g_localPawn + O::WDChar_WeaponBehavior);
    if (!IsValidPtr(weapBehav)) return;
    uintptr_t statsData = Read<uintptr_t>(weapBehav + O::WeaponBehavior_StatsData);
    if (!IsValidPtr(statsData)) return;

    SafeWrite<float>(statsData + O::WeaponStats_ViewKick, 0.0f);
    SafeWrite<int32_t>(weapBehav + O::WeaponBehavior_ShotIndex, 0);

    uintptr_t patArr = Read<uintptr_t>(weapBehav + O::WeaponBehavior_PatternArray);
    int32_t patCount = Read<int32_t>(weapBehav + O::WeaponBehavior_PatternArray + 0x8);
    if (IsValidPtr(patArr) && patCount > 0 && patCount < 200) {
        for (int i = 0; i < patCount; i++) {
            SafeWrite<float>(patArr + i * 0x40 + 0x08, 0.0f);
            SafeWrite<float>(patArr + i * 0x40 + 0x10, 0.0f);
        }
    }

    SafeWrite<float>(statsData + O::WeaponStats_PostPatternRandomH, 0.0f);
    SafeWrite<float>(statsData + O::WeaponStats_PostPatternRandomV, 0.0f);
    SafeWrite<float>(statsData + O::WeaponStats_RecoilYawMinMax, 0.0f);
    SafeWrite<float>(statsData + O::WeaponStats_RecoilYawMinMax + 4, 0.0f);
    SafeWrite<float>(statsData + O::WeaponStats_RecoilPitchMax, 0.0f);
}

static void ApplyNoSway() {
    auto& cfg = g_config.Active();
    if (!cfg.noSway || !s_workerCam.valid || !IsValidPtr(g_localPawn)) return;
    uintptr_t weapBehav = Read<uintptr_t>(g_localPawn + O::WDChar_WeaponBehavior);
    if (!IsValidPtr(weapBehav)) return;

    SafeWrite<uint8_t>(weapBehav + O::WeaponBehavior_SwayMode, 2);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayEnergyX, 0.0f);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayEnergyY, 0.0f);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayEnergyZ, 0.0f);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayEnergyW, 0.0f);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayLiveX, 0.0f);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayLiveY, 0.0f);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayLiveZ, 0.0f);
    SafeWrite<float>(weapBehav + O::WeaponBehavior_SwayLiveW, 0.0f);
}

static void ApplyNoSpread() {
    auto& cfg = g_config.Active();
    if (!cfg.noSpread || !g_localPawn) return;
    // TODO: Spread is in FAttributeSet at WDWeaponOwner + 0x3F8
    // Need to find the specific attribute name/index for spread multiplier.
}

static bool WorkerTick_Safe() {
    __try {
        UpdateCamera();
        UpdatePlayers();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        WriteStartupLog("WorkerSEH", "detection exception — continuing");
        Sleep(100);
        return false;
    }
    __try { ApplyNoRecoil(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
    __try { ApplyNoSway(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
    __try { ApplyNoSpread(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return true;
}

void EntityCache::WorkerThread() {
    while (g_running.load()) {
        if (WorkerTick_Safe()) {
            auto snap = std::make_shared<Snapshot>();
            snap->players = s_workerPlayers;
            snap->vehicles = s_workerVehicles;
            snap->worldItems = s_workerWorldItems;
            snap->localPawn = g_localPawn;
            snap->localFaction = g_localFactionId;
            snap->localFactionObj = g_localFactionObj;
            snap->localFactionData = g_localFactionData;
            snap->timestamp = std::chrono::steady_clock::now();

            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_snapshot = snap;
            }
        }

        Stealth::JitteredSleep(12, 4);
    }
}

// ============================================================================
// Drawing helpers
// ============================================================================
static ImU32 GetColor(const float c[4]) {
    return IM_COL32((int)(c[0]*255), (int)(c[1]*255), (int)(c[2]*255), (int)(c[3]*255));
}

static void DrawBoneLine(ImDrawList* dl, const PlayerData& p, int from, int to, ImU32 col) {
    if (from >= p.boneCount || to >= p.boneCount) return;
    const DVec3& a = p.bones[from];
    const DVec3& b = p.bones[to];
    if (a.x == 0 && a.y == 0 && a.z == 0) return;
    if (b.x == 0 && b.y == 0 && b.z == 0) return;
    double dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
    if (dx*dx + dy*dy + dz*dz > 100.0*100.0) return;
    Vec2 s1, s2;
    if (WorldToScreen(a, s1) && WorldToScreen(b, s2)) {
        float sdx = s2.x - s1.x, sdy = s2.y - s1.y;
        if (sdx*sdx + sdy*sdy < 4.f) return;
        dl->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), IM_COL32(0,0,0,180), 2.5f);
        dl->AddLine(ImVec2(s1.x, s1.y), ImVec2(s2.x, s2.y), col, 1.5f);
    }
}

static void DrawSkeleton(ImDrawList* dl, const PlayerData& p, ImU32 col) {
    DrawBoneLine(dl, p, 0, 1, col);   // head->neck
    DrawBoneLine(dl, p, 1, 2, col);   // neck->spine3
    DrawBoneLine(dl, p, 2, 3, col);   // spine3->spine2
    DrawBoneLine(dl, p, 3, 4, col);   // spine2->spine1
    DrawBoneLine(dl, p, 4, 5, col);   // spine1->pelvis
    if (p.bones[18].x != 0 || p.bones[18].y != 0 || p.bones[18].z != 0) {
        DrawBoneLine(dl, p, 2, 18, col);  // spine3->L_clavicle
        DrawBoneLine(dl, p, 18, 6, col);  // L_clavicle->L_upper
    } else {
        DrawBoneLine(dl, p, 2, 6, col);   // spine3->L_upper
    }
    DrawBoneLine(dl, p, 6, 7, col);   // L_upper->L_fore
    DrawBoneLine(dl, p, 7, 8, col);   // L_fore->L_hand
    if (p.bones[19].x != 0 || p.bones[19].y != 0 || p.bones[19].z != 0) {
        DrawBoneLine(dl, p, 2, 19, col);  // spine3->R_clavicle
        DrawBoneLine(dl, p, 19, 9, col);  // R_clavicle->R_upper
    } else {
        DrawBoneLine(dl, p, 2, 9, col);   // spine3->R_upper
    }
    DrawBoneLine(dl, p, 9, 10, col);  // R_upper->R_fore
    DrawBoneLine(dl, p, 10, 11, col); // R_fore->R_hand
    DrawBoneLine(dl, p, 5, 12, col);  // pelvis->L_thigh
    DrawBoneLine(dl, p, 12, 13, col); // L_thigh->L_calf
    DrawBoneLine(dl, p, 13, 14, col); // L_calf->L_foot
    DrawBoneLine(dl, p, 14, 20, col); // L_foot->L_toe
    DrawBoneLine(dl, p, 5, 15, col);  // pelvis->R_thigh
    DrawBoneLine(dl, p, 15, 16, col); // R_thigh->R_calf
    DrawBoneLine(dl, p, 16, 17, col); // R_calf->R_foot
    DrawBoneLine(dl, p, 17, 21, col); // R_foot->R_toe
}

// Render-thread copies of state (avoids race with worker thread)
static uintptr_t s_renderPawn = 0;
static uintptr_t s_renderFaction = 0;
static uintptr_t s_renderFactionObj = 0;
static uintptr_t s_renderFactionData = 0;

// ============================================================================
// ESP
// ============================================================================
static void DrawESP() {
    auto& cfg = g_config.Active();
    if (!cfg.esp) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float scrCx = (float)g_screenW * 0.5f;
    float scrCy = (float)g_screenH * 0.5f;

    static int s_espDiagCounter = 0;
    int dTotal = 0, dEnemy = 0, dTeamSkip = 0, dVehSkip = 0, dDistSkip = 0, dVisSkip = 0, dW2SFail = 0, dDrawn = 0;
    bool diagFrame = (s_espDiagCounter % 300 == 0);

    for (auto& p : g_players) {
        if (!p.isValid) continue;
        dTotal++;
        if (!p.isTeammate) dEnemy++;
        if (p.isDead && !cfg.espShowDead) continue;
        if (p.isTeammate && !cfg.espShowTeam) { dTeamSkip++; continue; }
        if (p.isDowned && !cfg.espShowDowned) continue;
        if (p.isInVehicle && p.isTeammate) { dVehSkip++; continue; }
        if (p.distance > cfg.espMaxDistance) { dDistSkip++; continue; }
        if (cfg.espVisCheck && !p.visible && !p.isTeammate) dVisSkip++;
        static const float colDead[4] = {0.45f, 0.45f, 0.45f, 0.55f};
        const float* boxCol  = p.isDead ? colDead : (p.isTeammate ? cfg.colTeamBox  : (p.visible ? cfg.colVisBox  : cfg.colHidBox));
        const float* skelCol = p.isDead ? colDead : (p.isTeammate ? cfg.colTeamSkel : (p.visible ? cfg.colVisSkel : cfg.colHidSkel));
        const float* nameCol = p.isDead ? colDead : (p.isTeammate ? cfg.colTeamName : (p.visible ? cfg.colVisName : cfg.colHidName));

        Vec2 headScr, feetScr;
        bool headOk = WorldToScreen(p.headPos, headScr);

        // Fallback: if headPos W2S fails, try raw position + standing height
        if (!headOk) {
            DVec3 standPos = p.position;
            standPos.z += 80.0;
            headOk = WorldToScreen(standPos, headScr);
        }

        DVec3 lFoot = p.bones[14], rFoot = p.bones[17];
        bool lValid = (lFoot.x != 0 || lFoot.y != 0 || lFoot.z != 0);
        bool rValid = (rFoot.x != 0 || rFoot.y != 0 || rFoot.z != 0);
        DVec3 feetWorld;
        if (lValid && rValid) feetWorld = (lFoot.z < rFoot.z) ? lFoot : rFoot;
        else if (lValid) feetWorld = lFoot;
        else if (rValid) feetWorld = rFoot;
        else feetWorld = {};
        bool feetOk = (lValid || rValid) && WorldToScreen(feetWorld, feetScr);

        // If still no head W2S, try position at feet level
        if (!headOk) {
            headOk = WorldToScreen(p.position, headScr);
        }

        // Log enemy data for diagnostics
        if (!p.isTeammate && diagFrame) {
            char enemyDbg[512];
            sprintf_s(enemyDbg, "ENEMY: name=%s dist=%.0f inVeh=%d headPos=(%.0f,%.0f,%.0f) pos=(%.0f,%.0f,%.0f) w2s=%d vis=%d hp=%.0f",
                p.name.c_str(), p.distance, p.isInVehicle ? 1 : 0,
                p.headPos.x, p.headPos.y, p.headPos.z,
                p.position.x, p.position.y, p.position.z,
                headOk ? 1 : 0, p.visible ? 1 : 0, p.health);
            WriteStartupLog("ESP", enemyDbg);
        }

        if (!headOk && cfg.espOffScreen && !p.isTeammate && !p.isDowned && !p.isDead) {
            dDrawn++;
            DVec3 delta = p.position - g_camera.location;
            float angle = atan2f((float)delta.y, (float)delta.x) - (float)(g_camera.rotation.yaw * 3.14159265 / 180.0);
            float arrowDist = (scrCx < scrCy ? scrCx : scrCy) - 30.f;
            float ax = scrCx + sinf(angle) * arrowDist;
            float ay = scrCy - cosf(angle) * arrowDist;
            float tipX = scrCx + sinf(angle) * (arrowDist + 10.f);
            float tipY = scrCy - cosf(angle) * (arrowDist + 10.f);
            ImU32 arrowCol = GetColor(boxCol);
            dl->AddTriangleFilled(
                ImVec2(tipX, tipY),
                ImVec2(ax + sinf(angle + 2.4f) * 8.f, ay - cosf(angle + 2.4f) * 8.f),
                ImVec2(ax + sinf(angle - 2.4f) * 8.f, ay - cosf(angle - 2.4f) * 8.f),
                arrowCol);
            char distTxt[16]; snprintf(distTxt, 16, "%.0fm", p.distance);
            ImVec2 dts = ImGui::CalcTextSize(distTxt);
            dl->AddText(ImVec2(ax - dts.x * 0.5f, ay - dts.y - 4.f), IM_COL32(255,255,255,200), distTxt);
            continue;
        }
        if (!headOk) { dW2SFail++; continue; }
        dDrawn++;

        float espAlpha = 1.0f;
        if (p.distance > 50.f) {
            float t = (p.distance - 50.f) / (400.f - 50.f);
            if (t > 1.f) t = 1.f;
            espAlpha = 1.0f - t * 0.7f;
        }
        auto ApplyAlpha = [espAlpha](ImU32 col) -> ImU32 {
            int a = (int)((col >> 24) * espAlpha);
            return (col & 0x00FFFFFF) | ((ImU32)a << 24);
        };

        float boxH = feetOk ? fabsf(feetScr.y - headScr.y) + 8.f : 80.f;
        float boxW = boxH * 0.45f;
        float topY = headScr.y - 3.f;
        float leftX = headScr.x - boxW * 0.5f;

        if (cfg.espGlow && !p.isDead) {
            int gr = (int)(cfg.colGlow[0]*255), gg = (int)(cfg.colGlow[1]*255), gb = (int)(cfg.colGlow[2]*255);
            const int glowBones[] = {0,1,2,3,4,5, 6,7,8, 9,10,11, 12,13,14, 15,16,17};
            const float glowRadius[] = {
                8,6,7,7,7,7,
                5,4,3,
                5,4,3,
                6,5,4,
                6,5,4
            };
            int validGlow = 0;
            for (int bi = 0; bi < 18; bi++) {
                int idx = glowBones[bi];
                if (idx < p.boneCount && (p.bones[idx].x != 0 || p.bones[idx].y != 0 || p.bones[idx].z != 0))
                    validGlow++;
            }
            if (validGlow >= 8) {
                for (int layer = cfg.espGlowIntensity; layer >= 1; layer--) {
                    float scale = 1.0f + (float)layer * 0.6f;
                    int alpha = (int)((12 + (cfg.espGlowIntensity - layer) * 5) * espAlpha);
                    for (int bi = 0; bi < 18; bi++) {
                        int idx = glowBones[bi];
                        if (idx >= p.boneCount) continue;
                        const DVec3& bone = p.bones[idx];
                        if (bone.x == 0 && bone.y == 0 && bone.z == 0) continue;
                        Vec2 bs;
                        if (!WorldToScreen(bone, bs)) continue;
                        float rad = glowRadius[bi] * scale * (boxH / 120.f);
                        if (rad < 2.f) rad = 2.f;
                        dl->AddCircleFilled(ImVec2(bs.x, bs.y), rad, IM_COL32(gr, gg, gb, alpha), 12);
                    }
                }
            } else {
                float cx2 = headScr.x;
                float segments[][3] = {
                    {0.05f, 0.08f, 1.0f},
                    {0.15f, 0.06f, 0.9f},
                    {0.25f, 0.10f, 0.9f},
                    {0.40f, 0.09f, 0.8f},
                    {0.55f, 0.08f, 0.8f},
                    {0.70f, 0.06f, 0.7f},
                    {0.90f, 0.05f, 0.6f},
                };
                for (int layer = cfg.espGlowIntensity; layer >= 1; layer--) {
                    float scale = 1.0f + (float)layer * 0.5f;
                    int baseAlpha = (int)((12 + (cfg.espGlowIntensity - layer) * 5) * espAlpha);
                    for (auto& seg : segments) {
                        float sy = topY + boxH * seg[0];
                        float rad = boxH * seg[1] * scale;
                        int alpha = (int)(baseAlpha * seg[2]);
                        dl->AddCircleFilled(ImVec2(cx2, sy), rad, IM_COL32(gr, gg, gb, alpha), 12);
                    }
                }
            }
        }

        ImU32 boxColor = ApplyAlpha(GetColor(boxCol));
        if (p.isDead) boxColor = ApplyAlpha(IM_COL32(120, 120, 120, 140));
        else if (p.isDowned) boxColor = ApplyAlpha(IM_COL32(180, 180, 40, 200));

        if (cfg.espBoxes) {
            if (cfg.espBoxStyle == 1) {
                float cLen = boxH * 0.2f;
                dl->AddLine(ImVec2(leftX, topY), ImVec2(leftX + cLen, topY), boxColor, 1.5f);
                dl->AddLine(ImVec2(leftX, topY), ImVec2(leftX, topY + cLen), boxColor, 1.5f);
                dl->AddLine(ImVec2(leftX + boxW, topY), ImVec2(leftX + boxW - cLen, topY), boxColor, 1.5f);
                dl->AddLine(ImVec2(leftX + boxW, topY), ImVec2(leftX + boxW, topY + cLen), boxColor, 1.5f);
                dl->AddLine(ImVec2(leftX, topY + boxH), ImVec2(leftX + cLen, topY + boxH), boxColor, 1.5f);
                dl->AddLine(ImVec2(leftX, topY + boxH), ImVec2(leftX, topY + boxH - cLen), boxColor, 1.5f);
                dl->AddLine(ImVec2(leftX + boxW, topY + boxH), ImVec2(leftX + boxW - cLen, topY + boxH), boxColor, 1.5f);
                dl->AddLine(ImVec2(leftX + boxW, topY + boxH), ImVec2(leftX + boxW, topY + boxH - cLen), boxColor, 1.5f);
            } else {
                dl->AddRect(ImVec2(leftX, topY), ImVec2(leftX + boxW, topY + boxH), boxColor, 0, 0, 1.5f);
                dl->AddRect(ImVec2(leftX-1, topY-1), ImVec2(leftX+boxW+1, topY+boxH+1), ApplyAlpha(IM_COL32(0,0,0,120)));
            }
        }
        if (cfg.espSkeleton && !p.isDowned && !p.isDead) DrawSkeleton(dl, p, ApplyAlpha(GetColor(skelCol)));
        if (cfg.espHealth && !p.isDead && p.healthValid) {
            float hpFrac = p.maxHealth > 0 ? p.health / p.maxHealth : 0.f;
            if (hpFrac > 1.f) hpFrac = 1.f;
            float barH = boxH * hpFrac;
            float barTop = topY + boxH - barH;
            int hpR, hpG, hpB;
            if (hpFrac > 0.5f) {
                float t = (hpFrac - 0.5f) * 2.f;
                hpR = (int)((1.f - t) * 222 + t * 64);
                hpG = (int)((1.f - t) * 170 + t * 237);
                hpB = (int)((1.f - t) * 40 + t * 81);
            } else {
                float t = hpFrac * 2.f;
                hpR = (int)((1.f - t) * 237 + t * 222);
                hpG = (int)((1.f - t) * 64 + t * 170);
                hpB = (int)((1.f - t) * 64 + t * 40);
            }
            ImU32 hpCol = ApplyAlpha(IM_COL32(hpR, hpG, hpB, 230));
            ImU32 hpDark = ApplyAlpha(IM_COL32(hpR*6/10, hpG*6/10, hpB*6/10, 230));
            dl->AddRectFilled(ImVec2(leftX-6, topY), ImVec2(leftX-1, topY+boxH), ApplyAlpha(IM_COL32(0,0,0,120)));
            dl->AddRectFilledMultiColor(ImVec2(leftX-6, barTop), ImVec2(leftX-1, topY+boxH), hpCol, hpCol, hpDark, hpDark);
            dl->AddRect(ImVec2(leftX-6, topY), ImVec2(leftX-1, topY+boxH), ApplyAlpha(IM_COL32(0,0,0,200)));
        }

        // Name + faction label
        float labelY = topY;
        if (cfg.espName && !p.name.empty()) {
            char nameLabel[160];
            char clanPrefix[32] = "";
            if (!p.clanTag.empty())
                snprintf(clanPrefix, sizeof(clanPrefix), "[%s] ", p.clanTag.c_str());
            if (p.isDead) {
                snprintf(nameLabel, sizeof(nameLabel), "%s%s [DEAD]", clanPrefix, p.name.c_str());
            }
            else if (p.isDowned) {
                if (p.giveUpTime > 0.f)
                    snprintf(nameLabel, sizeof(nameLabel), "%s%s [KNOCKED %.0fs]", clanPrefix, p.name.c_str(), p.giveUpTime);
                else
                    snprintf(nameLabel, sizeof(nameLabel), "%s%s [KNOCKED]", clanPrefix, p.name.c_str());
            }
            else if (p.isInvincible)
                snprintf(nameLabel, sizeof(nameLabel), "%s%s [INV]", clanPrefix, p.name.c_str());
            else if (p.isAdmin)
                snprintf(nameLabel, sizeof(nameLabel), "%s%s [ADMIN]", clanPrefix, p.name.c_str());
            else if (p.isDeveloper)
                snprintf(nameLabel, sizeof(nameLabel), "%s%s [DEV]", clanPrefix, p.name.c_str());
            else if (p.isBot)
                snprintf(nameLabel, sizeof(nameLabel), "%s%s [BOT]", clanPrefix, p.name.c_str());
            else if (p.isADS)
                snprintf(nameLabel, sizeof(nameLabel), "%s%s [ADS]", clanPrefix, p.name.c_str());
            else
                snprintf(nameLabel, sizeof(nameLabel), "%s%s", clanPrefix, p.name.c_str());
            ImVec2 ts = ImGui::CalcTextSize(nameLabel);
            ImU32 nameLabelCol;
            if (p.isDead) nameLabelCol = ApplyAlpha(IM_COL32(120,120,120,180));
            else if (p.isDowned) nameLabelCol = ApplyAlpha(IM_COL32(180,180,40,230));
            else if (p.isAdmin || p.isDeveloper) nameLabelCol = ApplyAlpha(IM_COL32(255,50,50,255));
            else nameLabelCol = ApplyAlpha(GetColor(nameCol));
            float namePad = 4.f;
            float nameBoxW = ts.x + namePad * 2.f;
            float nameBoxH = ts.y + namePad * 2.f;
            float nameBoxX = headScr.x - nameBoxW * 0.5f;
            float nameBoxY = topY - nameBoxH - 3.f;
            labelY = nameBoxY;
            dl->AddRectFilled(ImVec2(nameBoxX, nameBoxY), ImVec2(nameBoxX + nameBoxW, nameBoxY + nameBoxH), ApplyAlpha(IM_COL32(0,0,0,80)), 2.f);
            dl->AddRect(ImVec2(nameBoxX, nameBoxY), ImVec2(nameBoxX + nameBoxW, nameBoxY + nameBoxH), ApplyAlpha(IM_COL32(123,66,245,100)), 2.f, 0, 0.85f);
            dl->AddText(ImVec2(nameBoxX + namePad, nameBoxY + namePad), nameLabelCol, nameLabel);
        }
        if (cfg.espFaction && !p.factionName.empty()) {
            ImU32 fCol = ApplyAlpha(GetFactionColor(p.factionName));
            ImVec2 fts = ImGui::CalcTextSize(p.factionName.c_str());
            float fx = headScr.x - fts.x * 0.5f;
            labelY -= fts.y + 1.f;
            dl->AddText(ImVec2(fx+1, labelY+1), ApplyAlpha(IM_COL32(0,0,0,160)), p.factionName.c_str());
            dl->AddText(ImVec2(fx, labelY), fCol, p.factionName.c_str());
        }

        float bottomLabelY = topY + boxH + 2.f;
        if (cfg.espDistance) {
            char dist[16]; snprintf(dist, 16, "%.0fm", p.distance);
            ImVec2 ts = ImGui::CalcTextSize(dist);
            float dx = headScr.x - ts.x * 0.5f;
            dl->AddText(ImVec2(dx+1, bottomLabelY+1), ApplyAlpha(IM_COL32(0,0,0,180)), dist);
            dl->AddText(ImVec2(dx, bottomLabelY), ApplyAlpha(IM_COL32(220,220,220,230)), dist);
            bottomLabelY += ts.y + 1.f;
        }
        if (cfg.espWeapon && !p.weaponName.empty()) {
            ImVec2 wts = ImGui::CalcTextSize(p.weaponName.c_str());
            float wx = headScr.x - wts.x * 0.5f;
            dl->AddText(ImVec2(wx+1, bottomLabelY+1), ApplyAlpha(IM_COL32(0,0,0,160)), p.weaponName.c_str());
            dl->AddText(ImVec2(wx, bottomLabelY), ApplyAlpha(IM_COL32(200,180,255,220)), p.weaponName.c_str());
        }
        if (!p.isDead && !p.isDowned) {
            const char* stanceStr = nullptr;
            ImU32 stanceCol = ApplyAlpha(IM_COL32(180,180,180,180));
            if (p.isSprinting) { stanceStr = "SPRINT"; stanceCol = ApplyAlpha(IM_COL32(255,180,50,220)); }
            else if (p.isADS) { stanceStr = "ADS"; stanceCol = ApplyAlpha(IM_COL32(255,80,80,220)); }
            else if (p.stance == 0) { stanceStr = "PRONE"; stanceCol = ApplyAlpha(IM_COL32(100,200,255,220)); }
            else if (p.stance == 1) { stanceStr = "CROUCH"; stanceCol = ApplyAlpha(IM_COL32(100,255,180,200)); }
            if (stanceStr) {
                ImVec2 sts = ImGui::CalcTextSize(stanceStr);
                float sx = headScr.x - sts.x * 0.5f;
                dl->AddText(ImVec2(sx+1, bottomLabelY+1), ApplyAlpha(IM_COL32(0,0,0,160)), stanceStr);
                dl->AddText(ImVec2(sx, bottomLabelY), stanceCol, stanceStr);
                bottomLabelY += sts.y + 1.f;
            }
        }
        if (cfg.espHeadDot && !p.isDowned && !p.isDead)
            dl->AddCircleFilled(ImVec2(headScr.x, headScr.y), 3.f, ApplyAlpha(IM_COL32(255,50,50,230)));
        if (cfg.espSnaplines && !p.isDowned && !p.isDead) {
            ImU32 snapCol = ApplyAlpha(GetColor(p.isTeammate ? cfg.colTeamBox : (p.visible ? cfg.colVisSnap : cfg.colHidSnap)));
            float originY;
            if (cfg.espSnapOrigin == 2) originY = 0.f;
            else if (cfg.espSnapOrigin == 1) originY = scrCy;
            else originY = (float)g_screenH;
            float endX = feetOk ? feetScr.x : headScr.x;
            float endY = feetOk ? feetScr.y : topY + boxH;
            dl->AddLine(ImVec2(scrCx, originY), ImVec2(endX, endY), snapCol);
        }
        // View direction indicator
        if (cfg.espViewDir && p.hasViewDir && headOk && !p.isDowned && !p.isDead) {
            float yawRad = (float)(p.viewRotation.yaw * 3.14159265 / 180.0);
            float pitchRad = (float)(p.viewRotation.pitch * 3.14159265 / 180.0);
            DVec3 fwd = { cos(pitchRad) * cos(yawRad), cos(pitchRad) * sin(yawRad), sin(pitchRad) };
            DVec3 viewTarget = p.headPos + fwd * 80.0;
            Vec2 viewScr;
            if (WorldToScreen(viewTarget, viewScr)) {
                dl->AddLine(ImVec2(headScr.x, headScr.y), ImVec2(viewScr.x, viewScr.y), ApplyAlpha(IM_COL32(0,255,255,180)), 1.5f);
                dl->AddCircleFilled(ImVec2(viewScr.x, viewScr.y), 2.5f, ApplyAlpha(IM_COL32(0,255,255,220)));
            }
        }
    }

    if (++s_espDiagCounter % 300 == 1) {
        char diag[384];
        sprintf_s(diag, "ESP DIAG: total=%d enemy=%d teamSkip=%d vehSkip=%d distSkip=%d visSkip=%d w2sFail=%d drawn=%d maxDist=%.0f visCheck=%d",
            dTotal, dEnemy, dTeamSkip, dVehSkip, dDistSkip, dVisSkip, dW2SFail, dDrawn, cfg.espMaxDistance, cfg.espVisCheck ? 1 : 0);
        WriteStartupLog("ESP", diag);
    }

    // On-screen ESP debug (bottom-left, multi-line diagnostic)
    {
        float y = (float)g_screenH - 50.f;
        ImU32 dbgCol = IM_COL32(200,200,200,160);
        char dbg[256];

        sprintf_s(dbg, "ESP: %d total %d enemy %d drawn %d w2s %d dist | fTag=0x%llX fObj=0x%llX fData=0x%llX",
            dTotal, dEnemy, dDrawn, dW2SFail, dDistSkip,
            (unsigned long long)s_renderFaction, (unsigned long long)s_renderFactionObj,
            (unsigned long long)s_renderFactionData);
        dl->AddText(ImVec2(10, y), dbgCol, dbg);
        y += 14.f;

        int paCnt = 0;
        if (g_gameState && g_playerArrayOffset)
            paCnt = Read<int32_t>(g_gameState + g_playerArrayOffset + 8);
        int visCount = 0;
        for (auto& p : g_players) { if (p.isValid && p.visible) visCount++; }
        float serverFPS = 0.f;
        bool cheatsOn = false, adminOn = false;
        if (g_gameState) {
            serverFPS = Read<float>(g_gameState + O::GS_CachedServerFPS);
            cheatsOn = Read<uint8_t>(g_gameState + O::GS_bServerCheatsEnabled) != 0;
            adminOn = Read<uint8_t>(g_gameState + O::GS_bAdminModeEnabled) != 0;
        }
        sprintf_s(dbg, "PA:%d vehs:%zu vis:%d cam:%s time:%.1f svFPS:%.0f bSpd:%.0f%s%s",
            paCnt, g_vehicles.size(), visCount,
            g_camera.valid ? "OK" : "NO", g_camera.timeSeconds, serverFPS, g_localBulletSpeed,
            cheatsOn ? " CHEATS" : "", adminOn ? " ADMIN" : "");
        dl->AddText(ImVec2(10, y), dbgCol, dbg);
    }
}

// ============================================================================
// Vehicle ESP
// ============================================================================
static void DrawVehicleESP() {
    auto& cfg = g_config.Active();
    if (!cfg.esp || !cfg.espVehicles || g_vehicles.empty()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImU32 vehCol = GetColor(cfg.colVehicle);

    ImU32 teamVehCol = GetColor(cfg.colVehicleTeam);

    for (auto& v : g_vehicles) {
        if (!v.isValid) continue;
        if (v.isDestroyed && !cfg.vehicleShowDead) continue;
        if (v.isAir && !cfg.vehicleFilterAir) continue;
        if (!v.isAir && !cfg.vehicleFilterLand) continue;
        if (v.isTeamVehicle && cfg.vehicleEnemyOnly) continue;
        if (v.occupantCount == 0 && !cfg.vehicleShowEmpty) continue;

        Vec2 scrPos;
        if (!WorldToScreen(v.position, scrPos)) continue;
        v.screenPos = scrPos;

        char label[160];
        char statusTag[48] = "";
        ImU32 labelCol = v.isNeutral ? IM_COL32(50, 255, 50, 230) :
                         (v.isTeamVehicle ? teamVehCol : vehCol);

        if (v.isDestroyed) {
            strcpy_s(statusTag, " DEAD");
            labelCol = IM_COL32(120, 120, 120, 180);
        } else if (v.speed > 2.f) {
            sprintf_s(statusTag, " MOVING %.0f", v.speed);
            if (v.isNeutral) labelCol = IM_COL32(50, 255, 50, 230);
        } else if (v.engineRunning) {
            strcpy_s(statusTag, " ENGINE ON");
            if (v.isNeutral) labelCol = IM_COL32(50, 255, 50, 230);
        } else if (v.occupantCount == 0) {
            strcpy_s(statusTag, " FREE");
            if (v.isNeutral) labelCol = IM_COL32(50, 255, 50, 230);
        }

        if (cfg.espVehicleOccupants && v.occupantCount > 0)
            snprintf(label, sizeof(label), "[%s%s] %d occ | %.0fm", v.typeName.c_str(), statusTag, v.occupantCount, v.distance);
        else
            snprintf(label, sizeof(label), "[%s%s] %.0fm", v.typeName.c_str(), statusTag, v.distance);

        ImVec2 ts = ImGui::CalcTextSize(label);
        float x = v.screenPos.x - ts.x * 0.5f;
        float y = v.screenPos.y - ts.y * 0.5f;

        dl->AddRectFilled(ImVec2(x - 4, y - 2), ImVec2(x + ts.x + 4, y + ts.y + 2),
            IM_COL32(0, 0, 0, 90), 3.f);
        dl->AddText(ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 100), label);
        dl->AddText(ImVec2(x, y), labelCol, label);

        // Occupant names below label
        if (cfg.espVehicleOccupants && !v.occupantNames.empty()) {
            float oy = y + ts.y + 3.f;
            for (auto& oName : v.occupantNames) {
                ImVec2 ots = ImGui::CalcTextSize(oName.c_str());
                float ox = v.screenPos.x - ots.x * 0.5f;
                dl->AddText(ImVec2(ox + 1, oy + 1), IM_COL32(0, 0, 0, 100), oName.c_str());
                dl->AddText(ImVec2(ox, oy), IM_COL32(255, 220, 150, 220), oName.c_str());
                oy += ots.y + 1.f;
            }
        }

        // Small dot marker above label
        float cx = v.screenPos.x, cy = y - 6.f;
        dl->AddCircleFilled(ImVec2(cx, cy), 3.f, labelCol);
    }
}

// ============================================================================
// World Item ESP (dropped items, mines, explosives)
// ============================================================================
static void DrawWorldItemESP() {
    auto& cfg = g_config.Active();
    if (!cfg.esp || g_worldItems.empty()) return;
    if (!cfg.espWorldItems && !cfg.espMines && !cfg.espEmplacements && !cfg.espFOBs && !cfg.espBodybags) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (auto& w : g_worldItems) {
        if (!w.isValid) continue;
        if (w.type == 0 && !cfg.espWorldItems) continue;
        if (w.type == 1 && !cfg.espMines) continue;
        if (w.type == 2 && !cfg.espEmplacements) continue;
        if (w.type == 3 && !cfg.espFOBs) continue;
        if (w.type == 4 && !cfg.espBodybags) continue;
        if (w.type == 0) {
            if (w.subType == 0 && !cfg.espItemWeapons) continue;
            if (w.subType == 1 && !cfg.espItemAmmo) continue;
            if (w.subType == 2 && !cfg.espItemAttachments) continue;
            if (w.subType == 3 && !cfg.espItemMedical) continue;
            if (w.subType == 4 && !cfg.espItemGrenades) continue;
            if (w.subType == 5 && !cfg.espItemOther) continue;
        }

        Vec2 scrPos;
        if (!WorldToScreen(w.position, scrPos)) continue;
        w.screenPos = scrPos;
        w.distance = (float)((w.position - g_camera.location).length() / 100.0);

        ImU32 col;
        const char* prefix;
        if (w.type == 0) {
            switch (w.subType) {
                case 0: col = IM_COL32(255, 180, 50, 240); prefix = "[W] "; break;
                case 1: col = IM_COL32(180, 220, 255, 200); prefix = "[A] "; break;
                case 2: col = IM_COL32(100, 255, 180, 220); prefix = "[+] "; break;
                case 3: col = IM_COL32(100, 255, 100, 230); prefix = "[M] "; break;
                case 4: col = IM_COL32(255, 120, 120, 230); prefix = "[G] "; break;
                default: col = IM_COL32(160, 160, 180, 180); prefix = ""; break;
            }
        } else if (w.type == 1) {
            col = IM_COL32(255, 80, 80, 240);
            prefix = "! ";
        } else if (w.type == 2) {
            col = IM_COL32(255, 160, 40, 240);
            prefix = ">> ";
        } else if (w.type == 4) {
            col = IM_COL32(200, 160, 120, 230);
            prefix = "";
        } else {
            col = IM_COL32(255, 50, 200, 240);
            prefix = "FOB ";
        }

        // Clean up item name for display
        std::string displayName = w.name;
        size_t bp = displayName.find("_BP");
        if (bp != std::string::npos) displayName = displayName.substr(0, bp);
        size_t ct = displayName.find("_C");
        if (ct != std::string::npos && ct > 3) displayName = displayName.substr(0, ct);

        char label[128];
        snprintf(label, sizeof(label), "%s%s [%.0fm]", prefix, displayName.c_str(), w.distance);
        ImVec2 ts = ImGui::CalcTextSize(label);
        float x = w.screenPos.x - ts.x * 0.5f;
        float y = w.screenPos.y;

        dl->AddText(ImVec2(x+1, y+1), IM_COL32(0,0,0,160), label);
        dl->AddText(ImVec2(x, y), col, label);

        if (w.type == 1) {
            dl->AddCircle(ImVec2(w.screenPos.x, w.screenPos.y - 6.f), 4.f, col, 0, 1.5f);
        } else if (w.type == 2) {
            dl->AddTriangle(ImVec2(w.screenPos.x, w.screenPos.y - 10.f),
                ImVec2(w.screenPos.x - 6.f, w.screenPos.y - 2.f),
                ImVec2(w.screenPos.x + 6.f, w.screenPos.y - 2.f), col, 1.5f);
        } else if (w.type == 3) {
            dl->AddRect(ImVec2(w.screenPos.x - 5.f, w.screenPos.y - 8.f),
                ImVec2(w.screenPos.x + 5.f, w.screenPos.y - 2.f), col, 0, 0, 2.f);
        } else if (w.type == 4) {
            dl->AddCircleFilled(ImVec2(w.screenPos.x, w.screenPos.y - 6.f), 3.5f, col);
        }
    }
}

// ============================================================================
// Admin Warning — persistent overlay when admins/devs are in-game
// ============================================================================
static void DrawAdminWarning() {
    if (!g_config.Active().esp) return;
    bool adminPresent = false;
    for (auto& p : g_players) {
        if (p.isValid && (p.isAdmin || p.isDeveloper)) {
            adminPresent = true;
            break;
        }
    }
    if (!adminPresent) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const char* warn = "ADMIN IN GAME — USE AIM AT OWN RISK";
    ImVec2 ts = ImGui::CalcTextSize(warn);
    float x = (float)g_screenW * 0.5f - ts.x * 0.5f;
    float y = 6.f;

    dl->AddRectFilled(ImVec2(x - 8, y - 4), ImVec2(x + ts.x + 8, y + ts.y + 4),
        IM_COL32(180, 0, 0, 160), 4.f);
    dl->AddRect(ImVec2(x - 8, y - 4), ImVec2(x + ts.x + 8, y + ts.y + 4),
        IM_COL32(255, 50, 50, 220), 4.f, 0, 1.5f);
    dl->AddText(ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 200), warn);
    dl->AddText(ImVec2(x, y), IM_COL32(255, 255, 255, 255), warn);
}

// ============================================================================
// Aimbot
// ============================================================================
static const int AIM_KEYS[] = { VK_RBUTTON, VK_LBUTTON, VK_XBUTTON1, VK_XBUTTON2, VK_SHIFT, VK_MBUTTON, VK_MENU, VK_CAPITAL };
static const char* AIM_KEY_NAMES[] = { "RMB", "LMB", "Mouse4", "Mouse5", "Shift", "MMB", "Alt", "CapsLk" };
static bool g_aimToggled = false;
static bool g_aimKeyWasDown = false;
static uintptr_t g_lockedTarget = 0;

static void RunAimbot() {
    auto& cfg = g_config.Active();
    if (!cfg.aimbot || g_players.empty()) return;
    if (!DynAPI::pAsyncKeyState) return;
    int aimKey = AIM_KEYS[cfg.aimKeyIdx % 8];
    bool keyDown = (DynAPI::pAsyncKeyState(aimKey) & 0x8000) != 0;

    if (cfg.aimHoldMode) {
        if (!keyDown) { g_lockedTarget = 0; return; }
    } else {
        if (keyDown && !g_aimKeyWasDown) g_aimToggled = !g_aimToggled;
        g_aimKeyWasDown = keyDown;
        if (!g_aimToggled) { g_lockedTarget = 0; return; }
    }

    float cx = (float)g_screenW * 0.5f, cy = (float)g_screenH * 0.5f;
    float fovPx = cfg.fov * ((float)g_screenW / 90.f);
    if (fovPx < 10.f) fovPx = 10.f;
    float bestDist = fovPx;
    Vec2 bestTarget{cx, cy};
    bool found = false;
    uintptr_t bestPawn = 0;

    int aimSlot = 0;
    switch (cfg.aimBone) {
        case 0: aimSlot = 0; break;
        case 1: aimSlot = 1; break;
        case 2: aimSlot = 2; break;
        case 3: aimSlot = 5; break;
    }

    auto AdjustAimBone = [aimSlot](const PlayerData& p) -> DVec3 {
        DVec3 b = p.bones[aimSlot];
        bool usingHead = (aimSlot == 0);
        if (b.x == 0.0 && b.y == 0.0 && b.z == 0.0) {
            b = p.bones[0];
            usingHead = true;
        }
        if (usingHead) {
            DVec3 neck = p.bones[1];
            if (neck.x != 0.0 || neck.y != 0.0 || neck.z != 0.0) {
                b.x = b.x * 0.45 + neck.x * 0.55;
                b.y = b.y * 0.45 + neck.y * 0.55;
                b.z = b.z * 0.45 + neck.z * 0.55;
            }
        }
        return b;
    };

    auto PredictAimPoint = [&cfg](DVec3 b, const PlayerData& p) -> DVec3 {
        if (!cfg.aimPrediction || g_localBulletSpeed <= 0.f) return b;
        double distCm = (double)p.distance * 100.0;
        double tof = distCm / (double)g_localBulletSpeed;
        if (tof > 2.0) tof = 2.0;
        b.z += 0.5 * 980.0 * tof * tof;
        b.x += p.velocity.x * tof;
        b.y += p.velocity.y * tof;
        b.z += p.velocity.z * tof;
        return b;
    };

    // If aimLock is on and we have a locked target, try to keep it
    if (cfg.aimLock && g_lockedTarget) {
        for (auto& p : g_players) {
            if (!p.isValid || p.pawn != g_lockedTarget) continue;
            if (p.isTeammate && cfg.teamCheck) break;
            if (p.isDead || p.isDowned || p.isInvincible) break;
            if (p.distance > cfg.aimMaxDistance) break;
            if (cfg.aimVisCheck && !p.aimVisible) break;
            DVec3 rawBone = AdjustAimBone(p);
            if (rawBone.x == 0.0 && rawBone.y == 0.0 && rawBone.z == 0.0) break;
            DVec3 boneWorld = PredictAimPoint(rawBone, p);
            Vec2 boneScr;
            if (!WorldToScreen(boneWorld, boneScr)) break;
            bestTarget = boneScr;
            found = true;
            bestPawn = p.pawn;
            break;
        }
        if (!found) g_lockedTarget = 0;
    }

    if (!found) {
        for (auto& p : g_players) {
            if (!p.isValid || (p.isTeammate && cfg.teamCheck)) continue;
            if (p.isDead || p.isDowned || p.isInvincible) continue;
            if (p.distance > cfg.aimMaxDistance) continue;
            if (cfg.aimVisCheck && !p.aimVisible) continue;
            DVec3 rawBone = AdjustAimBone(p);
            if (rawBone.x == 0.0 && rawBone.y == 0.0 && rawBone.z == 0.0) continue;
            DVec3 boneWorld = PredictAimPoint(rawBone, p);
            Vec2 boneScr;
            if (!WorldToScreen(boneWorld, boneScr)) continue;
            float dx = boneScr.x - cx, dy = boneScr.y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < bestDist) { bestDist = dist; bestTarget = boneScr; found = true; bestPawn = p.pawn; }
        }
    }
    if (!found) return;

    if (cfg.aimLock) g_lockedTarget = bestPawn;

    static float accumX = 0.f, accumY = 0.f;
    static uintptr_t lastAimPawn = 0;
    if (bestPawn != lastAimPawn) { accumX = 0.f; accumY = 0.f; lastAimPawn = bestPawn; }

    float dx = bestTarget.x - cx;
    float dy = bestTarget.y - cy;
    float dt = ImMax(g_deltaTime, 0.001f);
    float smoothFactor = 1.f - expf(-dt * 60.f / ImMax(cfg.smooth, 1.f));
    accumX += dx * smoothFactor;
    accumY += dy * smoothFactor;
    LONG mx = (LONG)accumX;
    LONG my = (LONG)accumY;
    if (mx != 0 || my != 0) {
        accumX -= (float)mx;
        accumY -= (float)my;
        INPUT input{}; input.type = INPUT_MOUSE;
        input.mi.dx = mx; input.mi.dy = my;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }
}

// ============================================================================
// Triggerbot
// ============================================================================
static bool s_triggerFiring = false;
static bool s_triggerHasTarget = false;
static std::chrono::steady_clock::time_point s_triggerAcquireTime;
static std::chrono::steady_clock::time_point s_triggerFireStart;
static std::chrono::steady_clock::time_point s_triggerLastOnTarget;

static void RunTriggerbot() {
    auto& cfg = g_config.Active();

    auto releaseTrigger = [&]() {
        if (s_triggerFiring) {
            INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &in, sizeof(INPUT));
            s_triggerFiring = false;
        }
        s_triggerHasTarget = false;
    };

    if (!cfg.triggerbot || !DynAPI::pAsyncKeyState) { releaseTrigger(); return; }
    int trigKey = AIM_KEYS[cfg.triggerKeyIdx % 8];
    if (!(DynAPI::pAsyncKeyState(trigKey) & 0x8000)) { releaseTrigger(); return; }

    float cx = (float)g_screenW * 0.5f, cy = (float)g_screenH * 0.5f;
    bool shouldFire = false;
    bool shouldFireLoose = false;
    float threshMul = s_triggerFiring ? 3.0f : 1.0f;

    for (auto& p : g_players) {
        if (!p.isValid || (p.isTeammate && cfg.teamCheck)) continue;
        if (p.isDead || p.isDowned || p.isInvincible) continue;
        if (cfg.aimVisCheck && !p.aimVisible) continue;

        float bestDist = 999999.f;
        bool hit = false;
        for (int bi : {0, 1, 2, 5}) {
            DVec3 bone = p.bones[bi];
            if (bone.x == 0.0 && bone.y == 0.0 && bone.z == 0.0) continue;
            Vec2 scr;
            if (!WorldToScreen(bone, scr)) continue;
            float dx = scr.x - cx, dy = scr.y - cy;
            float d = sqrtf(dx*dx + dy*dy);
            if (d < bestDist) { bestDist = d; hit = true; }
        }
        if (!hit) {
            Vec2 headScr;
            if (!WorldToScreen(p.headPos, headScr)) continue;
            float dx = headScr.x - cx, dy = headScr.y - cy;
            bestDist = sqrtf(dx*dx + dy*dy);
            hit = true;
        }
        if (!hit) continue;

        float threshold = 800.f / (p.distance + 1.f);
        if (threshold < 8.f) threshold = 8.f;
        if (threshold > 60.f) threshold = 60.f;
        if (bestDist < threshold) { shouldFire = true; shouldFireLoose = true; break; }
        if (bestDist < threshold * threshMul) { shouldFireLoose = true; break; }
    }

    auto now = std::chrono::steady_clock::now();

    if (shouldFire || (s_triggerFiring && shouldFireLoose)) {
        s_triggerLastOnTarget = now;
        if (!s_triggerHasTarget) {
            s_triggerAcquireTime = now;
            s_triggerHasTarget = true;
        }
        if (!s_triggerFiring) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - s_triggerAcquireTime).count() >= cfg.triggerDelay) {
                INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &in, sizeof(INPUT));
                s_triggerFiring = true;
                s_triggerFireStart = now;
                s_triggerLastOnTarget = now;
            }
        }
    } else {
        if (s_triggerFiring) {
            auto sinceFire = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_triggerFireStart).count();
            auto sinceSeen = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_triggerLastOnTarget).count();
            if (sinceFire >= 100 && sinceSeen >= 200)
                releaseTrigger();
        } else {
            s_triggerHasTarget = false;
        }
    }
}

// ============================================================================
// Radar
// ============================================================================
static bool s_radarDragging = false;
static bool s_radarResizing = false;

static void DrawRadar() {
    auto& cfg = g_config.Active();
    if (!cfg.radar) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float sz = cfg.radarSize;

    float rx, ry;
    if (cfg.radarPosX >= 0.f && cfg.radarPosY >= 0.f) {
        rx = cfg.radarPosX;
        ry = cfg.radarPosY;
    } else {
        rx = (float)g_screenW - sz - 16.f;
        ry = 16.f;
    }

    if (!cfg.radarDraggable || !g_menuOpen) {
        s_radarDragging = false;
        s_radarResizing = false;
    }

    if (cfg.radarDraggable && g_menuOpen) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        bool mouseDown = ImGui::GetIO().MouseDown[0];
        bool clicked = ImGui::GetIO().MouseClicked[0];
        bool inRadar = mouse.x >= rx && mouse.x <= rx + sz && mouse.y >= ry && mouse.y <= ry + sz;
        bool inCorner = mouse.x >= rx + sz - 14.f && mouse.y >= ry + sz - 14.f && inRadar;

        if (!mouseDown) {
            s_radarDragging = false;
            s_radarResizing = false;
        }

        if (inCorner && clicked && !s_radarDragging) {
            s_radarResizing = true;
        } else if (inRadar && clicked && !s_radarResizing) {
            s_radarDragging = true;
        }

        if (s_radarResizing) {
            cfg.radarSize += delta.x;
            if (cfg.radarSize < 80.f) cfg.radarSize = 80.f;
            if (cfg.radarSize > 300.f) cfg.radarSize = 300.f;
            sz = cfg.radarSize;
            if (rx + sz > (float)g_screenW) rx = (float)g_screenW - sz;
            if (ry + sz > (float)g_screenH) ry = (float)g_screenH - sz;
            if (rx < 0.f) rx = 0.f;
            if (ry < 0.f) ry = 0.f;
            cfg.radarPosX = rx;
            cfg.radarPosY = ry;
        } else if (s_radarDragging) {
            rx += delta.x;
            ry += delta.y;
            if (rx < 0.f) rx = 0.f;
            if (ry < 0.f) ry = 0.f;
            if (rx + sz > (float)g_screenW) rx = (float)g_screenW - sz;
            if (ry + sz > (float)g_screenH) ry = (float)g_screenH - sz;
            cfg.radarPosX = rx;
            cfg.radarPosY = ry;
        }

        if (inCorner || s_radarResizing) {
            dl->AddTriangleFilled(
                ImVec2(rx + sz, ry + sz),
                ImVec2(rx + sz - 12.f, ry + sz),
                ImVec2(rx + sz, ry + sz - 12.f),
                IM_COL32(255, 255, 255, 80));
        }
    }

    dl->AddRectFilled(ImVec2(rx, ry), ImVec2(rx+sz, ry+sz), IM_COL32(15,15,20,200), 4.f);
    dl->AddRect(ImVec2(rx, ry), ImVec2(rx+sz, ry+sz), ACCENT_COL(150), 4.f);
    float mid = sz * 0.5f;
    dl->AddLine(ImVec2(rx+mid, ry), ImVec2(rx+mid, ry+sz), IM_COL32(255,255,255,30));
    dl->AddLine(ImVec2(rx, ry+mid), ImVec2(rx+sz, ry+mid), IM_COL32(255,255,255,30));
    dl->AddCircleFilled(ImVec2(rx+mid, ry+mid), 3.f, ACCENT_COL(255));

    double yaw = g_camera.rotation.yaw * DEG2RAD;
    double cosY = cos(-yaw), sinY = sin(-yaw);
    for (auto& p : g_players) {
        if (!p.isValid) continue;
        double dx = (p.position.x - g_camera.location.x) / 100.0 / cfg.radarZoom;
        double dy = (p.position.y - g_camera.location.y) / 100.0 / cfg.radarZoom;
        float px = (float)(rx + mid + dx * sinY + dy * cosY);
        float py = (float)(ry + mid - dx * cosY + dy * sinY);
        if (px < rx || px > rx+sz || py < ry || py > ry+sz) continue;
        if (p.isDead) continue;
        if (p.isDowned && !cfg.espShowDowned) continue;
        ImU32 col;
        if (p.isDowned)        col = IM_COL32(180,180,40,200);
        else if (p.isTeammate) col = IM_COL32(80,150,255,255);
        else                   col = IM_COL32(255,60,60,255);
        float dotR = p.isDowned ? 2.f : 3.f;
        dl->AddCircleFilled(ImVec2(px, py), dotR, col);
    }
    if (cfg.espVehicles) {
        for (auto& v : g_vehicles) {
            if (!v.isValid || v.isDestroyed) continue;
            if (v.isTeamVehicle && cfg.vehicleEnemyOnly) continue;
            if (v.occupantCount == 0 && !cfg.vehicleShowEmpty) continue;
            double dx = (v.position.x - g_camera.location.x) / 100.0 / cfg.radarZoom;
            double dy = (v.position.y - g_camera.location.y) / 100.0 / cfg.radarZoom;
            float px = (float)(rx + mid + dx * sinY + dy * cosY);
            float py = (float)(ry + mid - dx * cosY + dy * sinY);
            if (px < rx || px > rx+sz || py < ry || py > ry+sz) continue;
            ImU32 col;
            if (v.isTeamVehicle) col = IM_COL32(80,150,255,200);
            else if (v.occupantCount > 0) col = IM_COL32(255,160,40,255);
            else col = IM_COL32(140,140,140,200);
            dl->AddRectFilled(ImVec2(px-3, py-2), ImVec2(px+3, py+2), col);
        }
    }
}

// ============================================================================
// Crosshair + FPS counter
// ============================================================================
static void DrawCrosshair() {
    if (!g_config.Active().crosshair) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float cx = (float)g_screenW * 0.5f;
    float cy = (float)g_screenH * 0.5f;
    float sz = 6.f;
    float gap = 3.f;
    float thick = 1.5f;
    ImU32 col = IM_COL32(100, 220, 100, 255);
    ImU32 shadow = IM_COL32(0, 0, 0, 150);
    // Horizontal lines
    dl->AddLine({cx - gap - sz, cy}, {cx - gap, cy}, shadow, thick + 1.f);
    dl->AddLine({cx + gap, cy}, {cx + gap + sz, cy}, shadow, thick + 1.f);
    dl->AddLine({cx - gap - sz, cy}, {cx - gap, cy}, col, thick);
    dl->AddLine({cx + gap, cy}, {cx + gap + sz, cy}, col, thick);
    // Vertical lines
    dl->AddLine({cx, cy - gap - sz}, {cx, cy - gap}, shadow, thick + 1.f);
    dl->AddLine({cx, cy + gap}, {cx, cy + gap + sz}, shadow, thick + 1.f);
    dl->AddLine({cx, cy - gap - sz}, {cx, cy - gap}, col, thick);
    dl->AddLine({cx, cy + gap}, {cx, cy + gap + sz}, col, thick);
    // Center dot
    dl->AddCircleFilled({cx, cy}, 1.5f, col);
}

static void DrawFPSCounter() {
    if (!g_config.Active().fpsCounter) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto& fpsCfg = g_config.Active();
    float fpsX, fpsY;
    if (fpsCfg.radar) {
        float radarRx = fpsCfg.radarPosX >= 0.f ? fpsCfg.radarPosX : (float)g_screenW - fpsCfg.radarSize - 16.f;
        float radarRy = fpsCfg.radarPosY >= 0.f ? fpsCfg.radarPosY : 16.f;
        fpsX = radarRx + fpsCfg.radarSize - 64.f;
        fpsY = radarRy + fpsCfg.radarSize + 6.f;
    } else {
        fpsX = (float)g_screenW - 80.f;
        fpsY = 4.f;
    }
    char fpsBuf[32];
    sprintf_s(fpsBuf, "%.0f FPS", ImGui::GetIO().Framerate);
    dl->AddText({fpsX, fpsY}, IM_COL32(180, 220, 180, 200), fpsBuf);

    int enemies = 0, team = 0;
    for (auto& p : g_players) { if (p.isValid) { if (p.isTeammate) team++; else enemies++; } }
    char countBuf[48];
    sprintf_s(countBuf, "E:%d T:%d V:%d", enemies, team, (int)g_vehicles.size());
    dl->AddText({fpsX - 40.f, fpsY + 14.f}, IM_COL32(160, 160, 170, 180), countBuf);
}

// ============================================================================
// Watermark + FOV circle
// ============================================================================
static void DrawWatermark() {
    if (!g_config.Active().watermark) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(ImVec2(10, 10), ImVec2(230, 38), IM_COL32(18,18,24,220), 6.f);
    dl->AddRect(ImVec2(10, 10), ImVec2(230, 38), ACCENT_COL(180), 6.f);
#ifdef UNBRANDED
    dl->AddText(ImVec2(18, 16), ACCENT_COL(255), "OVERLAY");
#else
    dl->AddText(ImVec2(18, 16), ACCENT_COL(255), XS("TakePeek WD").c_str());
    dl->AddText(ImVec2(120, 16), IM_COL32(160,160,170,200), XS("| WarDogs").c_str());
#endif
}

static void DrawDebugHUD() {
    if (!g_showDebugHud) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float x = 10.f, y = 44.f;
    float lineH = 16.f;
    float panelW = 340.f;

    uintptr_t gworld = Read<uintptr_t>(g_base + g_gworldOff);
    uintptr_t gameInst = gworld ? Read<uintptr_t>(gworld + O::UWorld_OwningGameInstance) : 0;
    uintptr_t level = gworld ? Read<uintptr_t>(gworld + O::UWorld_PersistentLevel) : 0;
    int32_t actorCnt = level ? Read<int32_t>(level + O::ULevel_Actors + 8) : 0;
    int32_t playerArrayCnt = 0;
    if (g_gameState && g_playerArrayOffset)
        playerArrayCnt = Read<int32_t>(g_gameState + g_playerArrayOffset + 8);

    int enemies = 0, team = 0;
    for (auto& p : g_players) { if (p.isValid) { if (p.isTeammate) team++; else enemies++; } }

    int lines = 14;
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + panelW, y + lines * lineH + 8), IM_COL32(10,10,16,210), 4.f);
    dl->AddRect(ImVec2(x, y), ImVec2(x + panelW, y + lines * lineH + 8), IM_COL32(80,80,120,180), 4.f);

    auto statusLine = [&](const char* label, const char* val, bool ok) {
        char buf[128]; snprintf(buf, sizeof(buf), "%-18s %s", label, val);
        dl->AddText(ImVec2(x + 6, y + 4), ok ? IM_COL32(80,255,80,230) : IM_COL32(255,80,80,230), buf);
        y += lineH;
    };

    char buf[128];
    statusLine("GWorld:", gworld ? "OK" : "NULL", gworld != 0);
    statusLine("GameInstance:", gameInst ? "OK" : "NULL", gameInst != 0);

    snprintf(buf, sizeof(buf), "%s (PA: %d)", g_gameState ? "OK" : "NULL", playerArrayCnt);
    statusLine("GameState:", buf, g_gameState != 0 && playerArrayCnt > 0);

    statusLine("Camera:", g_camera.valid ? "OK" : "NULL", g_camera.valid);

    snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)s_renderPawn);
    statusLine("LocalPawn:", s_renderPawn ? buf : "NULL", s_renderPawn != 0);

    snprintf(buf, sizeof(buf), "E:%d T:%d", enemies, team);
    statusLine("Players:", buf, (enemies + team) > 0);

    snprintf(buf, sizeof(buf), "%d", (int)g_vehicles.size());
    statusLine("Vehicles:", buf, true);

    snprintf(buf, sizeof(buf), "%d actors", actorCnt);
    statusLine("Level:", buf, actorCnt > 20);

    bool factionOk = g_localFactionId != 0 || g_localTeamId != 0xFF;
    snprintf(buf, sizeof(buf), "%s tid=%d fid=0x%X",
        g_localFactionName.empty() ? "???" : g_localFactionName.c_str(),
        g_localTeamId, g_localFactionId);
    statusLine("Faction:", buf, factionOk);

    snprintf(buf, sizeof(buf), "obj=%s data=%s",
        g_localFactionObj ? "OK" : "NULL", g_localFactionData ? "OK" : "NULL");
    statusLine("FactionPtrs:", buf, g_localFactionObj != 0);

    snprintf(buf, sizeof(buf), "C2W=0x%X bone=0x%X/%d FOV=%.0f",
        (unsigned)g_c2wOffset, (unsigned)g_discoveredBoneOffset, (int)g_boneStride, g_camera.fov);
    statusLine("Offsets:", buf, g_c2wOffset != 0 && g_discoveredBoneOffset != 0);

    bool gwOffDefault = (g_gworldOff == O::GWorld);
    bool gnOffDefault = (g_gnamesOff == O::GNames);
    snprintf(buf, sizeof(buf), "GW=0x%X%s GN=0x%X%s",
        (unsigned)g_gworldOff, gwOffDefault ? "" : "!",
        (unsigned)g_gnamesOff, gnOffDefault ? "" : "!");
    statusLine("RVAs:", buf, gwOffDefault && gnOffDefault);

    uintptr_t rawGS = gworld ? Read<uintptr_t>(gworld + O::UWorld_GameState) : 0;
    snprintf(buf, sizeof(buf), "GS@1B0=0x%llX", (unsigned long long)rawGS);
    statusLine("RawGS:", buf, rawGS > 0x10000000);

    dl->AddText(ImVec2(x + 6, y + 8), IM_COL32(120,120,140,180), "F3=HUD  F4=Dump offsets");
}

static void DrawFovCircle() {
    auto& cfg = g_config.Active();
    if (!cfg.aimbot || !cfg.showFov) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float cx = (float)g_screenW * 0.5f, cy = (float)g_screenH * 0.5f;
    float radius = cfg.fov * ((float)g_screenW / 90.f);
    dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(255, 255, 255, 40), 64, 1.f);
}

// ============================================================================
// Night mode — darken the screen with a semi-transparent overlay
// ============================================================================
static void DrawNightMode() {
    auto& cfg = g_config.Active();
    if (!cfg.nightMode) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int alpha = (int)(cfg.nightAlpha * 255.f);
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)g_screenW, (float)g_screenH),
        IM_COL32(0, 0, 15, alpha));
}

// ============================================================================
// Hitmarker — flash crosshair lines when damage dealt
// ============================================================================
static void DrawHitmarker() {
    auto& cfg = g_config.Active();
    float hmTimer = g_hitmarkerTimer.load();
    if (!cfg.espHitmarker || hmTimer <= 0.f) return;
    float newHm = hmTimer - g_deltaTime;
    if (newHm < 0.f) newHm = 0.f;
    while (!g_hitmarkerTimer.compare_exchange_weak(hmTimer, newHm)) {
        if (hmTimer <= 0.f) return;
        newHm = hmTimer - g_deltaTime;
        if (newHm < 0.f) newHm = 0.f;
    }
    hmTimer = newHm;

    float alpha = hmTimer / 0.25f;
    ImU32 col = IM_COL32(255, 255, 255, (int)(alpha * 255));
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float cx = (float)g_screenW * 0.5f, cy = (float)g_screenH * 0.5f;
    float inner = 5.f, outer = 14.f;
    dl->AddLine(ImVec2(cx - outer, cy - outer), ImVec2(cx - inner, cy - inner), col, 2.f);
    dl->AddLine(ImVec2(cx + outer, cy - outer), ImVec2(cx + inner, cy - inner), col, 2.f);
    dl->AddLine(ImVec2(cx - outer, cy + outer), ImVec2(cx - inner, cy + inner), col, 2.f);
    dl->AddLine(ImVec2(cx + outer, cy + outer), ImVec2(cx + inner, cy + inner), col, 2.f);
}

// ============================================================================
// Menu — TakePeek-style vertical rail + two-panel layout
// ============================================================================
#define WD_ACCENT_R ACCENT_R
#define WD_ACCENT_G ACCENT_G
#define WD_ACCENT_B ACCENT_B
#define WD_ACCENT(a) ACCENT_COL(a)

static int g_menuPage = 0;

#ifndef UNBRANDED
static void DrawMenuReticle(ImDrawList* dl, ImVec2 center, float size) {
    float half = size * 0.5f, pad = size * 0.08f, arm = size * 0.22f;
    float thick = 2.0f, tickLen = size * 0.16f, gap = size * 0.05f;
    ImU32 cCol = IM_COL32(200, 200, 210, 255), tCol = IM_COL32(160, 160, 172, 255);
    ImU32 rCol = WD_ACCENT(255);
    float L = center.x - half, T = center.y - half, R = center.x + half, B = center.y + half;
    float cx = center.x, cy = center.y;
    dl->AddLine({L+pad,T+pad},{L+pad+arm,T+pad},cCol,thick); dl->AddLine({L+pad,T+pad},{L+pad,T+pad+arm},cCol,thick);
    dl->AddLine({R-pad,T+pad},{R-pad-arm,T+pad},cCol,thick); dl->AddLine({R-pad,T+pad},{R-pad,T+pad+arm},cCol,thick);
    dl->AddLine({L+pad,B-pad},{L+pad+arm,B-pad},cCol,thick); dl->AddLine({L+pad,B-pad},{L+pad,B-pad-arm},cCol,thick);
    dl->AddLine({R-pad,B-pad},{R-pad-arm,B-pad},cCol,thick); dl->AddLine({R-pad,B-pad},{R-pad,B-pad-arm},cCol,thick);
    dl->AddLine({cx,cy-gap-tickLen},{cx,cy-gap},tCol,thick);
    dl->AddLine({cx,cy+gap},{cx,cy+gap+tickLen},tCol,thick);
    dl->AddLine({cx-gap-tickLen,cy},{cx-gap,cy},tCol,thick);
    dl->AddLine({cx+gap,cy},{cx+gap+tickLen*1.6f,cy},rCol,thick+0.5f);
    dl->AddRectFilled({cx-3,cy-3},{cx+3,cy+3},rCol,1.f);
}
#endif // !UNBRANDED

static void DrawRailIcon(int index, ImVec2 center, ImU32 color, float scale) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float r = 10.0f * scale;
    if (index == 0) {
        draw->AddCircle(center, r, color, 16, 1.5f * scale);
        draw->AddLine(ImVec2(center.x - r - 4, center.y), ImVec2(center.x + r + 4, center.y), color, 1.5f * scale);
        draw->AddLine(ImVec2(center.x, center.y - r - 4), ImVec2(center.x, center.y + r + 4), color, 1.5f * scale);
    } else if (index == 1) {
        draw->AddEllipse(center, ImVec2(r + 4, r - 2), color, 0, 16, 1.5f * scale);
        draw->AddCircle(center, 3.5f * scale, color, 12, 1.5f * scale);
    } else if (index == 2) {
        for (int i = -1; i <= 1; ++i) {
            float y = center.y + i * 8.0f * scale;
            draw->AddLine(ImVec2(center.x - r, y), ImVec2(center.x + r, y), color, 1.5f * scale);
            draw->AddCircleFilled(ImVec2(center.x + ((i + 2) % 3 - 1) * 6.0f * scale, y), 2.5f * scale, color);
        }
    } else {
        draw->AddCircle(center, r, color, 16, 2.0f * scale);
        draw->AddCircle(center, r * 0.42f, color, 12, 1.5f * scale);
        for (int i = 0; i < 8; ++i) {
            float a = i * 0.785398f;
            ImVec2 outer(center.x + cosf(a) * (r + 4), center.y + sinf(a) * (r + 4));
            ImVec2 inner(center.x + cosf(a) * (r - 1), center.y + sinf(a) * (r - 1));
            draw->AddLine(inner, outer, color, 1.5f * scale);
        }
    }
}

static void DrawMenuRail() {
    static const char* labels[] = { "Combat", "Visuals", "Misc", "Settings" };

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.063f, 0.063f, 0.082f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
    ImGui::BeginChild("##menu_rail", ImVec2(56, 0), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* rdl = ImGui::GetWindowDrawList();
    ImVec2 rwp = ImGui::GetWindowPos();
    rdl->AddLine({rwp.x + 55, rwp.y + 4}, {rwp.x + 55, rwp.y + ImGui::GetWindowHeight() - 4},
        IM_COL32(50, 50, 64, 100), 1.f);

    ImGui::Dummy({0, 8});
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        bool selected = g_menuPage == i;
        ImGui::SetCursorPosX(7);
        if (ImGui::InvisibleButton("##nav", {42, 42})) g_menuPage = i;
        bool hovered = ImGui::IsItemHovered();
        ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        ImVec2 ct = {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f};
        if (selected) rdl->AddRectFilled(mn, mx, WD_ACCENT(30), 6.f);
        else if (hovered) rdl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, 8), 6.f);
        if (selected) rdl->AddRectFilled({mn.x - 5, mn.y + 8}, {mn.x - 2, mx.y - 8}, WD_ACCENT(255), 2.f);
        ImU32 iconCol = selected ? WD_ACCENT(255) :
            (hovered ? IM_COL32(220, 220, 228, 255) : IM_COL32(130, 132, 148, 255));
        DrawRailIcon(i, ct, iconCol, 0.68f);
        if (hovered) ImGui::SetTooltip("%s", labels[i]);
        ImGui::PopID();
        ImGui::Dummy({0, 4});
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

// --- Soldier texture for ESP preview (embedded image from soldier_texture.h) ---

static constexpr int STEX_W = SOLDIER_TEX_W;
static constexpr int STEX_H = SOLDIER_TEX_H;

static void CreateSoldierTexture() {
    if (!g_device || g_soldierSRV) return;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = STEX_W; td.Height = STEX_H;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = SOLDIER_TEX_DATA;
    sd.SysMemPitch = STEX_W * 4;
    ID3D11Texture2D* tex = nullptr;
    if (SUCCEEDED(g_device->CreateTexture2D(&td, &sd, &tex))) {
        g_device->CreateShaderResourceView(tex, nullptr, &g_soldierSRV);
        tex->Release();
    }
}


static void DrawESPPreview(float panelW, float panelH) {
    ImGui::BeginChild("##esp_preview", ImVec2(panelW, panelH), true);
    auto& cfg = g_config.Active();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetContentRegionAvail().y;

    NxWidgets::Header("PREVIEW");
    cp = ImGui::GetCursorScreenPos();
    h = ImGui::GetContentRegionAvail().y;

    dl->AddRectFilled(cp, ImVec2(cp.x + w, cp.y + h), IM_COL32(8, 8, 14, 255), 4.f);

    float groundY = cp.y + h * 0.92f;
    for (int i = 0; i < 7; i++) {
        float x0 = cp.x + w * (0.15f + i * 0.1f);
        dl->AddLine(ImVec2(x0, groundY - 4), ImVec2(x0, groundY + 4), IM_COL32(40, 40, 50, 80));
    }
    dl->AddLine(ImVec2(cp.x + 10, groundY), ImVec2(cp.x + w - 10, groundY), IM_COL32(40, 40, 50, 100));

    float cx = cp.x + w * 0.5f;
    float figTop = cp.y + 14.f;
    float figH = h - 36.f;
    float sc = figH / 300.f;

    // Joint positions mapped to texture proportions (160x400 soldier)
    float texAR = (float)STEX_W / (float)STEX_H;
    float imgH = figH;
    float imgW = imgH * texAR;
    float imgL = cx - imgW * 0.5f;

    auto P = [&](float tx, float ty) -> ImVec2 {
        return ImVec2(imgL + imgW * tx, figTop + imgH * ty);
    };

    ImVec2 head     = P(0.50f, 0.055f);
    ImVec2 neck     = P(0.50f, 0.13f);
    ImVec2 spine3   = P(0.50f, 0.19f);
    ImVec2 lShldr   = P(0.28f, 0.19f);
    ImVec2 rShldr   = P(0.72f, 0.19f);
    ImVec2 pelvis   = P(0.50f, 0.48f);
    ImVec2 lElbow   = P(0.22f, 0.34f);
    ImVec2 lHand    = P(0.30f, 0.46f);
    ImVec2 rElbow   = P(0.78f, 0.34f);
    ImVec2 rHand    = P(0.70f, 0.46f);
    ImVec2 lHip     = P(0.42f, 0.50f);
    ImVec2 rHip     = P(0.58f, 0.50f);
    ImVec2 lKnee    = P(0.40f, 0.70f);
    ImVec2 rKnee    = P(0.58f, 0.70f);
    ImVec2 lFoot    = P(0.38f, 0.92f);
    ImVec2 rFoot    = P(0.60f, 0.92f);

    if (!cfg.esp) {
        const char* off = "ESP OFF";
        ImVec2 ts = ImGui::CalcTextSize(off);
        dl->AddText(ImVec2(cp.x + w * 0.5f - ts.x * 0.5f, cp.y + h * 0.5f - ts.y * 0.5f),
            IM_COL32(100, 100, 110, 200), off);
        ImGui::EndChild();
        return;
    }

    // --- Character model (rendered texture) ---
    float headR = imgH * 0.06f;
    float bootH = imgH * 0.025f;

    if (g_soldierSRV) {
        dl->AddImage((ImTextureID)g_soldierSRV,
                     ImVec2(imgL, figTop),
                     ImVec2(imgL + imgW, figTop + imgH),
                     ImVec2(0, 0), ImVec2(1, 1),
                     IM_COL32(255, 255, 255, 240));
    }

    // --- ESP overlays on top of body ---
    const float* boxCol  = cfg.colVisBox;
    const float* skelCol = cfg.colVisSkel;
    const float* nameCol = cfg.colVisName;

    float boxTop = head.y - headR - 4.f;
    float boxBot = lFoot.y + bootH + 4.f;
    float boxW_half = imgW * 0.5f + 4.f;
    float boxLeft = cx - boxW_half;
    float boxRight = cx + boxW_half;
    float boxHeight = boxBot - boxTop;

    if (cfg.espBoxes) {
        dl->AddRect(ImVec2(boxLeft-1, boxTop-1), ImVec2(boxRight+1, boxBot+1), IM_COL32(0,0,0,100));
        dl->AddRect(ImVec2(boxLeft, boxTop), ImVec2(boxRight, boxBot), GetColor(boxCol), 0, 0, 1.5f);
    }

    if (cfg.espSkeleton) {
        ImU32 skc = GetColor(skelCol);
        float skt = 1.5f;
        dl->AddLine(head, neck, skc, skt);
        dl->AddLine(neck, spine3, skc, skt);
        dl->AddLine(spine3, pelvis, skc, skt);
        dl->AddLine(spine3, lShldr, skc, skt);
        dl->AddLine(lShldr, lElbow, skc, skt);
        dl->AddLine(lElbow, lHand, skc, skt);
        dl->AddLine(spine3, rShldr, skc, skt);
        dl->AddLine(rShldr, rElbow, skc, skt);
        dl->AddLine(rElbow, rHand, skc, skt);
        dl->AddLine(pelvis, lHip, skc, skt);
        dl->AddLine(lHip, lKnee, skc, skt);
        dl->AddLine(lKnee, lFoot, skc, skt);
        dl->AddLine(pelvis, rHip, skc, skt);
        dl->AddLine(rHip, rKnee, skc, skt);
        dl->AddLine(rKnee, rFoot, skc, skt);
        for (auto& jt : {head, neck, spine3, pelvis, lShldr, lElbow, lHand, rShldr, rElbow, rHand, lHip, lKnee, lFoot, rHip, rKnee, rFoot})
            dl->AddCircleFilled(jt, 2.5f*sc, skc, 6);
    }

    if (cfg.espHealth) {
        float hpFrac = 0.75f;
        float barH = boxHeight * hpFrac;
        int r = (int)((1.f - hpFrac) * 255), g = (int)(hpFrac * 255);
        dl->AddRectFilled(ImVec2(boxLeft - 7, boxTop), ImVec2(boxLeft - 3, boxBot), IM_COL32(0,0,0,120));
        dl->AddRectFilled(ImVec2(boxLeft - 7, boxBot - barH), ImVec2(boxLeft - 3, boxBot), IM_COL32(r, g, 30, 230));
        dl->AddRect(ImVec2(boxLeft - 7, boxTop), ImVec2(boxLeft - 3, boxBot), IM_COL32(0,0,0,160));
    }

    if (cfg.espHeadDot)
        dl->AddCircleFilled(head, 4.0f * sc, IM_COL32(255, 50, 50, 230), 10);

    if (cfg.espName) {
        const char* name = "Player";
        ImVec2 ts = ImGui::CalcTextSize(name);
        float nx = cx - ts.x * 0.5f;
        dl->AddText(ImVec2(nx + 1, boxTop - ts.y - 3), IM_COL32(0, 0, 0, 180), name);
        dl->AddText(ImVec2(nx, boxTop - ts.y - 4), GetColor(nameCol), name);
    }

    if (cfg.espDistance) {
        const char* dist = "42m";
        ImVec2 ts = ImGui::CalcTextSize(dist);
        float dx = cx - ts.x * 0.5f;
        dl->AddText(ImVec2(dx + 1, boxBot + 3), IM_COL32(0, 0, 0, 180), dist);
        dl->AddText(ImVec2(dx, boxBot + 2), IM_COL32(220, 220, 220, 230), dist);
    }

    if (cfg.espSnaplines) {
        ImU32 snapCol = GetColor(cfg.colVisSnap);
        dl->AddLine(ImVec2(cx, cp.y + h), ImVec2(cx, boxBot), snapCol);
    }

    ImGui::EndChild();
}

static void DrawMenu() {
    if (!g_menuOpen) return;
    auto& cfg = g_config.Active();

    ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({940, 500}, ImGuiCond_FirstUseEver);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.075f, 0.098f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.16f, 0.16f, 0.21f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.098f, 0.098f, 0.125f, 0.9f));

    ImGui::Begin("##tpwd_menu", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImDrawList* hdl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();

    // Header background with gradient
    float hdrH = 48.f;
    hdl->AddRectFilled(wp, {wp.x + ws.x, wp.y + hdrH},
        IM_COL32(16, 16, 22, 255), 10.f, ImDrawFlags_RoundCornersTop);
    hdl->AddRectFilledMultiColor(
        {wp.x, wp.y + hdrH - 12}, {wp.x + ws.x, wp.y + hdrH},
        IM_COL32(16, 16, 22, 0), IM_COL32(16, 16, 22, 0),
        IM_COL32(22, 22, 28, 255), IM_COL32(22, 22, 28, 255));
    hdl->AddRectFilled({wp.x, wp.y + hdrH - 1}, {wp.x + ws.x, wp.y + hdrH}, WD_ACCENT(80));

    // Brand text
    ImFont* hdrFont = ImGui::GetIO().Fonts->Fonts.Size > 1 ?
        ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont();
#ifdef UNBRANDED
    hdl->AddText(hdrFont, 16.f, {wp.x + 18, wp.y + 10},
        IM_COL32(240, 240, 244, 255), BRAND_TITLE);
    hdl->AddText(ImGui::GetFont(), 10.f, {wp.x + 18, wp.y + 28},
        IM_COL32(100, 100, 112, 200), "External Overlay");
#else
    // Reticle icon
    DrawMenuReticle(hdl, {wp.x + 28, wp.y + hdrH * 0.5f}, 30.f);
    hdl->AddText(hdrFont, 16.f, {wp.x + 50, wp.y + 10},
        IM_COL32(240, 240, 244, 255), XS("TAKEPEEK").c_str());
    float bx = wp.x + 130, by = wp.y + 11;
    hdl->AddRect({bx, by}, {bx + 30, by + 15}, WD_ACCENT(200), 3.f, 0, 1.2f);
    hdl->AddText(ImGui::GetFont(), 10.f, {bx + 5, by + 2}, WD_ACCENT(200), XS("WD").c_str());
    hdl->AddText(ImGui::GetFont(), 10.f, {wp.x + 50, wp.y + 28},
        IM_COL32(100, 100, 112, 200), XS("WarDogs External").c_str());
#endif

    // Player count + status in header (right side)
    char hdrInfo[64];
    sprintf_s(hdrInfo, "P:%d", (int)g_players.size());
    ImVec2 his = ImGui::GetFont()->CalcTextSizeA(11.f, FLT_MAX, 0, hdrInfo);
    float infoX = wp.x + ws.x - his.x - 18;
    ImU32 dotC = !g_players.empty() ? IM_COL32(60, 200, 90, 255) : IM_COL32(100, 100, 110, 255);
    hdl->AddCircleFilled({infoX - 10, wp.y + hdrH * 0.5f}, 3.f, dotC);
    hdl->AddText(ImGui::GetFont(), 11.f, {infoX, wp.y + hdrH * 0.5f - 6},
        IM_COL32(120, 120, 135, 255), hdrInfo);

    ImGui::SetCursorPos({0.f, hdrH + 4.f});
    ImGui::Dummy({0, 0});

    // Rail + content
    DrawMenuRail();
    ImGui::SameLine(0, 8);
    ImGui::BeginChild("##menu_content", ImVec2(0, ws.y - hdrH - 12), false);

    // ============ COMBAT TAB ============
    if (g_menuPage == 0) {
        float panelW = (ImGui::GetContentRegionAvail().x - 12) * 0.5f;
        float panelH = ImGui::GetContentRegionAvail().y - 4;

        ImGui::BeginChild("##aim_panel", ImVec2(panelW, panelH), true);
        {
            NxWidgets::Header("AIMBOT");
            NxWidgets::Toggle("Enable Aimbot", &cfg.aimbot);
            NxWidgets::Toggle("Team Check", &cfg.teamCheck);
            NxWidgets::Toggle("Vis Check", &cfg.aimVisCheck);
            NxWidgets::Toggle("Show FOV Circle", &cfg.showFov);
            NxWidgets::Toggle("Target Lock", &cfg.aimLock);
            NxWidgets::SliderFloat("FOV Radius", &cfg.fov, 1.f, 50.f);
            NxWidgets::SliderFloat("Smoothing", &cfg.smooth, 1.f, 20.f);
            NxWidgets::SliderFloat("Max Distance", &cfg.aimMaxDistance, 10.f, 1000.f);
            NxWidgets::Toggle("Prediction (Drop + Lead)", &cfg.aimPrediction);

            NxWidgets::Header("AIM KEY");
            {
                const char* modes[] = {"Hold", "Toggle"};
                bool isHold = cfg.aimHoldMode;
                for (int i = 0; i < 2; i++) {
                    bool sel = (i == 0) ? isHold : !isHold;
                    ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.4f) : ImVec4(0.106f,0.102f,0.129f,1));
                    ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : ImVec4(0.6f,0.6f,0.65f,1));
                    if (ImGui::Button(modes[i], ImVec2(60, 24))) cfg.aimHoldMode = (i == 0);
                    ImGui::PopStyleColor(2);
                    if (i == 0) ImGui::SameLine();
                }
                ImGui::Text("Bind:");
                ImGui::SameLine();
                for (int i = 0; i < 8; i++) {
                    bool sel = (cfg.aimKeyIdx == i);
                    ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.4f) : ImVec4(0.106f,0.102f,0.129f,1));
                    ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : ImVec4(0.6f,0.6f,0.65f,1));
                    if (ImGui::Button(AIM_KEY_NAMES[i], ImVec2(0, 22))) cfg.aimKeyIdx = i;
                    ImGui::PopStyleColor(2);
                    if (i < 7) ImGui::SameLine();
                }
            }

            NxWidgets::Header("AIM BONE");
            const char* bones[] = {"Head", "Neck", "Chest", "Pelvis"};
            for (int i = 0; i < 4; i++) {
                bool sel = (cfg.aimBone == i);
                ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.4f) : ImVec4(0.106f,0.102f,0.129f,1));
                ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : ImVec4(0.6f,0.6f,0.65f,1));
                if (ImGui::Button(bones[i], ImVec2(72, 26))) cfg.aimBone = i;
                ImGui::PopStyleColor(2);
                if (i < 3) ImGui::SameLine();
            }
            NxWidgets::Header("RECOIL / SWAY");
            NxWidgets::Toggle("No Recoil", &cfg.noRecoil);
            NxWidgets::Toggle("No Sway", &cfg.noSway);
            NxWidgets::Toggle("No Spread (WIP)", &cfg.noSpread);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##trig_panel", ImVec2(panelW, panelH), true);
        {
            NxWidgets::Header("TRIGGERBOT");
            NxWidgets::Toggle("Enable Triggerbot", &cfg.triggerbot);
            NxWidgets::SliderInt("Trigger Delay (ms)", &cfg.triggerDelay, 0, 200);
            if (cfg.triggerbot) {
                ImGui::Text("Trigger Key:");
                ImGui::SameLine();
                for (int i = 0; i < 8; i++) {
                    bool sel = (cfg.triggerKeyIdx == i);
                    ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.4f) : ImVec4(0.106f,0.102f,0.129f,1));
                    ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : ImVec4(0.6f,0.6f,0.65f,1));
                    if (ImGui::Button(AIM_KEY_NAMES[i], ImVec2(0, 22))) cfg.triggerKeyIdx = i;
                    ImGui::PopStyleColor(2);
                    if (i < 7) ImGui::SameLine();
                }
            }
        }
        ImGui::EndChild();
    }

    // ============ VISUALS TAB ============
    if (g_menuPage == 1) {
        float totalW = ImGui::GetContentRegionAvail().x;
        float panelH = ImGui::GetContentRegionAvail().y - 4;
        float settingsW = (totalW - 24) * 0.33f;
        float colorsW   = (totalW - 24) * 0.33f;
        float previewW  = totalW - settingsW - colorsW - 24;

        ImGui::BeginChild("##esp_panel", ImVec2(settingsW, panelH), true);
        {
            NxWidgets::Header("ESP ELEMENTS");
            NxWidgets::Toggle("Enable ESP", &cfg.esp);
            if (cfg.esp) {
                ImGui::Spacing();
                NxWidgets::Toggle("Bounding Boxes", &cfg.espBoxes);
                if (cfg.espBoxes) {
                    const char* boxStyles[] = {"Full", "Corners"};
                    for (int i = 0; i < 2; i++) {
                        bool sel = (cfg.espBoxStyle == i);
                        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.4f) : ImVec4(0.106f,0.102f,0.129f,1));
                        ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : ImVec4(0.6f,0.6f,0.65f,1));
                        if (ImGui::Button(boxStyles[i], ImVec2(60, 22))) cfg.espBoxStyle = i;
                        ImGui::PopStyleColor(2);
                        if (i == 0) ImGui::SameLine();
                    }
                }
                NxWidgets::Toggle("Skeleton", &cfg.espSkeleton);
                NxWidgets::Toggle("Glow", &cfg.espGlow);
                if (cfg.espGlow) {
                    NxWidgets::SliderInt("Glow Intensity", &cfg.espGlowIntensity, 1, 5);
                    ImGui::ColorEdit3("Glow Color", cfg.colGlow, ImGuiColorEditFlags_NoInputs);
                }
                NxWidgets::Toggle("Health Bar", &cfg.espHealth);
                NxWidgets::Toggle("Player Name", &cfg.espName);
                NxWidgets::Toggle("Distance", &cfg.espDistance);
                NxWidgets::Toggle("Snaplines", &cfg.espSnaplines);
                if (cfg.espSnaplines) {
                    const char* origins[] = {"Bottom", "Center", "Top"};
                    for (int i = 0; i < 3; i++) {
                        bool sel = (cfg.espSnapOrigin == i);
                        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.4f) : ImVec4(0.106f,0.102f,0.129f,1));
                        ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(1,1,1,1) : ImVec4(0.6f,0.6f,0.65f,1));
                        if (ImGui::Button(origins[i], ImVec2(52, 22))) cfg.espSnapOrigin = i;
                        ImGui::PopStyleColor(2);
                        if (i < 2) ImGui::SameLine();
                    }
                }
                NxWidgets::Toggle("Head Dot", &cfg.espHeadDot);
                NxWidgets::Toggle("Hitmarker", &cfg.espHitmarker);
                NxWidgets::Toggle("Vis Check", &cfg.espVisCheck);
                NxWidgets::Toggle("Show Team", &cfg.espShowTeam);
                NxWidgets::Toggle("Show Downed", &cfg.espShowDowned);
                NxWidgets::Toggle("Show Dead", &cfg.espShowDead);
                NxWidgets::Toggle("Faction Tag", &cfg.espFaction);
                NxWidgets::Toggle("Weapon Name", &cfg.espWeapon);
                NxWidgets::Toggle("View Direction", &cfg.espViewDir);
                NxWidgets::Toggle("Off-Screen Arrows", &cfg.espOffScreen);
                NxWidgets::SliderFloat("Max Distance", &cfg.espMaxDistance, 50.f, 2000.f);

                NxWidgets::Header("VEHICLES");
                NxWidgets::Toggle("Vehicle ESP", &cfg.espVehicles);
                if (cfg.espVehicles) {
                    NxWidgets::Toggle("Show Occupants", &cfg.espVehicleOccupants);
                    NxWidgets::Toggle("Enemy Only", &cfg.vehicleEnemyOnly);
                    NxWidgets::Toggle("Show Dead Vehicles", &cfg.vehicleShowDead);
                    NxWidgets::Toggle("Show Empty", &cfg.vehicleShowEmpty);
                    NxWidgets::Toggle("Land Vehicles", &cfg.vehicleFilterLand);
                    NxWidgets::Toggle("Air Vehicles", &cfg.vehicleFilterAir);
                    NxWidgets::SliderFloat("Vehicle Max Dist", &cfg.vehicleMaxDistance, 50.f, 3000.f);
                    ImGui::ColorEdit4("Enemy##veh", cfg.colVehicle, ImGuiColorEditFlags_NoInputs);
                    ImGui::SameLine(); ImGui::ColorEdit4("Team##veh", cfg.colVehicleTeam, ImGuiColorEditFlags_NoInputs);
                }

                NxWidgets::Header("WORLD ESP");
                NxWidgets::Toggle("Dropped Items", &cfg.espWorldItems);
                if (cfg.espWorldItems) {
                    NxWidgets::Toggle("  Weapons", &cfg.espItemWeapons);
                    NxWidgets::Toggle("  Ammo", &cfg.espItemAmmo);
                    NxWidgets::Toggle("  Attachments", &cfg.espItemAttachments);
                    NxWidgets::Toggle("  Medical", &cfg.espItemMedical);
                    NxWidgets::Toggle("  Grenades", &cfg.espItemGrenades);
                    NxWidgets::Toggle("  Other Items", &cfg.espItemOther);
                }
                NxWidgets::Toggle("Mines / Explosives", &cfg.espMines);
                NxWidgets::Toggle("Emplacements / Mortars", &cfg.espEmplacements);
                NxWidgets::Toggle("FOBs / Spawn Points", &cfg.espFOBs);
                NxWidgets::Toggle("Bodybags", &cfg.espBodybags);

                NxWidgets::Header("RADAR");
                NxWidgets::Toggle("2D Radar", &cfg.radar);
                if (cfg.radar) {
                    NxWidgets::SliderFloat("Radar Size", &cfg.radarSize, 80.f, 300.f);
                    NxWidgets::SliderFloat("Radar Zoom", &cfg.radarZoom, 0.5f, 5.f);
                    NxWidgets::Toggle("Drag & Resize", &cfg.radarDraggable);
                    if (cfg.radarPosX >= 0.f) {
                        if (ImGui::Button("Reset Position")) {
                            cfg.radarPosX = -1.f;
                            cfg.radarPosY = -1.f;
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##color_panel", ImVec2(colorsW, panelH), true);
        {
            NxWidgets::Header("VISIBLE ENEMIES");
            ImGui::ColorEdit4("Box##vis", cfg.colVisBox, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Skel##vis", cfg.colVisSkel, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Name##vis", cfg.colVisName, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Snap##vis", cfg.colVisSnap, ImGuiColorEditFlags_NoInputs);

            NxWidgets::Header("HIDDEN ENEMIES");
            ImGui::ColorEdit4("Box##hid", cfg.colHidBox, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Skel##hid", cfg.colHidSkel, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Name##hid", cfg.colHidName, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Snap##hid", cfg.colHidSnap, ImGuiColorEditFlags_NoInputs);

            NxWidgets::Header("TEAM");
            ImGui::ColorEdit4("Box##team", cfg.colTeamBox, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Skel##team", cfg.colTeamSkel, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine(); ImGui::ColorEdit4("Name##team", cfg.colTeamName, ImGuiColorEditFlags_NoInputs);
        }
        ImGui::EndChild();

        ImGui::SameLine();
        DrawESPPreview(previewW, panelH);
    }

    // ============ MISC TAB ============
    if (g_menuPage == 2) {
        float panelW = (ImGui::GetContentRegionAvail().x - 12) * 0.5f;
        float panelH = ImGui::GetContentRegionAvail().y - 4;

        ImGui::BeginChild("##misc_left", ImVec2(panelW, panelH), true);
        {
            NxWidgets::Header("VISUAL");
            NxWidgets::Toggle("Night Mode", &cfg.nightMode);
            if (cfg.nightMode)
                NxWidgets::SliderFloat("Night Alpha", &cfg.nightAlpha, 0.f, 1.f);
            NxWidgets::Toggle("FOV Changer", &cfg.fovChanger);
            if (cfg.fovChanger)
                NxWidgets::SliderInt("FOV Value", &cfg.fovValue, 30, 170);

            NxWidgets::Header("HUD");
            NxWidgets::Toggle("Watermark", &cfg.watermark);
            NxWidgets::Toggle("Crosshair", &cfg.crosshair);
            NxWidgets::Toggle("FPS Counter", &cfg.fpsCounter);
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##misc_right", ImVec2(panelW, panelH), true);
        {
            NxWidgets::Header("INFO");
            ImGui::TextColored(ImVec4(0.45f,0.85f,0.55f,1.f), "Active license");
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.55f,1), "Status: %s", GetAuthStatus());
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.55f,1), "License: %s", GetLicenseDuration());
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.55f,1), "Process: %s", g_pid ? "Attached" : "Not found");
            ImGui::TextColored(ImVec4(0.5f,0.5f,0.55f,1), "Players: %d", (int)g_players.size());

            NxWidgets::Header("HOTKEYS");
            ImGui::TextColored(ImVec4(0.45f,0.45f,0.5f,1), "INSERT  Toggle Menu");
            ImGui::TextColored(ImVec4(0.45f,0.45f,0.5f,1), "F3      Debug HUD");
            ImGui::TextColored(ImVec4(0.45f,0.45f,0.5f,1), "F4      Dump Offsets");

            NxWidgets::Header("SDK DUMPER");
            if (g_dumpRunning) {
                ImGui::TextColored(ImVec4(1.f,0.8f,0.2f,1.f), "Scanning GObjects...");
            } else if (g_dumpDone) {
                ImGui::TextColored(ImVec4(0.3f,1.f,0.4f,1.f), "Dump saved to Desktop");
            } else {
                if (ImGui::Button("Dump Offsets (F4)", ImVec2(-1, 28))) {
                    std::thread(RunOffsetDumper).detach();
                }
            }
        }
        ImGui::EndChild();
    }

    // ============ SETTINGS TAB ============
    if (g_menuPage == 3) {
        float panelW = (ImGui::GetContentRegionAvail().x - 12) * 0.5f;
        float panelH = ImGui::GetContentRegionAvail().y - 4;

        ImGui::BeginChild("##cfg_panel", ImVec2(panelW, panelH), true);
        {
            NxWidgets::Header("CONFIG PROFILES");
            for (int i = 0; i < MAX_PROFILES; i++) {
                bool isActive = (g_config.activeProfile == i);
                if (isActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.3f));
                if (ImGui::Button(g_config.profiles[i].name, ImVec2(80, 28))) g_config.activeProfile = i;
                if (isActive) ImGui::PopStyleColor();
                if (i < MAX_PROFILES - 1) ImGui::SameLine();
            }
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.25f));
            if (ImGui::Button("Save", ImVec2(100, 30))) g_config.Save(g_config.activeProfile);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::Button("Load", ImVec2(100, 30))) g_config.Load(g_config.activeProfile);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f,0.15f,0.15f,0.5f));
            if (ImGui::Button("Reset", ImVec2(100, 30))) g_config.Reset(g_config.activeProfile);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##cfg_right", ImVec2(panelW, panelH), true);
        {
            NxWidgets::Header("ACTIVE FEATURES");
            auto& c = g_config.Active();
            if (c.aimbot) ImGui::BulletText("Aimbot%s%s", c.aimLock ? " (Lock)" : "", c.aimPrediction ? " (Pred)" : "");
            if (c.triggerbot) ImGui::BulletText("Triggerbot");
            if (c.noRecoil) ImGui::BulletText("No Recoil");
            if (c.noSway) ImGui::BulletText("No Sway");
            if (c.noSpread) ImGui::BulletText("No Spread (WIP)");
            if (c.esp) ImGui::BulletText("ESP");
            if (c.espOffScreen) ImGui::BulletText("Off-Screen Arrows");
            if (c.espFaction) ImGui::BulletText("Faction Tags");
            if (c.espWeapon) ImGui::BulletText("Weapon Names");
            if (c.espViewDir) ImGui::BulletText("View Direction");
            if (c.espVehicles) ImGui::BulletText("Vehicle ESP");
            if (c.espWorldItems) ImGui::BulletText("World Items");
            if (c.espMines) ImGui::BulletText("Mines/Explosives");
            if (c.espEmplacements) ImGui::BulletText("Emplacements");
            if (c.espFOBs) ImGui::BulletText("FOBs");
            if (c.radar) ImGui::BulletText("Radar");
            if (c.nightMode) ImGui::BulletText("Night Mode");
            if (c.fovChanger) ImGui::BulletText("FOV Changer");
            if (c.crosshair) ImGui::BulletText("Crosshair");
            if (!c.aimbot && !c.triggerbot && !c.esp && !c.radar &&
                !c.nightMode && !c.fovChanger && !c.crosshair)
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.55f,0.7f), "No features active");
        }
        ImGui::EndChild();
    }

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

// ============================================================================
// Auto-Updater — checks GitHub for new versions, downloads + self-replaces
// ============================================================================

static struct {
    enum State { IDLE, CHECKING, DOWNLOADING, READY, APPLYING, FAILED, UP_TO_DATE };
    std::atomic<int> state{IDLE};
    std::atomic<int> pct{0};
    char newVer[32] = {};
    char changelog[512] = {};
    char dlUrl[1024] = {};
    char err[128] = {};
    DWORD readyTime = 0;
    DWORD failTime = 0;
    DWORD uptodateTime = 0;
} g_update;

static std::wstring ToWideStr(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string UpdateJsonStr(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return {};
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return {};
    p = j.find('"', p + 1);
    if (p == std::string::npos) return {};
    size_t e = j.find('"', p + 1);
    if (e == std::string::npos) return {};
    return j.substr(p + 1, e - p - 1);
}

static bool VerNewer(const char* remote, const char* local) {
    int rM = 0, rm = 0, rp = 0, lM = 0, lm = 0, lp = 0;
    sscanf_s(remote, "%d.%d.%d", &rM, &rm, &rp);
    sscanf_s(local, "%d.%d.%d", &lM, &lm, &lp);
    if (rM != lM) return rM > lM;
    if (rm != lm) return rm > lm;
    return rp > lp;
}

static std::string UpdateHttpGet(const wchar_t* host, const wchar_t* path) {
    std::string result;
    HINTERNET ses = WinHttpOpen(L"Updater/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!ses) return result;
    HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!con) { WinHttpCloseHandle(ses); return result; }
    wchar_t fullPath[4096];
    swprintf_s(fullPath, L"%s?t=%u", path, (unsigned)GetTickCount());
    HINTERNET req = WinHttpOpenRequest(con, L"GET", fullPath,
        nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return result; }
    const wchar_t* noCache = L"Cache-Control: no-cache\r\nPragma: no-cache";
    if (WinHttpSendRequest(req, noCache, -1, nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &status, &sz, nullptr);
        if (status == 200) {
            char buf[4096]; DWORD n;
            while (WinHttpReadData(req, buf, sizeof(buf), &n) && n > 0)
                result.append(buf, n);
        }
    }
    WinHttpCloseHandle(req); WinHttpCloseHandle(con); WinHttpCloseHandle(ses);
    return result;
}

static bool UpdateDownloadFile(const std::string& url, const std::string& outPath) {
    std::wstring wurl = ToWideStr(url);

    HINTERNET ses = WinHttpOpen(L"Updater/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!ses) return false;

    std::wstring currentUrl = wurl;
    HINTERNET con = nullptr, req = nullptr;
    DWORD status = 0;
    for (int redir = 0; redir < 5; redir++) {
        URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
        wchar_t host[256] = {}, path[4096] = {};
        uc.lpszHostName = host; uc.dwHostNameLength = 256;
        uc.lpszUrlPath = path; uc.dwUrlPathLength = 4096;
        if (!WinHttpCrackUrl(currentUrl.c_str(), 0, 0, &uc)) break;

        if (con) WinHttpCloseHandle(con);
        if (req) WinHttpCloseHandle(req);
        con = WinHttpConnect(ses, host, uc.nPort, 0);
        if (!con) break;
        DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        req = WinHttpOpenRequest(con, L"GET", path, nullptr, nullptr, nullptr, flags);
        if (!req) break;
        DWORD noAutoRedir = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(req, WINHTTP_OPTION_DISABLE_FEATURE, &noAutoRedir, sizeof(noAutoRedir));

        if (!WinHttpSendRequest(req, nullptr, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(req, nullptr)) break;

        status = 0; DWORD sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &status, &sz, nullptr);

        if (status == 200) break;
        if (status == 301 || status == 302 || status == 303 || status == 307) {
            wchar_t loc[4096] = {};
            DWORD locSz = sizeof(loc);
            if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                loc, &locSz, WINHTTP_NO_HEADER_INDEX)) break;
            currentUrl = loc;
            continue;
        }
        break;
    }

    bool ok = false;
    if (status == 200 && req) {
        DWORD total = 0, sz = sizeof(total);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            nullptr, &total, &sz, nullptr);
        FILE* f = nullptr; fopen_s(&f, outPath.c_str(), "wb");
        if (f) {
            char buf[8192]; DWORD n, downloaded = 0;
            ok = true;
            while (WinHttpReadData(req, buf, sizeof(buf), &n) && n > 0) {
                fwrite(buf, 1, n, f);
                downloaded += n;
                if (total > 0)
                    g_update.pct.store((int)((downloaded * 100ULL) / total));
            }
            fclose(f);
            if (downloaded < 100 * 1024) {
                DeleteFileA(outPath.c_str());
                ok = false;
            }
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

static void UpdateWorkerThread() {
    g_update.state.store(g_update.CHECKING);

    std::string json = UpdateHttpGet(UPDATE_HOST, UPDATE_PATH);
    if (json.empty()) {
        strcpy_s(g_update.err, "Cannot reach update server");
        g_update.failTime = GetTickCount();
        g_update.state.store(g_update.FAILED);
        return;
    }

    std::string ver = UpdateJsonStr(json, "version");
    std::string url = UpdateJsonStr(json, "url");
    std::string log = UpdateJsonStr(json, "changelog");

    if (ver.empty() || url.empty()) {
        strcpy_s(g_update.err, "Invalid update info");
        g_update.failTime = GetTickCount();
        g_update.state.store(g_update.FAILED);
        return;
    }

    if (!VerNewer(ver.c_str(), TAKEPEEK_VERSION)) {
        g_update.uptodateTime = GetTickCount();
        g_update.state.store(g_update.UP_TO_DATE);
        return;
    }

    strcpy_s(g_update.newVer, ver.c_str());
    strcpy_s(g_update.dlUrl, url.c_str());
    if (!log.empty()) strcpy_s(g_update.changelog, log.c_str());

    g_update.state.store(g_update.DOWNLOADING);
    g_update.pct.store(0);

    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string dir(exePath);
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
#ifdef UNBRANDED
    std::string updateFile = dir + "Overlay_update.exe";
#else
    std::string updateFile = dir + "TakePeekWD_update.exe";
#endif

    if (!UpdateDownloadFile(url, updateFile)) {
        strcpy_s(g_update.err, "Download failed");
        g_update.failTime = GetTickCount();
        g_update.state.store(g_update.FAILED);
        return;
    }

    g_update.readyTime = GetTickCount();
    g_update.state.store(g_update.READY);
}

static void StartUpdateCheck() {
    if (g_update.state.load() != g_update.IDLE) return;
    std::thread(UpdateWorkerThread).detach();
}

static bool ApplyUpdate() {
    g_update.state.store(g_update.APPLYING);

    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeStr(exePath);
    std::string dir = exeStr.substr(0, exeStr.find_last_of("\\/") + 1);
#ifdef UNBRANDED
    std::string oldPath = dir + "Overlay_old.exe";
    std::string updatePath = dir + "Overlay_update.exe";
#else
    std::string oldPath = dir + "TakePeekWD_old.exe";
    std::string updatePath = dir + "TakePeekWD_update.exe";
#endif

    DeleteFileA(oldPath.c_str());

    if (!MoveFileA(exePath, oldPath.c_str())) {
        strcpy_s(g_update.err, "Cannot rename current exe");
        g_update.failTime = GetTickCount();
        g_update.state.store(g_update.FAILED);
        return false;
    }

    if (!MoveFileA(updatePath.c_str(), exePath)) {
        MoveFileA(oldPath.c_str(), exePath);
        strcpy_s(g_update.err, "Cannot place new exe");
        g_update.failTime = GetTickCount();
        g_update.state.store(g_update.FAILED);
        return false;
    }

    WriteStartupLog("Update", "Launching new version...");
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL launched = CreateProcessA(exePath, nullptr, nullptr, nullptr,
        FALSE, 0, nullptr, dir.c_str(), &si, &pi);
    if (!launched) {
        char errBuf[128];
        sprintf_s(errBuf, "CreateProcess failed (err=%lu), restoring old exe", GetLastError());
        WriteStartupLog("Update", errBuf);
        MoveFileA(exePath, updatePath.c_str());
        MoveFileA(oldPath.c_str(), exePath);
        strcpy_s(g_update.err, "Failed to launch updated exe");
        g_update.failTime = GetTickCount();
        g_update.state.store(g_update.FAILED);
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

static void CleanupOldUpdate() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string dir(exePath);
    dir = dir.substr(0, dir.find_last_of("\\/") + 1);
#ifdef UNBRANDED
    DeleteFileA((dir + "Overlay_old.exe").c_str());
    DeleteFileA((dir + "Overlay_update.exe").c_str());
#else
    DeleteFileA((dir + "TakePeekWD_old.exe").c_str());
    DeleteFileA((dir + "TakePeekWD_update.exe").c_str());
#endif
}

static void DrawUpdateBar() {
    int st = g_update.state.load();
    if (st == g_update.IDLE) return;

    if (st == g_update.UP_TO_DATE) {
        if (GetTickCount() - g_update.uptodateTime > 3000) return;
    }
    if (st == g_update.FAILED) {
        if (GetTickCount() - g_update.failTime > 5000) return;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float scrW = (float)g_screenW;
    float barW = 340.f, barH = 30.f;
    float x = (scrW - barW) * 0.5f, y = 4.f;

    dl->AddRectFilled({x, y}, {x + barW, y + barH}, IM_COL32(12, 12, 16, 230), 6.f);

    char text[128] = {};
    ImU32 borderCol = WD_ACCENT(160);

    switch (st) {
    case g_update.CHECKING:
        strcpy_s(text, "Checking for updates...");
        borderCol = IM_COL32(100, 100, 120, 120);
        break;
    case g_update.DOWNLOADING: {
        int pct = g_update.pct.load();
        sprintf_s(text, "Downloading v%s... %d%%", g_update.newVer, pct);
        float progW = (barW - 4) * (pct / 100.f);
        dl->AddRectFilled({x + 2, y + barH - 3}, {x + 2 + progW, y + barH - 1},
            WD_ACCENT(200), 1.f);
        break;
    }
    case g_update.READY:
        sprintf_s(text, "Update v%s ready - restarting...", g_update.newVer);
        borderCol = IM_COL32(50, 200, 50, 180);
        break;
    case g_update.APPLYING:
        strcpy_s(text, "Installing update...");
        borderCol = IM_COL32(50, 200, 50, 180);
        break;
    case g_update.UP_TO_DATE:
#ifdef UNBRANDED
        sprintf_s(text, "Overlay v%s - up to date", TAKEPEEK_VERSION);
#else
        sprintf_s(text, "TakePeek WD v%s - up to date", TAKEPEEK_VERSION);
#endif
        borderCol = IM_COL32(80, 80, 100, 80);
        break;
    case g_update.FAILED:
        sprintf_s(text, "Update: %s", g_update.err);
        borderCol = IM_COL32(200, 50, 50, 120);
        break;
    }

    dl->AddRect({x, y}, {x + barW, y + barH}, borderCol, 6.f);
    ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText({x + (barW - ts.x) * 0.5f, y + (barH - ts.y) * 0.5f},
        IM_COL32(255, 255, 255, 220), text);
}

// ============================================================================
// Overlay window + DX11
// ============================================================================
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 0;
    switch (msg) {
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_SIZE:
            if (g_device && g_swapchain && wParam != SIZE_MINIMIZED) {
                if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
                HRESULT hr = g_swapchain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                if (SUCCEEDED(hr)) {
                    ID3D11Texture2D* buf = nullptr;
                    g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
                    if (buf) { g_device->CreateRenderTargetView(buf, nullptr, &g_rtv); buf->Release(); }
                }
            }
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Random class name storage (must outlive the window)
static std::wstring g_overlayClassName;
static std::wstring g_authClassName;
static std::wstring g_waitClassName;
static std::wstring g_updateClassName;

static bool InitOverlay() {
    g_overlayClassName = Stealth::RandomClassName();
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0, 0, GetModuleHandleW(nullptr),
        nullptr, nullptr, nullptr, nullptr, g_overlayClassName.c_str(), nullptr};
    RegisterClassExW(&wc);
    g_screenW = GetSystemMetrics(SM_CXSCREEN);
    g_screenH = GetSystemMetrics(SM_CYSCREEN);
#ifdef UNBRANDED
    std::wstring overlayTitle = L"Overlay";
#else
    std::wstring overlayTitle = XSW(L"TakePeek WD");
#endif
    g_overlayWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName, overlayTitle.c_str(), WS_POPUP,
        0, 0, g_screenW, g_screenH, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_overlayWnd) return false;
    SetLayeredWindowAttributes(g_overlayWnd, 0, 255, LWA_ALPHA);
    MARGINS m = {-1}; DwmExtendFrameIntoClientArea(g_overlayWnd, &m);
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = g_screenW; sd.BufferDesc.Height = g_screenH;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_overlayWnd;
    sd.SampleDesc = {1, 0}; sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    HRESULT hrInit = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, &fl, &g_ctx);
    if (FAILED(hrInit)) {
        WriteStartupLog("DX11", "HW init failed, trying WARP");
        hrInit = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, &fl, &g_ctx);
        if (FAILED(hrInit)) return false;
        WriteStartupLog("DX11", "using WARP fallback");
    }
    ID3D11Texture2D* buf = nullptr;
    g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
    g_device->CreateRenderTargetView(buf, nullptr, &g_rtv);
    buf->Release();
    ShowWindow(g_overlayWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_overlayWnd);
    return true;
}

static void CleanupOverlay() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_soldierSRV) { g_soldierSRV->Release(); g_soldierSRV = nullptr; }
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    if (g_swapchain) { g_swapchain->Release(); g_swapchain = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
    if (g_overlayWnd) DestroyWindow(g_overlayWnd);
}

// ============================================================================
// Auth window
// ============================================================================
static HWND g_authWnd = nullptr;
static bool g_authPhaseActive = false;
static LRESULT WINAPI AuthWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 0;
    if (msg == WM_DESTROY) {
        if (g_authPhaseActive) PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
HWND GetAuthWindow() { return g_authWnd ? g_authWnd : g_overlayWnd; }

static void EarlyLog(const char* msg);

static bool RunUpdatePhase() {
    EarlyLog("Update: starting pre-auth update check");
    g_updateClassName = Stealth::RandomClassName();
    WNDCLASSEXW uwc{sizeof(uwc), CS_CLASSDC, AuthWndProc, 0, 0, GetModuleHandleW(nullptr),
        nullptr, nullptr, nullptr, nullptr, g_updateClassName.c_str(), nullptr};
    RegisterClassExW(&uwc);
#ifdef UNBRANDED
    std::wstring updateTitle = L"Overlay";
#else
    std::wstring updateTitle = XSW(L"TakePeek WD");
#endif
    HWND uWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED,
        uwc.lpszClassName, updateTitle.c_str(), WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN)-480)/2, (GetSystemMetrics(SM_CYSCREEN)-300)/2,
        480, 300, nullptr, nullptr, uwc.hInstance, nullptr);
    if (!uWnd) { EarlyLog("Update: window creation failed, skipping"); return true; }
    SetLayeredWindowAttributes(uWnd, 0, 255, LWA_ALPHA);
    MARGINS um = {-1}; DwmExtendFrameIntoClientArea(uWnd, &um);

    DXGI_SWAP_CHAIN_DESC usd{};
    usd.BufferCount = 2; usd.BufferDesc.Width = 480; usd.BufferDesc.Height = 300;
    usd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    usd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    usd.OutputWindow = uWnd; usd.SampleDesc = {1, 0};
    usd.Windowed = TRUE; usd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* udev = nullptr; ID3D11DeviceContext* udc = nullptr;
    IDXGISwapChain* usc = nullptr; ID3D11RenderTargetView* urtv = nullptr;
    D3D_FEATURE_LEVEL ufl;
    HRESULT uhr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &usd, &usc, &udev, &ufl, &udc);
    if (FAILED(uhr)) {
        uhr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &usd, &usc, &udev, &ufl, &udc);
        if (FAILED(uhr)) {
            EarlyLog("Update: DX11 init failed, skipping update check");
            DestroyWindow(uWnd); return true;
        }
    }
    ID3D11Texture2D* ubuf = nullptr;
    usc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&ubuf);
    udev->CreateRenderTargetView(ubuf, nullptr, &urtv); ubuf->Release();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& uio = ImGui::GetIO(); uio.IniFilename = nullptr;
    {
        ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 1; cfg.PixelSnapH = true;
        ImFont* mf = uio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 15.0f, &cfg);
        if (!mf) mf = uio.Fonts->AddFontDefault(&cfg);
        ImFontConfig hcfg = cfg; hcfg.SizePixels = 18.0f;
        if (!uio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 18.0f, &hcfg))
            uio.Fonts->AddFontDefault(&hcfg);
        uio.Fonts->Build();
        if (mf) uio.FontDefault = mf;
    }
    ImGui_ImplWin32_Init(uWnd);
    ImGui_ImplDX11_Init(udev, udc);
    ShowWindow(uWnd, SW_SHOWDEFAULT);

    StartUpdateCheck();
    DWORD updateStart = GetTickCount();
    const DWORD UPDATE_TIMEOUT = 15000;
    bool updateDone = false;
    bool shouldRestart = false;

    while (!updateDone && g_running) {
        MSG umsg;
        while (PeekMessageW(&umsg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&umsg); DispatchMessageW(&umsg);
            if (umsg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;

        int st = g_update.state.load();
        if (st == g_update.UP_TO_DATE || st == g_update.FAILED || st == g_update.APPLYING)
            updateDone = true;
        if (st == g_update.READY)
            updateDone = true;
        if (GetTickCount() - updateStart > UPDATE_TIMEOUT && st == g_update.CHECKING) {
            strcpy_s(g_update.err, "Update check timed out");
            g_update.state.store(g_update.FAILED);
            updateDone = true;
        }

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();

        float W = 480.f, H = 300.f;
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({W, H});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(16, 16, 20, 245));
        ImGui::Begin("##updater", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float cx = wp.x + W / 2.f;

#ifdef UNBRANDED
        const char* title = "OVERLAY";
#else
        const char* title = "TAKEPEEK WD";
#endif
        ImFont* hf = uio.Fonts->Fonts.Size > 1 ? uio.Fonts->Fonts[1] : ImGui::GetFont();
        ImVec2 ts = hf->CalcTextSizeA(22.f, FLT_MAX, 0, title);
        dl->AddText(hf, 22.f, {cx - ts.x/2, wp.y + 40}, WD_ACCENT(255), title);
        dl->AddLine({wp.x + 40, wp.y + 75}, {wp.x + W - 40, wp.y + 75}, WD_ACCENT(100), 1.f);

        char statusBuf[256] = {};
        ImU32 statusCol = IM_COL32(200, 200, 210, 255);
        float progressPct = -1.f;

        switch (st) {
        case g_update.IDLE:
        case g_update.CHECKING:
            strcpy_s(statusBuf, "Checking for updates...");
            break;
        case g_update.DOWNLOADING: {
            int pct = g_update.pct.load();
            sprintf_s(statusBuf, "Downloading v%s... %d%%", g_update.newVer, pct);
            statusCol = IM_COL32(100, 200, 255, 255);
            progressPct = pct / 100.f;
            break;
        }
        case g_update.READY:
            sprintf_s(statusBuf, "Update v%s downloaded - restarting...", g_update.newVer);
            statusCol = IM_COL32(50, 255, 50, 255);
            break;
        case g_update.APPLYING:
            strcpy_s(statusBuf, "Installing update...");
            statusCol = IM_COL32(50, 255, 50, 255);
            break;
        case g_update.UP_TO_DATE:
            sprintf_s(statusBuf, "You are running the latest version (v%s)", TAKEPEEK_VERSION);
            statusCol = IM_COL32(100, 200, 100, 255);
            break;
        case g_update.FAILED:
            sprintf_s(statusBuf, "Update check failed: %s", g_update.err);
            statusCol = IM_COL32(255, 80, 80, 255);
            break;
        }

        ImVec2 ss = ImGui::GetFont()->CalcTextSizeA(15.f, FLT_MAX, 0, statusBuf);
        dl->AddText(ImGui::GetFont(), 15.f, {cx - ss.x/2, wp.y + 140}, statusCol, statusBuf);

        if (progressPct >= 0.f) {
            float barW = W - 120.f, barH2 = 8.f;
            float bx = wp.x + 60.f, by = wp.y + 180.f;
            dl->AddRectFilled({bx, by}, {bx + barW, by + barH2}, IM_COL32(40, 40, 50, 200), 4.f);
            dl->AddRectFilled({bx, by}, {bx + barW * progressPct, by + barH2}, WD_ACCENT(255), 4.f);
        }

        if ((st == g_update.DOWNLOADING || st == g_update.READY) && g_update.changelog[0]) {
            char clBuf[256];
            sprintf_s(clBuf, "Changelog: %s", g_update.changelog);
            ImVec2 cs = ImGui::GetFont()->CalcTextSizeA(12.f, FLT_MAX, 0, clBuf);
            dl->AddText(ImGui::GetFont(), 12.f, {cx - cs.x/2, wp.y + 210}, IM_COL32(150, 150, 160, 200), clBuf);
        }

        char verBuf[64];
        sprintf_s(verBuf, "v%s", TAKEPEEK_VERSION);
        ImVec2 vs2 = ImGui::GetFont()->CalcTextSizeA(11.f, FLT_MAX, 0, verBuf);
        dl->AddText(ImGui::GetFont(), 11.f, {cx - vs2.x/2, wp.y + H - 35}, IM_COL32(80, 80, 90, 180), verBuf);

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::Render();
        const float uclr[4] = {0, 0, 0, 0};
        udc->OMSetRenderTargets(1, &urtv, nullptr);
        udc->ClearRenderTargetView(urtv, uclr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        usc->Present(1, 0);

        if (updateDone && st == g_update.UP_TO_DATE) Sleep(1000);
        if (updateDone && st == g_update.FAILED) Sleep(1500);
        if (updateDone && st == g_update.READY) Sleep(1500);
    }

    if (g_update.state.load() == g_update.READY) {
        Sleep(500);
        if (ApplyUpdate()) {
            shouldRestart = true;
        }
    }

    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (urtv) urtv->Release(); if (usc) usc->Release();
    if (udc) udc->Release(); if (udev) udev->Release();
    DestroyWindow(uWnd);
    MSG qm;
    while (PeekMessageW(&qm, nullptr, 0, 0, PM_REMOVE)) {}

    if (shouldRestart) {
        EarlyLog("Update: restarting with new version");
        return false;
    }
    EarlyLog("Update: done, proceeding to auth");
    return true;
}

static bool RunAuthPhase() {
    g_authPhaseActive = true;
    EarlyLog("Auth: creating window...");
    g_authClassName = Stealth::RandomClassName();
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, AuthWndProc, 0, 0, GetModuleHandleW(nullptr),
        nullptr, nullptr, nullptr, nullptr, g_authClassName.c_str(), nullptr};
    RegisterClassExW(&wc);
#ifdef UNBRANDED
    std::wstring authTitle = L"Overlay";
#else
    std::wstring authTitle = XSW(L"TakePeek WD Auth");
#endif
    g_authWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED,
        wc.lpszClassName, authTitle.c_str(), WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN)-560)/2, (GetSystemMetrics(SM_CYSCREEN)-360)/2,
        560, 360, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_authWnd) { EarlyLog("Auth: CreateWindowEx FAILED"); return false; }
    SetLayeredWindowAttributes(g_authWnd, 0, 255, LWA_ALPHA);
    MARGINS m = {-1}; DwmExtendFrameIntoClientArea(g_authWnd, &m);
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2; sd.BufferDesc.Width = 560; sd.BufferDesc.Height = 360;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_authWnd; sd.SampleDesc = {1, 0};
    sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* dev = nullptr; ID3D11DeviceContext* dc = nullptr;
    IDXGISwapChain* sc = nullptr; ID3D11RenderTargetView* rtv = nullptr;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &dc);
    if (FAILED(hr)) {
        char hrbuf[64]; sprintf_s(hrbuf, "Auth: HW DX11 failed 0x%08X, trying WARP", (unsigned)hr);
        EarlyLog(hrbuf);
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &sd, &sc, &dev, &fl, &dc);
        if (FAILED(hr)) {
            sprintf_s(hrbuf, "Auth: WARP DX11 also FAILED hr=0x%08X", (unsigned)hr);
            EarlyLog(hrbuf); return false;
        }
        EarlyLog("Auth: DX11 init OK (WARP fallback)");
    } else {
        EarlyLog("Auth: DX11 init OK");
    }
    ID3D11Texture2D* buf = nullptr;
    sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
    dev->CreateRenderTargetView(buf, nullptr, &rtv); buf->Release();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& aio = ImGui::GetIO();
    aio.IniFilename = nullptr;
    {
        ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 1; cfg.PixelSnapH = true;
        ImFont* mf = aio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 15.0f, &cfg);
        if (!mf) mf = aio.Fonts->AddFontDefault(&cfg);
        ImFontConfig hcfg = cfg; hcfg.SizePixels = 18.0f;
        ImFont* hf = aio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 18.0f, &hcfg);
        if (!hf) aio.Fonts->AddFontDefault(&hcfg);
        ImFontConfig scfg = cfg; scfg.SizePixels = 12.0f;
        ImFont* sf = aio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 12.0f, &scfg);
        if (!sf) aio.Fonts->AddFontDefault(&scfg);
        aio.Fonts->Build();
        if (mf) aio.FontDefault = mf;
    }
    ImGui_ImplWin32_Init(g_authWnd);
    ImGui_ImplDX11_Init(dev, dc);
    ShowWindow(g_authWnd, SW_SHOWDEFAULT);
    EarlyLog("Auth: window visible — calling InitAuth");
    InitAuth();
    EarlyLog("Auth: entering auth loop");
    MSG msg;
    while (g_running && !IsAuthenticated()) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if (!g_running) break;
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        DrawAuthWindow();
        ImGui::Render();
        float clear[4] = {0,0,0,0};
        dc->OMSetRenderTargets(1, &rtv, nullptr);
        dc->ClearRenderTargetView(rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        sc->Present(1, 0);
    }
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (rtv) rtv->Release(); if (sc) sc->Release();
    if (dc) dc->Release(); if (dev) dev->Release();
    g_authPhaseActive = false;
    DestroyWindow(g_authWnd); g_authWnd = nullptr;
    MSG qmsg;
    while (PeekMessageW(&qmsg, nullptr, 0, 0, PM_REMOVE)) {}
    g_running = true;
    return IsAuthenticated();
}

// ============================================================================
// Waiting-for-game screen with step indicators
// ============================================================================
static void DrawWaitingScreen(float animTime) {
    const float W = 480.f, H = 300.f;
    ImGui::SetNextWindowSize({W, H}, ImGuiCond_Always);
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.086f, 0.086f, 0.11f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.19f, 0.19f, 0.24f, 1.f));
    ImGui::Begin("##waitwd", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    if (ImGui::IsMouseClicked(0)) {
        ImVec2 mp = ImGui::GetMousePos();
        if (mp.y >= wp.y && mp.y <= wp.y + 50.f && mp.x >= wp.x && mp.x <= wp.x + W - 50.f) {
            ReleaseCapture();
            SendMessageW(GetAuthWindow(), WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
    }

    // Window controls
    ImGui::SetCursorScreenPos({wp.x + W - 60, wp.y + 10});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.25f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ACCENT_Rf,ACCENT_Gf,ACCENT_Bf,0.4f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f,0.45f,0.5f,1));
    if (ImGui::Button("-##wmin", {22,22})) ShowWindow(GetAuthWindow(), SW_MINIMIZE);
    ImGui::SameLine(0, 4);
    if (ImGui::Button("x##wcls", {22,22})) { g_running = false; PostMessage(GetAuthWindow(), WM_CLOSE, 0, 0); }
    ImGui::PopStyleColor(4);

    float cx = wp.x + W * 0.5f;
#ifndef UNBRANDED
    DrawTakePeekBrand(dl, {cx - 105, wp.y + 16}, 0.85f, false);
#endif

    // 3-step progress indicator
    float stepY = wp.y + 110.f, stepSpacing = 120.f, startX = cx - stepSpacing;
    const char* stepLabels[] = {"Authenticated", "Scanning", "Attaching"};
    int currentStep = 1;

    for (int s = 0; s < 3; s++) {
        float sx = startX + s * stepSpacing;
        bool done = (s < currentStep), active = (s == currentStep);
        float boxSz = 44.f;
        ImU32 boxBg = done ? WD_ACCENT(40) : (active ? WD_ACCENT(25) : IM_COL32(34, 34, 44, 255));
        ImU32 boxBrd = done ? WD_ACCENT(200) : (active ? WD_ACCENT(140) : IM_COL32(60, 60, 74, 255));
        dl->AddRectFilled({sx-boxSz/2,stepY-boxSz/2},{sx+boxSz/2,stepY+boxSz/2}, boxBg, 8.f);
        dl->AddRect({sx-boxSz/2,stepY-boxSz/2},{sx+boxSz/2,stepY+boxSz/2}, boxBrd, 8.f, 0, 1.5f);
        ImU32 iconCol = done ? WD_ACCENT(255) : (active ? WD_ACCENT(200) : IM_COL32(100, 100, 112, 255));
        if (done) {
            dl->AddLine({sx-8,stepY},{sx-2,stepY+6}, iconCol, 2.5f);
            dl->AddLine({sx-2,stepY+6},{sx+10,stepY-6}, iconCol, 2.5f);
        } else if (active) {
            float pulse = 0.5f + 0.5f * sinf(animTime * 3.f);
            dl->AddCircleFilled({sx,stepY}, 5.f + pulse * 2.f, WD_ACCENT((int)(140+pulse*115)), 16);
        } else {
            dl->AddCircle({sx,stepY}, 5.f, iconCol, 16, 1.5f);
        }
        ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(11.f, FLT_MAX, 0, stepLabels[s]);
        dl->AddText(ImGui::GetFont(), 11.f, {sx-ts.x/2, stepY+boxSz/2+8},
            done ? WD_ACCENT(200) : (active ? IM_COL32(220,220,228,255) : IM_COL32(100,100,112,255)),
            stepLabels[s]);
        if (s < 2) {
            float lx1 = sx + boxSz/2 + 6, lx2 = sx + stepSpacing - boxSz/2 - 6;
            ImU32 lineCol = done ? WD_ACCENT(120) : IM_COL32(60, 60, 74, 180);
            for (float dx = lx1; dx < lx2; dx += 13.f)
                dl->AddLine({dx, stepY}, {(dx+8.f<lx2?dx+8.f:lx2), stepY}, lineCol, 1.5f);
        }
    }

    ImFont* sfont = ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont();
#ifdef UNBRANDED
    const char* statusTxt = "Waiting for game...";
#else
    const char* statusTxt = "Waiting for WarDogs...";
#endif
    ImVec2 sts = sfont->CalcTextSizeA(14.f, FLT_MAX, 0, statusTxt);
    dl->AddText(sfont, 14.f, {cx-sts.x/2, stepY+60}, IM_COL32(180,180,190,255), statusTxt);
#ifdef UNBRANDED
    const char* subTxt = "Open the game if not already running  |  Press INSERT in-game";
#else
    const char* subTxt = "Open WarDogs if not already running  |  Press INSERT in-game";
#endif
    ImVec2 sub = ImGui::GetFont()->CalcTextSizeA(11.f, FLT_MAX, 0, subTxt);
    dl->AddText(ImGui::GetFont(), 11.f, {cx-sub.x/2, stepY+80}, IM_COL32(100,100,112,255), subTxt);

    // Bottom info
    char bottomTxt[128]; sprintf_s(bottomTxt, "Licensed  |  %s", GetLicenseDuration());
    float padX = 24.f;
    dl->AddLine({wp.x+padX, wp.y+H-36}, {wp.x+W-padX, wp.y+H-36}, IM_COL32(50,50,64,80), 1.f);
    ImVec2 bt = ImGui::GetFont()->CalcTextSizeA(11.f, FLT_MAX, 0, bottomTxt);
    float textX = (bt.x < W-padX*2) ? (cx - bt.x/2) : (wp.x + padX);
    dl->AddText(ImGui::GetFont(), 11.f, {textX, wp.y+H-24}, IM_COL32(100,100,112,200), bottomTxt);

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

// ============================================================================
// Entry point
// ============================================================================
// Early startup log — writes to exe directory, available before game is found
static void EarlyLog(const char* msg) {
    char logPath[MAX_PATH];
    GetModuleFileNameA(nullptr, logPath, MAX_PATH);
    std::string lp(logPath);
#ifdef UNBRANDED
    lp = lp.substr(0, lp.find_last_of("\\/") + 1) + "Overlay_Startup.log";
#else
    lp = lp.substr(0, lp.find_last_of("\\/") + 1) + "TakePeek_Startup.log";
#endif
    std::ofstream lf(lp, std::ios::app);
    if (lf.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char tb[64]; ctime_s(tb, sizeof(tb), &t);
        tb[strlen(tb)-1] = '\0';
        lf << "[" << tb << "] " << msg << std::endl;
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
#ifdef UNBRANDED
    EarlyLog("=== Overlay starting ===");
#else
    EarlyLog("=== TakePeek WD starting ===");
#endif
    CleanupOldUpdate();
    DynAPI::Init();
    EarlyLog("DynAPI initialized");
#ifndef DEV_AUTH_STUB
    if (Stealth::IsBeingDebugged()) { EarlyLog("ABORT: debugger detected"); return 1; }
    if (Stealth::TimingCheck()) { EarlyLog("ABORT: timing check failed"); return 1; }
    EarlyLog("Pre-auth checks passed");
#else
    EarlyLog("DEV_AUTH_STUB build — protection skipped");
#endif

    if (!RunUpdatePhase()) { EarlyLog("Restarting for update"); return 0; }

    if (!RunAuthPhase()) { EarlyLog("Auth phase cancelled/failed"); return 0; }
    EarlyLog("Auth passed — waiting for WardogsClient-Win64-Shipping.exe");

#ifndef DEV_AUTH_STUB
    EarlyLog("Stealth: anti-debug + timing passed, skipping PE erasure (breaks DX11)");
#endif

    g_config.Init();

    // Waiting-for-game phase with visual UI
    {
        g_waitClassName = Stealth::RandomClassName();
        WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, AuthWndProc, 0, 0, GetModuleHandleW(nullptr),
            nullptr, nullptr, nullptr, nullptr, g_waitClassName.c_str(), nullptr};
        RegisterClassExW(&wc);
#ifdef UNBRANDED
        std::wstring waitTitle = L"Overlay";
#else
        std::wstring waitTitle = XSW(L"TakePeek WD");
#endif
        g_authWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED,
            wc.lpszClassName, waitTitle.c_str(), WS_POPUP,
            (GetSystemMetrics(SM_CXSCREEN)-480)/2, (GetSystemMetrics(SM_CYSCREEN)-300)/2,
            480, 300, nullptr, nullptr, wc.hInstance, nullptr);
        if (g_authWnd) {
            EarlyLog("Waiting window created OK");
            SetLayeredWindowAttributes(g_authWnd, 0, 255, LWA_ALPHA);
            MARGINS m = {-1}; DwmExtendFrameIntoClientArea(g_authWnd, &m);
            DXGI_SWAP_CHAIN_DESC sd{};
            sd.BufferCount = 2; sd.BufferDesc.Width = 480; sd.BufferDesc.Height = 300;
            sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = g_authWnd; sd.SampleDesc = {1, 0};
            sd.Windowed = TRUE; sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            ID3D11Device* wdev = nullptr; ID3D11DeviceContext* wdc = nullptr;
            IDXGISwapChain* wsc = nullptr; ID3D11RenderTargetView* wrtv = nullptr;
            D3D_FEATURE_LEVEL wfl;
            HRESULT whr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                nullptr, 0, D3D11_SDK_VERSION, &sd, &wsc, &wdev, &wfl, &wdc);
            if (FAILED(whr))
                whr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                    nullptr, 0, D3D11_SDK_VERSION, &sd, &wsc, &wdev, &wfl, &wdc);
            if (SUCCEEDED(whr)) {
                EarlyLog("Waiting window DX11 init OK — entering visual wait loop");
                ID3D11Texture2D* wbuf = nullptr;
                wsc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&wbuf);
                wdev->CreateRenderTargetView(wbuf, nullptr, &wrtv); wbuf->Release();
                IMGUI_CHECKVERSION(); ImGui::CreateContext();
                ImGuiIO& wio = ImGui::GetIO();
                wio.IniFilename = nullptr;
                {
                    ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 1; cfg.PixelSnapH = true;
                    ImFont* mf = wio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 15.0f, &cfg);
                    if (!mf) mf = wio.Fonts->AddFontDefault(&cfg);
                    ImFontConfig hcfg = cfg; hcfg.SizePixels = 18.0f;
                    if (!wio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 18.0f, &hcfg))
                        wio.Fonts->AddFontDefault(&hcfg);
                    ImFontConfig scfg = cfg; scfg.SizePixels = 12.0f;
                    if (!wio.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 12.0f, &scfg))
                        wio.Fonts->AddFontDefault(&scfg);
                    wio.Fonts->Build();
                    if (mf) wio.FontDefault = mf;
                }
                ImGui_ImplWin32_Init(g_authWnd); ImGui_ImplDX11_Init(wdev, wdc);
                ShowWindow(g_authWnd, SW_SHOWDEFAULT);

                // Update check now runs before auth in RunUpdatePhase()

                float waitAnim = 0.f;
                int waitTicks = 0;
                MSG msg;
                while (g_running) {
                    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                        if (msg.message == WM_QUIT) { g_running = false; break; }
                        TranslateMessage(&msg); DispatchMessageW(&msg);
                    }
                    if (!g_running) break;
                    waitTicks++;
                    if (waitTicks <= 3) EarlyLog(("Loop tick " + std::to_string(waitTicks)).c_str());
                    int crashes = 0;
                    g_pid = SafeFindProcess(XSW(L"WardogsClient-Win64-Shipping.exe").c_str(), &crashes);
                    if (crashes && waitTicks <= 3) {
                        std::string cm = "Search crash mask=" + std::to_string(crashes) +
                            (crashes&1?" [Toolhelp]":"") + (crashes&2?" [Window]":"") + (crashes&4?" [NtQuery]":"");
                        EarlyLog(cm.c_str());
                    }
                    if (g_pid) { EarlyLog(("Game found! PID=" + std::to_string(g_pid)).c_str()); break; }
                    if (waitTicks % 300 == 1) EarlyLog("Still searching...");
                    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
                    waitAnim += ImGui::GetIO().DeltaTime;
                    DrawWaitingScreen(waitAnim);
                    ImGui::Render();
                    float clear[4] = {0,0,0,0};
                    wdc->OMSetRenderTargets(1, &wrtv, nullptr);
                    wdc->ClearRenderTargetView(wrtv, clear);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                    wsc->Present(1, 0);
                }
                ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
                if (wrtv) wrtv->Release(); if (wsc) wsc->Release();
                if (wdc) wdc->Release(); if (wdev) wdev->Release();
            } else {
                EarlyLog("Waiting window DX11 FAILED — using headless wait loop");
                DestroyWindow(g_authWnd); g_authWnd = nullptr;
                while (g_running) {
                    g_pid = SafeFindProcess(XSW(L"WardogsClient-Win64-Shipping.exe").c_str(), nullptr);
                    if (g_pid) { EarlyLog(("Game found! PID=" + std::to_string(g_pid)).c_str()); break; }
                    Stealth::JitteredSleep(1000, 200);
                }
            }
            if (g_authWnd) { DestroyWindow(g_authWnd); g_authWnd = nullptr; }
        } else {
            EarlyLog("Waiting window creation FAILED — using headless wait loop");
            while (g_running) {
                g_pid = SafeFindProcess(XSW(L"WardogsClient-Win64-Shipping.exe").c_str(), nullptr);
                if (g_pid) { EarlyLog(("Game found! PID=" + std::to_string(g_pid)).c_str()); break; }
                Stealth::JitteredSleep(1000, 200);
            }
        }
    }
    if (!g_pid || !g_running) return 0;

    // WriteStartupLog is now a static function defined above PatternScan

    WriteStartupLog("Game found", (std::string("PID=") + std::to_string(g_pid)).c_str());

    // Load BYOVD driver for physical memory access
    {
        DriverLoader::DisableVulnerableDriverBlocklist();

        std::string byovdStatus;
        g_byovdActive = GdrvMapper::LoadBYOVDOnly(Memory::g_sysCtx, g_vulnState, byovdStatus);
        WriteStartupLog(g_byovdActive ? "BYOVD loaded" : "BYOVD failed", byovdStatus.c_str());

        DriverLoader::RestoreVulnerableDriverBlocklist();

        if (g_byovdActive) {
            g_ntoskrnlBase = (DWORD64)GdrvMapper::GetKernelModuleAddress(XS("ntoskrnl.exe").c_str());
            WriteStartupLog("Physical R/W ready", ("ntoskrnl=0x" + ([&]{ char b[32]; sprintf_s(b, "%llX", (unsigned long long)g_ntoskrnlBase); return std::string(b); })()).c_str());
        }
    }

    // Attach to game via physical memory or usermode fallback
    if (g_byovdActive && g_ntoskrnlBase) {
        DWORD64 eprocess = GdrvMapper::FindEPROCESS(Memory::g_sysCtx, g_ntoskrnlBase, g_pid);
        if (eprocess) {
            WriteStartupLog("EPROCESS found", ("0x" + ([&]{ char b[32]; sprintf_s(b, "%llX", (unsigned long long)eprocess); return std::string(b); })()).c_str());
            DWORD64 gameCR3 = GdrvMapper::GetProcessCR3(Memory::g_sysCtx, eprocess);
            DWORD64 pebAddr = GdrvMapper::GetPebAddress(Memory::g_sysCtx, eprocess);

            if (gameCR3) {
                KernelRW::Context gameCtx = KernelRW::MakeContextWithCR3(Memory::g_sysCtx, gameCR3);
                uintptr_t moduleBase = 0;
                if (pebAddr) {
                    moduleBase = (uintptr_t)GdrvMapper::GetModuleBaseFromPEB(gameCtx, pebAddr);
                }
                if (moduleBase && GdrvMapper::ValidateCR3(Memory::g_sysCtx, gameCR3, (DWORD64)moduleBase)) {
                    size_t moduleSize = Memory::ReadModuleSizeFromPE(moduleBase);
                    Memory::AttachPhysicalDirect(g_pid, gameCtx, moduleBase, moduleSize);
                    g_base = moduleBase;
                    WriteStartupLog("Physical attach OK", ("base=0x" + ([&]{ char b[32]; sprintf_s(b, "%llX", (unsigned long long)g_base); return std::string(b); })()).c_str());
                } else {
                    WriteStartupLog("Physical attach failed — CR3 validation failed, trying usermode");
                }
            } else {
                WriteStartupLog("GetProcessCR3 failed — trying usermode");
            }
        } else {
            WriteStartupLog("FindEPROCESS failed — trying usermode");
        }
    }

    // Usermode RPM fallback
    if (!Memory::g_usePhysRW) {
        if (!DynAPI::pOpenProcess) { WriteStartupLog("FATAL: pOpenProcess is NULL"); return 0; }
        g_proc = DynAPI::pOpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, g_pid);
        if (!g_proc) {
            WriteStartupLog("FATAL: OpenProcess failed", (std::string("GetLastError=") + std::to_string(GetLastError())).c_str());
            return 0;
        }
        WriteStartupLog("OpenProcess OK");
        std::string toolhelpDiag, pebDiag;
        g_base = GetModuleBase(g_pid, XSW(L"WardogsClient-Win64-Shipping.exe").c_str(), &toolhelpDiag);
        if (!g_base) {
            WriteStartupLog("GetModuleBase (toolhelp) failed", toolhelpDiag.c_str());
            WriteStartupLog("Trying PEB fallback...");
            g_base = GetModuleBasePEB(g_proc, &pebDiag);
        }
        if (!g_base) {
            WriteStartupLog("PEB fallback failed", pebDiag.c_str());
            WriteStartupLog("FATAL: GetModuleBase failed — both methods failed.");
            CloseHandle(g_proc); return 0;
        }
        Memory::g_baseAddress = g_base;
        Memory::g_pid = g_pid;
        Memory::g_attachMethod = 2;
        WriteStartupLog("Usermode base found", (std::string("0x") + ([&]{ char b[32]; sprintf_s(b, "%llX", (unsigned long long)g_base); return std::string(b); })()).c_str());
    }
    // Verify kernel reads work by reading PE header
    {
        uint16_t mz = Read<uint16_t>(g_base);
        bool readOk = (mz == 0x5A4D);
        WriteStartupLog(readOk ? "Read verify: MZ header OK" : "Read verify: FAILED (reads broken!)",
            (std::string("got 0x") + ([&]{ char b[16]; sprintf_s(b, "%04X", mz); return std::string(b); })()).c_str());

        if (readOk) {
            int32_t peOff = Read<int32_t>(g_base + 0x3C);
            uint32_t imgSize = Read<uint32_t>(g_base + peOff + 0x50);
            WriteStartupLog("Image size",
                (std::string("0x") + ([&]{ char b[16]; sprintf_s(b, "%X", imgSize); return std::string(b); })()
                + (g_gworldOff < imgSize ? " (GWorld in range)" : " *** GWorld OUT OF RANGE ***")).c_str());
        }
    }

    if (AutoResolveGlobals()) {
        char msg[256];
        sprintf_s(msg, "GWorld=0x%llX GNames=0x%llX", (unsigned long long)g_gworldOff, (unsigned long long)g_gnamesOff);
        WriteStartupLog("AutoResolve updated offsets", msg);
    } else {
        char msg[256];
        sprintf_s(msg, "using defaults GWorld=0x%llX GNames=0x%llX", (unsigned long long)g_gworldOff, (unsigned long long)g_gnamesOff);
        WriteStartupLog("AutoResolve no changes", msg);
    }

    WriteStartupLog("Waiting for game to initialize (window + GWorld)...");
    {
        const int MAX_WAIT_SEC = 120;
        const int POLL_MS = 2000;
        int elapsed = 0;
        bool gworldOk = false;

        while (elapsed < MAX_WAIT_SEC * 1000 && g_running) {
            if (!g_gameWnd || !IsWindow(g_gameWnd))
                g_gameWnd = FindGameWindow();

            uintptr_t testWorld = Read<uintptr_t>(g_base + g_gworldOff);
            if (testWorld && testWorld > 0x10000 && testWorld < 0x7FFFFFFFFFFF) {
                uintptr_t level = Read<uintptr_t>(testWorld + 0x30);
                if (level && level > 0x10000 && level < 0x7FFFFFFFFFFF) {
                    char msg[256];
                    sprintf_s(msg, "GWorld live at +0x%llX = 0x%llX (level=0x%llX) after %ds",
                        (unsigned long long)g_gworldOff, (unsigned long long)testWorld,
                        (unsigned long long)level, elapsed / 1000);
                    WriteStartupLog("Game initialized", msg);
                    gworldOk = true;
                    break;
                }
            }

            Sleep(POLL_MS);
            elapsed += POLL_MS;

            if ((elapsed % 10000) == 0) {
                char msg[128];
                sprintf_s(msg, "still waiting... %ds elapsed, wnd=%s, gworld=0x%llX",
                    elapsed / 1000, g_gameWnd ? "YES" : "NO",
                    (unsigned long long)Read<uintptr_t>(g_base + g_gworldOff));
                WriteStartupLog("Init wait", msg);
            }
        }

        WriteStartupLog(g_gameWnd ? "Game window found" : "WARNING: Game window NOT found (UnrealWindow)");

        if (!gworldOk) {
            WriteStartupLog("GWorld not ready yet — main loop will wait for level to load");
        }
    }

    RunDiagnostics();
    WriteStartupLog("Diagnostics dump written");

    if (!InitOverlay()) { WriteStartupLog("FATAL: InitOverlay failed — DX11/window creation error"); CloseHandle(g_proc); return 0; }
    WriteStartupLog("Overlay initialized — entering main loop");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(g_overlayWnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);
    CreateSoldierTexture();
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 2;
    fontCfg.OversampleV = 1;
    fontCfg.PixelSnapH = true;

    ImFontConfig mainFontCfg = fontCfg;
    mainFontCfg.SizePixels = 15.0f;
    ImFont* mainFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 15.0f, &mainFontCfg);
    if (!mainFont) mainFont = io.Fonts->AddFontDefault(&mainFontCfg);

    ImFontConfig headerFontCfg = fontCfg;
    headerFontCfg.SizePixels = 18.0f;
    ImFont* headerFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 18.0f, &headerFontCfg);
    if (!headerFont) headerFont = io.Fonts->AddFontDefault(&headerFontCfg);

    ImFontConfig smallFontCfg = fontCfg;
    smallFontCfg.SizePixels = 12.0f;
    ImFont* smallFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 12.0f, &smallFontCfg);
    if (!smallFont) smallFont = io.Fonts->AddFontDefault(&smallFontCfg);

    io.Fonts->Build();
    if (mainFont) io.FontDefault = mainFont;

    // Dark theme
    {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding    = 10.f;
        s.ChildRounding     = 8.f;
        s.FrameRounding     = 5.f;
        s.PopupRounding     = 6.f;
        s.ScrollbarRounding = 8.f;
        s.GrabRounding      = 4.f;
        s.TabRounding       = 5.f;
        s.WindowBorderSize  = 1.f;
        s.FrameBorderSize   = 0.f;
        s.ChildBorderSize   = 1.f;
        s.WindowPadding     = ImVec2(10, 10);
        s.FramePadding      = ImVec2(8, 4);
        s.ItemSpacing       = ImVec2(8, 6);
        s.ItemInnerSpacing  = ImVec2(6, 4);
        s.ScrollbarSize     = 10.f;
        s.GrabMinSize       = 8.f;

        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg]             = ImVec4(0.078f, 0.078f, 0.094f, 0.97f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.090f, 0.086f, 0.106f, 0.9f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.090f, 0.090f, 0.114f, 0.98f);
        c[ImGuiCol_Border]               = ImVec4(0.160f, 0.160f, 0.210f, 0.5f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg]              = ImVec4(0.114f, 0.114f, 0.149f, 1.f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.150f, 0.150f, 0.196f, 1.f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.35f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.063f, 0.063f, 0.082f, 1.f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.078f, 0.078f, 0.102f, 1.f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.063f, 0.063f, 0.082f, 0.5f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.063f, 0.063f, 0.082f, 0.5f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.200f, 0.200f, 0.260f, 1.f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.260f, 0.260f, 0.340f, 1.f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.6f);
        c[ImGuiCol_CheckMark]            = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 1.f);
        c[ImGuiCol_SliderGrab]           = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.7f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 1.f);
        c[ImGuiCol_Button]               = ImVec4(0.114f, 0.114f, 0.149f, 1.f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.25f);
        c[ImGuiCol_ButtonActive]         = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.45f);
        c[ImGuiCol_Header]               = ImVec4(0.114f, 0.114f, 0.149f, 1.f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.20f);
        c[ImGuiCol_HeaderActive]         = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.35f);
        c[ImGuiCol_Separator]            = ImVec4(0.160f, 0.160f, 0.210f, 0.5f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.5f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 1.f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.15f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.35f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.65f);
        c[ImGuiCol_Tab]                  = ImVec4(0.090f, 0.090f, 0.118f, 1.f);
        c[ImGuiCol_TabHovered]           = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.35f);
        c[ImGuiCol_TabSelected]          = ImVec4(ACCENT_Rf, ACCENT_Gf, ACCENT_Bf, 0.20f);
        c[ImGuiCol_Text]                 = ImVec4(0.92f, 0.92f, 0.94f, 1.f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.45f, 0.52f, 1.f);
    }

    // Start entity cache background thread
    EntityCache::Start();

    MSG msg{};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if (!g_running) break;

        {
            static int s_aliveCheck = 0;
            if (++s_aliveCheck >= 120) {
                s_aliveCheck = 0;
                if (!Memory::IsProcessAlive()) {
                    WriteStartupLog("GameExit", "Process gone — exiting overlay");
                    break;
                }
            }
        }
        {
            static bool s_insertWas = false, s_f3Was = false, s_f4Was = false;
            if (DynAPI::pAsyncKeyState) {
                bool insertNow = (DynAPI::pAsyncKeyState(VK_INSERT) & 0x8000) != 0;
                bool f3Now     = (DynAPI::pAsyncKeyState(VK_F3)     & 0x8000) != 0;
                bool f4Now     = (DynAPI::pAsyncKeyState(VK_F4)     & 0x8000) != 0;
                if (insertNow && !s_insertWas) g_menuOpen = !g_menuOpen;
                if (f3Now && !s_f3Was) g_showDebugHud = !g_showDebugHud;
                if (f4Now && !s_f4Was && !g_dumpRunning) std::thread(RunOffsetDumper).detach();
                s_insertWas = insertNow;
                s_f3Was = f3Now;
                s_f4Was = f4Now;
            }
        }

        {
            LONG exStyle = GetWindowLongW(g_overlayWnd, GWL_EXSTYLE);
            LONG newStyle = exStyle;
            if (g_menuOpen) newStyle &= ~WS_EX_TRANSPARENT;
            else            newStyle |= WS_EX_TRANSPARENT;
            if (newStyle != exStyle)
                SetWindowLongW(g_overlayWnd, GWL_EXSTYLE, newStyle);
        }

        if (!g_gameWnd || !IsWindow(g_gameWnd))
            g_gameWnd = FindGameWindow();
        if (g_gameWnd && IsWindow(g_gameWnd)) {
            RECT gr; GetWindowRect(g_gameWnd, &gr);
            int w = gr.right - gr.left, h = gr.bottom - gr.top;
            SetWindowPos(g_overlayWnd, HWND_TOPMOST, gr.left, gr.top, w, h, SWP_NOACTIVATE | SWP_FRAMECHANGED);
            g_screenW = w;
            g_screenH = h;
        }

        // Delta time for animations
        auto now = std::chrono::steady_clock::now();
        g_deltaTime = std::chrono::duration<float>(now - g_lastFrameTime).count();
        g_lastFrameTime = now;
        if (g_deltaTime > 0.1f) g_deltaTime = 0.1f;

        // Fresh camera every frame — worker thread camera is too stale for accurate W2S
        UpdateCameraView();

        // Read latest entity snapshot from background thread + extrapolate positions
        {
            auto snap = EntityCache::GetSnapshot();
            g_players = snap->players;
            g_vehicles = snap->vehicles;
            g_worldItems = snap->worldItems;
            s_renderPawn = snap->localPawn;
            s_renderFaction = snap->localFaction;
            s_renderFactionObj = snap->localFactionObj;
            s_renderFactionData = snap->localFactionData;

            float interpDt = std::chrono::duration<float>(now - snap->timestamp).count();

            for (auto& p : g_players) {
                if (!p.isValid) continue;
                bool freshOk = false;
                if (p.rootComp > 0x10000000 && p.rootComp < 0x7FFFFFFFFFFF) {
                    DVec3 freshPos{};
                    bool gotFresh = false;
                    for (uintptr_t off : {(uintptr_t)0x1D0, (uintptr_t)0x1E0, (uintptr_t)0x1F0, (uintptr_t)0x200, (uintptr_t)0x210}) {
                        DTransform t = Read<DTransform>(p.rootComp + off);
                        if (fabs(t.rotation.w) >= 0.01 && fabs(t.rotation.w) <= 1.01 &&
                            (t.translation.x != 0.0 || t.translation.y != 0.0) &&
                            fabs(t.translation.x) < 1e9 && fabs(t.translation.y) < 1e9) {
                            freshPos = t.translation;
                            gotFresh = true;
                            break;
                        }
                    }
                    if (!gotFresh) freshPos = Read<DVec3>(p.rootComp + O::Scene_RelativeLocation);
                    double dx = freshPos.x - p.position.x;
                    double dy = freshPos.y - p.position.y;
                    double dz = freshPos.z - p.position.z;
                    double moveDistSq = dx*dx + dy*dy + dz*dz;
                    bool posValid = (freshPos.x != 0.0 || freshPos.y != 0.0) &&
                                    fabs(freshPos.x) < 1e9 && fabs(freshPos.y) < 1e9 &&
                                    moveDistSq < 15000.0 * 15000.0;
                    if (posValid) {
                        DVec3 delta = {dx, dy, dz};
                        p.position = freshPos;
                        p.headPos = p.headPos + delta;
                        for (int b = 0; b < 18; b++) {
                            if (p.bones[b].x != 0.0 || p.bones[b].y != 0.0 || p.bones[b].z != 0.0)
                                p.bones[b] = p.bones[b] + delta;
                        }
                        freshOk = true;
                    }
                }
                if (!freshOk && interpDt > 0.001f && interpDt < 2.f) {
                    double vLen = p.velocity.length();
                    if (vLen >= 1.0 && vLen <= 100000.0) {
                        DVec3 offset = p.velocity * interpDt;
                        p.position = p.position + offset;
                        p.headPos = p.headPos + offset;
                        for (int b = 0; b < 18; b++) {
                            if (p.bones[b].x != 0.0 || p.bones[b].y != 0.0 || p.bones[b].z != 0.0)
                                p.bones[b] = p.bones[b] + offset;
                        }
                    }
                }
                p.distance = (float)((p.position - g_camera.location).length() / 100.0);
            }

            if (interpDt > 0.001f && interpDt < 2.f) {
                for (auto& v : g_vehicles) {
                    if (!v.isValid) continue;
                    double vLen = v.velocity.length();
                    if (vLen < 1.0 || vLen > 100000.0) continue;
                    v.position = v.position + v.velocity * interpDt;
                    v.distance = (float)((v.position - g_camera.location).length() / 100.0);
                }
            }
        }

        RunAimbot();
        RunTriggerbot();

        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        DrawNightMode();
        DrawFovCircle();
        DrawESP();
        DrawVehicleESP();
        DrawWorldItemESP();
        DrawAdminWarning();
        DrawHitmarker();
        DrawRadar();
        DrawCrosshair();
        DrawFPSCounter();
        DrawWatermark();
        DrawDebugHUD();
        DrawUpdateBar();
        DrawMenu();
        ImGui::Render();
        if (!g_rtv) { g_swapchain->Present(0, 0); continue; }
        float clear[4] = {0,0,0,0};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapchain->Present(1, 0);
    }

    // Release triggerbot mouse if held
    if (s_triggerFiring) {
        INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &in, sizeof(INPUT));
        s_triggerFiring = false;
    }

    // Stop entity cache and clean up
    EntityCache::Stop();
    g_boneCache.clear();
    g_dummyClasses.clear();
    g_playerClasses.clear();
    CleanupOverlay();
    if (g_byovdActive) {
        GdrvMapper::CleanupVulnDriver(g_vulnState);
    }
    Memory::Detach();
    if (g_proc) { CloseHandle(g_proc); g_proc = nullptr; }
    return 0;
}
