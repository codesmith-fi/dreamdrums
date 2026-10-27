#pragma once

#include <algorithm>
#include <cmath>
#include "DrumSample.hpp"

class DrumVoice
{
public:
	DrumVoice() = default;

	void startNote (const DrumSample* sample, float velocity, double targetSampleRate, uint64_t age)
	{
		currentSample = sample;
		voiceAge = age;
		currentPosition = 0.0;

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
		currentSample = nullptr;
		currentPosition = 0.0;
	}

	bool isActive() const noexcept { return active; }
	uint64_t getAge() const noexcept { return voiceAge; }

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
				for (int i = 0; i < samplesToCopy; ++i)
				{
					const int outIdx = startSample + i;
					const int inIdx = srcPos + i;

					const float sL = srcLeft[inIdx] * gain;
					const float sR = srcRight[inIdx] * gain;

					leftOut[outIdx] += sL;
					if (rightOut != nullptr)
						rightOut[outIdx] += sR;
				}

				currentPosition += samplesToCopy;
			}

			if (currentPosition >= srcLength || samplesToCopy < numSamples)
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

				const float frac = static_cast<float> (currentPosition - idx0);
				const float val0L = srcLeft[idx0];
				const float val1L = (idx1 < srcLength) ? srcLeft[idx1] : 0.0f;
				const float sampleL = (val0L + frac * (val1L - val0L)) * gain;

				const float val0R = srcRight[idx0];
				const float val1R = (idx1 < srcLength) ? srcRight[idx1] : 0.0f;
				const float sampleR = (val0R + frac * (val1R - val0R)) * gain;

				const int outIdx = startSample + i;
				leftOut[outIdx] += sampleL;
				if (rightOut != nullptr)
					rightOut[outIdx] += sampleR;

				currentPosition += playbackRatio;
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
};
