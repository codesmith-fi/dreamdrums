#pragma once

#include <vector>
#include <juce_core/juce_core.h>
#include "DrumSample.hpp"

struct VelocityLayer
{
	int minVel = 0;
	int maxVel = 127;
	juce::String dynamicName;
	std::vector<DrumSample> variations;
	int lastVariationIndex = -1;

	const DrumSample* getNextSample()
	{
		if (variations.empty())
			return nullptr;

		const auto count = static_cast<int> (variations.size());
		if (count == 1)
		{
			lastVariationIndex = 0;
			return &variations[0];
		}

		// Round-robin cycling: move to next, avoid repeating the same variation
		int nextIndex = (lastVariationIndex + 1) % count;
		if (nextIndex == lastVariationIndex)
			nextIndex = (nextIndex + 1) % count;

		lastVariationIndex = nextIndex;
		return &variations[static_cast<size_t> (nextIndex)];
	}
};

class DrumPad
{
public:
	static constexpr int kHiHatChokeGroup = 1;

	DrumPad (int midiNoteNumber = 0, const juce::String& name = {})
		: midiNote (midiNoteNumber), padName (name)
	{
	}

	int midiNote = 0;
	juce::String padName;
	int chokeGroup = 0; // 0 = no choke group, >0 = group identifier
	std::vector<VelocityLayer> layers;

	const DrumSample* findSampleForVelocity (int velocity)
	{
		if (layers.empty())
			return nullptr;

		// 1. Exact range match
		for (auto& layer : layers)
		{
			if (velocity >= layer.minVel && velocity <= layer.maxVel)
			{
				if (! layer.variations.empty())
					return layer.getNextSample();
			}
		}

		// 2. Fallback: find nearest non-empty layer
		VelocityLayer* bestLayer = nullptr;
		int minDistance = 999;

		for (auto& layer : layers)
		{
			if (layer.variations.empty())
				continue;

			int dist = 0;
			if (velocity < layer.minVel)
				dist = layer.minVel - velocity;
			else if (velocity > layer.maxVel)
				dist = velocity - layer.maxVel;

			if (dist < minDistance)
			{
				minDistance = dist;
				bestLayer = &layer;
			}
		}

		if (bestLayer != nullptr)
			return bestLayer->getNextSample();

		return nullptr;
	}
};
