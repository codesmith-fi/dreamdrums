#pragma once

#include <algorithm>
#include <cmath>
#include "DrumSample.hpp"

class DrumVoice
{
public:
	DrumVoice() = default;

	void startNote (const DrumSample* sample, float velocity, double targetSampleRate, uint64_t age, int chokeGroup = 0)
	{
		currentSample = sample;
		voiceAge = age;
		currentPosition = 0.0;
		currentChokeGroup = chokeGroup;
		isFadingOut = false;
		fadeMultiplier = 1.0f;
		fadeStep = 0.0f;

		if (sample == nullptr || sample->buffer.getNumSamples() == 0 || targetSampleRate <= 0.0)
		{
			active = false;
			return;
		}

		playbackRatio = sample->sampleRate / targetSampleRate;

		const float clampedVel = std::clamp (velocity, 0.01f, 1.0f);
		gain = 0.15f + 0.85f * (clampedVel * clampedVel);

		active = true;
	}

	void stopNote()
	{
		active = false;
		isFadingOut = false;
		currentSample = nullptr;
		currentPosition = 0.0;
		currentChokeGroup = 0;
	}

	bool isActive() const noexcept { return active; }
	uint64_t getAge() const noexcept { return voiceAge; }
	int getChokeGroup() const noexcept { return currentChokeGroup; }
	void setChokeGroup (int group) noexcept { currentChokeGroup = group; }
	bool isFadingOutVoice() const noexcept { return isFadingOut; }

	void quickFadeOut (double targetSampleRate, double fadeTimeMs = 5.0)
	{
		if (active && ! isFadingOut)
		{
			isFadingOut = true;
			const int fadeSamples = std::max (1, static_cast<int> (targetSampleRate * (fadeTimeMs / 1000.0)));
			fadeStep = 1.0f / static_cast<float> (fadeSamples);
			fadeMultiplier = 1.0f;
		}
	}

	void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
	{
		if (! active || currentSample == nullptr)
			return;

		const auto& srcBuffer = currentSample->buffer;
		const int srcLength = srcBuffer.getNumSamples();
		const int srcChannels = srcBuffer.getNumChannels();
		const int dstChannels = outputBuffer.getNumChannels();

		if (srcLength <= 0 || srcChannels <= 0 || dstChannels <= 0)
		{
			active = false;
			return;
		}

		auto* leftOut = outputBuffer.getWritePointer (0);
		auto* rightOut = dstChannels > 1 ? outputBuffer.getWritePointer (1) : nullptr;

		const auto* srcLeft = srcBuffer.getReadPointer (0);
		const auto* srcRight = srcChannels > 1 ? srcBuffer.getReadPointer (1) : srcLeft;

		// Fast path: exact sample rate match
		if (std::abs (playbackRatio - 1.0) < 1e-6)
		{
			const int srcPos = static_cast<int> (currentPosition);
			const int samplesToCopy = std::min (numSamples, srcLength - srcPos);

			if (samplesToCopy > 0)
			{
				int i = 0;
				for (; i < samplesToCopy; ++i)
				{
					const int outIdx = startSample + i;
					const int inIdx = srcPos + i;

					float currentGain = gain;
					if (isFadingOut)
					{
						currentGain *= fadeMultiplier;
						fadeMultiplier -= fadeStep;
					}

					const float sL = srcLeft[inIdx] * currentGain;
					const float sR = srcRight[inIdx] * currentGain;

					leftOut[outIdx] += sL;
					if (rightOut != nullptr)
						rightOut[outIdx] += sR;

					if (isFadingOut && fadeMultiplier <= 0.0f)
					{
						++i;
						active = false;
						isFadingOut = false;
						currentSample = nullptr;
						break;
					}
				}

				currentPosition += i;
			}

			if (currentPosition >= srcLength || (samplesToCopy < numSamples && ! isFadingOut))
			{
				active = false;
				currentSample = nullptr;
			}
		}
		else
		{
			// Linear interpolation path for sample-rate conversion
			for (int i = 0; i < numSamples; ++i)
			{
				const int idx0 = static_cast<int> (currentPosition);
				const int idx1 = idx0 + 1;

				if (idx0 >= srcLength)
				{
					active = false;
					currentSample = nullptr;
					break;
				}

				float currentGain = gain;
				if (isFadingOut)
				{
					currentGain *= fadeMultiplier;
					fadeMultiplier -= fadeStep;
				}

				const float frac = static_cast<float> (currentPosition - idx0);
				const float val0L = srcLeft[idx0];
				const float val1L = (idx1 < srcLength) ? srcLeft[idx1] : 0.0f;
				const float sampleL = (val0L + frac * (val1L - val0L)) * currentGain;

				const float val0R = srcRight[idx0];
				const float val1R = (idx1 < srcLength) ? srcRight[idx1] : 0.0f;
				const float sampleR = (val0R + frac * (val1R - val0R)) * currentGain;

				const int outIdx = startSample + i;
				leftOut[outIdx] += sampleL;
				if (rightOut != nullptr)
					rightOut[outIdx] += sampleR;

				currentPosition += playbackRatio;

				if (isFadingOut && fadeMultiplier <= 0.0f)
				{
					active = false;
					isFadingOut = false;
					currentSample = nullptr;
					break;
				}
			}
		}
	}

private:
	const DrumSample* currentSample = nullptr;
	double currentPosition = 0.0;
	float gain = 1.0f;
	bool active = false;
	uint64_t voiceAge = 0;
	double playbackRatio = 1.0;

	int currentChokeGroup = 0;
	bool isFadingOut = false;
	float fadeMultiplier = 1.0f;
	float fadeStep = 0.0f;
};
