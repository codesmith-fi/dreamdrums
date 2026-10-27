#pragma once

#include <array>
#include <atomic>
#include <algorithm>
#include <functional>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SampleBank.hpp"
#include "DrumVoicePool.hpp"

struct Step
{
	std::atomic<bool> active { false };
	std::atomic<int> midiNote { 38 };
	std::atomic<float> velocity { 0.85f };

	Step() = default;

	Step (bool act, int note, float vel)
		: active (act), midiNote (note), velocity (vel)
	{
	}

	Step (const Step& other)
		: active (other.active.load (std::memory_order_relaxed)),
		  midiNote (other.midiNote.load (std::memory_order_relaxed)),
		  velocity (other.velocity.load (std::memory_order_relaxed))
	{
	}

	Step& operator= (const Step& other)
	{
		if (this != &other)
		{
			active.store (other.active.load (std::memory_order_relaxed), std::memory_order_relaxed);
			midiNote.store (other.midiNote.load (std::memory_order_relaxed), std::memory_order_relaxed);
			velocity.store (other.velocity.load (std::memory_order_relaxed), std::memory_order_relaxed);
		}
		return *this;
	}

	int getMidiVelocity() const noexcept
	{
		const float vel = velocity.load (std::memory_order_relaxed);
		return std::clamp (static_cast<int> (vel * 127.0f), 1, 127);
	}
};

class StepSequencer
{
public:
	static constexpr int kNumTracks = 16;
	static constexpr int kNumSteps = 32;

	StepSequencer()
	{
		// Default Launchkey 16 notes (36 to 51)
		for (int t = 0; t < kNumTracks; ++t)
		{
			const int defaultNote = 36 + t;
			trackDefaultNotes_[static_cast<size_t> (t)] = defaultNote;
			for (int s = 0; s < kNumSteps; ++s)
			{
				tracks[static_cast<size_t> (t)][static_cast<size_t> (s)].midiNote.store (defaultNote, std::memory_order_relaxed);
				tracks[static_cast<size_t> (t)][static_cast<size_t> (s)].velocity.store (0.85f, std::memory_order_relaxed);
				tracks[static_cast<size_t> (t)][static_cast<size_t> (s)].active.store (false, std::memory_order_relaxed);
			}
		}
	}

	void setTrackDefaultNote (int trackIndex, int note)
	{
		if (trackIndex >= 0 && trackIndex < kNumTracks)
		{
			trackDefaultNotes_[static_cast<size_t> (trackIndex)] = note;
			for (int s = 0; s < kNumSteps; ++s)
			{
				if (! tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (s)].active.load (std::memory_order_relaxed))
				{
					tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (s)].midiNote.store (note, std::memory_order_relaxed);
				}
			}
		}
	}

	int getTrackDefaultNote (int trackIndex) const noexcept
	{
		if (trackIndex >= 0 && trackIndex < kNumTracks)
			return trackDefaultNotes_[static_cast<size_t> (trackIndex)];
		return 36;
	}

	void setStep (int trackIndex, int stepIndex, bool active, int midiNote, float velocity)
	{
		if (trackIndex >= 0 && trackIndex < kNumTracks && stepIndex >= 0 && stepIndex < kNumSteps)
		{
			auto& step = tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (stepIndex)];
			step.active.store (active, std::memory_order_relaxed);
			step.midiNote.store (midiNote, std::memory_order_relaxed);
			step.velocity.store (std::clamp (velocity, 0.01f, 1.0f), std::memory_order_relaxed);
		}
	}

	void setStepActive (int trackIndex, int stepIndex, bool active)
	{
		if (trackIndex >= 0 && trackIndex < kNumTracks && stepIndex >= 0 && stepIndex < kNumSteps)
			tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (stepIndex)].active.store (active, std::memory_order_relaxed);
	}

	void setStepVelocity (int trackIndex, int stepIndex, float velocity)
	{
		if (trackIndex >= 0 && trackIndex < kNumTracks && stepIndex >= 0 && stepIndex < kNumSteps)
			tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (stepIndex)].velocity.store (
				std::clamp (velocity, 0.01f, 1.0f), std::memory_order_relaxed);
	}

	void setStepNote (int trackIndex, int stepIndex, int note)
	{
		if (trackIndex >= 0 && trackIndex < kNumTracks && stepIndex >= 0 && stepIndex < kNumSteps)
			tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (stepIndex)].midiNote.store (note, std::memory_order_relaxed);
	}

	const Step& getStep (int trackIndex, int stepIndex) const noexcept
	{
		return tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (stepIndex)];
	}

	void clearTrack (int trackIndex)
	{
		if (trackIndex >= 0 && trackIndex < kNumTracks)
		{
			for (int s = 0; s < kNumSteps; ++s)
				tracks[static_cast<size_t> (trackIndex)][static_cast<size_t> (s)].active.store (false, std::memory_order_relaxed);
		}
	}

	void clearAll()
	{
		for (int t = 0; t < kNumTracks; ++t)
			clearTrack (t);
	}

	void setBpm (float newBpm) noexcept
	{
		bpm_.store (std::clamp (newBpm, 20.0f, 300.0f), std::memory_order_relaxed);
	}

	float getBpm() const noexcept
	{
		return bpm_.load (std::memory_order_relaxed);
	}

	void setLoopRange (int startStep, int endStep)
	{
		int s = std::clamp (startStep, 0, kNumSteps - 1);
		int e = std::clamp (endStep, 0, kNumSteps - 1);
		if (s > e)
			std::swap (s, e);

		loopStart_.store (s, std::memory_order_relaxed);
		loopEnd_.store (e, std::memory_order_relaxed);
	}

	int getLoopStart() const noexcept
	{
		return loopStart_.load (std::memory_order_relaxed);
	}

	int getLoopEnd() const noexcept
	{
		return loopEnd_.load (std::memory_order_relaxed);
	}

	void setSyncToHost (bool sync) noexcept
	{
		syncToHost_.store (sync, std::memory_order_relaxed);
	}

	bool isSyncToHost() const noexcept
	{
		return syncToHost_.load (std::memory_order_relaxed);
	}

	float getEffectiveBpm() const noexcept
	{
		return currentEffectiveBpm_.load (std::memory_order_relaxed);
	}

	void setPlaying (bool play) noexcept
	{
		if (! play)
		{
			currentStep_.store (-1, std::memory_order_relaxed);
			sampleAccumulator_ = 0.0;
		}
		isPlaying_.store (play, std::memory_order_relaxed);
	}

	bool isPlaying() const noexcept
	{
		return isPlaying_.load (std::memory_order_relaxed);
	}

	int getCurrentStep() const noexcept
	{
		return currentStep_.load (std::memory_order_relaxed);
	}

	void reset()
	{
		currentStep_.store (-1, std::memory_order_relaxed);
		sampleAccumulator_ = 0.0;
	}

	// 16 parallel tracks (each corresponding to a drum pad)
	std::array<std::array<Step, kNumSteps>, kNumTracks> tracks;

	// Audio processing method
	void processBlock (SampleBank& sampleBank,
	                   DrumVoicePool& voicePool,
	                   double sampleRate,
	                   int numSamples,
	                   juce::AudioPlayHead* playHead,
	                   const std::function<void (int note, float velocity)>& onTriggerCallback)
	{
		bool hostPlaying = false;
		double effectiveBpm = bpm_.load (std::memory_order_relaxed);
		const bool syncToHost = syncToHost_.load (std::memory_order_relaxed);

		int start = loopStart_.load (std::memory_order_relaxed);
		int end = loopEnd_.load (std::memory_order_relaxed);
		if (start > end)
			std::swap (start, end);
		start = std::clamp (start, 0, kNumSteps - 1);
		end = std::clamp (end, 0, kNumSteps - 1);
		const int loopLen = end - start + 1;

		// Check DAW host playhead if available
		if (playHead != nullptr)
		{
			if (auto optPos = playHead->getPosition())
			{
				hostPlaying = optPos->getIsPlaying();

				if (syncToHost)
				{
					if (auto optBpm = optPos->getBpm())
					{
						if (*optBpm > 20.0 && *optBpm < 320.0)
							effectiveBpm = *optBpm;
					}

					if (hostPlaying)
					{
						// Synchronize with host PPQ position (16th notes)
						if (auto optPpq = optPos->getPpqPosition())
						{
							const double ppq = *optPpq;
							// In 4/4 time, 1 beat (quarter note) = 4 sixteenth notes
							const double sixteenths = ppq * 4.0;
							const int hostStep = static_cast<int> (std::floor (sixteenths));
							const int normalizedHostStep = start + ((hostStep % loopLen + loopLen) % loopLen);

							const int prevStep = currentStep_.load (std::memory_order_relaxed);
							if (normalizedHostStep != prevStep)
							{
								currentStep_.store (normalizedHostStep, std::memory_order_relaxed);
								triggerStep (normalizedHostStep, sampleBank, voicePool, sampleRate, onTriggerCallback);
							}
							currentEffectiveBpm_.store (static_cast<float> (effectiveBpm), std::memory_order_relaxed);
							return;
						}
					}
				}
			}
		}

		currentEffectiveBpm_.store (static_cast<float> (effectiveBpm), std::memory_order_relaxed);

		// Internal transport playback (standalone or when host is stopped/not syncing)
		if (! isPlaying_.load (std::memory_order_relaxed) && ! hostPlaying)
		{
			if (currentStep_.load (std::memory_order_relaxed) != -1)
				currentStep_.store (-1, std::memory_order_relaxed);
			return;
		}

		const double samplesPerStep = (sampleRate * 60.0) / (effectiveBpm * 4.0);
		if (samplesPerStep <= 1.0)
			return;

		sampleAccumulator_ += static_cast<double> (numSamples);

		while (sampleAccumulator_ >= samplesPerStep)
		{
			sampleAccumulator_ -= samplesPerStep;
			int nextStep = currentStep_.load (std::memory_order_relaxed) + 1;
			if (nextStep > end || nextStep < start)
				nextStep = start;

			currentStep_.store (nextStep, std::memory_order_relaxed);
			triggerStep (nextStep, sampleBank, voicePool, sampleRate, onTriggerCallback);
		}
	}

	void triggerStep (int stepIndex,
	                  SampleBank& sampleBank,
	                  DrumVoicePool& voicePool,
	                  double sampleRate,
	                  const std::function<void (int, float)>& onTriggerCallback)
	{
		if (stepIndex < 0 || stepIndex >= kNumSteps)
			return;

		for (size_t t = 0; t < kNumTracks; ++t)
		{
			const auto& step = tracks[t][static_cast<size_t> (stepIndex)];
			if (step.active.load (std::memory_order_relaxed))
			{
				const int note = step.midiNote.load (std::memory_order_relaxed);
				const float vel = step.velocity.load (std::memory_order_relaxed);
				const int midiVel = step.getMidiVelocity();

				if (auto* pad = sampleBank.getPad (note))
				{
					if (const auto* sample = pad->findSampleForVelocity (midiVel))
					{
						voicePool.triggerSample (sample, vel, sampleRate, pad->chokeGroup);
					}
				}

				if (onTriggerCallback != nullptr)
					onTriggerCallback (note, vel);
			}
		}
	}

	// State serialization for DAW project saving / preset loading
	juce::ValueTree serializeToValueTree() const
	{
		juce::ValueTree tree ("StepSequencer");
		tree.setProperty ("bpm", bpm_.load (std::memory_order_relaxed), nullptr);
		tree.setProperty ("syncToHost", syncToHost_.load (std::memory_order_relaxed), nullptr);
		tree.setProperty ("loopStart", loopStart_.load (std::memory_order_relaxed), nullptr);
		tree.setProperty ("loopEnd", loopEnd_.load (std::memory_order_relaxed), nullptr);

		for (int t = 0; t < kNumTracks; ++t)
		{
			juce::ValueTree trackTree ("Track");
			trackTree.setProperty ("index", t, nullptr);
			trackTree.setProperty ("defaultNote", trackDefaultNotes_[static_cast<size_t> (t)], nullptr);

			for (int s = 0; s < kNumSteps; ++s)
			{
				const auto& step = tracks[static_cast<size_t> (t)][static_cast<size_t> (s)];
				if (step.active.load (std::memory_order_relaxed) ||
				    step.midiNote.load (std::memory_order_relaxed) != trackDefaultNotes_[static_cast<size_t> (t)] ||
				    std::abs (step.velocity.load (std::memory_order_relaxed) - 0.85f) > 0.01f)
				{
					juce::ValueTree stepTree ("Step");
					stepTree.setProperty ("step", s, nullptr);
					stepTree.setProperty ("active", step.active.load (std::memory_order_relaxed), nullptr);
					stepTree.setProperty ("note", step.midiNote.load (std::memory_order_relaxed), nullptr);
					stepTree.setProperty ("velocity", step.velocity.load (std::memory_order_relaxed), nullptr);
					trackTree.addChild (stepTree, -1, nullptr);
				}
			}
			tree.addChild (trackTree, -1, nullptr);
		}
		return tree;
	}

	void deserializeFromValueTree (const juce::ValueTree& tree)
	{
		if (! tree.isValid() || tree.getType() != juce::Identifier ("StepSequencer"))
			return;

		if (tree.hasProperty ("bpm"))
			setBpm (static_cast<float> (tree.getProperty ("bpm")));

		if (tree.hasProperty ("syncToHost"))
			setSyncToHost (static_cast<bool> (tree.getProperty ("syncToHost")));

		if (tree.hasProperty ("loopStart") && tree.hasProperty ("loopEnd"))
			setLoopRange (static_cast<int> (tree.getProperty ("loopStart")),
			              static_cast<int> (tree.getProperty ("loopEnd")));

		for (int i = 0; i < tree.getNumChildren(); ++i)
		{
			auto trackTree = tree.getChild (i);
			if (trackTree.getType() == juce::Identifier ("Track"))
			{
				const int t = static_cast<int> (trackTree.getProperty ("index", -1));
				if (t >= 0 && t < kNumTracks)
				{
					if (trackTree.hasProperty ("defaultNote"))
						trackDefaultNotes_[static_cast<size_t> (t)] = static_cast<int> (trackTree.getProperty ("defaultNote"));

					// Reset track steps first
					for (int s = 0; s < kNumSteps; ++s)
					{
						tracks[static_cast<size_t> (t)][static_cast<size_t> (s)].active.store (false, std::memory_order_relaxed);
						tracks[static_cast<size_t> (t)][static_cast<size_t> (s)].midiNote.store (trackDefaultNotes_[static_cast<size_t> (t)], std::memory_order_relaxed);
						tracks[static_cast<size_t> (t)][static_cast<size_t> (s)].velocity.store (0.85f, std::memory_order_relaxed);
					}

					for (int j = 0; j < trackTree.getNumChildren(); ++j)
					{
						auto stepTree = trackTree.getChild (j);
						if (stepTree.getType() == juce::Identifier ("Step"))
						{
							const int s = static_cast<int> (stepTree.getProperty ("step", -1));
							if (s >= 0 && s < kNumSteps)
							{
								const bool act = static_cast<bool> (stepTree.getProperty ("active", false));
								const int note = static_cast<int> (stepTree.getProperty ("note", trackDefaultNotes_[static_cast<size_t> (t)]));
								const float vel = static_cast<float> (stepTree.getProperty ("velocity", 0.85f));
								setStep (t, s, act, note, vel);
							}
						}
					}
				}
			}
		}
	}

private:
	std::array<int, kNumTracks> trackDefaultNotes_ {};
	std::atomic<bool> isPlaying_ { false };
	std::atomic<int> currentStep_ { -1 };
	std::atomic<float> bpm_ { 120.0f };
	std::atomic<bool> syncToHost_ { true }; // true = SYNC to DAW, false = FREE
	std::atomic<float> currentEffectiveBpm_ { 120.0f };
	std::atomic<int> loopStart_ { 0 };   // 0..31 (default 0 / Step 1)
	std::atomic<int> loopEnd_   { 15 };  // 0..31 (default 15 / Step 16)
	double sampleAccumulator_ { 0.0 };
};
