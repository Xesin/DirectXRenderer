#pragma once

class Time {
	
public:
	static float deltaTime;
	static float fixedDeltaTime;

private:
	long long currentTime = 0;

public:
	Time();
	void Update();
private:
	long long GetTime();
};