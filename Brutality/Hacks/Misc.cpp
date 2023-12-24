#include <mutex>
#include <numeric>
#include <sstream>
#include <string>

#include "../imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../imgui/imgui_internal.h"

#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../Netvars.h"
#include "../GUI.h"
#include "../Helpers.h"
#include "../GameData.h"

#include "EnginePrediction.h"
#include "Misc.h"

#include "../SDK/Client.h"
#include "../SDK/ClientMode.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/FrameStage.h"
#include "../SDK/GameEvent.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Input.h"
#include "../SDK/ItemSchema.h"
#include "../SDK/Localize.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/Panorama.h"
#include "../SDK/PlayerResource.h"
#include "../SDK/Prediction.h"
#include "../SDK/Surface.h"
#include "../SDK/UserCmd.h"
#include "../SDK/ViewSetup.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponSystem.h"

#include "../imguiCustom.h"
#include "../xor.h"
#include "Tickbase.h"
#include "../RenderSystem.h"
#include "../Hooks.h"

bool Misc::isInChat() noexcept
{
    if (!localPlayer)
        return false;

    const auto hudChat = memory->findHudElement(memory->hud, c_xor("CCSGO_HudChat"));
    if (!hudChat)
        return false;

    const bool isInChat = *(bool*)((uintptr_t)hudChat + 0xD);

    return isInChat;
}

std::string currentForwardKey = "";
std::string currentBackKey = "";
std::string currentRightKey = "";
std::string currentLeftKey = "";
int currentButtons = 0;
Vector viewAngles{ };

void Misc::gatherDataOnTick(UserCmd* cmd) noexcept
{
    currentButtons = cmd->buttons;
    viewAngles = cmd->viewangles;
}

void Misc::handleKeyEvent(int keynum, const char* currentBinding) noexcept
{
    if (!currentBinding || keynum <= 0 || keynum >= ARRAYSIZE(ButtonCodes))
        return;

    const auto buttonName = ButtonCodes[keynum];

    switch (fnv::hash(currentBinding))
    {
    case fnv::hash("+forward"):
        currentForwardKey = std::string(buttonName);
        break;
    case fnv::hash("+back"):
        currentBackKey = std::string(buttonName);
        break;
    case fnv::hash("+moveright"):
        currentRightKey = std::string(buttonName);
        break;
    case fnv::hash("+moveleft"):
        currentLeftKey = std::string(buttonName);
        break;
    default:
        break;
    }
}

void Misc::drawPlayerList() noexcept
{
    if (!config->misc.playerList.enabled)
        return;

    if (config->misc.playerList.pos != ImVec2{}) {
        ImGui::SetNextWindowPos(config->misc.playerList.pos);
        config->misc.playerList.pos = {};
    }

    static bool changedName = true;
    static std::string nameToChange = "";

    if (!changedName && nameToChange != "")
        changedName = changeName(false, (nameToChange + '\x1').c_str(), 1.0f);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!gui->isOpen())
    {
        windowFlags |= ImGuiWindowFlags_NoInputs;
        return;
    }

    GameData::Lock lock;
    if ((GameData::players().empty()) && !gui->isOpen())
        return;

    ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f), ImGuiCond_Once);

    if (ImGui::Begin("Player List", nullptr, windowFlags)) {
        if (ImGui::beginTable("", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 120.0f);
            ImGui::TableSetupColumn("Steam ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Wins", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Armor", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Money", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetColumnEnabled(2, config->misc.playerList.steamID);
            ImGui::TableSetColumnEnabled(3, config->misc.playerList.rank);
            ImGui::TableSetColumnEnabled(4, config->misc.playerList.wins);
            ImGui::TableSetColumnEnabled(5, config->misc.playerList.health);
            ImGui::TableSetColumnEnabled(6, config->misc.playerList.armor);
            ImGui::TableSetColumnEnabled(7, config->misc.playerList.money);

            ImGui::TableHeadersRow();

            std::vector<std::reference_wrapper<const PlayerData>> playersOrdered{ GameData::players().begin(), GameData::players().end() };
            std::ranges::sort(playersOrdered, [](const PlayerData& a, const PlayerData& b) {
                // enemies first
                if (a.enemy != b.enemy)
                    return a.enemy && !b.enemy;

                return a.handle < b.handle;
                });

            ImGui::PushFont(gui->getUnicodeFont());

            for (const PlayerData& player : playersOrdered) {
                ImGui::TableNextRow();
                ImGui::PushID(ImGui::TableGetRowIndex());

                if (ImGui::TableNextColumn())
                    ImGui::Text("%d", player.userId);

                if (ImGui::TableNextColumn())
                {
                    ImGui::Image(player.getAvatarTexture(), { ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight() });
                    ImGui::SameLine();
                    ImGui::textEllipsisInTableCell(player.name.c_str());
                }

                if (ImGui::TableNextColumn() && ImGui::smallButtonFullWidth("Copy", player.steamID == 0))
                    ImGui::SetClipboardText(std::to_string(player.steamID).c_str());

                if (ImGui::TableNextColumn()) {
                    ImGui::Image(player.getRankTexture(), { 2.45f /* -> proportion 49x20px */ * ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight() });
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::PushFont(nullptr);
                        ImGui::TextUnformatted(player.getRankName().data());
                        ImGui::PopFont();
                        ImGui::EndTooltip();
                    }
                    
                }

                if (ImGui::TableNextColumn())
                    ImGui::Text("%d", player.competitiveWins);

                if (ImGui::TableNextColumn()) {
                    if (!player.alive)
                        ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "%s", "Dead");
                    else
                        ImGui::Text("%d HP", player.health);
                }

                if (ImGui::TableNextColumn())
                    ImGui::Text("%d", player.armor);

                if (ImGui::TableNextColumn())
                    ImGui::TextColored({ 0.0f, 1.0f, 0.0f, 1.0f }, "$%d", player.money);

                if (ImGui::TableNextColumn()){
                    if (ImGui::smallButtonFullWidth("...", false))
                        ImGui::OpenPopup("");

                    if (ImGui::BeginPopup("")) {
                        if (ImGui::Button("Steal name"))
                        {
                            changedName = changeName(false, (std::string{ player.name } + '\x1').c_str(), 1.0f);
                            nameToChange = player.name;

                            if (PlayerInfo playerInfo; interfaces->engine->isInGame() && localPlayer
                                && interfaces->engine->getPlayerInfo(localPlayer->index(), playerInfo) && (playerInfo.name == std::string{ "?empty" } || playerInfo.name == std::string{ "\n\xAD\xAD\xAD" }))
                                changedName = false;
                        }

                        if (ImGui::Button("Steal clantag"))
                            memory->setClanTag(player.clanTag.c_str(), player.clanTag.c_str());

                        if (GameData::local().exists && player.team == GameData::local().team && player.steamID != 0)
                        {
                            if (ImGui::Button("Kick"))
                            {
                                const std::string cmd = "callvote kick " + std::to_string(player.userId);
                                interfaces->engine->clientCmdUnrestricted(cmd.c_str());
                            }
                        }

                        ImGui::EndPopup();
                    }
                }

                ImGui::PopID();
            }

            ImGui::PopFont();
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

static int buttons = 0;

void Misc::viewModelChanger(ViewSetup* setup) noexcept
{
    if (!localPlayer)
        return;

    constexpr auto setViewmodel = [](Entity* viewModel, const Vector& angles) constexpr noexcept
    {
        if (viewModel)
        {
            Vector forward = Vector::fromAngle(angles);
            Vector up = Vector::fromAngle(angles - Vector{ 90.0f, 0.0f, 0.0f });
            Vector side = forward.cross(up);
            Vector offset = side * config->visuals.viewModel.x + forward * config->visuals.viewModel.y + up * config->visuals.viewModel.z;
            memory->setAbsOrigin(viewModel, viewModel->getRenderOrigin() + offset);
            memory->setAbsAngle(viewModel, angles + Vector{ 0.0f, 0.0f, config->visuals.viewModel.roll });
        }
    };

    if (localPlayer->isAlive())
    {
        if (config->visuals.viewModel.enabled && !localPlayer->isScoped() && !memory->input->isCameraInThirdPerson)
            setViewmodel(interfaces->entityList->getEntityFromHandle(localPlayer->viewModel()), setup->angles);
    }
    else if (auto observed = localPlayer->getObserverTarget(); observed && localPlayer->getObserverMode() == ObsMode::InEye)
    {
        if (config->visuals.viewModel.enabled && !observed->isScoped())
            setViewmodel(interfaces->entityList->getEntityFromHandle(observed->viewModel()), setup->angles);
    }
}

static Vector peekPosition{};
void RadialGradient3D(Vector pos, float radius, Color4 in, float alpha) noexcept
{
    ImVec2 center; ImVec2 g_pos;
    auto drawList = ImGui::GetBackgroundDrawList();
    Helpers::worldToScreen(Vector(pos), g_pos);
    center = ImVec2(g_pos.x, g_pos.y);
    drawList->_PathArcToFastEx(center, radius, 0, 128, 0);
    const int count = drawList->_Path.Size - 1;
    float step = (3.141592654f * 2.0f) / (count + 1);
    std::vector<ImVec2> point;
    for (float lat = 0.f; lat <= 3.141592654f * 2.0f; lat += step)
    {
        const auto& point3d = Vector(sin(lat), cos(lat), 0.f) * radius;
        ImVec2 point2d;
        if (Helpers::worldToScreen(Vector(pos) + point3d, point2d))
            point.push_back(ImVec2(point2d.x, point2d.y));
    }
    if (in.color[3] == 0 && alpha == 0 || radius < 0.5f || point.size() < count + 1)
        return;

    unsigned int vtx_base = drawList->_VtxCurrentIdx;
    drawList->PrimReserve(count * 3, count + 1);

    // Submit vertices
    const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
    drawList->PrimWriteVtx(center, uv, ImColor(in.color[0], in.color[1], in.color[2], in.color[3]));
    for (int n = 0; n < count; n++)
        drawList->PrimWriteVtx(point[n + 1], uv, ImColor(in.color[0], in.color[1], in.color[2], alpha));

    // Submit a fan of triangles
    for (int n = 0; n < count; n++)
    {
        drawList->PrimWriteIdx((ImDrawIdx)(vtx_base));
        drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
        drawList->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
    }
    drawList->_Path.Size = 0;
}

void Misc::drawAutoPeek(ImDrawList* drawList) noexcept
{
    if (!config->misc.autoPeek.enabled)
        return;

    if (peekPosition.notNull())
    {
        constexpr float step = 3.141592654f * 2.0f / 20.0f;
        std::vector<ImVec2> points;
        for (float lat = 0.f; lat <= 3.141592654f * 2.0f; lat += step)
        {
            const auto& point3d = Vector{ std::sin(lat), std::cos(lat), 0.f } *15.f;
            ImVec2 point2d;
            if (Helpers::worldToScreen(peekPosition + point3d, point2d))
                points.push_back(point2d);
        }

        const ImU32 color = (Helpers::calculateColor(config->misc.autoPeek));
        auto flags_backup = drawList->Flags;
        drawList->Flags |= ImDrawListFlags_AntiAliasedFill;
        RadialGradient3D(peekPosition, 25.f, config->misc.autoPeek, 0.f);
        drawList->Flags = flags_backup;
    }
}

void Misc::autoPeek(UserCmd* cmd, Vector currentViewAngles) noexcept
{
    static bool hasShot = false;

    if (!config->misc.autoPeek.enabled)
    {
        hasShot = false;
        peekPosition = Vector{};
        return;
    }

    if (!localPlayer)
        return;

    if (!localPlayer->isAlive())
    {
        hasShot = false;
        peekPosition = Vector{};
        return;
    }

    if (const auto mt = localPlayer->moveType(); mt == MoveType::LADDER || mt == MoveType::NOCLIP || !(localPlayer->flags() & 1))
        return;

    if (config->binds.autoPeek.isActive())
    {
        if (peekPosition.null())
            peekPosition = localPlayer->getRenderOrigin();

        if (cmd->buttons & UserCmd::IN_ATTACK)
            hasShot = true;

        if (hasShot)
        {
            const float yaw = currentViewAngles.y;
            const auto difference = localPlayer->getRenderOrigin() - peekPosition;

            if (difference.length2D() > 5.0f)
            {
                const auto velocity = Vector{
                    difference.x * std::cos(yaw / 180.0f * 3.141592654f) + difference.y * std::sin(yaw / 180.0f * 3.141592654f),
                    difference.y * std::cos(yaw / 180.0f * 3.141592654f) - difference.x * std::sin(yaw / 180.0f * 3.141592654f),
                    difference.z };

                cmd->forwardmove = -velocity.x * 20.f;
                cmd->sidemove = velocity.y * 20.f;
            }
            else
            {
                hasShot = false;
            }
        }
    }
    else
    {
        hasShot = false;
        peekPosition = Vector{};
    }
}

void Misc::forceRelayCluster() noexcept
{
    const std::string dataCentersList[] = { "", "syd", "vie", "gru", "scl", "dxb", "par", "fra", "hkg",
    "maa", "bom", "tyo", "lux", "ams", "limc", "man", "waw", "sgp", "jnb",
    "mad", "sto", "lhr", "atl", "eat", "ord", "lax", "mwh", "okc", "sea", "iad" };

    *memory->relayCluster = dataCentersList[config->misc.forceRelayCluster];
}

std::vector<conCommandBase*> dev;
std::vector<conCommandBase*> hidden;

void Misc::initHiddenCvars() noexcept {

    conCommandBase* iterator = **reinterpret_cast<conCommandBase***>(interfaces->cvar + 0x34);

    for (auto c = iterator->next; c != nullptr; c = c->next)
    {
        conCommandBase* cmd = c;

        if (cmd->flags & CvarFlags::DEVELOPMENTONLY)
            dev.push_back(cmd);

        if (cmd->flags & CvarFlags::HIDDEN)
            hidden.push_back(cmd);

    }
}

void Misc::unlockHiddenCvars() noexcept {

    static bool toggle = true;

    if (config->misc.unhideConvars == toggle)
        return;

    if (config->misc.unhideConvars) {
        for (unsigned x = 0; x < dev.size(); x++)
            dev.at(x)->flags &= ~CvarFlags::DEVELOPMENTONLY;

        for (unsigned x = 0; x < hidden.size(); x++)
            hidden.at(x)->flags &= ~CvarFlags::HIDDEN;

    }
    if (!config->misc.unhideConvars) {
        for (unsigned x = 0; x < dev.size(); x++)
            dev.at(x)->flags |= CvarFlags::DEVELOPMENTONLY;

        for (unsigned x = 0; x < hidden.size(); x++)
            hidden.at(x)->flags |= CvarFlags::HIDDEN;
    }

    toggle = config->misc.unhideConvars;

}

void Misc::fakeDuck(UserCmd* cmd, bool& sendPacket) noexcept
{
    if (!config->misc.fakeduck || !config->binds.fakeduck.isActive())
        return;

    if (const auto gameRules = (*memory->gameRules); gameRules)
        if (getGameMode() != GameMode::Competitive && gameRules->isValveDS())
            return;

    if (!localPlayer || !localPlayer->isAlive() || !(localPlayer->flags() & 1))
        return;

    const auto netChannel = interfaces->engine->getNetworkChannel();
    if (!netChannel)
        return;

    cmd->buttons |= UserCmd::IN_BULLRUSH;
    const bool crouch = netChannel->chokedPackets >= (maxUserCmdProcessTicks / 2);
    if (crouch)
        cmd->buttons |= UserCmd::IN_DUCK;
    else
        cmd->buttons &= ~UserCmd::IN_DUCK;
    sendPacket = netChannel->chokedPackets >= maxUserCmdProcessTicks;
}


void Misc::edgejump(UserCmd* cmd) noexcept
{
    if (!config->misc.edgeJump || !config->binds.edgeJump.isActive())
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto mt = localPlayer->moveType(); mt == MoveType::LADDER || mt == MoveType::NOCLIP)
        return;

    if ((EnginePrediction::getFlags() & 1) && !(localPlayer->flags() & 1))
        cmd->buttons |= UserCmd::IN_JUMP;
}

void Misc::slowwalk(UserCmd* cmd) noexcept
{
    if (!config->misc.slowwalk || !config->binds.slowwalk.isActive())
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon)
        return;

    const auto weaponData = activeWeapon->getWeaponData();
    if (!weaponData)
        return;

    const float maxSpeed = config->misc.slowwalkAmnt ? config->misc.slowwalkAmnt : (localPlayer->isScoped() ? weaponData->maxSpeedAlt : weaponData->maxSpeed) / 3;

    if (cmd->forwardmove && cmd->sidemove) {
        const float maxSpeedRoot = maxSpeed * static_cast<float>(M_SQRT1_2);
        cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
        cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeedRoot : maxSpeedRoot;
    } else if (cmd->forwardmove) {
        cmd->forwardmove = cmd->forwardmove < 0.0f ? -maxSpeed : maxSpeed;
    } else if (cmd->sidemove) {
        cmd->sidemove = cmd->sidemove < 0.0f ? -maxSpeed : maxSpeed;
    }
}

void Misc::inverseRagdollGravity() noexcept
{
    static auto ragdollGravity = interfaces->cvar->findVar("cl_ragdoll_gravity");
    ragdollGravity->setValue(config->visuals.inverseRagdollGravity ? -600 : 600);
}

void Misc::updateClanTag(bool tagChanged) noexcept
{
    static bool wasEnabled = false;
    static auto clanId = interfaces->cvar->findVar("cl_clanid");
    if (!config->misc.clanTag && wasEnabled)
    {
        interfaces->engine->clientCmdUnrestricted(("cl_clanid " + std::to_string(clanId->getInt())).c_str());
        wasEnabled = false;
    }
    if (!config->misc.clanTag)
    {
        return;
    }

    static std::string clanTag;

    static int lastTime = 0;
    int time = memory->globalVars->currenttime * M_PI;
    if (config->misc.clanTag)
    {
        wasEnabled = true;
        if (time != lastTime)
        {
            switch (time % 9)
            {
            case 0: clanTag = ("❄ Brutality"); break;
            case 1: clanTag = ("❄ y Brutalit"); break;
            case 2: clanTag = ("❄ ty Brutali"); break;
            case 3: clanTag = ("❄ ity Brutal"); break;
            case 4: clanTag = ("❄ lity Bruta"); break;
            case 5: clanTag = ("❄ ality Brut"); break;
            case 6: clanTag = ("❄ tality Bru"); break;
            case 7: clanTag = ("❄ utality Br"); break;
            case 8: clanTag = ("❄ rutality B"); break;
            }
        }
        lastTime = time;
    }
}

void Misc::spectatorList() noexcept
{
    if (!config->misc.spectatorList.enabled)
        return;

    GameData::Lock lock;

    const auto& observers = GameData::observers();

    if (std::ranges::none_of(observers, [](const auto& obs) { return obs.targetIsLocalPlayer; }) && !gui->isOpen())
        return;

    if (config->misc.spectatorList.pos != ImVec2{}) {
        ImGui::SetNextWindowPos(config->misc.spectatorList.pos);
        config->misc.spectatorList.pos = {};
    }

    ImGui::SetNextWindowSize({ 250.f, 0.f }, ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints({ 250.f, 0.f }, { 250.f, FLT_MAX });

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!gui->isOpen())
        windowFlags |= ImGuiWindowFlags_NoInputs;

    if (config->misc.spectatorList.noTitleBar)
        windowFlags |= ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, { 0.5f, 0.5f });
    ImGui::Begin("Spectator list", nullptr, windowFlags);
    ImGui::PopStyleVar();

    for (const auto& observer : observers) {
        if (!observer.targetIsLocalPlayer)
            continue;

        if (const auto it = std::ranges::find(GameData::players(), observer.playerHandle, &PlayerData::handle); it != GameData::players().cend()) {
             if (const auto texture = it->getAvatarTexture()) {
                 const auto textSize = ImGui::CalcTextSize(it->name.c_str());
                 ImGui::Image(texture, ImVec2(textSize.y, textSize.y), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.3f));
                 ImGui::SameLine();
                 ImGui::TextWrapped("%s", it->name.c_str());
            }
        }
    }

    ImGui::End();
}

void Misc::noscopeCrosshair(ImDrawList* drawList) noexcept
{
    static auto showSpread = interfaces->cvar->findVar(skCrypt("weapon_debug_spread_show"));
    if (!config->misc.forceCrosshair)
    {
        showSpread->setValue( 0);
        return;
    }

    showSpread->setValue(localPlayer && !localPlayer->isScoped() ? 3 : 0);
}

void Misc::watermark() noexcept
{
    if (!config->misc.watermark.enabled)
        return;

    if (config->misc.watermark.pos != ImVec2{}) {
        ImGui::SetNextWindowPos(config->misc.watermark.pos);
        config->misc.watermark.pos = {};
    }

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;
    if (!gui->isOpen())
        windowFlags |= ImGuiWindowFlags_NoInputs;

    ImGui::SetNextWindowBgAlpha(0.3f);
    ImGui::Begin("Watermark", nullptr, windowFlags);

    static auto frameRate = 1.0f;
    frameRate = 0.9f * frameRate + 0.1f * memory->globalVars->absoluteFrameTime;

    ImGui::Text("Osiris | %d fps | %d ms", frameRate != 0.0f ? static_cast<int>(1 / frameRate) : 0, GameData::getNetOutgoingLatency());
    ImGui::End();
}

void Misc::prepareRevolver(UserCmd* cmd) noexcept
{
    if (!config->misc.prepareRevolver)
        return;

    if (!localPlayer)
        return;

    if (cmd->buttons & UserCmd::IN_ATTACK)
        return;

    constexpr float revolverPrepareTime{ 0.234375f };

    if (auto activeWeapon = localPlayer->getActiveWeapon(); activeWeapon && activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
    {
        const auto time = memory->globalVars->serverTime();
        if (localPlayer->nextAttack() > time)
            return;

        cmd->buttons &= ~UserCmd::IN_ATTACK2;

        static auto readyTime = time + revolverPrepareTime;
        if (activeWeapon->nextPrimaryAttack() <= time)
        {
            if (readyTime <= time)
            {
                if (activeWeapon->nextSecondaryAttack() <= time)
                    readyTime = time + revolverPrepareTime;
                else
                    cmd->buttons |= UserCmd::IN_ATTACK2;
            }
            else
                cmd->buttons |= UserCmd::IN_ATTACK;
        }
        else
            readyTime = time + revolverPrepareTime;
    }
}

void Misc::fastPlant(UserCmd* cmd) noexcept
{
    if (!config->misc.fastPlant)
        return;

    static auto plantAnywhere = interfaces->cvar->findVar("mp_plant_c4_anywhere");

    if (plantAnywhere->getInt())
        return;

    if (!localPlayer || !localPlayer->isAlive() || (localPlayer->inBombZone() && localPlayer->flags() & 1))
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || activeWeapon->getClientClass()->classId != ClassId::C4)
        return;

    cmd->buttons &= ~UserCmd::IN_ATTACK;

    constexpr auto doorRange = 200.0f;

    Trace trace;
    const auto startPos = localPlayer->getEyePosition();
    const auto endPos = startPos + Vector::fromAngle(cmd->viewangles) * doorRange;
    interfaces->engineTrace->traceRay({ startPos, endPos }, 0x46004009, localPlayer.get(), trace);

    if (!trace.entity || trace.entity->getClientClass()->classId != ClassId::PropDoorRotating)
        cmd->buttons &= ~UserCmd::IN_USE;
}

void Misc::fastStop(UserCmd* cmd) noexcept
{
    if (!config->misc.fastStop)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER || !(localPlayer->flags() & 1) || cmd->buttons & UserCmd::IN_JUMP)
        return;

    if (cmd->buttons & (UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT | UserCmd::IN_FORWARD | UserCmd::IN_BACK))
        return;
    
    const auto velocity = localPlayer->velocity();
    const auto speed = velocity.length2D();
    if (speed < 15.0f)
        return;
    
    Vector direction = velocity.toAngle();
    direction.y = cmd->viewangles.y - direction.y;

    const auto negatedDirection = Vector::fromAngle(direction) * -speed;
    cmd->forwardmove = negatedDirection.x;
    cmd->sidemove = negatedDirection.y;
}

void Misc::drawBombTimer() noexcept
{
    if (!config->misc.bombTimer.enabled)
        return;

    if (!localPlayer)
        return;

    GameData::Lock lock;
    const auto& plantedC4 = GameData::plantedC4();
    if (plantedC4.blowTime == 0.0f && !gui->isOpen())
        return;

    if (!gui->isOpen()) {
        ImGui::SetNextWindowBgAlpha(0.3f);
    }

    static float windowWidth = 200.0f;
    ImGui::SetNextWindowPos({ (ImGui::GetIO().DisplaySize.x - 200.0f) / 2.0f, 60.0f }, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ windowWidth, 0 }, ImGuiCond_Once);

    if (!gui->isOpen())
        ImGui::SetNextWindowSize({ windowWidth, 0 });

    ImGui::SetNextWindowSizeConstraints({ 0, -1 }, { FLT_MAX, -1 });
    ImGui::Begin("Bomb Timer", nullptr, ImGuiWindowFlags_NoTitleBar | (gui->isOpen() ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration));    
    std::ostringstream ss; ss << "Bomb on " << (!plantedC4.bombsite ? 'A' : 'B') << " : " << std::fixed << std::showpoint << std::setprecision(3) << (std::max)(plantedC4.blowTime - memory->globalVars->currenttime, 0.0f) << " s";

    ImGui::textUnformattedCentered(ss.str().c_str());
    
    bool drawDamage = true;

    auto targetEntity = !localPlayer->isAlive() ? localPlayer->getObserverTarget() : localPlayer.get();
    auto bombEntity = interfaces->entityList->getEntityFromHandle(plantedC4.bombHandle);

    if (!bombEntity || bombEntity->isDormant() || bombEntity->getClientClass()->classId != ClassId::PlantedC4)
        drawDamage = false;

    if (!targetEntity || targetEntity->isDormant())
        drawDamage = false;

    constexpr float bombDamage = 500.f;
    constexpr float bombRadius = bombDamage * 3.5f;
    constexpr float sigma = bombRadius / 3.0f;

    constexpr float armorRatio = 0.5f;
    constexpr float armorBonus = 0.5f;

    if (drawDamage) {
        const float armorValue = static_cast<float>(targetEntity->armor());
        const int health = targetEntity->health();

        float finalBombDamage = 0.f;
        float distanceToLocalPlayer = (bombEntity->origin() - targetEntity->origin()).length();
        float gaussianFalloff = exp(-distanceToLocalPlayer * distanceToLocalPlayer / (2.0f * sigma * sigma));

        finalBombDamage = bombDamage * gaussianFalloff;

        if (armorValue > 0) {
            float newRatio = finalBombDamage * armorRatio;
            float armor = (finalBombDamage - newRatio) * armorBonus;

            if (armor > armorValue) {
                armor = armorValue * (1.f / armorBonus);
                newRatio = finalBombDamage - armor;
            }
            finalBombDamage = newRatio;
        }

        int displayBombDamage = static_cast<int>(floor(finalBombDamage));

        if (health <= (truncf(finalBombDamage * 10) / 10)) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
            ImGui::textUnformattedCentered("Lethal");
            ImGui::PopStyleColor();
        }
        else {
            std::ostringstream text; text << "Damage: " << std::clamp(displayBombDamage, 0, health - 1);
            const auto color = Helpers::healthColor(std::clamp(1.f - (finalBombDamage / static_cast<float>(health)), 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::textUnformattedCentered(text.str().c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Helpers::calculateColor(config->misc.bombTimer));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f });
    ImGui::progressBarFullWidth((plantedC4.blowTime - memory->globalVars->currenttime) / plantedC4.timerLength, 5.0f);

    if (plantedC4.defuserHandle != -1) {
        const bool canDefuse = plantedC4.blowTime >= plantedC4.defuseCountDown;

        if (plantedC4.defuserHandle == GameData::local().handle) {
            if (canDefuse) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                ImGui::textUnformattedCentered("You can defuse!");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                ImGui::textUnformattedCentered("You can not defuse!");
            }
            ImGui::PopStyleColor();
        } else if (const auto defusingPlayer = GameData::playerByHandle(plantedC4.defuserHandle)) {
            std::ostringstream ss; ss << defusingPlayer->name << " is defusing: " << std::fixed << std::showpoint << std::setprecision(3) << (std::max)(plantedC4.defuseCountDown - memory->globalVars->currenttime, 0.0f) << " s";

            ImGui::textUnformattedCentered(ss.str().c_str());

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, canDefuse ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255));
            ImGui::progressBarFullWidth((plantedC4.defuseCountDown - memory->globalVars->currenttime) / plantedC4.defuseLength, 5.0f);
            ImGui::PopStyleColor();
        }
    }

    windowWidth = ImGui::GetCurrentWindow()->SizeFull.x;

    ImGui::PopStyleColor(2);
    ImGui::End();
}

void Misc::hurtIndicator() noexcept
{
    if (!config->misc.hurtIndicator.enabled)
        return;

    GameData::Lock lock;
    const auto& local = GameData::local();
    if ((!local.exists || !local.alive) && !gui->isOpen())
        return;

    if (local.velocityModifier >= 0.99f && !gui->isOpen())
        return;

    if (!gui->isOpen()) {
        ImGui::SetNextWindowBgAlpha(0.3f);
    }

    static float windowWidth = 140.0f;
    ImGui::SetNextWindowPos({ (ImGui::GetIO().DisplaySize.x - 140.0f) / 2.0f, 260.0f }, ImGuiCond_Once);
    ImGui::SetNextWindowSize({ windowWidth, 0 }, ImGuiCond_Once);

    if (!gui->isOpen())
        ImGui::SetNextWindowSize({ windowWidth, 0 });

    ImGui::SetNextWindowSizeConstraints({ 0, -1 }, { FLT_MAX, -1 });
    ImGui::Begin("Hurt Indicator", nullptr, ImGuiWindowFlags_NoTitleBar | (gui->isOpen() ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration));

    std::ostringstream ss; ss << "Slowed down " << static_cast<int>(local.velocityModifier * 100.f) << "%";
    ImGui::textUnformattedCentered(ss.str().c_str());

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Helpers::calculateColor(config->misc.hurtIndicator));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f });
    ImGui::progressBarFullWidth(local.velocityModifier, 1.0f);

    windowWidth = ImGui::GetCurrentWindow()->SizeFull.x;

    ImGui::PopStyleColor(2);
    ImGui::End();
}

void Misc::stealNames() noexcept
{
    if (!config->misc.nameStealer)
        return;

    if (!localPlayer)
        return;

    static std::vector<int> stolenIds;

    for (int i = 1; i <= memory->globalVars->maxClients; ++i) {
        const auto entity = interfaces->entityList->getEntity(i);

        if (!entity || entity == localPlayer.get())
            continue;

        PlayerInfo playerInfo;
        if (!interfaces->engine->getPlayerInfo(entity->index(), playerInfo))
            continue;

        if (playerInfo.fakeplayer || std::find(stolenIds.cbegin(), stolenIds.cend(), playerInfo.userId) != stolenIds.cend())
            continue;

        if (changeName(false, (std::string{ playerInfo.name } +'\x1').c_str(), 1.0f))
            stolenIds.push_back(playerInfo.userId);

        return;
    }
    stolenIds.clear();
}

void Misc::disablePanoramablur() noexcept
{
    static auto blur = interfaces->cvar->findVar("@panorama_disable_blur");
    blur->setValue(config->misc.disablePanoramablur);
}

bool Misc::changeName(bool reconnect, const char* newName, float delay) noexcept
{
    static auto exploitInitialized{ false };

    static auto name{ interfaces->cvar->findVar("name") };

    if (reconnect) {
        exploitInitialized = false;
        return false;
    }

    if (!exploitInitialized && interfaces->engine->isInGame()) {
        if (PlayerInfo playerInfo; localPlayer && interfaces->engine->getPlayerInfo(localPlayer->index(), playerInfo) && (!strcmp(playerInfo.name, "?empty") || !strcmp(playerInfo.name, "\n\xAD\xAD\xAD"))) {
            exploitInitialized = true;
        } else {
            name->onChangeCallbacks.size = 0;
            name->setValue("\n\xAD\xAD\xAD");
            return false;
        }
    }

    static auto nextChangeTime{ 0.0f };
    if (nextChangeTime <= memory->globalVars->realtime) {
        name->setValue(newName);
        nextChangeTime = memory->globalVars->realtime + delay;
        return true;
    }
    return false;
}

void Misc::bunnyHop(UserCmd* cmd) noexcept
{
    if (!localPlayer)
        return;

    static auto wasLastTimeOnGround{ localPlayer->flags() & 1 };

    if (config->misc.bunnyHop && !(localPlayer->flags() & 1) && localPlayer->moveType() != MoveType::LADDER && !wasLastTimeOnGround)
        cmd->buttons &= ~UserCmd::IN_JUMP;

    wasLastTimeOnGround = localPlayer->flags() & 1;
}

void Misc::fixTabletSignal() noexcept
{
    if (config->misc.fixTabletSignal && localPlayer) {
        if (auto activeWeapon{ localPlayer->getActiveWeapon() }; activeWeapon && activeWeapon->getClientClass()->classId == ClassId::Tablet)
            activeWeapon->tabletReceptionIsBlocked() = false;
    }
}

void Misc::killfeedChanger(GameEvent& event) noexcept
{
    if (!config->misc.killfeedChanger.enabled)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
        return;

    if (config->misc.killfeedChanger.headshot)
        event.setInt("headshot", 1);

    if (config->misc.killfeedChanger.dominated)
        event.setInt("Dominated", 1);

    if (config->misc.killfeedChanger.revenge)
        event.setInt("Revenge", 1);

    if (config->misc.killfeedChanger.penetrated)
        event.setInt("penetrated", 1);

    if (config->misc.killfeedChanger.noscope)
        event.setInt("noscope", 1);

    if (config->misc.killfeedChanger.thrusmoke)
        event.setInt("thrusmoke", 1);

    if (config->misc.killfeedChanger.attackerblind)
        event.setInt("attackerblind", 1);
}

void Misc::killMessage(GameEvent& event) noexcept
{
    if (!config->misc.killMessage)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
        return;

    std::string cmd = "say \"";
    cmd += config->misc.killMessageString;
    cmd += '"';
    interfaces->engine->clientCmdUnrestricted(cmd.c_str());
}

void Misc::fixMovement(UserCmd* cmd, float yaw) noexcept
{
    float oldYaw = yaw + (yaw < 0.0f ? 360.0f : 0.0f);
    float newYaw = cmd->viewangles.y + (cmd->viewangles.y < 0.0f ? 360.0f : 0.0f);
    float yawDelta = newYaw < oldYaw ? fabsf(newYaw - oldYaw) : 360.0f - fabsf(newYaw - oldYaw);
    yawDelta = 360.0f - yawDelta;

    const float forwardmove = cmd->forwardmove;
    const float sidemove = cmd->sidemove;
    cmd->forwardmove = std::cos(Helpers::deg2rad(yawDelta)) * forwardmove + std::cos(Helpers::deg2rad(yawDelta + 90.0f)) * sidemove;
    cmd->sidemove = std::sin(Helpers::deg2rad(yawDelta)) * forwardmove + std::sin(Helpers::deg2rad(yawDelta + 90.0f)) * sidemove;
    if (!config->misc.moonwalk)
        cmd->buttons &= ~(UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVERIGHT | UserCmd::IN_MOVELEFT);
}

void Misc::antiAfkKick(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;
    if (localPlayer->velocity().length2D() >= 5.f)
        return;
    if (config->misc.antiAfkKick && cmd->commandNumber % 2)
        cmd->buttons |= 1 << 27;
}

void Misc::fixAnimationLOD(FrameStage stage) noexcept
{
    if (stage != FrameStage::RENDER_START)
        return;

    if (!localPlayer)
        return;

    for (int i = 1; i <= interfaces->engine->getMaxClients(); i++) {
        Entity* entity = interfaces->entityList->getEntity(i);
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive())
            continue;
        *reinterpret_cast<int*>(entity + 0xA28) = 0;
        *reinterpret_cast<int*>(entity + 0xA30) = memory->globalVars->framecount;
    }
}

void Misc::autoPistol(UserCmd* cmd) noexcept
{
    if (config->misc.autoPistol && localPlayer) {
        const auto activeWeapon = localPlayer->getActiveWeapon();
        if (activeWeapon && activeWeapon->isPistol() && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime()) {
            if (activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
                cmd->buttons &= ~UserCmd::IN_ATTACK2;
            else
                cmd->buttons &= ~UserCmd::IN_ATTACK;
        }
    }
}

void Misc::autoReload(UserCmd* cmd) noexcept
{
    if (config->misc.autoReload && localPlayer) {
        const auto activeWeapon = localPlayer->getActiveWeapon();
        if (activeWeapon && getWeaponIndex(activeWeapon->itemDefinitionIndex2()) && !activeWeapon->clip())
            cmd->buttons &= ~(UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2);
    }
}

void Misc::revealRanks(UserCmd* cmd) noexcept
{
    if (config->misc.revealRanks && cmd->buttons & UserCmd::IN_SCORE)
        interfaces->client->dispatchUserMessage(50, 0, 0, nullptr);
}

void Misc::customScope() noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (!localPlayer->getActiveWeapon())
        return;

    if (localPlayer->getActiveWeapon()->itemDefinitionIndex2() == WeaponId::Sg553 || localPlayer->getActiveWeapon()->itemDefinitionIndex2() == WeaponId::Aug)
        return;

    if (config->visuals.scope.type == 0)
    {
        return;
    }
    if (config->visuals.scope.type == 1)
    {
        if (localPlayer->isScoped())
        {
            const auto [width, height] = interfaces->surface->getScreenSize();
            interfaces->surface->setDrawColor(0, 0, 0, 255);
            interfaces->surface->drawFilledRect(0, height / 2, width, height / 2 + 1);
            interfaces->surface->setDrawColor(0, 0, 0, 255);
            interfaces->surface->drawFilledRect(width / 2, 0, width / 2 + 1, height);
        }
    }
    if (config->visuals.scope.type == 2)
    {
        if (localPlayer->isScoped() && config->visuals.scope.fade)
        {
            auto offset = config->visuals.scope.offset;
            auto leng = config->visuals.scope.length;
            int r1 = config->visuals.scope.color.color[0] * 255;
            int g1 = config->visuals.scope.color.color[1] * 255;
            int b1 = config->visuals.scope.color.color[2] * 255;
            int a1 = config->visuals.scope.color.color[3] * 255;
            int a2 = config->visuals.scope.color.color[3] * 0;
            const auto [width, height] = interfaces->surface->getScreenSize();
            //right
            if (!config->visuals.scope.removeRight)
                Render::gradient(width / 2 + offset, height / 2, leng, 1, r1,g1,b1,a1, r1,g1,b1,a2, Render::GradientType::GRADIENT_HORIZONTAL);
            //left
            if (!config->visuals.scope.removeLeft)
                Render::gradient(width / 2 - leng - offset, height / 2, leng, 1, r1, g1, b1, a2, r1, g1, b1, a1, Render::GradientType::GRADIENT_HORIZONTAL);
            //bottom
            if (!config->visuals.scope.removeBottom)
                Render::gradient(width / 2, height / 2 + offset, 1, leng, r1, g1, b1, a1, r1, g1, b1, a2, Render::GradientType::GRADIENT_VERTICAL);
            //top
            if (!config->visuals.scope.removeTop)
                Render::gradient(width / 2, height / 2 - leng - offset, 1, leng, r1, g1, b1, a2, r1, g1, b1, a1, Render::GradientType::GRADIENT_VERTICAL);
        }
        else if (localPlayer->isScoped() && !config->visuals.scope.fade)
        {
            auto offset = config->visuals.scope.offset;
            auto leng = config->visuals.scope.length;
            int r1 = config->visuals.scope.color.color[0] * 255;
            int g1 = config->visuals.scope.color.color[1] * 255;
            int b1 = config->visuals.scope.color.color[2] * 255;
            int a1 = config->visuals.scope.color.color[3] * 255;
            const auto [width, height] = interfaces->surface->getScreenSize();
            //right
            if (!config->visuals.scope.removeRight)
                Render::rectFilled(width / 2 + offset, height / 2, leng, 1, r1,g1,b1,a1);
            //left
            if (!config->visuals.scope.removeLeft)
                Render::rectFilled(width / 2 - leng - offset, height / 2, leng, 1, r1, g1, b1, a1);
            //bottom
            if (!config->visuals.scope.removeBottom)
                Render::rectFilled(width / 2, height / 2 + offset, 1, leng, r1, g1, b1, a1);
            //top
            if (!config->visuals.scope.removeTop)
                Render::rectFilled(width / 2, height / 2 - leng - offset, 1, leng, r1, g1, b1, a1);
        }
    }
    if (config->visuals.scope.type == 3)
    {
        return;
    }
}

void Misc::indicators() noexcept
{
    if (!config->misc.indicators.enable)
        return;

    static bool canDT = Tickbase::canShift(Tickbase::getTargetTickShift(), false);

    auto io = ImGui::GetIO();
    auto displaySize = io.DisplaySize;
    int offsetY = 0;
    int posY = round(displaySize.y - displaySize.y / 2.25f);
    int posX = 15;

    if (config->binds.doubletap.isActive())
    {
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(L"DT");
        offsetY += 28;
    }
    if (config->binds.hideshots.isActive() && !config->binds.doubletap.isActive())
    {
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(L"OSAA");
        offsetY += 28;
    }
    if (config->binds.dmgOverride.isActive())
    {
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(L"DMG");
        offsetY += 28;
    }
    if (config->binds.freestand.isActive())
    {
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(L"FS");
        offsetY += 28;
    }
    if (config->binds.fakeduck.isActive() && config->misc.fakeduck)
    {
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(L"FD");
        offsetY += 28;
    }   
    if (config->binds.autoPeek.isActive() && config->misc.autoPeek.enabled)
    {
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(L"PEEK");
        offsetY += 28;
    }
    if (config->binds.edgeJump.isActive() && config->misc.edgeJump)
    {
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(L"EJ");
        offsetY += 28;
    }
    GameData::Lock lock;
    const auto& plantedC4 = GameData::plantedC4();
    Misc::bombSiteCeva = !plantedC4.bombsite ? c_xor("A") : c_xor("B");
    /*if (plantedC4.blowTime != 0.f)
    {
        std::wstringstream ss; ss << (!plantedC4.bombsite ? L"A" : L"B") << L" - " << std::showpoint << std::setprecision(3) << (std::max)(plantedC4.blowTime - memory->globalVars->currenttime, 0.0f) << L"s";
        interfaces->surface->setTextColor(255, 255, 255, 255);
        interfaces->surface->setTextFont(hooks->indicators);
        interfaces->surface->setTextPosition(posX, posY + offsetY);
        interfaces->surface->printText(ss.str());
        offsetY += 28;
    }*/ //scrap code
}

void Misc::autoStrafe(UserCmd* cmd, Vector& currentViewAngles) noexcept
{
    if (!config->misc.autoStrafe)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if ((EnginePrediction::getFlags() & 1) || localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER)
        return;

    {
        const float speed = localPlayer->velocity().length2D();
        if (speed < 5.0f)
            return;

        static float angle = 0.f;

        const bool back = cmd->buttons & UserCmd::IN_BACK;
        const bool forward = cmd->buttons & UserCmd::IN_FORWARD;
        const bool right = cmd->buttons & UserCmd::IN_MOVERIGHT;
        const bool left = cmd->buttons & UserCmd::IN_MOVELEFT;

        if (back) {
            angle = -180.f;
            if (left)
                angle -= 45.f;
            else if (right)
                angle += 45.f;
        }
        else if (left) {
            angle = 90.f;
            if (back)
                angle += 45.f;
            else if (forward)
                angle -= 45.f;
        }
        else if (right) {
            angle = -90.f;
            if (back)
                angle -= 45.f;
            else if (forward)
                angle += 45.f;
        }
        else {
            angle = 0.f;
        }

        //If we are on ground, noclip or in a ladder return
        if ((EnginePrediction::getFlags() & 1) || localPlayer->moveType() == MoveType::NOCLIP || localPlayer->moveType() == MoveType::LADDER)
            return;

        currentViewAngles.y += angle;

        cmd->forwardmove = 0.f;
        cmd->sidemove = 0.f;
        float smooth = (1.f - (0.15f * (1.f - config->misc.autoStraferSmoothness * 0.01f)));
        const auto delta = Helpers::normalizeYaw(currentViewAngles.y - Helpers::rad2deg(std::atan2(EnginePrediction::getVelocity().y, EnginePrediction::getVelocity().x)));

        cmd->sidemove = delta > 0.f ? -450.f : 450.f;

        currentViewAngles.y = Helpers::normalizeYaw(currentViewAngles.y - delta * smooth);
    }
}

void Misc::removeCrouchCooldown(UserCmd* cmd) noexcept
{
    if (const auto gameRules = (*memory->gameRules); gameRules)
        if (getGameMode() != GameMode::Competitive && gameRules->isValveDS())
            return;

    if (config->misc.fastDuck)
        cmd->buttons |= UserCmd::IN_BULLRUSH;
}

void Misc::moonwalk(UserCmd* cmd) noexcept
{
    if (config->misc.moonwalk && localPlayer && localPlayer->moveType() != MoveType::LADDER)
        cmd->buttons ^= UserCmd::IN_FORWARD | UserCmd::IN_BACK | UserCmd::IN_MOVELEFT | UserCmd::IN_MOVERIGHT;
}

void Misc::playHitSound(GameEvent& event) noexcept
{
    if (!config->misc.hitSound)
        return;

    if (!localPlayer)
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
        return;

    constexpr std::array hitSounds{
        "play physics/metal/metal_solid_impact_bullet2",
        "play buttons/arena_switch_press_02",
        "play training/timer_bell",
        "play physics/glass/glass_impact_bullet1"
    };

    if (static_cast<std::size_t>(config->misc.hitSound - 1) < hitSounds.size())
        interfaces->engine->clientCmdUnrestricted(hitSounds[config->misc.hitSound - 1]);
    else if (config->misc.hitSound == 5)
        interfaces->engine->clientCmdUnrestricted(("play " + config->misc.customHitSound).c_str());
}

void Misc::killSound(GameEvent& event) noexcept
{
    if (!config->misc.killSound)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    if (const auto localUserId = localPlayer->getUserId(); event.getInt("attacker") != localUserId || event.getInt("userid") == localUserId)
        return;

    constexpr std::array killSounds{
        "play physics/metal/metal_solid_impact_bullet2",
        "play buttons/arena_switch_press_02",
        "play training/timer_bell",
        "play physics/glass/glass_impact_bullet1"
    };

    if (static_cast<std::size_t>(config->misc.killSound - 1) < killSounds.size())
        interfaces->engine->clientCmdUnrestricted(killSounds[config->misc.killSound - 1]);
    else if (config->misc.killSound == 5)
        interfaces->engine->clientCmdUnrestricted(("play " + config->misc.customKillSound).c_str());
}

void Misc::autoBuy(GameEvent* event) noexcept
{
    static const std::array<std::string, 17> primary = {
    "",
    "mac10;buy mp9;",
    "mp7;",
    "ump45;",
    "p90;",
    "bizon;",
    "galilar;buy famas;",
    "ak47;buy m4a1;",
    "ssg08;",
    "sg556;buy aug;",
    "awp;",
    "g3sg1; buy scar20;",
    "nova;",
    "xm1014;",
    "sawedoff;buy mag7;",
    "m249; ",
    "negev;"
    };
    static const std::array<std::string, 6> secondary = {
        "",
        "glock;buy hkp2000;",
        "elite;",
        "p250;",
        "tec9;buy fiveseven;",
        "deagle;buy revolver;"
    };
    static const std::array<std::string, 3> armor = {
        "",
        "vest;",
        "vesthelm;",
    };
    static const std::array<std::string, 2> utility = {
        "defuser;",
        "taser;"
    };
    static const std::array<std::string, 5> nades = {
        "hegrenade;",
        "smokegrenade;",
        "molotov;buy incgrenade;",
        "flashbang;buy flashbang;",
        "decoy;"
    };

    if (!config->misc.autoBuy.enabled)
        return;

    std::string cmd = "";

    if (event) 
    {
        if (config->misc.autoBuy.primaryWeapon)
            cmd += "buy " + primary[config->misc.autoBuy.primaryWeapon];
        if (config->misc.autoBuy.secondaryWeapon)
            cmd += "buy " + secondary[config->misc.autoBuy.secondaryWeapon];
        if (config->misc.autoBuy.armor)
            cmd += "buy " + armor[config->misc.autoBuy.armor];

        for (size_t i = 0; i < utility.size(); i++)
        {
            if ((config->misc.autoBuy.utility & 1 << i) == 1 << i)
                cmd += "buy " + utility[i];
        }

        for (size_t i = 0; i < nades.size(); i++)
        {
            if ((config->misc.autoBuy.grenades & 1 << i) == 1 << i)
                cmd += "buy " + nades[i];
        }

        interfaces->engine->clientCmdUnrestricted(cmd.c_str());
    }
}

void Misc::preserveKillfeed(bool roundStart) noexcept
{
    if (!config->misc.preserveKillfeed.enabled)
        return;

    static auto nextUpdate = 0.0f;

    if (roundStart) {
        nextUpdate = memory->globalVars->realtime + 10.0f;
        return;
    }

    if (nextUpdate > memory->globalVars->realtime)
        return;

    nextUpdate = memory->globalVars->realtime + 2.0f;

    const auto deathNotice = std::uintptr_t(memory->findHudElement(memory->hud, "CCSGO_HudDeathNotice"));
    if (!deathNotice)
        return;

    const auto deathNoticePanel = (*(UIPanel**)(*reinterpret_cast<std::uintptr_t*>(deathNotice - 20 + 88) + sizeof(std::uintptr_t)));

    const auto childPanelCount = deathNoticePanel->getChildCount();

    for (int i = 0; i < childPanelCount; ++i) {
        const auto child = deathNoticePanel->getChild(i);
        if (!child)
            continue;

        if (child->hasClass("DeathNotice_Killer") && (!config->misc.preserveKillfeed.onlyHeadshots || child->hasClass("DeathNoticeHeadShot")))
            child->setAttributeFloat("SpawnTime", memory->globalVars->currenttime);
    }
}

void Misc::voteRevealer(GameEvent& event) noexcept
{
    if (!config->misc.revealVotes)
        return;

    const auto entity = interfaces->entityList->getEntity(event.getInt("entityid"));
    if (!entity || !entity->isPlayer())
        return;
    
    const auto votedYes = event.getInt("vote_option") == 0;
    const auto isLocal = localPlayer && entity == localPlayer.get();
    const char color = votedYes ? '\x06' : '\x07';

    memory->clientMode->getHudChat()->printf(0, " \x0C\u2022Osiris\u2022 %c%s\x01 voted %c%s\x01", isLocal ? '\x01' : color, isLocal ? "You" : entity->getPlayerName().c_str(), color, votedYes ? "Yes" : "No");
}

void Misc::chatRevealer(GameEvent& event, GameEvent* events) noexcept
{
    if (!config->misc.chatRevealer)
        return;

    if (!localPlayer)
        return;

    const auto entity = interfaces->entityList->getEntity(interfaces->engine->getPlayerForUserID(events->getInt("userid")));
    if (!entity)
        return;

    std::string output = "\x0C\u2022Osiris\u2022\x01 ";

    auto team = entity->getTeamNumber();
    bool isAlive = entity->isAlive();
    bool dormant = entity->isDormant();
    if (dormant) {
        if (const auto pr = *memory->playerResource) 
            isAlive = pr->getIPlayerResource()->isAlive(entity->index());
    }

    const char* text = event.getString("text");
    const char* lastLocation = entity->lastPlaceName();
    const std::string name = entity->getPlayerName();

    if (team == localPlayer->getTeamNumber())
        return;

    if (!isAlive)
        output += "*DEAD* ";

    team == Team::TT ? output += "(Terrorist) " : output += "(Counter-Terrorist) ";

    output = output + name + " @ " + lastLocation + " : " + text;
    memory->clientMode->getHudChat()->printf(0, output.c_str());
}

// ImGui::ShadeVertsLinearColorGradientKeepAlpha() modified to do interpolation in HSV
static void shadeVertsHSVColorGradientKeepAlpha(ImDrawList* draw_list, int vert_start_idx, int vert_end_idx, ImVec2 gradient_p0, ImVec2 gradient_p1, ImU32 col0, ImU32 col1)
{
    ImVec2 gradient_extent = gradient_p1 - gradient_p0;
    float gradient_inv_length2 = 1.0f / ImLengthSqr(gradient_extent);
    ImDrawVert* vert_start = draw_list->VtxBuffer.Data + vert_start_idx;
    ImDrawVert* vert_end = draw_list->VtxBuffer.Data + vert_end_idx;

    ImVec4 col0HSV = ImGui::ColorConvertU32ToFloat4(col0);
    ImVec4 col1HSV = ImGui::ColorConvertU32ToFloat4(col1);
    ImGui::ColorConvertRGBtoHSV(col0HSV.x, col0HSV.y, col0HSV.z, col0HSV.x, col0HSV.y, col0HSV.z);
    ImGui::ColorConvertRGBtoHSV(col1HSV.x, col1HSV.y, col1HSV.z, col1HSV.x, col1HSV.y, col1HSV.z);
    ImVec4 colDelta = col1HSV - col0HSV;

    for (ImDrawVert* vert = vert_start; vert < vert_end; vert++)
    {
        float d = ImDot(vert->pos - gradient_p0, gradient_extent);
        float t = ImClamp(d * gradient_inv_length2, 0.0f, 1.0f);

        float h = col0HSV.x + colDelta.x * t;
        float s = col0HSV.y + colDelta.y * t;
        float v = col0HSV.z + colDelta.z * t;

        ImVec4 rgb;
        ImGui::ColorConvertHSVtoRGB(h, s, v, rgb.x, rgb.y, rgb.z);
        vert->col = (ImGui::ColorConvertFloat4ToU32(rgb) & ~IM_COL32_A_MASK) | (vert->col & IM_COL32_A_MASK);
    }
}

void Misc::drawOffscreenEnemies(ImDrawList* drawList) noexcept
{
    if (!config->misc.offscreenEnemies.enabled)
        return;

    const auto yaw = Helpers::deg2rad(interfaces->engine->getViewAngles().y);

    GameData::Lock lock;
    for (auto& player : GameData::players()) {
        if ((player.dormant && player.fadingAlpha() == 0.0f) || !player.alive || !player.enemy || player.inViewFrustum)
            continue;

        const auto positionDiff = GameData::local().origin - player.origin;

        auto x = std::cos(yaw) * positionDiff.y - std::sin(yaw) * positionDiff.x;
        auto y = std::cos(yaw) * positionDiff.x + std::sin(yaw) * positionDiff.y;
        if (const auto len = std::sqrt(x * x + y * y); len != 0.0f) {
            x /= len;
            y /= len;
        }

        auto triangleSize = config->misc.offscreenEnemies.size;

        const auto pos = ImGui::GetIO().DisplaySize / 2 + ImVec2{ x, y } * config->misc.offscreenEnemies.offset;
        const auto trianglePos = pos + ImVec2{ x, y };

        Helpers::setAlphaFactor(player.fadingAlpha());
        const auto color = Helpers::calculateColor(config->misc.offscreenEnemies.color[0] * 255, config->misc.offscreenEnemies.color[1] * 255, config->misc.offscreenEnemies.color[2] * 255, config->misc.offscreenEnemies.color[3] * 255);
        const auto color1 = Helpers::calculateColor(config->misc.offscreenEnemies.color[0] * 255, config->misc.offscreenEnemies.color[1] * 255, config->misc.offscreenEnemies.color[2] * 255, config->misc.offscreenEnemies.color[3] * 90);
        Helpers::setAlphaFactor(1.0f);

        const ImVec2 trianglePoints[]{
            trianglePos + ImVec2{  0.4f * y, -0.4f * x } * triangleSize,
            trianglePos + ImVec2{  1.0f * x,  1.0f * y } * triangleSize,
            trianglePos + ImVec2{ -0.4f * y,  0.4f * x } * triangleSize
        };

        drawList->AddTriangleFilledMulticolor(trianglePoints[0], trianglePoints[1], trianglePoints[2], color1, color, color1);      
    }
}

void Misc::fastLadder(UserCmd* cmd) noexcept
{
    if (!config->misc.fastLadder)
        return;

    if (!localPlayer || !localPlayer->isAlive())
        return;

    auto moveType = localPlayer->moveType();
    if (moveType == MoveType::LADDER)
    {
        if (cmd->sidemove == 0) {
            cmd->viewangles.y += 45.0f;
        }

        if (cmd->buttons & UserCmd::IN_FORWARD) {
            if (cmd->sidemove > 0) {
                cmd->viewangles.y -= 1.0f;
            }

            if (cmd->sidemove < 0) {
                cmd->viewangles.y += 90.0f;
            }

            cmd->buttons &= ~UserCmd::IN_MOVELEFT;
            cmd->buttons |= UserCmd::IN_MOVERIGHT;
        }

        if (cmd->buttons & UserCmd::IN_BACK) {
            if (cmd->sidemove < 0) {
                cmd->viewangles.y -= 1.0f;
            }

            if (cmd->sidemove > 0) {
                cmd->viewangles.y += 90.0f;
            }

            cmd->buttons &= ~UserCmd::IN_MOVERIGHT;
            cmd->buttons |= UserCmd::IN_MOVELEFT;
        }
    }
}

void Misc::autoAccept(const char* soundEntry) noexcept
{
    if (!config->misc.autoAccept)
        return;

    if (std::strcmp(soundEntry, "UIPanorama.popup_accept_match_beep"))
        return;

    if (const auto idx = memory->registeredPanoramaEvents->find(memory->makePanoramaSymbol("MatchAssistedAccept")); idx != -1) {
        if (const auto eventPtr = memory->registeredPanoramaEvents->memory[idx].value.makeEvent(nullptr))
            interfaces->panoramaUIEngine->accessUIEngine()->dispatchEvent(eventPtr);
    }

    auto window = FindWindowW(L"Valve001", NULL);
    FLASHWINFO flash{ sizeof(FLASHWINFO), window, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0 };
    FlashWindowEx(&flash);
    ShowWindow(window, SW_RESTORE);
}

void Misc::updateInput() noexcept
{
    config->binds.slowwalk.handleToggle();
    config->binds.edgeJump.handleToggle();
    config->binds.fakeduck.handleToggle();
    config->binds.autoPeek.handleToggle();
}

void Misc::reset(int resetType) noexcept
{
    if (resetType == 1)
    {
        static auto ragdollGravity = interfaces->cvar->findVar(c_xor("cl_ragdoll_gravity"));
        static auto blur = interfaces->cvar->findVar(c_xor("@panorama_disable_blur"));
        ragdollGravity->setValue(600);
        blur->setValue(0);
    }
}
