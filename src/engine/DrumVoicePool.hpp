#pragma once

#include <array>
#include "DrumVoice.hpp"

class DrumVoicePool
{
public:
	static constexpr int kMaxVoices = 32;

	DrumVoicePool() = default;

	void reset()
	{
		for (auto& voice : voices_)
			voice.stopNote();
		voiceCounter_ = 0;
	}

	void triggerSample (const DrumSample* sample, float velocity, double targetSampleRate)
	{
		if (sample == nullptr)
			return;

		const uint64_t newAge = ++voiceCounter_;

		// 1. Find inactive voice
		for (auto& voice : voices_)
		{
			if (! voice.isActive())
			{
				voice.startNote (sample, velocity, targetSampleRate, newAge);
				return;
			}
		}

		// 2. Voice stealing: steal oldest voice
		DrumVoice* oldestVoice = &voices_[0];
		uint64_t oldestAge = oldestVoice->getAge();

		for (size_t i = 1; i < voices_.size(); ++i)
		{
			if (voices_[i].getAge() < oldestAge)
			{
				oldestAge = voices_[i].getAge();
				oldestVoice = &voices_[i];
			}
		}

		oldestVoice->startNote (sample, velocity, targetSampleRate, newAge);
	}

	void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
	{
		for (auto& voice : voices_)
		{
			if (voice.isActive())
				voice.renderNextBlock (buffer, startSample, numSamples);
		}
	}

private:
	std::array<DrumVoice, kMaxVoices> voices_;
	uint64_t voiceCounter_ = 0;
};
