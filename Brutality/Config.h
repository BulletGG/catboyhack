#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "imgui/imgui.h"

#include "Hacks/SkinChanger.h"

#include "ConfigStructs.h"
#include "InputUtil.h"

class Config {
public:
    Config() noexcept;
    void load(size_t, bool incremental) noexcept;
    void load(const char8_t* name, bool incremental) noexcept;
    void save(size_t) const noexcept;
    void add(const char*) noexcept;
    void remove(size_t) noexcept;
    void rename(size_t, const char*) noexcept;
    void reset() noexcept;
    void listConfigs() noexcept;
    void createConfigDir() const noexcept;
    void openConfigDir() const noexcept;

    constexpr auto& getConfigs() noexcept
    {
        return configs;
    }

    struct Keybinds
    {
        KeyBind desyncInvert{ "desync invert", KeyMode::Off };
        KeyBind manualForward{ "forward", KeyMode::Off };
        KeyBind manualBackward{ "backward", KeyMode::Off };
        KeyBind manualLeft{ "left", KeyMode::Off };
        KeyBind manualRight{ "right", KeyMode::Off };
        KeyBind dmgOverride{ "damage override", KeyMode::Off };
        KeyBind doubletap{ std::string("doubletap"), KeyMode::Off };
        KeyBind hideshots{ std::string("hideshots"), KeyMode::Off };
        KeyBind thirdperson{ std::string("thirdperson"), KeyMode::Off };
        KeyBind slowwalk{ std::string("slowwalk"), KeyMode::Off };
        KeyBind autoPeek{ std::string("autopeek"), KeyMode::Off };
        KeyBind fakeduck{ std::string("fakeduck"), KeyMode::Off };
        KeyBind edgeJump{ std::string("edgejump"), KeyMode::Off };
        KeyBind freestand{ std::string("freestand"), KeyMode::Off };
    }binds;

    struct ConditionalAA
    {
        bool global = false; // if global is false then we AA will be off
        bool standing = false, moving = false, chrouching = false, slowwalk = false, jumping = false, cjumping = false;
    }condAA;

    struct RagebotGlobal {
        bool enabled{ false };
        bool silent{ false };
        int fov{ 0 }; //this will be 180 not 255.f
        bool autoShot{ false };
        bool knifebot{ false };
        bool autoScope{ false };
    }ragebot;

    struct Ragebot {
        bool override{ false };
        int multiPointHead{ 0 };
        int multiPointBody{ 0 };
        int minDamage{ 1 };
        bool dmgov{ false };
        int minDamageOverride{ 1 };
        bool autoStop{ false };
        int autoStopMod{ 0 };
        int hitboxes{ 0 };
        int hitChance{ 50 };
    };
    std::array<Ragebot, 9> rageBot;

    struct Fakelag {
        bool enabled = false;
        int mode = 0;
        int limit = 1;
    } fakelag;

    struct RageAntiAimConfig {
        bool enabled = false;
        int pitch = 0; //Off, Down, Zero, Up
        Yaw yawBase = Yaw::off;
        bool freestand = false;
        int yawModifier = 0; //off, center, offset, 3way
        int yawAdd = 0; //-180/180
        int spinBase = 0; //-180/180
        int jitterRange = 0;
        bool atTargets = false;
        struct Desync {
            bool enabled = false;       
            int leftLimit = 60;
            int rightLimit = 60;
            int peekMode = 0; //Off, Peek real, Peek fake
            int lbyMode = 0; // Normal, Opposite, Sway
        } desync;
    };
    std::array<RageAntiAimConfig, 7> rageAntiAim;

    struct Tickbase {
        float rechargeDelay = 0.5f;
        bool teleport{ false };
    } tickbase;

    bool disableInFreezetime{ true };

    struct Backtrack {
        bool enabled = false;
        bool ignoreSmoke = false;
        bool ignoreFlash = false;
        int timeLimit = 200;
        bool fakeLatency = false;
        int fakeLatencyAmount = 200;
    } backtrack;

    struct Chams {
        struct Material : Color4 {
            bool enabled = false;
            bool healthBased = false;
            bool blinking = false;
            bool wireframe = false;
            bool cover = false;
            bool ignorez = false;
            int material = 0;
        };
        std::array<Material, 7> materials;
    };

    std::unordered_map<std::string, Chams> chams;

    struct GlowItem : Color4 {
        bool enabled = false;
        bool healthBased = false;
        int style = 0;
    };

    struct PlayerGlow {
        GlowItem all, visible, occluded;
    };

    std::unordered_map<std::string, PlayerGlow> playerGlow;
    std::unordered_map<std::string, GlowItem> glow;


    struct StreamProofESP {
        std::unordered_map<std::string, Player> allies;
        std::unordered_map<std::string, Player> enemies;
        std::unordered_map<std::string, Weapon> weapons;
        std::unordered_map<std::string, Projectile> projectiles;
        std::unordered_map<std::string, Shared> lootCrates;
        std::unordered_map<std::string, Shared> otherEntities;
    } streamProofESP;

    struct Visuals {
        struct Scope
        {
            int zoomFov = 100;
            bool skipSecondZoom = false;
            Color4 color{ 1.f, 1.f, 1.f, 1.f };
            bool fade{ true };
            int offset{ 10 };
            int length{ 50 };
            int type{ 0 };
            bool removeTop = false;
            bool removeBottom = false;
            bool removeLeft = false;
            bool removeRight = false;
        }scope;
        bool partyMode{ false };
        bool disablePostProcessing{ false };
        bool inverseRagdollGravity{ false };
        bool noFog{ false };
        struct Fog
        {
            float start{ 0 };
            float end{ 0 };
            float density{ 0 };
        } fogOptions;
        ColorToggle3 fog;
        bool no3dSky{ false };
        bool noAimPunch{ false };
        bool noViewPunch{ false };
        bool noViewBob{ false };
        bool noHands{ false };
        bool noSleeves{ false };
        bool noWeapons{ false };
        bool noSmoke{ false };
        bool wireframeSmoke{ false };
        bool smokeCircle{ false };
        bool noMolotov{ false };
        bool wireframeMolotov{ false };
        bool noBlur{ false };
        bool noScopeOverlay{ false };
        bool noGrass{ false };
        struct ShadowsChanger
        {
            bool enabled{ false };
            int x{ 0 };
            int y{ 0 };
        } shadowsChanger;
        bool fullBright{ false };
        struct Thirdperson
        {
            bool enable{ false };
            int distance{ 100 };
            bool animated{ true };
            bool whileDead{ false };
            bool disableOnGrenade{ false };
            int thirdpersonTransparency = 0;
        }thirdperson;
        int fov{ 0 };
        float cmslider{ 1.0f };
        int farZ{ 0 };
        int flashReduction{ 0 };
        int skybox{ 0 };
        ColorToggle3 world;
        ColorToggle3 props;
        ColorToggle3 sky;
        ColorToggle molotovColor{ 1.0f, 0.27f, 0.0f, 0.5f };
        ColorToggle smokeColor{ .75f, .75f, .75f, 0.5f };
        std::string customSkybox;
        bool deagleSpinner{ false };
        struct MotionBlur
        {
            bool enabled{ false };
            bool forwardEnabled{ false };
            float fallingMin{ 10.0f };
            float fallingMax{ 20.0f };
            float fallingIntensity{ 1.0f };
            float rotationIntensity{ 1.0f };
            float strength{ 1.0f };
        } motionBlur;
        
        struct FootstepESP {
            ColorToggle footstepBeams{ 0.2f, 0.5f, 1.f, 1.0f };
            int footstepBeamRadius = 0;
            int footstepBeamThickness = 0;
        } footsteps;
        float thirdpersonTransparency = 0.f;
        int screenEffect{ 0 };
        bool hitEffect{ false };
        float hitEffectTime{ 0.6f };
        int hitMarker{ 0 };
        float hitMarkerTime{ 0.6f };
        ColorToggle bulletImpacts{ 0.0f, 0.0f, 1.f, 0.5f };
        float bulletImpactsTime{ 4.f };
        int playerModelT{ 0 };
        int modelscalecombo{ 0 };
        int playerModelCT{ 0 };
        char playerModel[256] { };
        BulletTracers bulletTracers;
        ColorToggle molotovHull{ 1.0f, 0.27f, 0.0f, 0.3f };
        ColorToggle smokeHull{ 0.5f, 0.5f, 0.5f, 0.3f };
        struct MolotovPolygon
        {
            bool enabled{ false };
            Color4 enemy{ 1.f, 0.27f, 0.f, 0.3f };
            Color4 team{ 0.37f, 1.f, 0.37f, 0.3f };
            Color4 self{ 1.f, 0.09f, 0.96f, 0.3f };
        } molotovPolygon;
        struct Viewmodel
        {
            bool enabled { false };
            int fov{ 0 };
            float x { 0.0f };
            float y { 0.0f };
            float z { 0.0f };
            float roll { 0.0f };
        } viewModel;
        struct OnHitHitbox
        {
            ColorToggle color{ 1.f, 1.f, 1.f, 1.f };
            float duration = 2.f;
        } onHitHitbox;
        ColorToggleOutline spreadCircle { 1.0f, 1.0f, 1.0f, 0.25f };
        int asusWalls = 100;
        int asusProps = 100;
        bool smokeTimer{ false };
        Color4 smokeTimerBG{ 1.0f, 1.0f, 1.0f, 0.5f };
        Color4 smokeTimerTimer{ 0.0f, 0.0f, 1.0f, 1.0f };
        Color4 smokeTimerText{ 0.0f, 0.0f, 0.0f, 1.0f };
        bool molotovTimer{ false };
        Color4 molotovTimerBG{ 1.0f, 1.0f, 1.0f, 0.5f };
        Color4 molotovTimerTimer{ 0.0f, 0.0f, 1.0f, 1.0f };
        Color4 molotovTimerText{ 0.0f, 0.0f, 0.0f, 1.0f };
        ColorToggle console{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool viewmodelInScope = false;
    } visuals;

    std::array<item_setting, 36> skinChanger;

    struct Misc {
        Misc() { menuKey.keyMode = KeyMode::Toggle; }

        KeyBind menuKey = KeyBind::INSERT;
        bool antiAfkKick{ false };
        bool adBlock{ false };
        int forceRelayCluster{ 0 };
        bool autoStrafe{ false };
        int autoStraferSmoothness{ 50 };
        struct Indicators
        {
            bool enable = false;
            Color4 avalible{ 1.f, 1.f, 1.f, 1.f };
            Color4 disabled{ 0.890196f, 0.443137f, 0.443137f, 1.f };
        }indicators;
        bool bunnyHop{ false };
        bool clanTag{ false };
        bool fastDuck{ false };
        bool moonwalk{ false };
        bool edgeJump{ false };
        
        bool slowwalk{ false };
        int slowwalkAmnt{ 0 };
        
        bool fakeduck{ false };
        
        ColorToggle autoPeek{ 1.0f, 1.0f, 1.0f, 1.0f };
        
        bool autoPistol{ false };
        bool autoReload{ false };
        bool autoAccept{ false };
        bool radarHack{ false };
        bool revealRanks{ false };
        bool revealMoney{ false };
        bool revealSuspect{ false };
        bool revealVotes{ false };
        bool chatRevealer{ false };
        bool disableModelOcclusion{ false };
        bool nameStealer{ false };
        bool disablePanoramablur{ false };
        bool killMessage{ false };
        bool nadePredict{ false };
        bool fixTabletSignal{ false };
        bool fastPlant{ false };
        bool fastStop{ false };
        bool prepareRevolver{ false };
        bool svPureBypass{ true };
        bool inventoryUnlocker{ false };
        bool unhideConvars{ false };
        KillfeedChanger killfeedChanger;
        PreserveKillfeed preserveKillfeed;
        bool forceCrosshair;
        ColorToggleThickness nadeDamagePredict;
        Color4 nadeTrailPredict;
        Color4 nadeCirclePredict{ 0.f, 0.5f, 1.f, 1.f };

        struct SpectatorList {
            bool enabled = false;
            bool noTitleBar = false;
            ImVec2 pos;
        };

        SpectatorList spectatorList;

        struct KeyBindList {
            bool enabled = false;
            bool noTitleBar = false;
            ImVec2 pos;
        };

        KeyBindList keybindList;

        struct Logger {
            int modes{ 0 };
            int events{ 0 };
        };

        Logger loggerOptions;

        ColorToggle3 logger;

        struct Watermark {
            bool enabled = false;
            ImVec2 pos;
        };
        Watermark watermark;
        float aspectratio{ 0 };
        std::string killMessageString{ "Gotcha!" };
        ColorToggle3 bombTimer{ 1.0f, 0.55f, 0.0f };
        ColorToggle3 hurtIndicator{ 0.0f, 0.8f, 0.7f };
        int hitSound{ 0 };
        int quickHealthshotKey{ 0 };
        float maxAngleDelta{ 255.0f };
        int killSound{ 0 };
        std::string customKillSound;
        std::string customHitSound;

        struct PlayerList {
            bool enabled = false;
            bool steamID = false;
            bool rank = false;
            bool wins = false;
            bool money = true;
            bool health = true;
            bool armor = false;

            ImVec2 pos;
        };

        PlayerList playerList;
        OffscreenEnemies offscreenEnemies;
        AutoBuy autoBuy;
        bool fastLadder = false;
    } misc;

    void scheduleFontLoad(const std::string& name) noexcept;
    const auto& getSystemFonts() noexcept { return systemFonts; }
private:
    std::vector<std::string> scheduledFonts{ "Default" };
    std::vector<std::string> systemFonts{ "Default" };
    std::filesystem::path path;
    std::vector<std::string> configs;
};

inline std::unique_ptr<Config> config;
