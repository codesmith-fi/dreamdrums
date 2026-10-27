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

	void triggerSample (const DrumSample* sample, float velocity, double targetSampleRate, int chokeGroup = 0)
	{
		if (sample == nullptr)
			return;

		// 1. Choke active voices in the same choke group
		if (chokeGroup > 0)
		{
			for (auto& voice : voices_)
			{
				if (voice.isActive() && voice.getChokeGroup() == chokeGroup)
				{
					voice.quickFadeOut (targetSampleRate, 5.0);
				}
			}
		}

		const uint64_t newAge = ++voiceCounter_;

		// 2. Find inactive voice
		for (auto& voice : voices_)
		{
			if (! voice.isActive())
			{
				voice.startNote (sample, velocity, targetSampleRate, newAge, chokeGroup);
				return;
			}
		}

		// 3. Voice stealing: steal oldest voice
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

		oldestVoice->startNote (sample, velocity, targetSampleRate, newAge, chokeGroup);
	}

	void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
	{
		for (auto& voice : voices_)
		{
			if (voice.isActive())
				voice.renderNextBlock (buffer, startSample, numSamples);
		}
	}

	const std::array<DrumVoice, kMaxVoices>& getVoices() const noexcept { return voices_; }
	std::array<DrumVoice, kMaxVoices>& getVoices() noexcept { return voices_; }

private:
	std::array<DrumVoice, kMaxVoices> voices_;
	uint64_t voiceCounter_ = 0;
};
