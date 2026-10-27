#include "SampleBank.hpp"
#include <algorithm>

SampleBank::SampleBank()
{
	formatManager_.registerBasicFormats();
}

juce::File SampleBank::findAssetsDirectory()
{
#ifdef DREAMDRUMS_DEFAULT_ASSETS_DIR
	juce::File defaultDir (DREAMDRUMS_DEFAULT_ASSETS_DIR);
	if (defaultDir.isDirectory())
		return defaultDir;
#endif

	auto cwd = juce::File::getCurrentWorkingDirectory();
	if (cwd.getChildFile ("assets/salamanderDrums").isDirectory())
		return cwd.getChildFile ("assets/salamanderDrums");

	auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
	auto exeDir = exe.getParentDirectory();
	if (exeDir.getChildFile ("assets/salamanderDrums").isDirectory())
		return exeDir.getChildFile ("assets/salamanderDrums");
	if (exeDir.getParentDirectory().getChildFile ("assets/salamanderDrums").isDirectory())
		return exeDir.getParentDirectory().getChildFile ("assets/salamanderDrums");
	if (exeDir.getParentDirectory().getParentDirectory().getChildFile ("assets/salamanderDrums").isDirectory())
		return exeDir.getParentDirectory().getParentDirectory().getChildFile ("assets/salamanderDrums");

	return {};
}

static std::pair<int, int> getVelocityRangeForDynamic (const juce::String& dyn)
{
	auto d = dyn.toUpperCase();
	if (d == "GHOST") return { 1, 30 };
	if (d == "PP")    return { 31, 50 };
	if (d == "P")     return { 31, 70 };
	if (d == "MP")    return { 45, 75 };
	if (d == "MF")    return { 71, 90 };
	if (d == "F")     return { 76, 105 };
	if (d == "FF")    return { 106, 127 };
	return { 1, 127 };
}

bool SampleBank::loadInstrument (int midiNote,
                                 const juce::File& sampleDir,
                                 const juce::String& prefix,
                                 const juce::String& padName)
{
	if (! sampleDir.isDirectory())
		return false;

	juce::Array<juce::File> files;
	sampleDir.findChildFiles (files, juce::File::findFiles, false, "*.wav");

	const juce::String expectedPrefix = prefix + "_";
	std::unordered_map<juce::String, std::vector<DrumSample>> layersByDynamic;

	for (const auto& file : files)
	{
		const auto filename = file.getFileNameWithoutExtension();
		if (! filename.startsWith (expectedPrefix))
			continue;

		const auto remainder = filename.substring (expectedPrefix.length());
		const int lastUnderscore = remainder.lastIndexOfChar ('_');
		if (lastUnderscore <= 0)
			continue;

		const auto dynamicStr = remainder.substring (0, lastUnderscore);

		std::unique_ptr<juce::AudioFormatReader> reader (formatManager_.createReaderFor (file));
		if (reader == nullptr)
			continue;

		DrumSample sample;
		sample.name = filename;
		sample.sampleRate = reader->sampleRate;
		sample.buffer.setSize (static_cast<int> (reader->numChannels),
		                       static_cast<int> (reader->lengthInSamples));

		reader->read (&sample.buffer, 0, static_cast<int> (reader->lengthInSamples), 0, true, true);

		layersByDynamic[dynamicStr].push_back (std::move (sample));
	}

	if (layersByDynamic.empty())
		return false;

	auto pad = std::make_unique<DrumPad> (midiNote, padName.isNotEmpty() ? padName : prefix);

	for (auto& [dynName, samples] : layersByDynamic)
	{
		VelocityLayer layer;
		layer.dynamicName = dynName;
		auto range = getVelocityRangeForDynamic (dynName);
		layer.minVel = range.first;
		layer.maxVel = range.second;
		layer.variations = std::move (samples);
		pad->layers.push_back (std::move (layer));
	}

	// Sort layers by minVel
	std::sort (pad->layers.begin(), pad->layers.end(),
	           [] (const VelocityLayer& a, const VelocityLayer& b) {
		           return a.minVel < b.minVel;
	           });

	// Normalize layer ranges to contiguous [1, 127] without gaps
	if (! pad->layers.empty())
	{
		pad->layers.front().minVel = 1;
		for (size_t i = 1; i < pad->layers.size(); ++i)
		{
			if (pad->layers[i].minVel <= pad->layers[i - 1].maxVel)
			{
				pad->layers[i].minVel = pad->layers[i - 1].maxVel + 1;
			}
			else if (pad->layers[i].minVel > pad->layers[i - 1].maxVel + 1)
			{
				pad->layers[i - 1].maxVel = pad->layers[i].minVel - 1;
			}
		}
		pad->layers.back().maxVel = 127;
	}

	pads_[midiNote] = std::move (pad);
	return true;
}

DrumPad* SampleBank::getPad (int midiNote)
{
	auto it = pads_.find (midiNote);
	if (it != pads_.end())
		return it->second.get();
	return nullptr;
}

const DrumPad* SampleBank::getPad (int midiNote) const
{
	auto it = pads_.find (midiNote);
	if (it != pads_.end())
		return it->second.get();
	return nullptr;
}

void SampleBank::clear()
{
	pads_.clear();
}
