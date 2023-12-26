#pragma once

#include "Animations.h"

#include "../SDK/GameEvent.h"
#include "../SDK/Entity.h"

enum
{
	MAIN,
	NONEE,
	FIRST,
	SECOND
};

enum resolver_type
{
	ORIGINAL,
	ZARO,
	FIRST2,
	SECOND2,
	THIRD,
	LOW_FIRST,
	LOW_SECOND

};

enum resolver_side
{
	RESOLVER_ORIGINAL,
	RESOLVER_ZERO,
	RESOLVER_FIRST,
	RESOLVER_SECOND,
	RESOLVER_THIRD,
	RESOLVER_LOW_FIRST,
	RESOLVER_LOW_SECOND
};

struct matrixes
{
	matrix3x4 main[MAXSTUDIOBONES];
	matrix3x4 zero[MAXSTUDIOBONES];
	matrix3x4 positive[MAXSTUDIOBONES];
	matrix3x4 negative[MAXSTUDIOBONES];
};

class adjust_data;


namespace Resolver
{

	Animations::Players* playerr = nullptr;
	adjust_data* player_record = nullptr;

	bool resolved = false;
	bool side = false;
	bool fake = false;
	bool was_first_bruteforce = false;
	bool was_second_bruteforce = false;

	int Shots = 0;

	int missedShots = 0;
	int freestandSide = 0;
	float lastFreestandYaw = 0.f;

	void AddCurrentYaw(Entity* pl, int i);
	bool IsStaticYaw(int i);
	bool GetAverageYaw(int i, float& ang);

	float lock_side = 0.0f;
	float original_goal_feet_yaw = 0.0f;
	float original_pitch = 0.0f;

	void reset() noexcept;

	void processMissedShots() noexcept;
	void saveRecord(int playerIndex, float playerSimulationTime) noexcept;
	void getEvent(GameEvent* event) noexcept;

	void runPreUpdate(Animations::Players player, Entity* entity) noexcept;
	void runPostUpdate(Animations::Players player, Entity* entity) noexcept;

	void detect_side(Entity* entity, int* side);

	void ResolveEntity(Animations::Players player, Entity* entity);

	float ResolveShot(Animations::Players player, Entity* entity);

	void setup_detect(Animations::Players player, Entity* entity);

	bool DesyncDetect(int* side, Entity* entity);

	void delta_side_detect(int* side, Entity* entity);

	void CmdGrabber(UserCmd* cmd);

	void updateEventListeners(bool forceRemove = false) noexcept;

	float resolvePitch();
	void resolveYaw();

	void initialize(Animations::Players* e, adjust_data* record, const float& goal_feet_yaw, const float& pitch);

	struct SnapShot
	{
		Animations::Players player;
		const Model* model{ };
		Vector eyePosition{};
		Vector bulletImpact{};
		bool gotImpact{ false };
		float time{ -1 };
		int playerIndex{ -1 };
		int backtrackRecord{ -1 };
	};

	struct Tick
	{
		matrix3x4 matrix[256];
		Vector origin;
		Vector mins;
		Vector max;
		float time;
	};
	struct Info
	{
		Info() : misses(0), hit(false) { }
		int misses;
		bool hit;
	};
	struct Ticks
	{
		Vector position;
		float time;
	};
	extern std::array<Info, 65> player;
	extern std::deque<Ticks> bulletImpacts;
	extern std::deque<Ticks> tick;
	extern std::deque<Tick> shot[65];
	Resolver::Ticks getClosest(const float time) noexcept;
}

class adjust_data //-V730
{
public:
	Animations::Players* player;
	int i;

	AnimationLayer layers[15];
	matrixes matrixes_data;

	resolver_type type;
	resolver_side side;

	bool invalid;
	bool immune;
	bool dormant;
	bool bot;

	bool shot;

	int flags;
	int bone_count;

	float simulation_time;
	float duck_amount;
	float lby;

	Vector angles;
	Vector abs_angles;
	Vector velocity;
	Vector origin;
	Vector mins;
	Vector maxs;

	adjust_data() //-V730
	{
		reset();
	}

	void reset()
	{
		player = nullptr;
		i = -1;

		type = ORIGINAL;
		side = RESOLVER_ORIGINAL;

		invalid = false;
		immune = false;
		dormant = false;
		bot = false;

		flags = 0;
		bone_count = 0;

		simulation_time = 0.0f;
		duck_amount = 0.0f;
		lby = 0.0f;

		angles.Zero();
		abs_angles.Zero();
		velocity.Zero();
		origin.Zero();
		mins.Zero();
		maxs.Zero();
	}

	adjust_data(Animations::Players* e, bool store = true)
	{
		type = ORIGINAL;
		side = RESOLVER_ORIGINAL;

		invalid = false;
		//store_data(e, store);
	}
};

class lagcompensation
{
public:
	int player_resolver[65];

	bool is_dormant[65];
	float previous_goal_feet_yaw[65];
};

struct SavedResolverData
{
	bool WasDormantBetweenMoving = true;
	float LastMovingLby = 0.f;
	float LastCycle = 0.f;
	bool Flip = false;
	float LastSimtime = 0.f;
	float LastResolvedYaw = 0.f;
	bool LastResolved = true;
	bool WasLastMoving = false;
	float LastStandingLby = 0.f;
	float MoveStandDelta = 0.f;
	bool CanUseMoveStandDelta = false;
	bool WasFakeWalking = false;
	//CanUseLbyPrediction
	float LastLby = 0.f;
	float LegitTestLastSimtime = 0.f;
	float lastLbyUpdateTime = 0.f;
	bool UsingAA = false;
	float LastWasUsingAA = 0.f;
	Vector LastPos = Vector(0, 0, 0);
	float FakeYaw = 0.f;
	float LastVel = 0.f;
};
