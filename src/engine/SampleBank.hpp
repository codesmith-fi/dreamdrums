#pragma once

#include <memory>
#include <unordered_map>
#include <juce_audio_formats/juce_audio_formats.h>
#include "DrumPad.hpp"

class SampleBank
{
public:
	SampleBank();
	~SampleBank() = default;

	static juce::File findAssetsDirectory();

	bool loadInstrument (int midiNote,
	                     const juce::File& sampleDir,
	                     const juce::String& prefix,
	                     const juce::String& padName = {},
	                     float pitchOffsetSemitones = 0.0f);

	DrumPad* getPad (int midiNote);
	const DrumPad* getPad (int midiNote) const;

	bool isEmpty() const noexcept { return pads_.empty(); }
	size_t getNumPads() const noexcept { return pads_.size(); }
	void clear();

private:
	juce::AudioFormatManager formatManager_;
	std::unordered_map<int, std::unique_ptr<DrumPad>> pads_;
};
