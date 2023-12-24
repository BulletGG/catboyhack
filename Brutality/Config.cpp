#include <fstream>
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>

#include "nlohmann/json.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "Config.h"
#include "Helpers.h"

#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Glow.h"
#include "Hacks/Sound.h"

#include "SDK/Platform.h"

int CALLBACK fontCallback(const LOGFONTW* lpelfe, const TEXTMETRICW*, DWORD, LPARAM lParam)
{
    const wchar_t* const fontName = reinterpret_cast<const ENUMLOGFONTEXW*>(lpelfe)->elfFullName;

    if (fontName[0] == L'@')
        return TRUE;

    if (HFONT font = CreateFontW(0, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH, fontName)) {

        DWORD fontData = GDI_ERROR;

        if (HDC hdc = CreateCompatibleDC(nullptr)) {
            SelectObject(hdc, font);
            // Do not use TTC fonts as we only support TTF fonts
            fontData = GetFontData(hdc, 'fctt', 0, NULL, 0);
            DeleteDC(hdc);
        }
        DeleteObject(font);

        if (fontData == GDI_ERROR) {
            if (char buff[1024]; WideCharToMultiByte(CP_UTF8, 0, fontName, -1, buff, sizeof(buff), nullptr, nullptr))
                reinterpret_cast<std::vector<std::string>*>(lParam)->emplace_back(buff);
        }
    }
    return TRUE;
}

Config::Config() noexcept
{
    if (PWSTR pathToDocuments; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &pathToDocuments))) {
        path = pathToDocuments;
        CoTaskMemFree(pathToDocuments);
    }

    path /= "Brutality";
    listConfigs();
    visuals.playerModel[0] = '\0';

    load(u8"default.json", false);

    LOGFONTW logfont;
    logfont.lfCharSet = ANSI_CHARSET;
    logfont.lfPitchAndFamily = DEFAULT_PITCH;
    logfont.lfFaceName[0] = L'\0';

    EnumFontFamiliesExW(GetDC(nullptr), &logfont, fontCallback, (LPARAM)&systemFonts, 0);

    std::sort(std::next(systemFonts.begin()), systemFonts.end());
}
#pragma region  Read

static void from_json(const json& j, ColorToggle& ct)
{
    from_json(j, static_cast<Color4&>(ct));
    read(j, "Enabled", ct.enabled);
}

static void from_json(const json& j, Color3& c)
{
    read(j, "Color", c.color);
}

static void from_json(const json& j, ColorToggle3& ct)
{
    from_json(j, static_cast<Color3&>(ct));
    read(j, "Enabled", ct.enabled);
}

static void from_json(const json& j, ColorToggleRounding& ctr)
{
    from_json(j, static_cast<ColorToggle&>(ctr));

    read(j, "Rounding", ctr.rounding);
}

static void from_json(const json& j, ColorToggleOutline& cto)
{
    from_json(j, static_cast<ColorToggle&>(cto));

    read(j, "Outline", cto.outline);
}

static void from_json(const json& j, ColorToggleThickness& ctt)
{
    from_json(j, static_cast<ColorToggle&>(ctt));

    read(j, "Thickness", ctt.thickness);
}

static void from_json(const json& j, ColorToggleThicknessRounding& cttr)
{
    from_json(j, static_cast<ColorToggleRounding&>(cttr));

    read(j, "Thickness", cttr.thickness);
}

static void from_json(const json& j, Snapline& s)
{
    from_json(j, static_cast<ColorToggleThickness&>(s));

    read(j, "Type", s.type);
}

static void from_json(const json& j, Box& b)
{
    from_json(j, static_cast<ColorToggleRounding&>(b));

    read(j, "Type", b.type);
    read(j, "Scale", b.scale);
    read<value_t::object>(j, "Fill", b.fill);
}

static void from_json(const json& j, Shared& s)
{
    read(j, "Enabled", s.enabled);
    read<value_t::object>(j, "Snapline", s.snapline);
    read<value_t::object>(j, "Box", s.box);
    read<value_t::object>(j, "Name", s.name);
}

static void from_json(const json& j, Weapon& w)
{
    from_json(j, static_cast<Shared&>(w));

    read<value_t::object>(j, "Ammo", w.ammo);
}

static void from_json(const json& j, Trail& t)
{
    from_json(j, static_cast<ColorToggleThickness&>(t));

    read(j, "Type", t.type);
    read(j, "Time", t.time);
}

static void from_json(const json& j, Trails& t)
{
    read(j, "Enabled", t.enabled);
    read<value_t::object>(j, "Local Player", t.localPlayer);
    read<value_t::object>(j, "Allies", t.allies);
    read<value_t::object>(j, "Enemies", t.enemies);
}

static void from_json(const json& j, Projectile& p)
{
    from_json(j, static_cast<Shared&>(p));

    read<value_t::object>(j, "Trails", p.trails);
}

static void from_json(const json& j, HealthBar& o)
{
    from_json(j, static_cast<ColorToggle&>(o));
    read(j, "Type", o.type);
    read<value_t::object>(j, "Bottom", o.BottomGradient);
    read<value_t::object>(j, "Top", o.TopGradient);
}

static void from_json(const json& j, Player& p)
{
    from_json(j, static_cast<Shared&>(p));

    read<value_t::object>(j, "Weapon", p.weapon);
    read<value_t::object>(j, "Weapon Icon", p.weaponIcon);
    read<value_t::object>(j, "Ammobar", p.ammobar);
    read<value_t::object>(j, "Flash Duration", p.flashDuration);
    read(j, "Audible Only", p.audibleOnly);
    read(j, "Spotted Only", p.spottedOnly);
    read<value_t::object>(j, "Health Bar", p.healthBar);
    read<value_t::object>(j, "Skeleton", p.skeleton);
    read<value_t::object>(j, "Head Box", p.headBox);
    read<value_t::object>(j, "Flags", p.flags);
    read<value_t::object>(j, "HPSIDE", p.hpside);
    read<value_t::object>(j, "Line of sight", p.lineOfSight);
    read(j, "SA", p.showArmor);
    read(j, "SB", p.showBomb);
    read(j, "SK", p.showKit);
    read(j, "SM", p.showMoney);
    read(j, "SD", p.showDuck);
}

static void from_json(const json& j, OffscreenEnemies& o)
{
    from_json(j, static_cast<ColorToggle&>(o));

    read(j, "OF", o.offset);
    read(j, "SZ", o.size);
}

static void from_json(const json& j, BulletTracers& o)
{
    from_json(j, static_cast<ColorToggle&>(o));
}

static void from_json(const json& j, ImVec2& v)
{
    read(j, "X", v.x);
    read(j, "Y", v.y);
}

static void from_json(const json& j, Config::Ragebot& r)
{

}

static void from_json(const json& j, Config::ConditionalAA& a)
{
    read(j, "G", a.global);
    read(j, "S", a.standing);
    read(j, "M", a.moving);
    read(j, "SW", a.slowwalk);
    read(j, "C", a.chrouching);
    read(j, "J", a.jumping);
    read(j, "CJ", a.cjumping);
}

static void from_json(const json& j, Config::RageAntiAimConfig& a)
{
    //regular
    read(j, "OV", a.enabled);
    read(j, "PH", a.pitch);
    read(j, "YB", reinterpret_cast<int&>(a.yawBase));
    read(j, "YM", a.yawModifier);
    read(j, "YD", a.yawAdd);
    read(j, "JR", a.jitterRange);
    read(j, "SB", a.spinBase);
    read(j, "AT", a.atTargets);
    //desync
    read(j, "DC", a.desync.enabled);
    read(j, "LL", a.desync.leftLimit);
    read(j, "RL", a.desync.rightLimit);
    read(j, "PM", a.desync.peekMode);
    read(j, "LM", a.desync.lbyMode);
}
static void from_json(const json& j, Config::Fakelag& f)
{
    read(j, "Enabled", f.enabled);
    read(j, "Mode", f.mode);
    read(j, "Limit", f.limit);
}

static void from_json(const json& j, Config::Tickbase& t)
{
    read(j, "Teleport", t.teleport);
    read(j, "Recharge delay", t.rechargeDelay);
}

static void from_json(const json& j, Config::Backtrack& b)
{
    read(j, "Enabled", b.enabled);
    read(j, "Ignore smoke", b.ignoreSmoke);
    read(j, "Ignore flash", b.ignoreFlash);
    read(j, "Time limit", b.timeLimit);
    read(j, "Fake Latency", b.fakeLatency);
    read(j, "Fake Latency Amount", b.fakeLatencyAmount);
}

static void from_json(const json& j, Config::Chams::Material& m)
{
    from_json(j, static_cast<Color4&>(m));

    read(j, "Enabled", m.enabled);
    read(j, "Health based", m.healthBased);
    read(j, "Blinking", m.blinking);
    read(j, "Wireframe", m.wireframe);
    read(j, "Cover", m.cover);
    read(j, "Ignore-Z", m.ignorez);
    read(j, "Material", m.material);
}

static void from_json(const json& j, Config::Chams& c)
{
    read_array_opt(j, "Materials", c.materials);
}

static void from_json(const json& j, Config::GlowItem& g)
{
    from_json(j, static_cast<Color4&>(g));

    read(j, "Enabled", g.enabled);
    read(j, "Health based", g.healthBased);
    read(j, "Style", g.style);
}

static void from_json(const json& j, Config::PlayerGlow& g)
{
    read<value_t::object>(j, "All", g.all);
    read<value_t::object>(j, "Visible", g.visible);
    read<value_t::object>(j, "Occluded", g.occluded);
}

static void from_json(const json& j, Config::StreamProofESP& e)
{
    read(j, "Allies", e.allies);
    read(j, "Enemies", e.enemies);
    read(j, "Weapons", e.weapons);
    read(j, "Projectiles", e.projectiles);
    read(j, "Loot Crates", e.lootCrates);
    read(j, "Other Entities", e.otherEntities);
}

static void from_json(const json& j, Config::Visuals::FootstepESP& ft)
{
    read<value_t::object>(j, "Enabled", ft.footstepBeams);
    read(j, "Thickness", ft.footstepBeamThickness);
    read(j, "Radius", ft.footstepBeamRadius);
}

static void from_json(const json& j, Config::Visuals& v)
{
    read(j, "Disable post-processing", v.disablePostProcessing);
    read(j, "Inverse ragdoll gravity", v.inverseRagdollGravity);
    read(j, "No fog", v.noFog);
    read<value_t::object>(j, "Fog controller", v.fog);
    read<value_t::object>(j, "Fog options", v.fogOptions);
    read(j, "No 3d sky", v.no3dSky);
    read(j, "No aim punch", v.noAimPunch);
    read(j, "No view punch", v.noViewPunch);
    read(j, "No view bob", v.noViewBob);
    read(j, "No hands", v.noHands);
    read(j, "No sleeves", v.noSleeves);
    read(j, "No weapons", v.noWeapons);
    read(j, "No smoke", v.noSmoke);
    read(j, "Smoke circle", v.smokeCircle);
    read(j, "Wireframe smoke", v.wireframeSmoke);
    read(j, "No molotov", v.noMolotov);
    read(j, "Wireframe molotov", v.wireframeMolotov);
    read(j, "No blur", v.noBlur);
    read(j, "No scope overlay", v.noScopeOverlay);
    read(j, "No grass", v.noGrass);
    read<value_t::object>(j, "Motion Blur", v.motionBlur);
    read<value_t::object>(j, "Shadows changer", v.shadowsChanger);
    read(j, "Full bright", v.fullBright);
    read(j, "Thirdperson Transparency", v.thirdpersonTransparency);
    read(j, "Thirdperson", v.thirdperson.enable);
    read(j, "Thirdperson distance", v.thirdperson.distance);
    read(j, "cm slider", v.cmslider);
    read(j, "FOV", v.fov);
    read(j, "Far Z", v.farZ);
    read(j, "Flash reduction", v.flashReduction);
    read(j, "Skybox", v.skybox);
    read<value_t::object>(j, "World", v.world);
    read<value_t::object>(j, "Props", v.props);
    read<value_t::object>(j, "Sky", v.sky);
    read<value_t::string>(j, "Custom skybox", v.customSkybox);
    read(j, "Deagle spinner", v.deagleSpinner);
    read(j, "Screen effect", v.screenEffect);
    read(j, "Hit effect", v.hitEffect);
    read(j, "Hit effect time", v.hitEffectTime);
    read(j, "Hit marker", v.hitMarker);
    read(j, "Hit marker time", v.hitMarkerTime);
    read(j, "Playermodel T", v.playerModelT);
    read(j, "msc", v.modelscalecombo);
    read(j, "Playermodel CT", v.playerModelCT);
    read(j, "Custom Playermodel", v.playerModel, sizeof(v.playerModel));
    read<value_t::object>(j, "Bullet Tracers", v.bulletTracers);
    read<value_t::object>(j, "Bullet Impacts", v.bulletImpacts);
    read<value_t::object>(j, "Hitbox on Hit", v.onHitHitbox);
    read(j, "Bullet Impacts time", v.bulletImpactsTime);
    read<value_t::object>(j, "Molotov Hull", v.molotovHull);
    read<value_t::object>(j, "Smoke Hull", v.smokeHull);
    read<value_t::object>(j, "Molotov Polygon", v.molotovPolygon);
    read<value_t::object>(j, "Viewmodel", v.viewModel);
    read<value_t::object>(j, "Spread circle", v.spreadCircle);
    read(j, "Asus walls", v.asusWalls);
    read(j, "Asus props", v.asusProps);
    read(j, "Smoke timer", v.smokeTimer);
    read<value_t::object>(j, "Smoke timer BG", v.smokeTimerBG);
    read<value_t::object>(j, "Smoke timer TIMER", v.smokeTimerTimer);
    read<value_t::object>(j, "Smoke timer TEXT", v.smokeTimerText);
    read(j, "Molotov timer", v.molotovTimer);
    read<value_t::object>(j, "Molotov timer BG", v.molotovTimerBG);
    read<value_t::object>(j, "Molotov timer TIMER", v.molotovTimerTimer);
    read<value_t::object>(j, "Molotov timer TEXT", v.molotovTimerText);
    read<value_t::object>(j, "Console Color", v.console);
    read<value_t::object>(j, "Smoke Color", v.smokeColor);
    read<value_t::object>(j, "Molotov Color", v.molotovColor);
    read<value_t::object>(j, "Footstep", v.footsteps);
}

static void from_json(const json& j, sticker_setting& s)
{
    read(j, "Kit", s.kit);
    read(j, "Wear", s.wear);
    read(j, "Scale", s.scale);
    read(j, "Rotation", s.rotation);

    s.onLoad();
}

static void from_json(const json& j, item_setting& i)
{
    read(j, "Enabled", i.enabled);
    read(j, "Definition index", i.itemId);
    read(j, "Quality", i.quality);
    read(j, "Paint Kit", i.paintKit);
    read(j, "Definition override", i.definition_override_index);
    read(j, "Seed", i.seed);
    read(j, "StatTrak", i.stat_trak);
    read(j, "Wear", i.wear);
    read(j, "Custom name", i.custom_name, sizeof(i.custom_name));
    read(j, "Stickers", i.stickers);

    i.onLoad();
}

static void from_json(const json& j, Config::Misc::SpectatorList& sl)
{
    read(j, "Enabled", sl.enabled);
    read(j, "No Title Bar", sl.noTitleBar);
    read<value_t::object>(j, "Pos", sl.pos);
}

static void from_json(const json& j, Config::Misc::KeyBindList& sl)
{
    read(j, "Enabled", sl.enabled);
    read(j, "No Title Bar", sl.noTitleBar);
    read<value_t::object>(j, "Pos", sl.pos);
}

static void from_json(const json& j, Config::Misc::PlayerList& o)
{
    read(j, "Enabled", o.enabled);
    read(j, "Steam ID", o.steamID);
    read(j, "Rank", o.rank);
    read(j, "Wins", o.wins);
    read(j, "Money", o.money);
    read(j, "Health", o.health);
    read(j, "Armor", o.armor);
    read<value_t::object>(j, "Pos", o.pos);
}

static void from_json(const json& j, Config::Misc::Watermark& o)
{
    read(j, "Enabled", o.enabled);
    read<value_t::object>(j, "Pos", o.pos);
}

static void from_json(const json& j, PreserveKillfeed& o)
{
    read(j, "Enabled", o.enabled);
    read(j, "Only Headshots", o.onlyHeadshots);
}

static void from_json(const json& j, KillfeedChanger& o)
{
    read(j, "Enabled", o.enabled);
    read(j, "Headshot", o.headshot);
    read(j, "Dominated", o.dominated);
    read(j, "Revenge", o.revenge);
    read(j, "Penetrated", o.penetrated);
    read(j, "Noscope", o.noscope);
    read(j, "Thrusmoke", o.thrusmoke);
    read(j, "Attackerblind", o.attackerblind);
}

static void from_json(const json& j, AutoBuy& o)
{
    read(j, "Enabled", o.enabled);
    read(j, "Primary weapon", o.primaryWeapon);
    read(j, "Secondary weapon", o.secondaryWeapon);
    read(j, "Armor", o.armor);
    read(j, "Utility", o.utility);
    read(j, "Grenades", o.grenades);
}

static void from_json(const json& j, Config::Misc::Logger& o)
{
    read(j, "Modes", o.modes);
    read(j, "Events", o.events);
}

static void from_json(const json& j, Config::Visuals::MotionBlur& mb)
{
    read(j, "Enabled", mb.enabled);
    read(j, "Forward", mb.forwardEnabled);
    read(j, "Falling min", mb.fallingMin);
    read(j, "Falling max", mb.fallingMax);
    read(j, "Falling intensity", mb.fallingIntensity);
    read(j, "Rotation intensity", mb.rotationIntensity);
    read(j, "Strength", mb.strength);
}

static void from_json(const json& j, Config::Visuals::Fog& f)
{
    read(j, "Start", f.start);
    read(j, "End", f.end);
    read(j, "Density", f.density);
}

static void from_json(const json& j, Config::Visuals::ShadowsChanger& sw)
{
    read(j, "Enabled", sw.enabled);
    read(j, "X", sw.x);
    read(j, "Y", sw.y);
}

static void from_json(const json& j, Config::Visuals::Viewmodel& vxyz)
{
    read(j, "Enabled", vxyz.enabled);
    read(j, "Fov", vxyz.fov);
    read(j, "X", vxyz.x);
    read(j, "Y", vxyz.y);
    read(j, "Z", vxyz.z);
    read(j, "Roll", vxyz.roll);
}

static void from_json(const json& j, Config::Visuals::MolotovPolygon& mp)
{
    read(j, "Enabled", mp.enabled);
    read<value_t::object>(j, "Self", mp.self);
    read<value_t::object>(j, "Team", mp.team);
    read<value_t::object>(j, "Enemy", mp.enemy);
}

static void from_json(const json& j, Config::Visuals::OnHitHitbox& h)
{
    read<value_t::object>(j, "Color", h.color);
    read(j, "Duration", h.duration);
}

static void from_json(const json& j, Config::Misc& m)
{
    read(j, "Menu key", m.menuKey);
    read(j, "Anti AFK kick", m.antiAfkKick);
    read(j, "Adblock", m.adBlock);
    read(j, "Force relay", m.forceRelayCluster);
    read(j, "Auto strafe", m.autoStrafe);
    read(j, "Bunny hop", m.bunnyHop);
    read(j, "Fast duck", m.fastDuck);
    read(j, "Moonwalk", m.moonwalk);
    read(j, "Slowwalk", m.slowwalk);
    read(j, "Slowwalk Amnt", m.slowwalkAmnt);
    read(j, "Fake duck", m.fakeduck);
    read<value_t::object>(j, "Auto peek", m.autoPeek);
    read(j, "Force crosshair", m.forceCrosshair);
    read(j, "Auto pistol", m.autoPistol);
    read(j, "Auto reload", m.autoReload);
    read(j, "Auto accept", m.autoAccept);
    read(j, "Radar hack", m.radarHack);
    read(j, "Reveal ranks", m.revealRanks);
    read(j, "Reveal money", m.revealMoney);
    read(j, "Reveal suspect", m.revealSuspect);
    read(j, "Reveal votes", m.revealVotes);
    read(j, "Chat revealer", m.chatRevealer);
    read<value_t::object>(j, "Spectator list", m.spectatorList);
    read<value_t::object>(j, "Keybind list", m.keybindList);
    read<value_t::object>(j, "Player list", m.playerList);
    read<value_t::object>(j, "Watermark", m.watermark);
    read<value_t::object>(j, "Offscreen Enemies", m.offscreenEnemies);
    read(j, "Disable model occlusion", m.disableModelOcclusion);
    read(j, "Aspect Ratio", m.aspectratio);
    read(j, "Kill message", m.killMessage);
    read<value_t::string>(j, "Kill message string", m.killMessageString);
    read(j, "Name stealer", m.nameStealer);
    read(j, "Disable HUD blur", m.disablePanoramablur);
    read(j, "Fast plant", m.fastPlant);
    read(j, "Fast Stop", m.fastStop);
    read<value_t::object>(j, "Bomb timer", m.bombTimer);
    read<value_t::object>(j, "Hurt indicator", m.hurtIndicator);
    read(j, "Prepare revolver", m.prepareRevolver);
    read(j, "Hit sound", m.hitSound);
    read(j, "Quick healthshot key", m.quickHealthshotKey);
    read(j, "Grenade predict", m.nadePredict);
    read<value_t::object>(j, "Grenade predict Damage", m.nadeDamagePredict);
    read<value_t::object>(j, "Grenade predict Trail", m.nadeTrailPredict);
    read<value_t::object>(j, "Grenade predict Circle", m.nadeCirclePredict);
    read(j, "Max angle delta", m.maxAngleDelta);
    read(j, "Fix tablet signal", m.fixTabletSignal);
    read<value_t::string>(j, "Custom Hit Sound", m.customHitSound);
    read(j, "Kill sound", m.killSound);
    read<value_t::string>(j, "Custom Kill Sound", m.customKillSound);
    read<value_t::object>(j, "Preserve Killfeed", m.preserveKillfeed);
    read<value_t::object>(j, "Killfeed changer", m.killfeedChanger);
    read(j, "Sv pure bypass", m.svPureBypass);
    read(j, "Inventory Unlocker", m.inventoryUnlocker);
    read(j, "Unlock hidden cvars", m.unhideConvars);
    read<value_t::object>(j, "Autobuy", m.autoBuy);
    read<value_t::object>(j, "Logger", m.logger);
    read<value_t::object>(j, "Logger options", m.loggerOptions);
}

void Config::load(size_t id, bool incremental) noexcept
{
    load((const char8_t*)configs[id].c_str(), incremental);
}

void Config::load(const char8_t* name, bool incremental) noexcept
{
    json j;

    if (std::ifstream in{ path / name }; in.good()) {
        j = json::parse(in, nullptr, false);
        if (j.is_discarded())
            return;
    } else {
        return;
    }

    if (!incremental)
        reset();

    read(j, "RagebotCofnig", rageBot);

    read(j, "AA", rageAntiAim);
    read<value_t::object>(j, "CAA", condAA);
    read(j, "Disable in freezetime", disableInFreezetime);
    read<value_t::object>(j, "Fakelag", fakelag);
    read<value_t::object>(j, "Tickbase", tickbase);
    read<value_t::object>(j, "Backtrack", backtrack);

    read(j["Glow"], "Items", glow);
    read(j["Glow"], "Players", playerGlow);

    read(j, "Chams", chams);
    read<value_t::object>(j, "ESP", streamProofESP);
    read<value_t::object>(j, "Visuals", visuals);
    read(j, "Skin changer", skinChanger);
    ::Sound::fromJson(j["Sound"]);
    read<value_t::object>(j, "Misc", misc);
}

#pragma endregion

#pragma region  Write

static void to_json(json& j, const ColorToggle& o, const ColorToggle& dummy = {})
{
    to_json(j, static_cast<const Color4&>(o), dummy);
    WRITE("Enabled", enabled);
}

static void to_json(json& j, const Color3& o, const Color3& dummy = {})
{
    WRITE("Color", color);
}

static void to_json(json& j, const ColorToggle3& o, const ColorToggle3& dummy = {})
{
    to_json(j, static_cast<const Color3&>(o), dummy);
    WRITE("Enabled", enabled);
}

static void to_json(json& j, const ColorToggleRounding& o, const ColorToggleRounding& dummy = {})
{
    to_json(j, static_cast<const ColorToggle&>(o), dummy);
    WRITE("Rounding", rounding);
}

static void to_json(json& j, const ColorToggleThickness& o, const ColorToggleThickness& dummy = {})
{
    to_json(j, static_cast<const ColorToggle&>(o), dummy);
    WRITE("Thickness", thickness);
}

static void to_json(json& j, const ColorToggleOutline& o, const ColorToggleOutline& dummy = {})
{
    to_json(j, static_cast<const ColorToggle&>(o), dummy);
    WRITE("Outline", outline);
}

static void to_json(json& j, const ColorToggleThicknessRounding& o, const ColorToggleThicknessRounding& dummy = {})
{
    to_json(j, static_cast<const ColorToggleRounding&>(o), dummy);
    WRITE("Thickness", thickness);
}

static void to_json(json& j, const Snapline& o, const Snapline& dummy = {})
{
    to_json(j, static_cast<const ColorToggleThickness&>(o), dummy);
    WRITE("Type", type);
}

static void to_json(json& j, const Box& o, const Box& dummy = {})
{
    to_json(j, static_cast<const ColorToggleRounding&>(o), dummy);
    WRITE("Type", type);
    WRITE("Scale", scale);
    WRITE("Fill", fill);
}

static void to_json(json& j, const Shared& o, const Shared& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Snapline", snapline);
    WRITE("Box", box);
    WRITE("Name", name);
}

static void to_json(json& j, const HealthBar& o, const HealthBar& dummy = {})
{
    to_json(j, static_cast<const ColorToggle&>(o), dummy);
    WRITE("Type", type);
    WRITE("Bottom", BottomGradient);
    WRITE("Top", TopGradient);
}

static void to_json(json& j, const Player& o, const Player& dummy = {})
{
    to_json(j, static_cast<const Shared&>(o), dummy);
    WRITE("Weapon", weapon);
    WRITE("Weapon Icon", weaponIcon);
    WRITE("Ammobar", ammobar);
    WRITE("Flash Duration", flashDuration);
    WRITE("Audible Only", audibleOnly);
    WRITE("Spotted Only", spottedOnly);
    WRITE("Health Bar", healthBar);
    WRITE("Skeleton", skeleton);
    WRITE("Head Box", headBox);
    WRITE("Flags", flags);
    WRITE("SA", showArmor);
    WRITE("SB", showBomb);
    WRITE("SK", showKit);
    WRITE("SD", showDuck);
    WRITE("SM", showMoney);
    WRITE("HPSIDE", hpside);
    WRITE("Line of sight", lineOfSight);
}

static void to_json(json& j, const Weapon& o, const Weapon& dummy = {})
{
    to_json(j, static_cast<const Shared&>(o), dummy);
    WRITE("Ammo", ammo);
}

static void to_json(json& j, const Trail& o, const Trail& dummy = {})
{
    to_json(j, static_cast<const ColorToggleThickness&>(o), dummy);
    WRITE("Type", type);
    WRITE("Time", time);
}

static void to_json(json& j, const Trails& o, const Trails& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Local Player", localPlayer);
    WRITE("Allies", allies);
    WRITE("Enemies", enemies);
}

static void to_json(json& j, const OffscreenEnemies& o, const OffscreenEnemies& dummy = {})
{
    to_json(j, static_cast<const ColorToggle&>(o), dummy);

    WRITE("OF", offset);
    WRITE("SZ", size);
}

static void to_json(json& j, const BulletTracers& o, const BulletTracers& dummy = {})
{
    to_json(j, static_cast<const ColorToggle&>(o), dummy);
}

static void to_json(json& j, const Projectile& o, const Projectile& dummy = {})
{
    j = static_cast<const Shared&>(o);

    WRITE("Trails", trails);
}

static void to_json(json& j, const ImVec2& o, const ImVec2& dummy = {})
{
    WRITE("X", x);
    WRITE("Y", y);
}

static void to_json(json& j, const Config::Ragebot& o, const Config::Ragebot& dummy = {})
{

}

static void to_json(json& j, const Config::Chams::Material& o)
{
    const Config::Chams::Material dummy;

    to_json(j, static_cast<const Color4&>(o), dummy);
    WRITE("Enabled", enabled);
    WRITE("Health based", healthBased);
    WRITE("Blinking", blinking);
    WRITE("Wireframe", wireframe);
    WRITE("Cover", cover);
    WRITE("Ignore-Z", ignorez);
    WRITE("Material", material);
}

static void to_json(json& j, const Config::Chams& o)
{
    j["Materials"] = o.materials;
}

static void to_json(json& j, const Config::GlowItem& o, const  Config::GlowItem& dummy = {})
{
    to_json(j, static_cast<const Color4&>(o), dummy);
    WRITE("Enabled", enabled);
    WRITE("Health based", healthBased);
    WRITE("Style", style);
}

static void to_json(json& j, const  Config::PlayerGlow& o, const  Config::PlayerGlow& dummy = {})
{
    WRITE("All", all);
    WRITE("Visible", visible);
    WRITE("Occluded", occluded);
}

static void to_json(json& j, const Config::StreamProofESP& o, const Config::StreamProofESP& dummy = {})
{
    j["Allies"] = o.allies;
    j["Enemies"] = o.enemies;
    j["Weapons"] = o.weapons;
    j["Projectiles"] = o.projectiles;
    j["Loot Crates"] = o.lootCrates;
    j["Other Entities"] = o.otherEntities;
}

static void to_json(json& j, const Config::ConditionalAA& o, const Config::ConditionalAA& dummy = {})
{
    WRITE("G", global);
    WRITE("S", standing);
    WRITE("M", moving);
    WRITE("SW", slowwalk);
    WRITE("C", chrouching);
    WRITE("J", jumping);
    WRITE("CJ", cjumping);
}

static void to_json(json& j, const Config::RageAntiAimConfig& o, const Config::RageAntiAimConfig& dummy = {})
{
    //regular
    WRITE("OV", enabled);
    WRITE("PH", pitch);
    WRITE_ENUM("YB", yawBase);
    WRITE("YM", yawModifier);
    WRITE("YD", yawAdd);
    WRITE("JR", jitterRange);
    WRITE("SB", spinBase);
    WRITE("AT", atTargets);
    //desync
    WRITE("DC", desync.enabled);
    WRITE("LL", desync.leftLimit);
    WRITE("RL", desync.rightLimit);
    WRITE("PM", desync.peekMode);
    WRITE("LM", desync.lbyMode);
}

static void to_json(json& j, const Config::Fakelag& o, const Config::Fakelag& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Mode", mode);
    WRITE("Limit", limit);
}

static void to_json(json& j, const Config::Tickbase& o, const Config::Tickbase& dummy = {})
{
    WRITE("Teleport", teleport);
}

static void to_json(json& j, const Config::Backtrack& o, const Config::Backtrack& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Ignore smoke", ignoreSmoke);
    WRITE("Ignore flash", ignoreFlash);
    WRITE("Time limit", timeLimit);
    WRITE("Fake Latency", fakeLatency);
    WRITE("Fake Latency Amount", fakeLatencyAmount);
}

static void to_json(json& j, const Config::Misc::SpectatorList& o, const Config::Misc::SpectatorList& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("No Title Bar", noTitleBar);

    if (const auto window = ImGui::FindWindowByName("Spectator list")) {
        j["Pos"] = window->Pos;
    }
}

static void to_json(json& j, const Config::Misc::KeyBindList& o, const Config::Misc::KeyBindList& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("No Title Bar", noTitleBar);

    if (const auto window = ImGui::FindWindowByName("Keybind list")) {
        j["Pos"] = window->Pos;
    }
}

static void to_json(json& j, const Config::Misc::PlayerList& o, const Config::Misc::PlayerList& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Steam ID", steamID);
    WRITE("Rank", rank);
    WRITE("Wins", wins);
    WRITE("Money", money);
    WRITE("Health", health);
    WRITE("Armor", armor);

    if (const auto window = ImGui::FindWindowByName("Player List")) {
        j["Pos"] = window->Pos;
    }
}

static void to_json(json& j, const Config::Misc::Watermark& o, const Config::Misc::Watermark& dummy = {})
{
    WRITE("Enabled", enabled);

    if (const auto window = ImGui::FindWindowByName("Watermark")) {
        j["Pos"] = window->Pos;
    }
}

static void to_json(json& j, const PreserveKillfeed& o, const PreserveKillfeed& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Only Headshots", onlyHeadshots);
}

static void to_json(json& j, const KillfeedChanger& o, const KillfeedChanger& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Headshot", headshot);
    WRITE("Dominated", dominated);
    WRITE("Revenge", revenge);
    WRITE("Penetrated", penetrated);
    WRITE("Noscope", noscope);
    WRITE("Thrusmoke", thrusmoke);
    WRITE("Attackerblind", attackerblind);
}

static void to_json(json& j, const AutoBuy& o, const AutoBuy& dummy = {})
{
    WRITE("Enabled", enabled);
    WRITE("Primary weapon", primaryWeapon);
    WRITE("Secondary weapon", secondaryWeapon);
    WRITE("Armor", armor);
    WRITE("Utility", utility);
    WRITE("Grenades", grenades);
}

static void to_json(json& j, const Config::Misc::Logger& o, const Config::Misc::Logger& dummy = {})
{
    WRITE("Modes", modes);
    WRITE("Events", events);
}

static void to_json(json& j, const Config::Visuals::FootstepESP& o, const Config::Visuals::FootstepESP& dummy)
{
    WRITE("Enabled", footstepBeams);
    WRITE("Thickness", footstepBeamThickness);
    WRITE("Radius", footstepBeamRadius);
}

static void to_json(json& j, const Config::Visuals::MotionBlur& o, const Config::Visuals::MotionBlur& dummy)
{
    WRITE("Enabled", enabled);
    WRITE("Forward", forwardEnabled);
    WRITE("Falling min", fallingMin);
    WRITE("Falling max", fallingMax);
    WRITE("Falling intensity", fallingIntensity);
    WRITE("Rotation intensity", rotationIntensity);
    WRITE("Strength", strength);
}

static void to_json(json& j, const Config::Visuals::Fog& o, const Config::Visuals::Fog& dummy)
{
    WRITE("Start", start);
    WRITE("End", end);
    WRITE("Density", density);
}

static void to_json(json& j, const Config::Visuals::ShadowsChanger& o, const Config::Visuals::ShadowsChanger& dummy)
{
    WRITE("Enabled", enabled);
    WRITE("X", x);
    WRITE("Y", y);
}

static void to_json(json& j, const Config::Visuals::Viewmodel& o, const Config::Visuals::Viewmodel& dummy)
{
    WRITE("Enabled", enabled);
    WRITE("Fov", fov);
    WRITE("X", x);
    WRITE("Y", y);
    WRITE("Z", z);
    WRITE("Roll", roll);
}

static void to_json(json& j, const Config::Visuals::MolotovPolygon& o, const Config::Visuals::MolotovPolygon& dummy)
{
    WRITE("Enabled", enabled);
    WRITE("Self", self);
    WRITE("Team", team);
    WRITE("Enemy", enemy);
}

static void to_json(json& j, const Config::Visuals::OnHitHitbox& o, const Config::Visuals::OnHitHitbox& dummy)
{
    WRITE("Color", color);
    WRITE("Duration", duration);
}

static void to_json(json& j, const Config::Misc& o)
{
    const Config::Misc dummy;

    WRITE("Menu key", menuKey);
    WRITE("Anti AFK kick", antiAfkKick);
    WRITE("Adblock", adBlock);
    WRITE("Force relay", forceRelayCluster);
    WRITE("Auto strafe", autoStrafe);
    WRITE("Bunny hop", bunnyHop);
    WRITE("Fast duck", fastDuck);
    WRITE("Moonwalk", moonwalk);
    WRITE("Edge Jump", edgeJump);
    WRITE("Slowwalk", slowwalk);
    WRITE("Slowwalk Amnt", slowwalkAmnt);
    WRITE("Fake duck", fakeduck);
    WRITE("Auto peek", autoPeek);
    WRITE("Force crosshair", forceCrosshair);
    WRITE("Auto pistol", autoPistol);
    WRITE("Auto reload", autoReload);
    WRITE("Auto accept", autoAccept);
    WRITE("Radar hack", radarHack);
    WRITE("Reveal ranks", revealRanks);
    WRITE("Reveal money", revealMoney);
    WRITE("Reveal suspect", revealSuspect);
    WRITE("Reveal votes", revealVotes);
    WRITE("Chat revealer", chatRevealer);
    WRITE("Spectator list", spectatorList);
    WRITE("Keybind list", keybindList);
    WRITE("Player list", playerList);
    WRITE("Watermark", watermark);
    WRITE("Offscreen Enemies", offscreenEnemies);
    WRITE("Disable model occlusion", disableModelOcclusion);
    WRITE("Aspect Ratio", aspectratio);
    WRITE("Kill message", killMessage);
    WRITE("Kill message string", killMessageString);
    WRITE("Name stealer", nameStealer);
    WRITE("Disable HUD blur", disablePanoramablur);
    WRITE("Fast plant", fastPlant);
    WRITE("Fast Stop", fastStop);
    WRITE("Bomb timer", bombTimer);
    WRITE("Hurt indicator", hurtIndicator);
    WRITE("Prepare revolver", prepareRevolver);
    WRITE("Hit sound", hitSound);
    WRITE("Quick healthshot key", quickHealthshotKey);
    WRITE("Grenade predict", nadePredict);
    WRITE("Grenade predict Damage", nadeDamagePredict);
    WRITE("Grenade predict Trail", nadeTrailPredict);
    WRITE("Grenade predict Circle", nadeCirclePredict);
    WRITE("Max angle delta", maxAngleDelta);
    WRITE("Fix tablet signal", fixTabletSignal);
    WRITE("Custom Hit Sound", customHitSound);
    WRITE("Kill sound", killSound);
    WRITE("Custom Kill Sound", customKillSound);
    WRITE("Preserve Killfeed", preserveKillfeed);
    WRITE("Killfeed changer", killfeedChanger);
    WRITE("Sv pure bypass", svPureBypass);
    WRITE("Inventory Unlocker", inventoryUnlocker);
    WRITE("Unlock hidden cvars", unhideConvars);
    WRITE("Autobuy", autoBuy);
    WRITE("Logger", logger);
    WRITE("Logger options", loggerOptions);
}

static void to_json(json& j, const Config::Visuals& o)
{
    const Config::Visuals dummy;

    WRITE("Disable post-processing", disablePostProcessing);
    WRITE("Inverse ragdoll gravity", inverseRagdollGravity);
    WRITE("No fog", noFog);
    WRITE("Fog controller", fog);
    WRITE("Fog options", fogOptions);
    WRITE("No 3d sky", no3dSky);
    WRITE("No aim punch", noAimPunch);
    WRITE("No view punch", noViewPunch);
    WRITE("No view bob", noViewBob);
    WRITE("No hands", noHands);
    WRITE("No sleeves", noSleeves);
    WRITE("No weapons", noWeapons);
    WRITE("No smoke", noSmoke);
    WRITE("Smoke circle", smokeCircle);
    WRITE("Wireframe smoke", wireframeSmoke);
    WRITE("No molotov", noMolotov);
    WRITE("Wireframe molotov", wireframeMolotov);
    WRITE("No blur", noBlur);
    WRITE("No scope overlay", noScopeOverlay);
    WRITE("No grass", noGrass);
    WRITE("Shadows changer", shadowsChanger);
    WRITE("Motion Blur", motionBlur);
    WRITE("Full bright", fullBright);
    WRITE("Thirdperson Transparency", thirdpersonTransparency);
    WRITE("Thirdperson", thirdperson.enable);
    WRITE("Thirdperson distance", thirdperson.distance);
    WRITE("cm slider", cmslider);
    WRITE("FOV", fov);
    WRITE("Far Z", farZ);
    WRITE("Flash reduction", flashReduction);
    WRITE("Skybox", skybox);
    WRITE("World", world);
    WRITE("Props", props);
    WRITE("Sky", sky);
    WRITE("Custom skybox", customSkybox);
    WRITE("Deagle spinner", deagleSpinner);
    WRITE("Screen effect", screenEffect);
    WRITE("Hit effect", hitEffect);
    WRITE("Hit effect time", hitEffectTime);
    WRITE("Hit marker", hitMarker);
    WRITE("Hit marker time", hitMarkerTime);
    WRITE("Playermodel T", playerModelT);
    WRITE("msc", modelscalecombo);
    WRITE("Playermodel CT", playerModelCT);
    if (o.playerModel[0])
        j["Custom Playermodel"] = o.playerModel;
    WRITE("Bullet Tracers", bulletTracers);
    WRITE("Bullet Impacts", bulletImpacts);
    WRITE("Hitbox on Hit", onHitHitbox);
    WRITE("Bullet Impacts time", bulletImpactsTime);
    WRITE("Molotov Hull", molotovHull);
    WRITE("Smoke Hull", smokeHull);
    WRITE("Molotov Polygon", molotovPolygon);
    WRITE("Viewmodel", viewModel);
    WRITE("Spread circle", spreadCircle);
    WRITE("Asus walls", asusWalls);
    WRITE("Asus props", asusProps);
    WRITE("Smoke timer", smokeTimer);
    WRITE("Smoke timer BG", smokeTimerBG);
    WRITE("Smoke timer TIMER", smokeTimerTimer);
    WRITE("Smoke timer TEXT", smokeTimerText);
    WRITE("Molotov timer", molotovTimer);
    WRITE("Molotov timer BG", molotovTimerBG);
    WRITE("Molotov timer TIMER", molotovTimerTimer);
    WRITE("Molotov timer TEXT", molotovTimerText);
    WRITE("Console Color", console);
    WRITE("Smoke Color", smokeColor);
    WRITE("Molotov Color", molotovColor);
    WRITE("Footstep", footsteps);
}

static void to_json(json& j, const ImVec4& o)
{
    j[0] = o.x;
    j[1] = o.y;
    j[2] = o.z;
    j[3] = o.w;
}

static void to_json(json& j, const sticker_setting& o)
{
    const sticker_setting dummy;

    WRITE("Kit", kit);
    WRITE("Wear", wear);
    WRITE("Scale", scale);
    WRITE("Rotation", rotation);
}

static void to_json(json& j, const item_setting& o)
{
    const item_setting dummy;

    WRITE("Enabled", enabled);
    WRITE("Definition index", itemId);
    WRITE("Quality", quality);
    WRITE("Paint Kit", paintKit);
    WRITE("Definition override", definition_override_index);
    WRITE("Seed", seed);
    WRITE("StatTrak", stat_trak);
    WRITE("Wear", wear);
    if (o.custom_name[0])
        j["Custom name"] = o.custom_name;
    WRITE("Stickers", stickers);
}

#pragma endregion

void removeEmptyObjects(json& j) noexcept
{
    for (auto it = j.begin(); it != j.end();) {
        auto& val = it.value();
        if (val.is_object() || val.is_array())
            removeEmptyObjects(val);
        if (val.empty() && !j.is_array())
            it = j.erase(it);
        else
            ++it;
    }
}

void Config::save(size_t id) const noexcept
{
    createConfigDir();

    if (std::ofstream out{ path / (const char8_t*)configs[id].c_str() }; out.good()) {
        json j;

        j["RagebotConfig"] = rageBot;

        j["AA"] = rageAntiAim;
        j["CAA"] = condAA;
        j["Disable in freezetime"] = disableInFreezetime;
        j["Fakelag"] = fakelag;
        j["Tickbase"] = tickbase;
        j["Backtrack"] = backtrack;

        j["Glow"]["Items"] = glow;
        j["Glow"]["Players"] = playerGlow;

        j["Chams"] = chams;
        j["ESP"] = streamProofESP;
        j["Sound"] = ::Sound::toJson();
        j["Visuals"] = visuals;
        j["Misc"] = misc;
        j["Skin changer"] = skinChanger;

        removeEmptyObjects(j);
        out << std::setw(2) << j;
    }
}

void Config::add(const char* name) noexcept
{
    if (*name && std::find(configs.cbegin(), configs.cend(), name) == configs.cend()) {
        configs.emplace_back(name);
        save(configs.size() - 1);
    }
}

void Config::remove(size_t id) noexcept
{
    std::error_code ec;
    std::filesystem::remove(path / (const char8_t*)configs[id].c_str(), ec);
    configs.erase(configs.cbegin() + id);
}

void Config::rename(size_t item, const char* newName) noexcept
{
    std::error_code ec;
    std::filesystem::rename(path / (const char8_t*)configs[item].c_str(), path / (const char8_t*)newName, ec);
    configs[item] = newName;
}

void Config::reset() noexcept
{
    ragebot = { };
    rageAntiAim = { };
    disableInFreezetime = true;
    fakelag = { };
    tickbase = { };
    backtrack = { };
    Glow::resetConfig();
    chams = { };
    streamProofESP = { };
    visuals = { };
    skinChanger = { };
    Sound::resetConfig();
    misc = { };
}

void Config::listConfigs() noexcept
{
    configs.clear();

    std::error_code ec;
    std::transform(std::filesystem::directory_iterator{ path, ec },
        std::filesystem::directory_iterator{ },
        std::back_inserter(configs),
        [](const auto& entry) { return std::string{ (const char*)entry.path().filename().u8string().c_str() }; });
}

void Config::createConfigDir() const noexcept
{
    std::error_code ec; std::filesystem::create_directory(path, ec);
}

void Config::openConfigDir() const noexcept
{
    createConfigDir();
    ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Config::scheduleFontLoad(const std::string& name) noexcept
{
    scheduledFonts.push_back(name);
}

static auto getFontData(const std::string& fontName) noexcept
{
    HFONT font = CreateFontA(0, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH, fontName.c_str());

    std::unique_ptr<std::byte[]> data;
    DWORD dataSize = GDI_ERROR;

    if (font) {
        HDC hdc = CreateCompatibleDC(nullptr);

        if (hdc) {
            SelectObject(hdc, font);
            dataSize = GetFontData(hdc, 0, 0, nullptr, 0);

            if (dataSize != GDI_ERROR) {
                data = std::make_unique<std::byte[]>(dataSize);
                dataSize = GetFontData(hdc, 0, 0, data.get(), dataSize);

                if (dataSize == GDI_ERROR)
                    data.reset();
            }
            DeleteDC(hdc);
        }
        DeleteObject(font);
    }
    return std::make_pair(std::move(data), dataSize);
}
