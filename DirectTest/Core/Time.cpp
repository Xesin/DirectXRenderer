#include "Time.h"

#include <chrono>

float Time::deltaTime = 0;
float Time::fixedDeltaTime = 0;

Time::Time() {
	currentTime = GetTime();
	fixedDeltaTime = 1.f / 60.f;
	deltaTime = fixedDeltaTime;
};

long long Time::GetTime() {
	using namespace std::chrono;

	__int64 time = system_clock::now().time_since_epoch() /
		milliseconds(1);
	return time;
}

void Time::Update() {
	long long newTime = GetTime();
	double frameTime = (newTime - currentTime) * 0.001;
	currentTime = newTime;
	float deltaTime = frameTime;
}