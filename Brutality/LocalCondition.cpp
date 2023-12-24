#include "LocalCondition.h"

/*
	0 = Global
	1 = Standing
	2 = Moving
	3 = Chrouching
	4 = Slow-walking
	5 = Jumping
	6 = Chrouch jumping
	7 = On ladder
	8 = No clip
*/

int Condition::getState(const UserCmd* cmd)
{
	if (config->condAA.slowwalk && config->binds.slowwalk.isActive() && config->misc.slowwalk)
		return 4;
	if (!localPlayer->getAnimstate()->onGround && cmd->buttons & UserCmd::IN_DUCK && config->condAA.cjumping)
		return 6;
	if (!localPlayer->getAnimstate()->onGround && config->condAA.jumping)
		return 5;
	if (cmd->buttons & UserCmd::IN_DUCK && config->condAA.chrouching)
		return 3;
	if (localPlayer->velocity().length2D() > 5.f && config->condAA.moving)
		return 2;
	else if (config->condAA.standing && localPlayer->velocity().length2D() < 5.f)
		return 1;
	return 0;
}
