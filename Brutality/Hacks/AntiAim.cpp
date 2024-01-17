#include "../Interfaces.h"

#include "AimbotFunctions.h"
#include "AntiAim.h"
#include "Tickbase.h"
#include "../LocalCondition.h"
#include "../SDK/Engine.h"
#include "../SDK/Entity.h"
#include "../SDK/EntityList.h"
#include "../SDK/NetworkChannel.h"
#include "../SDK/UserCmd.h"

bool updateLby(bool update = false) noexcept
{
    static float timer = 0.f;
    static bool lastValue = false;

    if (!update)
        return lastValue;

    if (!(localPlayer->flags() & 1) || !localPlayer->getAnimstate())
    {
        lastValue = false;
        return false;
    }

    if (localPlayer->velocity().length2D() > 0.1f || fabsf(localPlayer->velocity().z) > 100.f)
        timer = memory->globalVars->serverTime() + 0.22f;

    if (timer < memory->globalVars->serverTime())
    {
        timer = memory->globalVars->serverTime() + 1.1f;
        lastValue = true;
        return true;
    }
    lastValue = false;
    return false;
}

bool autoDirection(Vector eyeAngle) noexcept
{
    constexpr float maxRange{ 8192.0f };

    Vector eye = eyeAngle;
    eye.x = 0.f;
    Vector eyeAnglesLeft45 = eye;
    Vector eyeAnglesRight45 = eye;
    eyeAnglesLeft45.y += 45.f;
    eyeAnglesRight45.y -= 45.f;


    eyeAnglesLeft45.toAngle();

    Vector viewAnglesLeft45 = {};
    viewAnglesLeft45 = viewAnglesLeft45.fromAngle(eyeAnglesLeft45) * maxRange;

    Vector viewAnglesRight45 = {};
    viewAnglesRight45 = viewAnglesRight45.fromAngle(eyeAnglesRight45) * maxRange;

    static Trace traceLeft45;
    static Trace traceRight45;

    Vector startPosition{ localPlayer->getEyePosition() };

    interfaces->engineTrace->traceRay({ startPosition, startPosition + viewAnglesLeft45 }, 0x4600400B, { localPlayer.get() }, traceLeft45);
    interfaces->engineTrace->traceRay({ startPosition, startPosition + viewAnglesRight45 }, 0x4600400B, { localPlayer.get() }, traceRight45);

    float distanceLeft45 = sqrtf(powf(startPosition.x - traceRight45.endpos.x, 2) + powf(startPosition.y - traceRight45.endpos.y, 2) + powf(startPosition.z - traceRight45.endpos.z, 2));
    float distanceRight45 = sqrtf(powf(startPosition.x - traceLeft45.endpos.x, 2) + powf(startPosition.y - traceLeft45.endpos.y, 2) + powf(startPosition.z - traceLeft45.endpos.z, 2));

    float mindistance = min(distanceLeft45, distanceRight45);

    if (distanceLeft45 == mindistance)
        return false;
    return true;
}

float RandomFloat(float a, float b, float multiplier) {
    float random = ((float)rand()) / (float)RAND_MAX;
    float diff = b - a;
    float r = random * diff;
    float result = a + r;
    return result * multiplier;
}

void AntiAim::rage(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
    static bool willgetstabbed = false;
    if ((cmd->viewangles.x == currentViewAngles.x || Tickbase::isShifting()) && config->condAA.global)
    {
        switch (config->rageAntiAim[Condition::getState(cmd)].pitch)
        {
        case 0: //None
            break;
        case 1: //Down
            cmd->viewangles.x = 89.f;
            break;
        case 2: //Zero
            cmd->viewangles.x = 0.f;
            break;
        case 3: //Up
            cmd->viewangles.x = -89.f;
            break;
        case 4: //Flick-up
        {
            float pitcher = 89.f;
            if (cmd->tickCount % 20 == 1)
            {
                pitcher = -89.f;
            }
            else
                pitcher = 89.f;
            cmd->viewangles.x = pitcher;
        }
        break;
        case 5: //random
            cmd->viewangles.x = RandomFloat(-89, 89, 1.f);
            break;
        default:
            break;
        }
    }

    static bool invert = true;
    if (cmd->viewangles.y == currentViewAngles.y || Tickbase::isShifting())
    {
        const bool forward = config->binds.manualForward.isActive();
        const bool back = config->binds.manualBackward.isActive();
        const bool right = config->binds.manualRight.isActive();
        const bool left = config->binds.manualLeft.isActive();
        const bool isManualSet = forward || back || right || left;
        bool isFreestanding{ false };
        {
            float yaw = 0.f;
            static float staticYaw = 0.f;
            static bool flipJitter = false;
            if (sendPacket)
                flipJitter ^= 1;
            if (config->rageAntiAim[Condition::getState(cmd)].atTargets && localPlayer->moveType() != MoveType::LADDER && !willgetstabbed)
            {
                Vector localPlayerEyePosition = localPlayer->getEyePosition();
                const auto aimPunch = localPlayer->getAimPunch();
                float bestFov = 255.f;
                float yawAngle = 0.f;
                for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
                    const auto entity{ interfaces->entityList->getEntity(i) };
                    if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
                        || !entity->isOtherEnemy(localPlayer.get()) || entity->gunGameImmunity())
                        continue;

                    const auto angle{ AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, entity->getAbsOrigin(), cmd->viewangles + aimPunch) };
                    const auto fov{ angle.length2D() };
                    if (fov < bestFov)
                    {
                        yawAngle = angle.y;
                        bestFov = fov;
                    }
                }
                yaw = yawAngle;
            }
            if (localPlayer->moveType() != MoveType::LADDER || localPlayer->moveType() != MoveType::NOCLIP)
            {
                Vector localPlayerEyePosition = localPlayer->getEyePosition();
                const auto aimPunch = localPlayer->getAimPunch();
                float bestFov = 255.f;
                float yawAngle = 0.f;
                for (int i = 1; i <= interfaces->engine->getMaxClients(); ++i) {
                    const auto entity{ interfaces->entityList->getEntity(i) };
                    if (!entity || entity == localPlayer.get() || entity->isDormant() || !entity->isAlive()
                        || !entity->isOtherEnemy(localPlayer.get()))
                    {
                        willgetstabbed = false;
                        continue;
                    }

                    auto enemyDist = entity->getAbsOrigin().distTo(localPlayer->getAbsOrigin());
                    if (enemyDist > 500)
                    {
                        willgetstabbed = false;
                        continue;
                    }

                    if (!entity->getActiveWeapon())
                    {
                        willgetstabbed = false;
                        continue;
                    }

                    if (entity->getActiveWeapon()->isKnife())
                    {
                        const auto angle{ AimbotFunction::calculateRelativeAngle(localPlayerEyePosition, entity->getAbsOrigin(), cmd->viewangles) };
                        const auto fov{ angle.length2D() };
                        if (fov < bestFov)
                        {
                            yawAngle = angle.y;
                            bestFov = fov;
                        }
                        yaw = yawAngle;
                        switch (config->rageAntiAim[Condition::getState(cmd)].yawBase)
                        {
                        case Yaw::forward:
                            yaw += 0.f;
                            break;
                        case Yaw::backward:
                            yaw -= 180.f;
                            break;
                        case Yaw::right:
                            yaw -= -90.f;
                            break;
                        case Yaw::left:
                            yaw -= 90.f;
                            break;
                        case Yaw::spin:
                            staticYaw += static_cast<float>(config->rageAntiAim[Condition::getState(cmd)].spinBase);
                            yaw -= staticYaw;
                            break;
                        default:
                            break;
                        }
                        willgetstabbed = true;
                    }
                    else
                        willgetstabbed = false;
                }
            }
            if (config->rageAntiAim[Condition::getState(cmd)].yawBase != Yaw::spin)
                staticYaw = 0.f;

            switch (config->rageAntiAim[Condition::getState(cmd)].yawBase)
            {
            case Yaw::forward:
                yaw += 0.f;
                break;
            case Yaw::backward:
                yaw += 180.f;
                break;
            case Yaw::right:
                yaw += -90.f;
                break;
            case Yaw::left:
                yaw += 90.f;
                break;
            case Yaw::spin:
                staticYaw += static_cast<float>(config->rageAntiAim[Condition::getState(cmd)].spinBase);
                yaw += staticYaw;
                break;
            default:
                break;
            }
            if (config->rageAntiAim[Condition::getState(cmd)].freestand && config->binds.freestand.isActive() && !willgetstabbed)
            {
                constexpr std::array positions = { -30.0f, 0.0f, 30.0f };
                std::array active = { false, false, false };
                const auto fwd = Vector::fromAngle2D(cmd->viewangles.y);
                const auto side = fwd.crossProduct(Vector::up());

                for (std::size_t i{}; i < positions.size(); i++)
                {
                    const auto start = localPlayer->getEyePosition() + side * positions[i];
                    const auto end = start + fwd * 100.0f;

                    Trace trace{};
                    TraceFilterWorldAndPropsOnly filter;
                    interfaces->engineTrace->traceRay1({ start, end }, 0x1 | 0x2, filter, trace);

                    if (trace.fraction != 1.0f)
                        active[i] = true;
                }

                if (active[0] && active[1] && !active[2])
                {
                    yaw = 90.f;
                    isFreestanding = true;
                }
                else if (!active[0] && active[1] && active[2])
                {
                    yaw = -90.f;
                    isFreestanding = true;
                }
                else
                {
                    isFreestanding = false;
                }
            }
            if (!willgetstabbed)
            {
                if (back) {
                    yaw = -180.f;
                    if (left)
                        yaw -= 45.f;
                    else if (right)
                        yaw += 45.f;
                }
                else if (left) {
                    yaw = 90.f;
                    if (back)
                        yaw += 45.f;
                    else if (forward)
                        yaw -= 45.f;
                }
                else if (right) {
                    yaw = -90.f;
                    if (back)
                        yaw -= 45.f;
                    else if (forward)
                        yaw += 45.f;
                }
                else if (forward) {
                    yaw = 0.f;
                }
            }
            switch (config->rageAntiAim[Condition::getState(cmd)].yawModifier)
            {
            case 1: //center jitter
                if (!isManualSet && isFreestanding == false && !willgetstabbed)
                {
                    if (isManualSet || isFreestanding == true || willgetstabbed)
                        break;
                    if (config->rageAntiAim[Condition::getState(cmd)].desync.peekMode != 3)
                        yaw -= flipJitter ? config->rageAntiAim[Condition::getState(cmd)].jitterRange : -config->rageAntiAim[Condition::getState(cmd)].jitterRange;
                    else if (config->rageAntiAim[Condition::getState(cmd)].desync.peekMode == 3)
                        yaw -= invert ? config->rageAntiAim[Condition::getState(cmd)].jitterRange : -config->rageAntiAim[Condition::getState(cmd)].jitterRange;
                }
                break;
            case 2: //offset jitter
                if (!isManualSet && isFreestanding == false && !willgetstabbed)
                    yaw += flipJitter ? !config->binds.desyncInvert.isActive() ? config->rageAntiAim[Condition::getState(cmd)].jitterRange : -config->rageAntiAim[Condition::getState(cmd)].jitterRange : 0;
                break;
            case 3: //3way
                if (!isManualSet && isFreestanding == false && !willgetstabbed)
                {
                    static int stage = 0;
                    if (stage == 0)
                    {
                        yaw -= config->rageAntiAim[Condition::getState(cmd)].jitterRange;
                        stage = 1;
                    }
                    else if (stage == 1)
                    {
                        yaw += 0;
                        stage = 2;
                    }
                    else if (stage == 2)
                    {
                        yaw += config->rageAntiAim[Condition::getState(cmd)].jitterRange;
                        stage = 0;
                    }
                    else
                        stage = 0; //fix for invalid stage
                }
                break;
            case 4: //3way with 3way pitch?
                if (!isManualSet && isFreestanding == false && !willgetstabbed)
                {
                    static int stage = 0;
                    if (stage == 0)
                    {
                        yaw -= config->rageAntiAim[Condition::getState(cmd)].jitterRange;
                        cmd->viewangles.x = -89.f;
                        cmd->viewangles.x = 89.f;
                        stage = 1;
                    }
                    else if (stage == 1)
                    {
                        yaw += 0;
                        cmd->viewangles.x = -89.f;
                        cmd->viewangles.x = 89.f;
                        stage = 2;
                    }
                    else if (stage == 2)
                    {
                        yaw += config->rageAntiAim[Condition::getState(cmd)].jitterRange;
                        cmd->viewangles.x = -89.f;
                        cmd->viewangles.x = 89.f;
                        stage = 0;
                    }
                    else
                        stage = 0; //for invalid nigger balls
                }
                break;
            default:
                break;
            }

            yaw += static_cast<float>(config->rageAntiAim[Condition::getState(cmd)].yawAdd);
            cmd->viewangles.y += yaw;
        }
        if (config->rageAntiAim[Condition::getState(cmd)].desync.enabled) //Fakeangle
        {
            if (const auto gameRules = (*memory->gameRules); gameRules)
                if (getGameMode() != GameMode::Competitive && gameRules->isValveDS())
                    return;

            bool isInvertToggled = config->binds.desyncInvert.isActive();
            if (config->rageAntiAim[Condition::getState(cmd)].desync.peekMode != 3)
                invert = isInvertToggled;
            float leftDesyncAngle = config->rageAntiAim[Condition::getState(cmd)].desync.leftLimit * 2.f;
            float rightDesyncAngle = config->rageAntiAim[Condition::getState(cmd)].desync.rightLimit * -2.f;

            switch (config->rageAntiAim[Condition::getState(cmd)].desync.peekMode)
            {
            case 0:
                break;
            case 1: // Peek real
                if (!isInvertToggled)
                    invert = !autoDirection(cmd->viewangles);
                else
                    invert = autoDirection(cmd->viewangles);
                break;
            case 2: // Peek fake
                if (isInvertToggled)
                    invert = !autoDirection(cmd->viewangles);
                else
                    invert = autoDirection(cmd->viewangles);
                break;
            case 3: // Jitter
                if (sendPacket)
                    invert = !invert;
                break;
            default:
                break;
            }

            switch (config->rageAntiAim[Condition::getState(cmd)].desync.lbyMode)
            {
            case 0: // Normal(forward)
                if (fabsf(cmd->forwardmove) < 5.0f)
                {
                    float modifier = 0.f;
                    if (cmd->buttons & UserCmd::IN_DUCK || (config->misc.fakeduck && config->binds.fakeduck.isActive()))
                        modifier = 3.25f;
                    else
                        modifier = 1.1f;

                    cmd->forwardmove = cmd->tickCount & 1 ? modifier : -modifier;
                }
                break;
            case 1: // Opposite (Lby break)
                if (updateLby())
                {
                    cmd->viewangles.y += !invert ? leftDesyncAngle : rightDesyncAngle;
                    sendPacket = false;
                    return;
                }
                break;
            case 2: //Sway (flip every lby update)
                static bool flip = false;
                if (updateLby())
                {
                    cmd->viewangles.y += !flip ? leftDesyncAngle : rightDesyncAngle;
                    sendPacket = false;
                    flip = !flip;
                    return;
                }
                if (!sendPacket)
                    cmd->viewangles.y += flip ? leftDesyncAngle : rightDesyncAngle;
                break;
            }

            if (sendPacket)
                return;

            desyncSide = invert ? 0 : 1;
            cmd->viewangles.y += invert ? leftDesyncAngle : rightDesyncAngle;
        }
    }
}

void AntiAim::updateInput() noexcept
{
    config->binds.desyncInvert.handleToggle();
}

void AntiAim::run(UserCmd* cmd, const Vector& previousViewAngles, const Vector& currentViewAngles, bool& sendPacket) noexcept
{
    if (cmd->buttons & (UserCmd::IN_USE))
        return;

    if (localPlayer->moveType() == MoveType::LADDER || localPlayer->moveType() == MoveType::NOCLIP)
        return;

    if (config->condAA.global)
        AntiAim::rage(cmd, previousViewAngles, currentViewAngles, sendPacket);
}

bool AntiAim::canRun(UserCmd* cmd) noexcept
{
    if (!localPlayer || !localPlayer->isAlive())
        return false;

    updateLby(true); //Update lby timer

    if (config->disableInFreezetime && (!*memory->gameRules || (*memory->gameRules)->freezePeriod()))
        return false;

    if (localPlayer->flags() & (1 << 6))
        return false;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip())
        return true;

    if (activeWeapon->isThrowing())
        return false;

    if (activeWeapon->isGrenade())
        return true;

    if (localPlayer->shotsFired() > 0 && !activeWeapon->isFullAuto() || localPlayer->waitForNoAttack())
        return true;

    if (localPlayer->nextAttack() > memory->globalVars->serverTime())
        return true;

    if (activeWeapon->nextPrimaryAttack() > memory->globalVars->serverTime())
        return true;

    if (activeWeapon->nextSecondaryAttack() > memory->globalVars->serverTime())
        return true;

    if (localPlayer->nextAttack() <= memory->globalVars->serverTime() && (cmd->buttons & (UserCmd::IN_ATTACK)))
        return false;

    if (activeWeapon->nextPrimaryAttack() <= memory->globalVars->serverTime() && (cmd->buttons & (UserCmd::IN_ATTACK)))
        return false;

    if (activeWeapon->isKnife())
    {
        if (activeWeapon->nextSecondaryAttack() <= memory->globalVars->serverTime() && cmd->buttons & (UserCmd::IN_ATTACK2))
            return false;
    }

    if (activeWeapon->itemDefinitionIndex2() == WeaponId::Revolver && activeWeapon->readyTime() <= memory->globalVars->serverTime() && cmd->buttons & (UserCmd::IN_ATTACK | UserCmd::IN_ATTACK2))
        return false;

    const auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
    if (!weaponIndex)
        return true;

    return true;
}
