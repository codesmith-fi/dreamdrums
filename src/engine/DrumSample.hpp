#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

struct DrumSample
{
	juce::AudioBuffer<float> buffer;
	double sampleRate = 48000.0;
	juce::String name;
};
