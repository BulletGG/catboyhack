#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"

#include "AimbotFunctions.h"
#include "Animations.h"
#include "Backtrack.h"
#include "Ragebot.h"
#include "EnginePrediction.h"
#include "Resolver.h"

#include "../SDK/Entity.h"
#include "../SDK/UserCmd.h"
#include "../SDK/Utils.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponId.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/LocalPlayer.h"
#include "../SDK/ModelInfo.h"
#include "../xor.h"

static bool keyPressed = false;

void Ragebot::updateInput() noexcept
{
    config->binds.dmgOverride.handleToggle();
}

void runRagebot(UserCmd* cmd, Entity* entity, matrix3x4* matrix, Ragebot::Enemies target, std::array<bool, Hitboxes::Max> hitbox, Entity* activeWeapon, int weaponIndex, Vector localPlayerEyePosition, Vector aimPunch, int multiPointH, int multiPointB, int minDamage, float& damageDiff, Vector& bestAngle, Vector& bestTarget) noexcept
{
    const auto& cfg = config->ragebot;
    const auto& cfg1 = config->rageBot;
    damageDiff = FLT_MAX;

    const Model* model = entity->getModel();
    if (!model)
        return;

    StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
    if (!hdr)
        return;

    StudioHitboxSet* set = hdr->getHitboxSet(0);
    if (!set)
        return;

    for (size_t i = 0; i < hitbox.size(); i++)
    {
        if (!hitbox[i])
            continue;

        StudioBbox* hitbox = set->getHitbox(i);
        if (!hitbox)
            continue;

        for (auto& bonePosition : AimbotFunction::multiPoint(entity, matrix, hitbox, localPlayerEyePosition, i, multiPointH, multiPointB))
        {
            const auto angle{ AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, bonePosition, cmd->viewangles + aimPunch) };
            const int fov{ (int)round(angle.length2D()) };
            if (fov > cfg.fov)
                continue;

            float damage = AimbotFunction::getScanDamage(entity, bonePosition, activeWeapon->getWeaponData(), minDamage);
            damage = std::clamp(damage, 0.0f, (float)entity->maxHealth());
            if (damage <= 0.5f)
                continue;

            if (cfg.autoScope && activeWeapon->isSniperRifle() && !localPlayer->isScoped() && activeWeapon->zoomLevel() < 1)
                cmd->buttons |= UserCmd::IN_ZOOM;

            if (cfg1[weaponIndex].autoStop && localPlayer->flags() & 1 && !(cmd->buttons & UserCmd::IN_JUMP))
            {
                if ((cfg1[weaponIndex].autoStopMod & 1 << 0) == 1 << 0)
                {
                    const auto velocity = EnginePrediction::getVelocity();
                    const auto speed = velocity.length2D();
                    const auto activeWeapon = localPlayer->getActiveWeapon();
                    const auto weaponData = activeWeapon->getWeaponData();
                    const float maxSpeed = (localPlayer->isScoped() ? weaponData->maxSpeedAlt : weaponData->maxSpeed) / 3;
                    if (speed >= maxSpeed)
                    {
                        Vector direction = velocity.toAngle();
                        direction.y = cmd->viewangles.y - direction.y;

                        const auto negatedDirection = Vector::fromAngle(direction) * -speed;
                        cmd->forwardmove = negatedDirection.x;
                        cmd->sidemove = negatedDirection.y;
                    }
                }
                if ((cfg1[weaponIndex].autoStopMod & 1 << 2) == 1 << 2)
                {
                    cmd->forwardmove = 0;
                    cmd->sidemove = 0;
                }
                if ((cfg1[weaponIndex].autoStopMod & 1 << 3) == 1 << 3)
                {
                    cmd->buttons |= UserCmd::IN_DUCK;
                }
                EnginePrediction::update();
            }

            if (std::fabsf((float)target.health - damage) <= damageDiff)
            {
                bestAngle = angle;
                damageDiff = std::fabsf((float)target.health - damage);
                bestTarget = bonePosition;
            }
        }
    }

    if (bestTarget.notNull())
    {
        if (!AimbotFunction::hitChance(localPlayer.get(), entity, set, matrix, activeWeapon, bestAngle, cmd, cfg1[weaponIndex].hitChance))
        {
            bestTarget = Vector{ };
            bestAngle = Vector{ };
            damageDiff = FLT_MAX;
        }
    }
}

void Ragebot::run(UserCmd* cmd) noexcept
{
    const auto& cfg = config->ragebot;
    const auto& cfg1 = config->rageBot;
    if (!cfg.enabled)
        return;

    if (!localPlayer || localPlayer->nextAttack() > memory->globalVars->serverTime() || localPlayer->isDefusing() || localPlayer->waitForNoAttack())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    int weaponIndex = 0;
    if (!activeWeapon || !activeWeapon->clip())
        return;

    if (activeWeapon->isKnife() || activeWeapon->isBomb() || activeWeapon->isGrenade())
        return;

    //magic shit aka weapon indexes (indexes: 1-pistols, 2-deagle, 3-revolver, 4-ssg08, 5-awp, 6-auto, 7-zeus, 8-other)
    {
        if (cfg1[1].override && (activeWeapon->itemDefinitionIndex2() == WeaponId::Elite || activeWeapon->itemDefinitionIndex2() == WeaponId::Hkp2000 || activeWeapon->itemDefinitionIndex2() == WeaponId::P250 || activeWeapon->itemDefinitionIndex2() == WeaponId::Usp_s || activeWeapon->itemDefinitionIndex2() == WeaponId::Cz75a || activeWeapon->itemDefinitionIndex2() == WeaponId::Tec9 || activeWeapon->itemDefinitionIndex2() == WeaponId::Fiveseven || activeWeapon->itemDefinitionIndex2() == WeaponId::Glock))
            weaponIndex = 1;
        else if (cfg1[2].override && activeWeapon->itemDefinitionIndex2() == WeaponId::Deagle)
            weaponIndex = 2;
        else if (cfg1[3].override && activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver)
            weaponIndex = 3;
        else if (cfg1[8].override && (activeWeapon->itemDefinitionIndex2() == WeaponId::Mac10 || activeWeapon->itemDefinitionIndex2() == WeaponId::Mp9 || activeWeapon->itemDefinitionIndex2() == WeaponId::Mp7 || activeWeapon->itemDefinitionIndex2() == WeaponId::Mp5sd || activeWeapon->itemDefinitionIndex2() == WeaponId::Ump45 || activeWeapon->itemDefinitionIndex2() == WeaponId::P90 || activeWeapon->itemDefinitionIndex2() == WeaponId::Bizon))
            weaponIndex = 8;
        else if (cfg1[8].override && (activeWeapon->itemDefinitionIndex2() == WeaponId::Ak47 || activeWeapon->itemDefinitionIndex2() == WeaponId::M4A1 || activeWeapon->itemDefinitionIndex2() == WeaponId::M4a1_s || activeWeapon->itemDefinitionIndex2() == WeaponId::GalilAr || activeWeapon->itemDefinitionIndex2() == WeaponId::Aug || activeWeapon->itemDefinitionIndex2() == WeaponId::Sg553 || activeWeapon->itemDefinitionIndex2() == WeaponId::Famas))
            weaponIndex = 8;
        else if (cfg1[4].override && activeWeapon->itemDefinitionIndex2() == WeaponId::Ssg08)
            weaponIndex = 4;
        else if (cfg1[5].override && activeWeapon->itemDefinitionIndex2() == WeaponId::Awp)
            weaponIndex = 5;
        else if (cfg1[6].override && (activeWeapon->itemDefinitionIndex2() == WeaponId::G3SG1 || activeWeapon->itemDefinitionIndex2() == WeaponId::Scar20))
            weaponIndex = 6;
        else if (cfg1[7].override && activeWeapon->itemDefinitionIndex2() == WeaponId::Taser)
            weaponIndex = 7;
        else
            weaponIndex = 0;
    }

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto())
        return;

    if ((cfg1[weaponIndex].autoStopMod & 1 << 1) != 1 << 1 && activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return;

    if (!(cfg.enabled && (cmd->buttons & UserCmd::IN_ATTACK || cfg.autoShot)))
        return;

    float damageDiff = FLT_MAX;
    Vector bestTarget{ };
    Vector bestAngle{ };
    int bestIndex{ -1 };
    float bestSimulationTime = 0;
    const auto localPlayerEyePosition = localPlayer->getEyePosition();
    const auto aimPunch = localPlayer->getAimPunch();

    std::array<bool, Hitboxes::Max> hitbox{ false };
    auto headshotonly = interfaces->cvar->findVar(skCrypt("mp_damage_headshot_only"));
    hitbox[Hitboxes::Head] = (cfg1[weaponIndex].hitboxes & 1 << 0) == 1 << 0;
    if (headshotonly->getInt() < 1) //headshot only check
    {
        // Chest
        hitbox[Hitboxes::UpperChest] = (cfg1[weaponIndex].hitboxes & 1 << 1) == 1 << 1;
        hitbox[Hitboxes::Thorax] = (cfg1[weaponIndex].hitboxes & 1 << 1) == 1 << 1;
        hitbox[Hitboxes::LowerChest] = (cfg1[weaponIndex].hitboxes & 1 << 1) == 1 << 1;
        //Stomach
        hitbox[Hitboxes::Belly] = (cfg1[weaponIndex].hitboxes & 1 << 2) == 1 << 2;
        hitbox[Hitboxes::Pelvis] = (cfg1[weaponIndex].hitboxes & 1 << 3) == 1 << 3;
        //Arms
        hitbox[Hitboxes::RightUpperArm] = (cfg1[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
        hitbox[Hitboxes::RightForearm] = (cfg1[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
        hitbox[Hitboxes::LeftUpperArm] = (cfg1[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
        hitbox[Hitboxes::LeftForearm] = (cfg1[weaponIndex].hitboxes & 1 << 4) == 1 << 4;
        //Legs
        hitbox[Hitboxes::RightCalf] = (cfg1[weaponIndex].hitboxes & 1 << 5) == 1 << 5;
        hitbox[Hitboxes::RightThigh] = (cfg1[weaponIndex].hitboxes & 1 << 5) == 1 << 5;
        hitbox[Hitboxes::LeftCalf] = (cfg1[weaponIndex].hitboxes & 1 << 5) == 1 << 5;
        hitbox[Hitboxes::LeftThigh] = (cfg1[weaponIndex].hitboxes & 1 << 5) == 1 << 5;
        //feet
        hitbox[Hitboxes::RightFoot] = (cfg1[weaponIndex].hitboxes & 1 << 6) == 1 << 6;
        hitbox[Hitboxes::LeftFoot] = (cfg1[weaponIndex].hitboxes & 1 << 6) == 1 << 6;
    }

    std::vector<Ragebot::Enemies> enemies;
    const auto localPlayerOrigin{ localPlayer->getAbsOrigin() };
    for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
        const auto player = Animations::getPlayer(i);
        if (!player.gotMatrix)
            continue;

        const auto entity{ interfaces->entityList->getEntity(i) };
        if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
            || !entity->isOtherEnemy(localPlayer.get()) || entity->gunGameImmunity())
            continue;

        const auto angle{ AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, player.matrix[8].origin(), cmd->viewangles + aimPunch) };
        const auto origin{ entity->getAbsOrigin() };
        const auto fov{ angle.length2D() }; //fov
        const auto health{ entity->health() }; //health
        const auto distance{ localPlayerOrigin.distTo(origin) }; //distance
        enemies.emplace_back(i, health, distance, fov);
    }

    if (enemies.empty())
        return;

    std::sort(enemies.begin(), enemies.end(), healthSort);

    static auto frameRate = 1.0f;
    frameRate = 0.9f * frameRate + 0.1f * memory->globalVars->absoluteFrameTime;

    auto multiPointH = cfg1[weaponIndex].multiPointHead;
    auto multiPointB = cfg1[weaponIndex].multiPointBody;

    for (const auto& target : enemies) 
    {
        auto entity{ interfaces->entityList->getEntity(target.id) };
        auto player = Animations::getPlayer(target.id);
        int minDamage = std::clamp(config->binds.dmgOverride.isActive() ? cfg1[weaponIndex].minDamageOverride : cfg1[weaponIndex].minDamage, 0, target.health);

        matrix3x4* backupBoneCache = entity->getBoneCache().memory;
        Vector backupMins = entity->getCollideable()->obbMins();
        Vector backupMaxs = entity->getCollideable()->obbMaxs();
        Vector backupOrigin = entity->getAbsOrigin();
        Vector backupAbsAngle = entity->getAbsAngle();

        for (int cycle = 0; cycle < 2; cycle++)
        {
            float currentSimulationTime = -1.0f;

            if (config->backtrack.enabled)
            {
                const auto records = Animations::getBacktrackRecords(entity->index());
                if (!records || records->empty())
                    continue;

                int bestTick = -1;
                if (cycle == 0)
                {
                    for (size_t i = 0; i < records->size(); i++)
                    {
                        if (Backtrack::valid(records->at(i).simulationTime))
                        {
                            bestTick = static_cast<int>(i);
                            break;
                        }
                    }
                }
                else
                {
                    for (int i = static_cast<int>(records->size() - 1U); i >= 0; i--)
                    {
                        if (Backtrack::valid(records->at(i).simulationTime))
                        {
                            bestTick = i;
                            break;
                        }
                    }
                }

                if (bestTick <= -1)
                    continue;

                memcpy(entity->getBoneCache().memory, records->at(bestTick).matrix, std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
                memory->setAbsOrigin(entity, records->at(bestTick).origin);
                memory->setAbsAngle(entity, Vector{ 0.f, records->at(bestTick).absAngle.y, 0.f });
                memory->setCollisionBounds(entity->getCollideable(), records->at(bestTick).mins, records->at(bestTick).maxs);

                currentSimulationTime = records->at(bestTick).simulationTime;
            }
            else
            {
                //We skip backtrack
                if (cycle == 1)
                    continue;

                memcpy(entity->getBoneCache().memory, player.matrix.data(), std::clamp(entity->getBoneCache().size, 0, MAXSTUDIOBONES) * sizeof(matrix3x4));
                memory->setAbsOrigin(entity, player.origin);
                memory->setAbsAngle(entity, Vector{ 0.f, player.absAngle.y, 0.f });
                memory->setCollisionBounds(entity->getCollideable(), player.mins, player.maxs);

                currentSimulationTime = player.simulationTime;
            }

            runRagebot(cmd, entity, entity->getBoneCache().memory, target, hitbox, activeWeapon, weaponIndex, localPlayerEyePosition, aimPunch, multiPointH, multiPointB, minDamage, damageDiff, bestAngle, bestTarget);
            resetMatrix(entity, backupBoneCache, backupOrigin, backupAbsAngle, backupMins, backupMaxs);
            if (bestTarget.notNull())
            {
                bestSimulationTime = currentSimulationTime;
                bestIndex = target.id;
                break;
            }
        }
        if (bestTarget.notNull())
            break;
    }

    if (bestTarget.notNull()) 
    {
        static Vector lastAngles{ cmd->viewangles };
        static int lastCommand{ };

        if (lastCommand == cmd->commandNumber - 1 && lastAngles.notNull() && cfg.silent)
            cmd->viewangles = lastAngles;

        auto angle = AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, bestTarget, cmd->viewangles + aimPunch);

        if (activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime())
        {
            cmd->viewangles += angle;
            if (!cfg.silent)
                interfaces->engine->setViewAngles(cmd->viewangles);

            if (cfg.autoShot)
                cmd->buttons |= UserCmd::IN_ATTACK;
        }

        if (cmd->buttons & UserCmd::IN_ATTACK)
        {
            cmd->tickCount = timeToTicks(bestSimulationTime + Backtrack::getLerp());
            Resolver::saveRecord(bestIndex, bestSimulationTime);
        }

        lastCommand = cmd->commandNumber;
    }
}
