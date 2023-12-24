#define NOMINMAX

#include "../imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui/imgui_internal.h"

#include "../Config.h"
#include "../fnv.h"
#include "../GameData.h"
#include "../Helpers.h"
#include "../Memory.h"
#include "StreamProofESP.h"

#include "../SDK/Engine.h"
#include "../SDK/GlobalVars.h"

#include <limits>
#include <tuple>
#include "../imgui/imgui_freetype.h"
#include "../GUI.h"
#include "../SDK/Surface.h"
#include "../Hooks.h"
#include "../GameData.h"
#include "../xor.h"
#include "../fonts.h"
static constexpr auto operator-(float sub, const std::array<float, 3>& a) noexcept
{
    return Vector{ sub - a[0], sub - a[1], sub - a[2] };
}
Color4 dormant;
bool isDromant{ false };
struct BoundingBox {
private:
    bool valid;
public:
    ImVec2 min, max;
    std::array<ImVec2, 8> vertices;

    BoundingBox(const Vector& mins, const Vector& maxs, const std::array<float, 3>& scale, const matrix3x4* matrix = nullptr) noexcept
    {
        min.y = min.x = std::numeric_limits<float>::max();
        max.y = max.x = -std::numeric_limits<float>::max();

        const auto scaledMins = mins + (maxs - mins) * 2 * (0.25f - scale);
        const auto scaledMaxs = maxs - (maxs - mins) * 2 * (0.25f - scale);

        for (int i = 0; i < 8; ++i) {
            const Vector point{ i & 1 ? scaledMaxs.x : scaledMins.x,
                                i & 2 ? scaledMaxs.y : scaledMins.y,
                                i & 4 ? scaledMaxs.z : scaledMins.z };

            if (!Helpers::worldToScreen(matrix ? point.transform(*matrix) : point, vertices[i])) {
                valid = false;
                return;
            }

            min.x = std::min(min.x, vertices[i].x);
            min.y = std::min(min.y, vertices[i].y);
            max.x = std::max(max.x, vertices[i].x);
            max.y = std::max(max.y, vertices[i].y);
        }
        valid = true;
    }

    BoundingBox(const BaseData& data, const std::array<float, 3>& scale) noexcept : BoundingBox{ data.obbMins, data.obbMaxs, scale, &data.coordinateFrame } {}
    BoundingBox(const Vector& center) noexcept : BoundingBox{ center - 2.0f, center + 2.0f, { 0.25f, 0.25f, 0.25f } } {}

    operator bool() const noexcept
    {
        return valid;
    }
};

static ImDrawList* drawList;
static Entity* entity;
static void addLineWithShadow(const ImVec2& p1, const ImVec2& p2, ImU32 col) noexcept
{
    drawList->AddLine(p1 + ImVec2{ 1.0f, 1.0f }, p2 + ImVec2{ 1.0f, 1.0f }, col & IM_COL32_A_MASK);
    drawList->AddLine(p1, p2, col);
}

// convex hull using Graham's scan
static std::pair<std::array<ImVec2, 8>, std::size_t> convexHull(std::array<ImVec2, 8> points) noexcept
{
    std::swap(points[0], *std::min_element(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.y < b.y || (a.y == b.y && a.x < b.x); }));

    constexpr auto orientation = [](const ImVec2& a, const ImVec2& b, const ImVec2& c) {
        return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        };

    std::sort(points.begin() + 1, points.end(), [&](const auto& a, const auto& b) {
        const auto o = orientation(points[0], a, b);
        return o == 0.0f ? ImLengthSqr(points[0] - a) < ImLengthSqr(points[0] - b) : o < 0.0f;
        });

    std::array<ImVec2, 8> hull;
    std::size_t count = 0;

    for (const auto& p : points) {
        while (count >= 2 && orientation(hull[count - 2], hull[count - 1], p) >= 0.0f)
            --count;
        hull[count++] = p;
    }

    return std::make_pair(hull, count);
}

static void addRectFilled(const ImVec2& p1, const ImVec2& p2, ImU32 col, bool shadow) noexcept
{
    if (shadow)
        drawList->AddRectFilled(p1 + ImVec2{ 1.0f, 1.0f }, p2 + ImVec2{ 1.0f, 1.0f }, col & IM_COL32_A_MASK);
    drawList->AddRectFilled(p1, p2, col);
}

static void renderBox(const BoundingBox& bbox, const Box& config) noexcept
{
    if (!config.enabled)
        return;

    const ImU32 color = Helpers::calculateColor(config);
    const ImU32 fillColor = Helpers::calculateColor(config.fill);

    switch (config.type) {
    case Box::_2d:
        if (config.fill.enabled)
            drawList->AddRectFilled(bbox.min + ImVec2{ 1.0f, 1.0f }, bbox.max - ImVec2{ 1.0f, 1.0f }, fillColor);
        drawList->AddRect(bbox.min + ImVec2{ 2.0f, 2.0f }, bbox.max - ImVec2{ 2.0f, 2.0f }, color & IM_COL32_A_MASK);
        drawList->AddRect(bbox.min, bbox.max, color & IM_COL32_A_MASK);
        drawList->AddRect(bbox.min + ImVec2{ 1.0f, 1.0f }, bbox.max - ImVec2{ 1.0f, 1.0f }, color);
        break;
    case Box::_2dCorners: {
        if (config.fill.enabled) {
            drawList->AddRectFilled(bbox.min + ImVec2{ 1.0f, 1.0f }, bbox.max - ImVec2{ 1.0f, 1.0f }, fillColor, config.rounding, ImDrawFlags_RoundCornersAll);
        }

        const bool wantsShadow = !config.fill.enabled;

        const auto quarterWidth = IM_FLOOR((bbox.max.x - bbox.min.x) * 0.25f);
        const auto quarterHeight = IM_FLOOR((bbox.max.y - bbox.min.y) * 0.25f);

        addRectFilled(bbox.min, { bbox.min.x + 1.0f, bbox.min.y + quarterHeight }, color, wantsShadow);
        addRectFilled(bbox.min, { bbox.min.x + quarterWidth, bbox.min.y + 1.0f }, color, wantsShadow);

        addRectFilled({ bbox.max.x, bbox.min.y }, { bbox.max.x - quarterWidth, bbox.min.y + 1.0f }, color, wantsShadow);
        addRectFilled({ bbox.max.x - 1.0f, bbox.min.y }, { bbox.max.x, bbox.min.y + quarterHeight }, color, wantsShadow);

        addRectFilled({ bbox.min.x, bbox.max.y }, { bbox.min.x + 1.0f, bbox.max.y - quarterHeight }, color, wantsShadow);
        addRectFilled({ bbox.min.x, bbox.max.y - 1.0f }, { bbox.min.x + quarterWidth, bbox.max.y }, color, wantsShadow);

        addRectFilled(bbox.max, { bbox.max.x - quarterWidth, bbox.max.y - 1.0f }, color, wantsShadow);
        addRectFilled(bbox.max, { bbox.max.x - 1.0f, bbox.max.y - quarterHeight }, color, wantsShadow);
        break;
    }
    }
}

static ImVec2 renderText(const Color4& textCfg, const char* text, const ImVec2& pos, bool centered, bool adjustHeight = true, bool outline = true, bool dropshadow = true) noexcept
{
    const auto textSize = ImGui::CalcTextSize(text);

    const auto horizontalOffset = centered ? textSize.x / 2 : 0.0f;
    const auto verticalOffset = adjustHeight ? textSize.y : 0.0f;

    const auto color = Helpers::calculateColor(textCfg);
    if (outline)
    {
        drawList->AddText({ pos.x - horizontalOffset + 1.0f, pos.y + verticalOffset - 1.0f }, color & IM_COL32_A_MASK, text);
        drawList->AddText({ pos.x - horizontalOffset - 1.0f, pos.y + verticalOffset - 1.0f }, color & IM_COL32_A_MASK, text);
        drawList->AddText({ pos.x - horizontalOffset + 1.0f, pos.y + verticalOffset + 1.0f }, color & IM_COL32_A_MASK, text);
        drawList->AddText({ pos.x - horizontalOffset - 1.0f, pos.y + verticalOffset + 1.0f }, color & IM_COL32_A_MASK, text);
        drawList->AddText({ pos.x - horizontalOffset + 1.0f, pos.y + verticalOffset }, color & IM_COL32_A_MASK, text);
        drawList->AddText({ pos.x - horizontalOffset - 1.0f, pos.y + verticalOffset }, color & IM_COL32_A_MASK, text);
    }
    else if (dropshadow)
    {
        drawList->AddText({ pos.x - horizontalOffset + 1.0f, pos.y + verticalOffset + 1.0f }, color & IM_COL32_A_MASK, text);
    }
    drawList->AddText({ pos.x - horizontalOffset, pos.y + verticalOffset }, color, text);

    return textSize;
}

static void drawSnapline(const Snapline& config, const ImVec2& min, const ImVec2& max) noexcept
{
    if (!config.enabled)
        return;

    const auto& screenSize = ImGui::GetIO().DisplaySize;

    ImVec2 p1, p2;
    p1.x = screenSize.x / 2;
    p2.x = (min.x + max.x) / 2;

    switch (config.type) {
    case Snapline::Bottom:
        p1.y = screenSize.y;
        p2.y = max.y;
        break;
    case Snapline::Top:
        p1.y = 0.0f;
        p2.y = min.y;
        break;
    case Snapline::Crosshair:
        p1.y = screenSize.y / 2;
        p2.y = (min.y + max.y) / 2;
        break;
    default:
        return;
    }

    drawList->AddLine(p1, p2, Helpers::calculateColor(config), config.thickness);
}

static void drawHealthBar(const HealthBar& config, const ImVec2& pos, float height, int health) noexcept
{
    if (!config.enabled)
        return;

    Player confige;
    constexpr float width = 2.f;
    auto alpha = config.color[3];
    drawList->AddRectFilled(pos + ImVec2{ -4.f, 0 }, pos + ImVec2{ 0.f, height }, IM_COL32(0, 0, 0, alpha * 255));
    //drawList->AddRectFilled(pos, pos + ImVec2{ 3.2f, height }, Helpers::calculateColor( 0 , 0 , 0 , static_cast<int>(alpha*255.f) ));
    if (config.type == HealthBar::Gradient) {
        const auto green = Helpers::calculateColor(config.TopGradient);
        const auto yellow = Helpers::calculateColor(config.BottomGradient);
        drawList->AddRectFilledMultiColor(pos + ImVec2{ -3.f, (100 - health) / 100.0f * height + 1 }, pos + ImVec2{ -1.f, height - 1 }, green, green, yellow, yellow);
    }
    else {
        const auto color = config.type == HealthBar::HealthBased ? Helpers::healthColor(std::clamp(health / 100.0f, 0.0f, 1.0f)) : Helpers::calculateColor(config);
        drawList->AddRectFilled(pos + ImVec2{ -3.f, (100 - health) / 100.0f * height + 1 }, pos + ImVec2{ -1.f, height - 1 }, color);
    }
}

static void renderPlayerBox(const PlayerData& playerData, const Player& config) noexcept
{
    const BoundingBox bbox{ playerData, config.box.scale };
    if (!bbox)
        return;

    renderBox(bbox, config.box);

    ImVec2 offsetMins{}, offsetMaxs{};

    const auto height1 = (bbox.min.y - bbox.max.y);    const auto height = (bbox.max.y - bbox.min.y);
    float drawPositionBottom = 2;
    float drawPositionBottomEh = 3;
    ImGui::PushFont(Fonts::pixel);
    drawHealthBar(config.healthBar, bbox.min - ImVec2{1.f, 0.f}, height, playerData.health);
    
    if (config.healthBar.enabled && playerData.health < 100)
    {
        if (config.hpside == 0)
        {
            auto black = Color4{ 0.f, 0.f, 0.f, 1.f };
            const auto position = bbox.min - ImVec2{ 5.0f, 0.0f } + ImVec2{ 0.0f, (100 - playerData.health) / 100.0f * height };
            renderText(Color4(), std::to_string(playerData.health).c_str(), { position.x , position.y }, true, false);
        }
        else if (config.hpside == 1)
        {
            auto black = Color4{ 0.f, 0.f, 0.f, 1.f };
            const auto position = bbox.max - ImVec2{ -5.0f, 0.0f + height } + ImVec2{ 0.0f, (100 - playerData.health) / 100.0f * height };
            renderText(black, std::to_string(playerData.health).c_str(), { position.x , position.y }, true, false);
        }
    }
    if (config.flags.enabled) {
        std::ostringstream flags;

        if (playerData.armor && playerData.hasHelmet && config.showArmor)
            flags << skCrypt("HK\n");
        else if (playerData.armor && !playerData.hasHelmet && config.showArmor)
            flags << skCrypt("K\n");
        if (playerData.isFakeDucking && config.showDuck)
            flags << skCrypt("FD\n");
        if (playerData.hasBomb && config.showBomb)
            flags << skCrypt("C4\n");
        if (playerData.hasDefuser && config.showKit)
            flags << skCrypt("KIT\n");
        if (playerData.money && config.showMoney)
            flags << playerData.money << "$\n";

        if (!flags.str().empty())
            renderText(config.flags, flags.str().c_str(), bbox.max - ImVec2{ -3.0f, height }, false, false);
    }
    if (config.name.enabled) {
        Color4 name = config.name;
        const auto textSize = ImGui::CalcTextSize(playerData.name.c_str());
        const auto nameSize = renderText(name, playerData.name.c_str(), { (bbox.min.x + bbox.max.x) / 2, bbox.min.y - 1 - textSize.y }, true, false);
    }

    if (config.flashDuration.enabled && playerData.flashDuration > 0.0f) {
        const auto radius = std::max(5.0f - playerData.distanceToLocal / 600.0f, 1.0f);
        ImVec2 flashDurationPos{ (bbox.min.x + bbox.max.x) / 2, bbox.min.y + offsetMins.y - radius * 1.5f };

        const auto color = Helpers::calculateColor(config.flashDuration);
        constexpr float pi = std::numbers::pi_v<float>;
        drawList->PathArcTo(flashDurationPos + ImVec2{ 1.0f, 1.0f }, radius, pi / 2 - (playerData.flashDuration / 255.0f * pi), pi / 2 + (playerData.flashDuration / 255.0f * pi), 40);
        drawList->PathStroke(color & IM_COL32_A_MASK, false, 0.9f + radius * 0.1f);

        drawList->PathArcTo(flashDurationPos, radius, pi / 2 - (playerData.flashDuration / 255.0f * pi), pi / 2 + (playerData.flashDuration / 255.0f * pi), 40);
        drawList->PathStroke(color, false, 0.9f + radius * 0.1f);

        offsetMins.y -= radius * 2.5f;
    }
    float size;
    if (!playerData.isBomb && !playerData.isGrenade && !playerData.isReloading && !playerData.isKnife)
        size = config.ammobar.enabled ? 6.f : 1.f;
    else
        size = 1.f;
    if (config.weapon.enabled && !playerData.activeWeapon.empty()) {
        Color4 name = config.weapon;
        //const auto weaponTextSize = renderText(isDromant ? dormant : name, playerData.activeWeapon.c_str(), { (bbox.min.x + bbox.max.x) / 2, bbox.max.y + 1 }, true, false);
        const auto weaponTextSize = renderText(config.weapon, playerData.activeWeapon.c_str(), { (bbox.min.x + bbox.max.x) / 2, bbox.max.y + size }, true, false);
    }
    ImGui::PopFont();
    ImGui::PushFont(Fonts::weaponIcons);
    if (config.weaponIcon.enabled && !playerData.activeWeapon.empty())
    {
        Color4 name = config.weapon;
        const auto weaponTextSize = renderText(config.weaponIcon, playerData.activeWeaponIcon.c_str(), { (bbox.min.x + bbox.max.x) / 2, !config.weapon.enabled ? (bbox.max.y + size) : (bbox.max.y + (size + 10.f)) }, true, false, false);
        offsetMaxs.y += weaponTextSize.y + 2.0f;
    }
    ImGui::PopFont();
    ImGui::PushFont(Fonts::pixel);
    if (config.ammobar.enabled && !playerData.activeWeapon.empty() && (!playerData.isBomb && !playerData.isGrenade && !playerData.isReloading && !playerData.isKnife && !playerData.isHealthshot)) {
        std::ostringstream text; text << playerData.clip;
        drawList->AddRectFilled({ bbox.min.x , bbox.max.y + 1.f }, { bbox.max.x, bbox.max.y + 5.f }, Helpers::calculateColor(0,0,0, config.ammobar.color[3] * 255));
        drawList->AddRectFilled({ bbox.min.x + 1 , bbox.max.y + 2.f }, { bbox.max.x - abs(bbox.max.x - bbox.min.x) * (playerData.maxClip - playerData.clip) / playerData.maxClip - 1, bbox.max.y + 4.f }, Helpers::calculateColor(config.ammobar));
        if (playerData.clip != playerData.maxClip)
            renderText(Color4(), text.str().c_str(), { bbox.max.x - abs(bbox.max.x - bbox.min.x) * (playerData.maxClip - playerData.clip) / playerData.maxClip + 6.f, bbox.max.y - 2.f }, true, false);
    }
    ImGui::PopFont();
    drawSnapline(config.snapline, bbox.min + offsetMins, bbox.max + offsetMaxs);
}

static void renderWeaponBox(const WeaponData& weaponData, const Weapon& config) noexcept
{
    const BoundingBox bbox{ weaponData, config.box.scale };

    if (!bbox)
        return;

    renderBox(bbox, config.box);
    drawSnapline(config.snapline, bbox.min, bbox.max);

    ImGui::PushFont(Fonts::pixel);

    if (config.name.enabled && !weaponData.displayName.empty()) {
        renderText(config.name, weaponData.displayName.c_str(), { (bbox.min.x + bbox.max.x) / 2, bbox.min.y - 2 }, true);
    }

    if (config.ammo.enabled && weaponData.clip != -1) {
        const auto text{ std::to_string(weaponData.clip) + " / " + std::to_string(weaponData.reserveAmmo) };
        renderText(config.ammo, text.c_str(), { (bbox.min.x + bbox.max.x) / 2, bbox.max.y + 1 }, true, false);
    }
    ImGui::PopFont();
}

static void renderEntityBox(const BaseData& entityData, const char* name, const Shared& config) noexcept
{
    const BoundingBox bbox{ entityData, config.box.scale };

    if (!bbox)
        return;

    renderBox(bbox, config.box);
    drawSnapline(config.snapline, bbox.min, bbox.max);

    ImGui::PushFont(Fonts::pixel);

    if (config.name.enabled)
        renderText(config.name, name, { (bbox.min.x + bbox.max.x) / 2, bbox.min.y - 5 }, true);

    ImGui::PopFont();
}

static void drawProjectileTrajectory(const Trail& config, const std::vector<std::pair<float, Vector>>& trajectory) noexcept
{
    if (!config.enabled)
        return;

    std::vector<ImVec2> points, shadowPoints;

    const auto color = Helpers::calculateColor(config);

    for (const auto& [time, point] : trajectory) {
        if (ImVec2 pos; time + config.time >= memory->globalVars->realtime && Helpers::worldToScreen(point, pos, false)) {
            if (config.type == Trail::Line) {
                points.push_back(pos);
                shadowPoints.push_back(pos + ImVec2{ 1.0f, 1.0f });
            }
            else if (config.type == Trail::Circles) {
                drawList->AddCircle(pos, 3.5f - point.distTo(GameData::local().origin) / 700.0f, color, 12, config.thickness);
            }
            else if (config.type == Trail::FilledCircles) {
                drawList->AddCircleFilled(pos, 3.5f - point.distTo(GameData::local().origin) / 700.0f, color);
            }
        }
    }

    if (config.type == Trail::Line) {
        drawList->AddPolyline(shadowPoints.data(), shadowPoints.size(), color & IM_COL32_A_MASK, false, config.thickness);
        drawList->AddPolyline(points.data(), points.size(), color, false, config.thickness);
    }
}

static void drawPlayerSkeleton(const ColorToggleThickness& config, const std::vector<std::pair<Vector, Vector>>& bones) noexcept
{
    if (!config.enabled)
        return;

    const auto color = Helpers::calculateColor(config);

    std::vector<std::pair<ImVec2, ImVec2>> points, shadowPoints;

    for (const auto& [bone, parent] : bones) {
        ImVec2 bonePoint;
        if (!Helpers::worldToScreen(bone, bonePoint))
            continue;

        ImVec2 parentPoint;
        if (!Helpers::worldToScreen(parent, parentPoint))
            continue;

        points.emplace_back(bonePoint, parentPoint);
        shadowPoints.emplace_back(bonePoint + ImVec2{ 1.0f, 1.0f }, parentPoint + ImVec2{ 1.0f, 1.0f });
    }

    for (const auto& [bonePoint, parentPoint] : shadowPoints)
        drawList->AddLine(bonePoint, parentPoint, color & IM_COL32_A_MASK, config.thickness);

    for (const auto& [bonePoint, parentPoint] : points)
        drawList->AddLine(bonePoint, parentPoint, color, config.thickness);
}

static void drawLineOfSight(const ColorToggleThickness& config, const PlayerData& playerData)
{
    if (!config.enabled)
        return;

    const auto color = Helpers::calculateColor(config);

    ImVec2 start, end;

    bool draw = Helpers::worldToScreen((playerData.headMaxs + playerData.headMins) / 2, start);
    draw = draw && Helpers::worldToScreen(playerData.lookingAt, end);

    if (draw)
    {
        drawList->AddLine(start, end, color, config.thickness);
        drawList->AddCircle(end, 5.0f, color, 0, config.thickness);
    }
}

static bool renderPlayerEsp(const PlayerData& playerData, const Player& playerConfig) noexcept
{
    if (!playerConfig.enabled)
        return false;

    if (playerConfig.audibleOnly && !playerData.audible && !playerConfig.spottedOnly
        || playerConfig.spottedOnly && !playerData.spotted && !(playerConfig.audibleOnly && playerData.audible)) // if both "Audible Only" and "Spotted Only" are on treat them as audible OR spotted
        return true;

    if (playerData.immune)
        Helpers::setAlphaFactor(0.5f);

    Helpers::setAlphaFactor(Helpers::getAlphaFactor() * playerData.fadingAlpha());

    renderPlayerBox(playerData, playerConfig);
    drawPlayerSkeleton(playerConfig.skeleton, playerData.bones);

    if (const BoundingBox headBbox{ playerData.headMins, playerData.headMaxs, playerConfig.headBox.scale })
        renderBox(headBbox, playerConfig.headBox);

    drawLineOfSight(playerConfig.lineOfSight, playerData);

    Helpers::setAlphaFactor(1.0f);

    //ImGui::PopStyleColor();
    return true;
}

static void renderWeaponEsp(const WeaponData& weaponData, const Weapon& parentConfig, const Weapon& itemConfig) noexcept
{
    const auto& config = itemConfig.enabled ? itemConfig : (parentConfig.enabled ? parentConfig : ::config->streamProofESP.weapons["All"]);
    if (config.enabled) {
        renderWeaponBox(weaponData, config);
    }
}

static void renderEntityEsp(const BaseData& entityData, const std::unordered_map<std::string, Shared>& map, const char* name) noexcept
{
    if (const auto cfg = map.find(name); cfg != map.cend() && cfg->second.enabled) {
        renderEntityBox(entityData, name, cfg->second);
    }
    else if (const auto cfg = map.find("All"); cfg != map.cend() && cfg->second.enabled) {
        renderEntityBox(entityData, name, cfg->second);
    }
}

static void renderProjectileEsp(const ProjectileData& projectileData, const Projectile& parentConfig, const Projectile& itemConfig, const char* name) noexcept
{
    const auto& config = itemConfig.enabled ? itemConfig : parentConfig;
    if (config.enabled) {
        if (!projectileData.exploded)
            renderEntityBox(projectileData, name, config);

        if (config.trails.enabled) {
            if (projectileData.thrownByLocalPlayer)
                drawProjectileTrajectory(config.trails.localPlayer, projectileData.trajectory);
            else if (!projectileData.thrownByEnemy)
                drawProjectileTrajectory(config.trails.allies, projectileData.trajectory);
            else
                drawProjectileTrajectory(config.trails.enemies, projectileData.trajectory);
        }
    }
}

void StreamProofESP::render() noexcept
{
    drawList = ImGui::GetBackgroundDrawList();

    GameData::Lock lock;

    for (const auto& weapon : GameData::weapons())
        renderWeaponEsp(weapon, config->streamProofESP.weapons[weapon.group], config->streamProofESP.weapons[weapon.name]);

    for (const auto& entity : GameData::entities())
        renderEntityEsp(entity, config->streamProofESP.otherEntities, entity.name);

    for (const auto& lootCrate : GameData::lootCrates()) {
        if (lootCrate.name)
            renderEntityEsp(lootCrate, config->streamProofESP.lootCrates, lootCrate.name);
    }

    for (const auto& projectile : GameData::projectiles())
        renderProjectileEsp(projectile, config->streamProofESP.projectiles["All"], config->streamProofESP.projectiles[projectile.name], projectile.name);

    Player confige;
    for (const auto& player : GameData::players()) {
        if (!player.alive || !player.inViewFrustum)
            continue;

        if (player.dormant && confige.dormant.enabled)
        {
            dormant = confige.dormant;
            isDromant = true;
        }
        else
            isDromant = false;

        auto& playerConfig = player.enemy ? config->streamProofESP.enemies : config->streamProofESP.allies;

        if (!renderPlayerEsp(player, playerConfig["All"]))
            renderPlayerEsp(player, playerConfig[player.visible ? "Visible" : "Occluded"]);
    }
}
