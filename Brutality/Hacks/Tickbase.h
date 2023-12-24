#pragma once

struct UserCmd;

namespace Tickbase
{
	int targetTickShift{ 0 };
	int tickShift{ 0 };
	int shiftCommand{ 0 };
	int shiftedTickbase{ 0 };
	int ticksAllowedForProcessing{ 0 };
	int chokedPackets{ 0 };
	int pauseTicks{ 0 };
	float realTime{ 0.0f };
	bool shifting{ false };
	bool finalTick{ false };
	bool hasHadTickbaseActive{ false };
	void start(UserCmd* cmd) noexcept;
	void end(UserCmd* cmd) noexcept;

	bool shift(UserCmd* cmd, int shiftAmount, bool forceShift = false) noexcept;

	bool canRun() noexcept;
	bool canShift(int shiftAmount, bool forceShift = false) noexcept;

	int getCorrectTickbase(int commandNumber) noexcept;

	int& pausedTicks() noexcept;
	
	int getTargetTickShift() noexcept;
	int getTickshift() noexcept;

	void resetTickshift() noexcept;

	bool& isFinalTick() noexcept;
	bool& isShifting() noexcept;

	void updateInput() noexcept;
	void reset() noexcept;
}