#pragma once
#include <iostream>
#include "SDK/Entity.h"
#include "SDK/LocalPlayer.h"
#include "GameData.h"
#include "SDK/UserCmd.h"
#include "Config.h"

namespace Condition
{
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
	int getState(const UserCmd* cmd);
}