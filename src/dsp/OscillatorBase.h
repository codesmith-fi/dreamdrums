#pragma once

#include <cmath>

class OscillatorBase
{
public:
	virtual ~OscillatorBase() = default;

	void setSampleRate (double sampleRate) noexcept
	{
		if (sampleRate > 0.0)
		{
			sampleRate_ = sampleRate;
			updatePhaseIncrement();
		}
	}

	void setFrequency (float frequency) noexcept
	{
		frequency_ = frequency;
		updatePhaseIncrement();
	}

	virtual void noteOn (int noteNumber, float velocity) noexcept
	{
		currentNote_ = noteNumber;
		velocity_ = velocity;
		isPlaying_ = true;
		phase_ = 0.0f;
		setFrequency (midiToFrequency (noteNumber));
	}

	virtual void noteOff (int noteNumber) noexcept
	{
		if (noteNumber == currentNote_)
		{
			isPlaying_ = false;
			currentNote_ = -1;
		}
	}

	void resetNotes() noexcept
	{
		isPlaying_ = false;
		currentNote_ = -1;
	}

	virtual float getNextSample() = 0;

	static float midiToFrequency (int noteNumber) noexcept
	{
		return static_cast<float> (440.0 * std::pow (2.0, (static_cast<double> (noteNumber) - 69.0) / 12.0));
	}

	bool isPlaying() const noexcept { return isPlaying_; }
	int getCurrentNote() const noexcept { return currentNote_; }
	float getVelocity() const noexcept { return velocity_; }
	float getFrequency() const noexcept { return frequency_; }

protected:
	void advancePhase() noexcept
	{
		phase_ += phaseIncrement_;
		if (phase_ >= 1.0f)
			phase_ -= 1.0f;
	}

	void updatePhaseIncrement() noexcept
	{
		if (sampleRate_ > 0.0)
			phaseIncrement_ = static_cast<float> (static_cast<double> (frequency_) / sampleRate_);
		else
			phaseIncrement_ = 0.0f;
	}

	double sampleRate_ { 44100.0 };
	float phase_ { 0.0f };
	float phaseIncrement_ { 0.0f };
	float frequency_ { 440.0f };
	int currentNote_ { -1 };
	bool isPlaying_ { false };
	float velocity_ { 0.0f };
};
