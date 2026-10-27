#pragma once

#include "OscillatorBase.h"
#include <cmath>
#include <numbers>

class SineOscillator : public OscillatorBase
{
public:
	float getNextSample() override
	{
		if (!isPlaying_)
			return 0.0f;

		float sample = std::sin (phase_ * 2.0f * std::numbers::pi_v<float>) * velocity_;
		advancePhase();
		return sample;
	}
};
