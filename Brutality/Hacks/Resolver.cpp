#include "AimbotFunctions.h"
#include "Animations.h"
#include "Resolver.h"

#include "../Logger.h"
#include "../Math.h"


#include "../SDK/GameEvent.h"
#include "../SDK/PredictionCopy.h"
#include "../SDK/NetworkChannel.h"

std::array<Resolver::Info, 65> Resolver::player;
std::deque<Resolver::Ticks> Resolver::bulletImpacts;
std::deque<Resolver::Ticks> Resolver::tick;
std::deque<Resolver::Tick> Resolver::shot[65];
std::deque<Resolver::SnapShot> snapshots;
std::deque<float> LastYaws[128];

float desyncAng{ 0 };
UserCmd* command;
bool resolver = false;

void Resolver::initialize(Animations::Players* e, adjust_data* record, const float& goal_feet_yaw, const float& pitch)
{
	Animations::Players e2{};

	original_goal_feet_yaw = Helpers::normalizeYaw(goal_feet_yaw);
	original_pitch = Helpers::normalize_pitch(pitch);
}

void Resolver::reset() noexcept
{
	snapshots.clear();

	playerr = nullptr;
	player_record = nullptr;

	side = false;
	fake = false;

	was_first_bruteforce = false;
	was_second_bruteforce = false;

	original_goal_feet_yaw = 0.0f;
	original_pitch = 0.0f;
}

float ApproachAngle(float flTarget, float flValue, float flSpeed)
{
	// flTarget = flAngleMod(flTarget);
	// flValue = flAngleMod(flValue);

	float delta = flTarget - flValue;

	if (flSpeed < 0)
		flSpeed = -flSpeed;

	if (delta < -180)
		delta += 360;
	else if (delta > 180)
		delta -= 360;

	if (delta > flSpeed)
		flValue += flSpeed;
	else if (delta < -flSpeed)
		flValue -= flSpeed;
	else
		flValue = flTarget;

	return flValue;
}

static auto GetSmoothedVelocity = [](float min_delta, Vector a, Vector b) {
	Vector delta = a - b;
	float delta_length = delta.length();

	if (delta_length <= min_delta) {
		Vector result;
		if (-min_delta <= delta_length) {
			return a;
		}
		else {
			float iradius = 1.0f / (delta_length + FLT_EPSILON);
			return b - ((delta * iradius) * min_delta);
		}
	}
	else {
		float iradius = 1.0f / (delta_length + FLT_EPSILON);
		return b + ((delta * iradius) * min_delta);
	}
	};

void Resolver::saveRecord(int playerIndex, float playerSimulationTime) noexcept
{
	const auto entity = interfaces->entityList->getEntity(playerIndex);
	const auto player = Animations::getPlayer(playerIndex);
	if (!player.gotMatrix || !entity)
		return;

	SnapShot snapshot;
	snapshot.player = player;
	snapshot.playerIndex = playerIndex;
	snapshot.eyePosition = localPlayer->getEyePosition();
	snapshot.model = entity->getModel();

	if (player.simulationTime == playerSimulationTime)
	{
		snapshots.push_back(snapshot);
		return;
	}

	for (int i = 0; i < static_cast<int>(player.backtrackRecords.size()); i++)
	{
		if (player.backtrackRecords.at(i).simulationTime == playerSimulationTime)
		{
			snapshot.backtrackRecord = i;
			snapshots.push_back(snapshot);
			return;
		}
	}
}

void Resolver::getEvent(GameEvent* event) noexcept
{
	if (!event || !localPlayer || interfaces->engine->isHLTV())
		return;

	switch (fnv::hashRuntime(event->getName())) {
	case fnv::hash("round_start"):
	{
		//Reset all
		auto players = Animations::setPlayers();
		if (players->empty())
			break;

		for (int i = 0; i < static_cast<int>(players->size()); i++)
		{
			players->at(i).misses = 0;
		}
		snapshots.clear();
		break;
	}
	case fnv::hash("player_death"):
	{
		//Reset player
		const auto playerId = event->getInt("userid");
		if (playerId == localPlayer->getUserId())
			break;

		const auto index = interfaces->engine->getPlayerForUserID(playerId);
		Animations::setPlayer(index)->misses = 0;
		break;
	}
	case fnv::hash("player_hurt"):
	{
		if (snapshots.empty())
			break;
		if (!localPlayer || !localPlayer->isAlive())
		{
			snapshots.clear();
			return;
		}

		if (event->getInt("attacker") != localPlayer->getUserId())
			break;

		const auto hitgroup = event->getInt("hitgroup");
		if (hitgroup < HitGroup::Head || hitgroup > HitGroup::RightLeg)
			break;

		const auto index = interfaces->engine->getPlayerForUserID(event->getInt("userid"));
		auto& snapshot = snapshots.front();
		if (desyncAng != 0.f)
		{
			if (hitgroup == HitGroup::Head)
			{
				Animations::setPlayer(index)->workingangle = desyncAng;
			}
		}
		const auto entity = interfaces->entityList->getEntity(snapshot.playerIndex);

		Logger::addLog("[Brutality] Hit " + entity->getPlayerName() + ", using Angle: " + std::to_string(desyncAng));
		if (!entity->isAlive())
			desyncAng = 0.f;

		snapshots.pop_front(); //Hit somebody so dont calculate
		break;
	}
	case fnv::hash("bullet_impact"):
	{
		if (snapshots.empty())
			break;

		if (event->getInt("userid") != localPlayer->getUserId())
			break;

		auto& snapshot = snapshots.front();

		if (!snapshot.gotImpact)
		{
			snapshot.time = memory->globalVars->serverTime();
			snapshot.bulletImpact = Vector{ event->getFloat("x"), event->getFloat("y"), event->getFloat("z") };
			snapshot.gotImpact = true;
		}
		break;
	}
	default:
		break;
	}
	if (!resolver)
		snapshots.clear();
}

void Resolver::processMissedShots() noexcept
{
	if (!resolver)
	{
		snapshots.clear();
		return;
	}

	if (!localPlayer || !localPlayer->isAlive())
	{
		snapshots.clear();
		return;
	}

	if (snapshots.empty())
		return;

	if (snapshots.front().time == -1) //Didnt get data yet
		return;

	auto snapshot = snapshots.front();
	snapshots.pop_front(); //got the info no need for this
	if (!snapshot.player.gotMatrix)
		return;

	const auto entity = interfaces->entityList->getEntity(snapshot.playerIndex);
	if (!entity)
		return;

	const Model* model = snapshot.model;
	if (!model)
		return;

	StudioHdr* hdr = interfaces->modelInfo->getStudioModel(model);
	if (!hdr)
		return;

	StudioHitboxSet* set = hdr->getHitboxSet(0);
	if (!set)
		return;

	const auto angle = AimbotFunction::calculateRelativeAngle(snapshot.eyePosition, snapshot.bulletImpact, Vector{ });
	const auto end = snapshot.bulletImpact + Vector::fromAngle(angle) * 2000.f;

	const auto matrix = snapshot.backtrackRecord == -1 ? snapshot.player.matrix.data() : snapshot.player.backtrackRecords.at(snapshot.backtrackRecord).matrix;

	bool resolverMissed = false;

	for (int hitbox = 0; hitbox < Hitboxes::Max; hitbox++)
	{
		if (AimbotFunction::hitboxIntersection(matrix, hitbox, set, snapshot.eyePosition, end))
		{
			resolverMissed = true;
			std::string missed = "Missed " + entity->getPlayerName() + " due to resolver";
			if (snapshot.backtrackRecord > 0)
				missed += "bt: " + std::to_string(snapshot.backtrackRecord);
			Logger::addLog(missed);
			Animations::setPlayer(snapshot.playerIndex)->misses++;
			break;
		}
	}
	if (!resolverMissed)
		Logger::addLog("Missed due to spread");
}

Vector calcAngle(Vector source, Vector entityPos) {
	Vector delta = {};
	delta.x = source.x - entityPos.x;
	delta.y = source.y - entityPos.y;
	delta.z = source.z - entityPos.z;
	Vector angles = {};
	Vector viewangles = command->viewangles;
	angles.x = Helpers::rad2deg(atan(delta.z / hypot(delta.x, delta.y))) - viewangles.x;
	angles.y = Helpers::rad2deg(atan(delta.y / delta.x)) - viewangles.y;
	angles.z = 0;
	if (delta.x >= 0.f)
		angles.y += 180;

	return angles;
}

float get_backward_side(Entity* entity) {
	if (!entity->isAlive())
		return -1.f;
	float result = Helpers::angleDiff(localPlayer->origin().y, entity->origin().y);
	return result;
}

float get_angle(Entity* entity) {
	return Helpers::angleNormalize(entity->eyeAngles().y);
}

float get_foword_yaw(Entity* entity) {
	return Helpers::angleNormalize(get_backward_side(entity) - 180.f);
}

void SideCheck(int* side, Entity* entity)
{
	if (!entity || !entity->isAlive())
		return;
	PlayerInfo player_info;

	if (!interfaces->engine->getPlayerInfo(entity->index(), player_info))
		return;

	Vector src3D, dst3D, forward, right, up, src, dst;
	float back_two, right_two, left_two;
	Trace tr;
	TraceFilter filter = entity;

	Math::angle_vectors(Vector(0, Math::calculate_angle(localPlayer->getAbsOrigin(), entity->getAbsOrigin()).y, 0), &forward, &right, &up);

	filter.skip = entity;
	src3D = entity->eyeAngles();
	dst3D = src3D + (forward * 384);

	interfaces->engineTrace->traceRay({ src3D, dst3D }, MASK_SHOT, filter, tr);
	back_two = (tr.endpos - tr.startpos).length();


	interfaces->engineTrace->traceRay({ src3D + right * 35, dst3D + right * 35 }, MASK_SHOT, filter, tr);
	right_two = (tr.endpos - tr.startpos).length();

	interfaces->engineTrace->traceRay({ src3D - right * 35, dst3D - right * 35 }, MASK_SHOT, filter, tr);
	left_two = (tr.endpos - tr.startpos).length();

	if (left_two > right_two) {
		*side = 1;
	}
	else if (right_two > left_two) {
		*side = -1;
	}
	else
		*side = 1;


}


void Resolver::runPreUpdate(Animations::Players player, Entity* entity) noexcept
{
	if (!resolver)
		return;

	const auto misses = player.misses;
	if (!entity || !entity->isAlive())
		return;



	if (player.chokedPackets <= 0)
		return;

	SnapShot snapshot;

	float Angle = 0;

	float max_dysenc;

	auto animstate = uintptr_t(entity->getAnimstate());

	float duckammount = *(float*)(animstate + 0xA4);
	float speedfraction = max(0, min(*reinterpret_cast<float*>(animstate + 0xF8), 1));

	float speedfactor = max(0, min(1, *reinterpret_cast<float*> (animstate + 0xFC)));

	float vel = ((*reinterpret_cast<float*> (animstate + 0x11C) * -0.30000001) - 0.19999999) * speedfraction;
	float duck = vel + 1.f;
	float final;

	if (duckammount > 0) {

		duck += ((duckammount * speedfactor) * (0.5f - duck));

	}

	final = *(float*)(animstate + 0x334) * duck;

	max_dysenc = (final * -1) / 2;

	AnimationLayer ResolverLayers[3][13];

	float orig_rate = player.oldlayers[6].playbackRate;

	float_t CenterYawPlaybackRate = ResolverLayers[0][6].playbackRate;
	float_t RightYawPlaybackRate = ResolverLayers[1][6].playbackRate;
	float_t LeftYawPlaybackRate = ResolverLayers[2][6].playbackRate;


	float_t LeftYawDelta = fabsf(LeftYawPlaybackRate - orig_rate);
	float_t RightYawDelta = fabsf(RightYawPlaybackRate - orig_rate);
	float_t CenterYawDelta = fabsf(LeftYawDelta - RightYawDelta);

	auto IsExtended = (player.layers[3].weight == 0.0f && player.layers[3].cycle == 0.0f);
	int side;


	SideCheck(&side, entity);
	int CurSide = 0;

	if (side == -1 && LeftYawDelta > RightYawDelta)
		CurSide = -1;
	else if (side == 1 && LeftYawDelta < RightYawDelta)
		CurSide = 1;

	if (CurSide == -1)
		Angle = max_dysenc * -1;
	else if (CurSide == 1)
		Angle = max_dysenc;


	entity->getAnimstate()->footYaw = Math::NormilizeYaw(entity->eyeAngles().y + Angle);
	//std::cout << "Enemy: " << entity->getPlayerName() << " | eye angle: " << Math::NormilizeYaw(entity->eyeAngles().y) << " | Full Resolve angle:" << entity->getAnimstate()->footYaw << "| Max desync: " << max_dysenc << "\n";


}

void Resolver::runPostUpdate(Animations::Players player, Entity* entity) noexcept
{
	if (!resolver)
		return;

	const auto misses = player.misses;
	if (!entity || !entity->isAlive())
		return;

	if (player.chokedPackets <= 0)
		return;

	if (!localPlayer || !localPlayer->isAlive())
		return;

	if (snapshots.empty())
		return;

	auto& snapshot = snapshots.front();
	auto animstate = entity->getAnimstate();
	Resolver::setup_detect(player, entity);
	Resolver::ResolveEntity(player, entity);
	desyncAng = animstate->footYaw;
	if (snapshot.player.workingangle != 0.f && fabs(desyncAng) > fabs(snapshot.player.workingangle))
	{
		if (snapshot.player.workingangle < 0.f && player.side == 1)
			snapshot.player.workingangle = fabs(snapshot.player.workingangle);
		else if (snapshot.player.workingangle > 0.f && player.side == -1)
			snapshot.player.workingangle = snapshot.player.workingangle * (-1.f);
		desyncAng = snapshot.player.workingangle;
		animstate->footYaw = desyncAng;
	}

	Resolver::resolveYaw();
	Resolver::resolvePitch();
}

float build_server_abs_yaw(Animations::Players player, Entity* entity, float angle)
{
	Vector velocity = entity->velocity();
	auto anim_state = entity->getAnimstate();
	float m_flEyeYaw = angle;
	float m_flGoalFeetYaw = 0.f;

	float eye_feet_delta = Helpers::angleDiff(m_flEyeYaw, m_flGoalFeetYaw);

	static auto GetSmoothedVelocity = [](float min_delta, Vector a, Vector b) {
		Vector delta = a - b;
		float delta_length = delta.length();

		if (delta_length <= min_delta)
		{
			Vector result;

			if (-min_delta <= delta_length)
				return a;
			else
			{
				float iradius = 1.0f / (delta_length + FLT_EPSILON);
				return b - ((delta * iradius) * min_delta);
			}
		}
		else
		{
			float iradius = 1.0f / (delta_length + FLT_EPSILON);
			return b + ((delta * iradius) * min_delta);
		}
		};

	float spd = velocity.squareLength();;

	if (spd > std::powf(1.2f * 260.0f, 2.f))
	{
		Vector velocity_normalized = velocity.normalized();
		velocity = velocity_normalized * (1.2f * 260.0f);
	}

	float m_flChokedTime = anim_state->lastUpdateTime;
	float clampDuck = std::clamp(entity->duckAmount() + anim_state->duckAdditional, 0.0f, 1.0f);
	float duckAmount = anim_state->animDuckAmount;
	float v27 = m_flChokedTime * 6.0f;
	float v28;

	// clamp
	if ((clampDuck - duckAmount) <= v27) {
		if (-v27 <= (clampDuck - duckAmount))
			v28 = clampDuck;
		else
			v28 = duckAmount - v27;
	}
	else {
		v28 = duckAmount + v27;
	}

	float flDuckAmount = std::clamp(v28, 0.0f, 1.0f);

	Vector animationVelocity = GetSmoothedVelocity(m_flChokedTime * 2000.0f, velocity, entity->velocity());
	float speed = std::fminf(animationVelocity.length(), 260.0f);

	float flMaxMovementSpeed = 260.0f;

	Entity* pWeapon = entity->getActiveWeapon();
	if (pWeapon && pWeapon->getWeaponData())
		flMaxMovementSpeed = std::fmaxf(pWeapon->getWeaponData()->maxSpeedAlt, 0.001f);

	float flRunningSpeed = speed / (flMaxMovementSpeed * 0.520f);
	float flDuckingSpeed = speed / (flMaxMovementSpeed * 0.340f);

	flRunningSpeed = std::clamp(flRunningSpeed, 0.0f, 1.0f);

	float flYawModifier = (((anim_state->walkToRunTransition * -0.30000001) - 0.19999999) * flRunningSpeed) + 1.0f;

	if (flDuckAmount > 0.0f)
	{
		float flDuckingSpeed = std::clamp(flDuckingSpeed, 0.0f, 1.0f);
		flYawModifier += (flDuckAmount * flDuckingSpeed) * (0.5f - flYawModifier);
	}

	const float pos = -58.f;
	const float negative = 58.f;

	float flMinYawModifier = pos * flYawModifier;
	float flMaxYawModifier = negative * flYawModifier;

	if (eye_feet_delta <= flMaxYawModifier)
	{
		if (flMinYawModifier > eye_feet_delta)
			m_flGoalFeetYaw = fabs(flMinYawModifier) + m_flEyeYaw;
	}
	else
	{
		m_flGoalFeetYaw = m_flEyeYaw - fabs(flMaxYawModifier);
	}

	Helpers::normalizeYaw(m_flGoalFeetYaw);

	if (speed > 0.1f || fabs(velocity.z) > 100.0f)
	{
		m_flGoalFeetYaw = Helpers::approachAngle(
			m_flEyeYaw,
			m_flGoalFeetYaw,
			((anim_state->walkToRunTransition * 20.0f) + 30.0f)
			* m_flChokedTime);
	}
	else
	{
		m_flGoalFeetYaw = Helpers::approachAngle(
			entity->lby(),
			m_flGoalFeetYaw,
			m_flChokedTime * 100.0f);
	}

	return m_flGoalFeetYaw;
}

void Resolver::delta_side_detect(int* side, Entity* entity)
{

	float EyeDelta = Helpers::normalizeYaw(entity->eyeAngles().y);

	if (fabs(EyeDelta) > 5)
	{
		if (EyeDelta > 5)
			*side = -1;
		else if (EyeDelta < -5)
			*side = 1;
	}
	else
		*side = 0;
}

void Resolver::detect_side(Entity* entity, int* side) {
	/* externs */
	Vector src3D, dst3D, forward, right, up;
	float back_two, right_two, left_two;
	Trace tr;
	Helpers::AngleVectors(Vector(0, get_backward_side(entity), 0), &forward, &right, &up);
	/* filtering */

	src3D = entity->getEyePosition();
	dst3D = src3D + (forward * 384);

	/* back engine tracers */
	interfaces->engineTrace->traceRay({ src3D, dst3D }, 0x200400B, { entity }, tr);
	back_two = (tr.endpos - tr.startpos).length();

	/* right engine tracers */
	interfaces->engineTrace->traceRay(Ray(src3D + right * 35, dst3D + right * 35), 0x200400B, { entity }, tr);
	right_two = (tr.endpos - tr.startpos).length();

	/* left engine tracers */
	interfaces->engineTrace->traceRay(Ray(src3D - right * 35, dst3D - right * 35), 0x200400B, { entity }, tr);
	left_two = (tr.endpos - tr.startpos).length();

	/* fix side */
	if (left_two > right_two) {
		*side = -1;
	}
	else if (right_two > left_two) {
		*side = 1;
	}
	else
	{
		delta_side_detect(side, entity);
	}
}

void Resolver::ResolveEntity(Animations::Players player, Entity* entity) {

	Resolver::detect_side(entity, &player.side);//dectside

	// get the players max rotation.
	float max_rotation = entity->getMaxDesyncAngle();
	int index = 0;
	float eye_yaw = entity->getAnimstate()->eyeYaw;
	bool extended = player.extended;
	if (!extended && fabs(max_rotation) > 60.f)
	{
		max_rotation = max_rotation / 1.8f;
	}

	// resolve shooting players separately.
	if (player.shot) {
		entity->getAnimstate()->footYaw = eye_yaw + Resolver::ResolveShot(player, entity);
		return;
	}
	else {
		if (entity->velocity().length2D() <= 0.1) {
			float angle_difference = Helpers::angleDiff(eye_yaw, entity->getAnimstate()->footYaw);
			index = 2 * angle_difference <= 0.0f ? 1 : -1;
		}
		else
		{
			if (!((int)player.layers[12].weight * 1000.f) && entity->velocity().length2D() > 0.1) {

				auto m_layer_delta1 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
				auto m_layer_delta2 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);
				auto m_layer_delta3 = abs(player.layers[6].playbackRate - player.oldlayers[6].playbackRate);

				if (m_layer_delta1 < m_layer_delta2
					|| m_layer_delta3 <= m_layer_delta2
					|| (signed int)(float)(m_layer_delta2 * 1000.0))
				{
					if (m_layer_delta1 >= m_layer_delta3
						&& m_layer_delta2 > m_layer_delta3
						&& !(signed int)(float)(m_layer_delta3 * 1000.0))
					{
						index = 1;
					}
				}
				else
				{
					index = -1;
				}
			}
		}
	}

	switch (player.misses % 3) {
	case 0: //default
		entity->getAnimstate()->footYaw = build_server_abs_yaw(player, entity, entity->eyeAngles().y + max_rotation * index);
		break;
	case 1: //reverse
		entity->getAnimstate()->footYaw = build_server_abs_yaw(player, entity, entity->eyeAngles().y + max_rotation * -index);
		break;
	case 2: //middle
		entity->getAnimstate()->footYaw = build_server_abs_yaw(player, entity, entity->eyeAngles().y);
		break;
	case 3: // roll!!!
		entity->getAnimstate()->footYaw = build_server_abs_yaw(player, entity, entity->eyeAngles().z);
		break;
	}

}

float Resolver::ResolveShot(Animations::Players player, Entity* entity) {
	/* fix unrestricted shot */
	float flPseudoFireYaw = Helpers::angleNormalize(Helpers::angleDiff(localPlayer->origin().y, player.matrix[8].origin().y));
	if (player.extended) {
		float flLeftFireYawDelta = fabsf(Helpers::angleNormalize(flPseudoFireYaw - (entity->eyeAngles().y + 58.f)));
		float flRightFireYawDelta = fabsf(Helpers::angleNormalize(flPseudoFireYaw - (entity->eyeAngles().y - 58.f)));

		return flLeftFireYawDelta > flRightFireYawDelta ? -58.f : 58.f;
	}
	else {
		float flLeftFireYawDelta = fabsf(Helpers::angleNormalize(flPseudoFireYaw - (entity->eyeAngles().y + 29.f)));
		float flRightFireYawDelta = fabsf(Helpers::angleNormalize(flPseudoFireYaw - (entity->eyeAngles().y - 29.f)));

		return flLeftFireYawDelta > flRightFireYawDelta ? -29.f : 29.f;
	}
}

void Resolver::AddCurrentYaw(Entity* pl, int i)
{
	LastYaws[i].push_back(pl->eyeAngles().y);

	if (LastYaws[i].size() > 8)
		LastYaws[i].erase(LastYaws[i].begin());
}

bool Resolver::IsStaticYaw(int i)
{
	if (LastYaws[i].size() < 3)
		return true;

	float HighestDiff = 0.f;

	for (size_t p = 0; p < LastYaws[i].size(); p++)
	{
		for (size_t p2 = 0; p2 < LastYaws[i].size(); p2++)
		{
			float c = fabs(fabs(LastYaws[i].at(p)) - fabs(LastYaws[i].at(p2)));

			if (c > HighestDiff)
				HighestDiff = c;
		}
	}

	if (HighestDiff > 15.f)
		return false;
	else
		return true;

}

bool Resolver::GetAverageYaw(int i, float& ang)
{
	if (LastYaws[i].size() < 3)
		return true;

	float add = 0.f;

	for (size_t p = 0; p < LastYaws[i].size(); p++)
		add += LastYaws[i].at(p);

	ang = add / LastYaws[i].size();
	return true;
}

void Resolver::resolveYaw()
{
	//getting proper values

	Entity* e{ };
	auto animstate = e->getAnimstate();

	if (!animstate)
		return;

	//	 save old goalfeet yaw
	auto flOldGoalFeetYaw = animstate->footYaw;

	//	 save old eye angles
	auto angOldEyeAngles = e->eyeAngles();

	//	 get max desync delta
	auto flMaxDesync = e->getMaxDesyncAngle();

	//	 get max desync delta when breaking lby
	auto flMaxDesyncLBY = e->getMaxDesyncAngle() * 2;

	//	 check if player has lowDelta
	auto bHasLowDelta = e->getAbsVelocity().length2D() > e->getMaxSpeed() / 6.99;

	//	 get local weapon
	auto pWeapon = localPlayer->getActiveWeapon();
	if (pWeapon == nullptr)
		return;

	//	 adjust desync based on delta
	if (bHasLowDelta) flMaxDesync = flMaxDesync / 2;

	//	 adjusting desync based on delta
	if (bHasLowDelta) flMaxDesyncLBY = flMaxDesyncLBY / 2;

	//	 lets count our missed shots
	auto iMissedShots = 0;

	//	 corrected angles
	auto flCorrectDesyncRange = 0.f; auto flPitchCorrection = 0.f; auto flRollCorrection = 0.f;

	switch (iMissedShots)
	{
	case 0:
		flCorrectDesyncRange = memory->randomFloat(0, e->getMaxDesyncAngle() * 2 / 4);
		flMaxDesync = memory->randomFloat(flMaxDesync, flCorrectDesyncRange);
		break;
	case 1:
		flCorrectDesyncRange = memory->randomFloat(0, e->getMaxSpeed());
		if (flCorrectDesyncRange > 32) flMaxDesync = 25.f;
		if (flCorrectDesyncRange > 82) flMaxDesync = 73.f;
		if (flCorrectDesyncRange > 132) flMaxDesync = 43.f;
		if (flCorrectDesyncRange > 50) flMaxDesync = 55.f;
		if (flCorrectDesyncRange > 250) flMaxDesync = 1.f;
		flMaxDesync = memory->randomFloat(flMaxDesync, flCorrectDesyncRange);
		break;
	case 2:
		flPitchCorrection = memory->randomFloat(-1935.f, 1935.f);
		e->eyeAngles().x + flPitchCorrection;
		break;
	case 3:
		flPitchCorrection = memory->randomFloat(-1935.f, 1935.f);
		e->eyeAngles().x - flPitchCorrection;
		break;
	case 4:
		flRollCorrection = memory->randomFloat(-45.f, 45.f);
		e->eyeAngles().z - flRollCorrection;
		break;
	default:
		animstate->footYaw = flOldGoalFeetYaw;
		e->eyeAngles() = angOldEyeAngles;
	}

	//	 reset missed shots
	if (iMissedShots == 6) iMissedShots = 0;

	//	 some random shit yes
	if (animstate->footYaw > 1.f) animstate->footYaw = -flMaxDesync; else if (animstate->footYaw == 0.f) animstate->footYaw = -flMaxDesyncLBY;
	if (animstate->footYaw < 1.f) animstate->footYaw = +flMaxDesync; else if (animstate->footYaw == -0.f) animstate->footYaw = +flMaxDesyncLBY;

	if (false);
	if (animstate->footYaw > 1.f)
		animstate->footYaw -= flMaxDesync;
	else if (animstate->footYaw == 0.f)
		animstate->footYaw = -flMaxDesyncLBY;

	if (true);
	if (animstate->footYaw < 1.f)
		animstate->footYaw = +flMaxDesync;
	else if (animstate->footYaw == -0.f)
		animstate->footYaw = +flMaxDesyncLBY;

	for (int i = 1; i < interfaces->engine->getMaxClients(); i++)
	{
		SavedResolverData srd{};
		bool Moving = e->velocity().length2D() > 0.1f;
		bool SlowWalk = Moving && e->velocity().length2D() < 52.f && fabs(srd.LastVel - e->velocity().length2D()) < 4.f;
		bool InAir = !(e->flags() & 1 << 0);
		bool Standing = !Moving && !InAir;

		srd.LastVel = e->velocity().length2D();

		float avgAng = 0.f;
		bool b = GetAverageYaw(i, avgAng);

		if (Moving && !SlowWalk && !InAir)
			resolved = true;
		else if (Moving && SlowWalk && !InAir)
		{
			if (IsStaticYaw(i) && b)
			{
				if (Shots >= 0)
				{
					switch (Shots % 4)
					{
					case 0:
						e->eyeAngles().y += 58.f;
						break;

					case 1:
						e->eyeAngles().y -= 58.f;
						break;

					case 2:
						e->eyeAngles().y += 29.f;
						break;

					case 3:
						e->eyeAngles().y -= 29.f;
						break;
					case 4:
						e->eyeAngles().z += 90.f;
						break;
					case 5:
						e->eyeAngles().z -= 90.f;
						break;
					}
				}
			}

			resolved = false;
		}
		else if (InAir)
			resolved = true;
		else
		{
			resolved = true;
			float flAng = fabs(fabs(e->lby()) - fabs(e->eyeAngles().y));
			bool fake = (flAng >= 45.f && flAng <= 85.f) || !IsStaticYaw(i);
			float Yaw = e->eyeAngles().y;

			if (fake)
			{
				resolved = false;
				if (!IsStaticYaw(i) && b)
					Yaw = avgAng;
			}

			if (fake && Shots >= 1)
			{
				switch (Shots % 4)
				{
				case 0:
					e->eyeAngles().y = Yaw + 58.f;
					break;
				case 1:
					e->eyeAngles().y = Yaw - 58.f;
					break;
				case 2:
					e->eyeAngles().y = Yaw + 29;
					break;
				case 3:
					e->eyeAngles().y = Yaw - 29;
					break;
				case 4:
					e->eyeAngles().z = Yaw + 90.f;
					break;
				case 5:
					e->eyeAngles().z = Yaw - 90.f;
					break;
				}
			}
		}
		fake = !resolved;
	}
	/*Entity* e{};
	auto animstate = e->getAnimstate();

	if (!animstate)
		return;

	auto flOldGoalFootYaw = animstate->footYaw;
	auto flOldEyeAngles = e->eyeAngles();

//	 vomiting shit from csgo code
	float maxDesync = e->getMaxDesyncAngle();

	float maxDesyncLBY = e->getMaxDesyncAngle();
	auto lowDelta = e->velocity().length() > e->getMaxSpeed() / 6.99;

//	 superior low_delta check, adjusting desync based on delta!
	if (lowDelta)
		maxDesync = maxDesync / 2;
	if (lowDelta)
		maxDesyncLBY = maxDesyncLBY / 2;

	auto missed_shots = 0;

	switch (missed_shots)
	{
	case 0:
		float randValue = memory->randomFloat(0, e->getMaxDesyncAngle() * 2 / 4);//  proper check ig
		maxDesync = memory->randomFloat(maxDesync, randValue);
		break;
	case 1:
		float randValue = memory->randomFloat(0, e->getMaxSpeed());//  speed check
		if (randValue > 33)
			maxDesync = 25.f;
		if (randValue > 82)
			maxDesync = 73.f;
		if (randValue > 132)
			maxDesync = 43.f;
		if (randValue > 50)
			maxDesync = 55.f;
		if (randValue > 250)
			maxDesync = 1.f;
		break;
	case 2:
		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f); // idek if its needed anymore LOL
		e->eyeAngles().x + flPitchCorrection;
		break;
	case 3:
		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f);///idek if its needed anymore LOL
		e->eyeAngles().x - flPitchCorrection;
		break;
	case 4:
		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f); // idek if its needed anymore LOL
		e->eyeAngles().z + flPitchCorrection;
		break;
	case 5:
		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f);//  idek if its needed anymore LOL
		e->eyeAngles().z - flPitchCorrection;
		break;
	default:
		e->eyeAngles() = flOldEyeAngles;
		animstate->footYaw = flOldGoalFootYaw;
		break;
	}

	if (missed_shots == 69)
		missed_shots = 0;

	// superor chek!!

	 if (false);
	if (animstate->footYaw > 1.f)
		animstate->footYaw -= maxDesync;
	else if (animstate->footYaw == 0.f)
		animstate->footYaw = -maxDesyncLBY;

	 if (true);
	if (animstate->footYaw < 1.f)
		animstate->footYaw = +maxDesync;
	else if (animstate->footYaw == -0.f)
		animstate->footYaw = +maxDesyncLBY;
*/
}

float Resolver::resolvePitch()
{
	Entity* ent{};

	if (ent->eyeAngles().x > 50.f) {
		ent->eyeAngles().x = 90.f;
	}

	if (ent->eyeAngles().x < -50.f) {
		ent->eyeAngles().x = -90.f;
	}

	return original_pitch;
}

void Resolver::setup_detect(Animations::Players player, Entity* entity) {

	// detect if player is using maximum desync.
	if (player.layers[3].cycle == 0.f)
	{
		if (player.layers[3].weight = 0.f)
		{
			player.extended = true;
		}
	}
	/* calling detect side */
	Resolver::detect_side(entity, &player.side);
	int side = player.side;
	/* bruting vars */
	float resolve_value = 50.f;
	static float brute = 0.f;
	float fl_max_rotation = entity->getMaxDesyncAngle();
	float fl_eye_yaw = entity->getAnimstate()->eyeYaw;
	float perfect_resolve_yaw = resolve_value;
	bool fl_foword = fabsf(Helpers::angleNormalize(get_angle(entity) - get_foword_yaw(entity))) < 90.f;
	int fl_shots = player.misses;

	/* clamp angle */
	if (fl_max_rotation < resolve_value) {
		resolve_value = fl_max_rotation;
	}

	/* detect if entity is using max desync angle */
	if (player.extended) {
		resolve_value = fl_max_rotation;
	}
	/* setup brting */
	if (fl_shots == 0) {
		brute = perfect_resolve_yaw * (fl_foword ? -side : side);
	}
	else {
		switch (fl_shots % 3) {
		case 0: {
			brute = perfect_resolve_yaw * (fl_foword ? -side : side);
		} break;
		case 1: {
			brute = perfect_resolve_yaw * (fl_foword ? side : -side);
		} break;
		case 2: {
			brute = 0;
		} break;
		}
	}

	/* fix goalfeet yaw */
	entity->getAnimstate()->footYaw = fl_eye_yaw + brute;
}

void Resolver::CmdGrabber(UserCmd* cmd)
{
	command = cmd;
}

void Resolver::updateEventListeners(bool forceRemove) noexcept
{
	class ImpactEventListener : public GameEventListener {
	public:
		void fireGameEvent(GameEvent* event) {
			getEvent(event);
		}
	};

	static ImpactEventListener listener[4];
	static bool listenerRegistered = false;

	if (resolver && !listenerRegistered) {
		interfaces->gameEventManager->addListener(&listener[0], "bullet_impact");
		interfaces->gameEventManager->addListener(&listener[1], "player_hurt");
		interfaces->gameEventManager->addListener(&listener[2], "round_start");
		interfaces->gameEventManager->addListener(&listener[3], "weapon_fire");
		listenerRegistered = true;
	}
	else if ((!resolver || forceRemove) && listenerRegistered) {
		interfaces->gameEventManager->removeListener(&listener[0]);
		interfaces->gameEventManager->removeListener(&listener[1]);
		interfaces->gameEventManager->removeListener(&listener[2]);
		interfaces->gameEventManager->removeListener(&listener[3]);
		listenerRegistered = false;
	}
}

//void Resolver::resolveYaw()
//{
//	// getting proper values
//
//	Entity* e{ };
//	auto animstate = e->getAnimstate( );
//
//	if ( !animstate )
//		return;
//
//	// save old goalfeet yaw
//	auto flOldGoalFeetYaw = animstate->footYaw;
//
//	// save old eye angles
//	auto angOldEyeAngles = e->eyeAngles( );
//
//	// get max desync delta
//	auto flMaxDesync = e->getMaxDesyncAngle( );
//
//	// get max desync delta when breaking lby
//	auto flMaxDesyncLBY = e->getMaxDesyncAngle( ) * 2;
//
//	// check if player has lowDelta
//	auto bHasLowDelta = e->getAbsVelocity( ).length2D( ) > e->getMaxSpeed( ) / 6.99;
//
//	// get local weapon
//	auto pWeapon = localPlayer->getActiveWeapon( ); 
//	if (pWeapon == nullptr)
//		return;
//
//	// adjust desync based on delta
//	if (bHasLowDelta) flMaxDesync = flMaxDesync / 2;
//
//	// adjusting desync based on delta
//	if (bHasLowDelta) flMaxDesyncLBY = flMaxDesyncLBY / 2;
//
//	// lets count our missed shots
//	auto iMissedShots = 0;
//
//	// corrected angles
//	auto flCorrectDesyncRange = 0.f; auto flPitchCorrection = 0.f; auto flRollCorrection = 0.f;
//
//	switch (iMissedShots)
//	{
//	case 0:
//		flCorrectDesyncRange = memory->randomFloat( 0, e->getMaxDesyncAngle( ) * 2 / 4 ); 
//		flMaxDesync = memory->randomFloat( flMaxDesync, flCorrectDesyncRange );
//		break;
//	case 1:
//		flCorrectDesyncRange = memory->randomFloat( 0, e->getMaxSpeed( ) );
//		if ( flCorrectDesyncRange > 32 ) flMaxDesync = 25.f;
//		if ( flCorrectDesyncRange > 82 ) flMaxDesync = 73.f;
//		if ( flCorrectDesyncRange > 132 ) flMaxDesync = 43.f;
//		if ( flCorrectDesyncRange > 50 ) flMaxDesync = 55.f;
//		if ( flCorrectDesyncRange > 250 ) flMaxDesync = 1.f;
//		flMaxDesync = memory->randomFloat( flMaxDesync, flCorrectDesyncRange );
//		break;
//	case 2:
//		flPitchCorrection = memory->randomFloat( -1935.f, 1935.f );
//		e->eyeAngles( ).x + flPitchCorrection;
//		break;
//	case 3:
//		flPitchCorrection = memory->randomFloat( -1935.f, 1935.f );
//		e->eyeAngles( ).x - flPitchCorrection;
//		break;
//	case 4:
//		flRollCorrection = memory->randomFloat( -45.f, 45.f );
//		e->eyeAngles( ).z - flRollCorrection;
//		break;
//	default:
//		animstate->footYaw = flOldGoalFeetYaw;
//		e->eyeAngles( ) = angOldEyeAngles;
//	}
//
//	// reset missed shots
//	if ( iMissedShots == 6 ) iMissedShots = 0;
//
//	// some random shit yes
//	if ( animstate->footYaw > 1.f ) animstate->footYaw = -flMaxDesync; else if ( animstate->footYaw == 0.f ) animstate->footYaw = -flMaxDesyncLBY;
//	if ( animstate->footYaw < 1.f ) animstate->footYaw = +flMaxDesync; else if ( animstate->footYaw == -0.f ) animstate->footYaw = +flMaxDesyncLBY;
//
//	// if negative
//	if (animstate->footYaw > 1.f)
//		animstate->footYaw -= flMaxDesync;
//	else if (animstate->footYaw == 0.f)
//		animstate->footYaw = -flMaxDesyncLBY;
//
//	// if positive
//	if (animstate->footYaw < 1.f)
//		animstate->footYaw = +flMaxDesync;
//	else if (animstate->footYaw == -0.f)
//		animstate->footYaw = +flMaxDesyncLBY;
//
//	for (int i = 1; i < interfaces->engine->getMaxClients(); i++)
//	{
//		SavedResolverData srd{};
//		bool Moving = e->velocity().length2D() > 0.1f;
//		bool SlowWalk = Moving && e->velocity().length2D() < 52.f && fabs(srd.LastVel - e->velocity().length2D()) < 4.f;
//		bool InAir = !(e->flags() & 1 << 0);
//		bool Standing = !Moving && !InAir;
//
//		srd.LastVel = e->velocity().length2D();
//
//		float avgAng = 0.f;
//		bool b = GetAverageYaw(i, avgAng);
//
//		if (Moving && !SlowWalk && !InAir)
//			resolved = true;
//		else if (Moving && SlowWalk && !InAir)
//		{
//			if (IsStaticYaw(i) && b)
//			{
//				if (Shots >= 0)
//				{
//					switch (Shots % 4)
//					{
//					case 0:
//						e->eyeAngles().y += 58.f;
//						break;
//
//					case 1:
//						e->eyeAngles().y -= 58.f;
//						break;
//
//					case 2:
//						e->eyeAngles().y += 29.f;
//						break;
//
//					case 3:
//						e->eyeAngles().y -= 29.f;
//						break;
//					case 4:
//						e->eyeAngles().z += 90.f;
//						break;
//					case 5:
//						e->eyeAngles().z -= 90.f;
//						break;
//					}
//				}
//			}
//
//			resolved = false;
//		}
//		else if (InAir)
//			resolved = true;
//		else
//		{
//			resolved = true;
//			float flAng = fabs(fabs(e->lby()) - fabs(e->eyeAngles().y));
//			bool fake = (flAng >= 45.f && flAng <= 85.f) || !IsStaticYaw(i);
//			float Yaw = e->eyeAngles().y;
//
//			if (fake)
//			{
//				resolved = false;
//				if (!IsStaticYaw(i) && b)
//					Yaw = avgAng;
//			}
//
//			if (fake && Shots >= 1)
//			{
//				switch (Shots % 4)
//				{
//				case 0:
//					e->eyeAngles().y = Yaw + 58.f;
//					break;
//				case 1:
//					e->eyeAngles().y = Yaw - 58.f;
//					break;
//				case 2:
//					e->eyeAngles().y = Yaw + 29;
//					break;
//				case 3:
//					e->eyeAngles().y = Yaw - 29;
//					break;
//				case 4:
//					e->eyeAngles().z = Yaw + 90.f;
//					break;
//				case 5:
//					e->eyeAngles().z = Yaw - 90.f;
//					break;
//				}
//			}
//		}
//		fake = !resolved;
//	}
//	/*Entity* e{};
//	auto animstate = e->getAnimstate();
//
//	if (!animstate)
//		return;
//
//	auto flOldGoalFootYaw = animstate->footYaw;
//	auto flOldEyeAngles = e->eyeAngles();
//
//	// vomiting shit from csgo code
//	float maxDesync = e->getMaxDesyncAngle();
//
//	float maxDesyncLBY = e->getMaxDesyncAngle();
//	auto lowDelta = e->velocity().length() > e->getMaxSpeed() / 6.99;
//
//	// superior low_delta check, adjusting desync based on delta!
//	if (lowDelta)
//		maxDesync = maxDesync / 2;
//	if (lowDelta)
//		maxDesyncLBY = maxDesyncLBY / 2;
//
//	auto missed_shots = 0;
//
//	switch (missed_shots)
//	{
//	case 0:
//		float randValue = memory->randomFloat(0, e->getMaxDesyncAngle() * 2 / 4); // proper check ig
//		maxDesync = memory->randomFloat(maxDesync, randValue);
//		break;
//	case 1:
//		float randValue = memory->randomFloat(0, e->getMaxSpeed()); // speed check
//		if (randValue > 33)
//			maxDesync = 25.f;
//		if (randValue > 82)
//			maxDesync = 73.f;
//		if (randValue > 132)
//			maxDesync = 43.f;
//		if (randValue > 50)
//			maxDesync = 55.f;
//		if (randValue > 250)
//			maxDesync = 1.f;
//		break;
//	case 2:
//		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f); // idek if its needed anymore LOL
//		e->eyeAngles().x + flPitchCorrection;
//		break;
//	case 3:
//		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f); // idek if its needed anymore LOL
//		e->eyeAngles().x - flPitchCorrection;
//		break;
//	case 4:
//		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f); // idek if its needed anymore LOL
//		e->eyeAngles().z + flPitchCorrection;
//		break;
//	case 5:
//		float flPitchCorrection = memory->randomFloat(-1935.f, 1935.f); // idek if its needed anymore LOL
//		e->eyeAngles().z - flPitchCorrection;
//		break;
//	default:
//		e->eyeAngles() = flOldEyeAngles;
//		animstate->footYaw = flOldGoalFootYaw;
//		break;
//	}
//
//	if (missed_shots == 69)
//		missed_shots = 0;
//
//	// superor chek!!
//
//	// if negative
//	if (animstate->footYaw > 1.f)
//		animstate->footYaw -= maxDesync;
//	else if (animstate->footYaw == 0.f)
//		animstate->footYaw = -maxDesyncLBY;
//
//	// if positive
//	if (animstate->footYaw < 1.f)
//		animstate->footYaw = +maxDesync;
//	else if (animstate->footYaw == -0.f)
//		animstate->footYaw = +maxDesyncLBY;*/
//}





