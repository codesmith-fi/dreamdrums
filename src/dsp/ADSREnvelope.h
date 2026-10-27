#pragma once

#include <algorithm>

class ADSREnvelope
{
public:
	enum class State
	{
		Idle,
		Attack,
		Decay,
		Sustain,
		Release
	};

	ADSREnvelope() noexcept
	{
		updateRates();
	}

	void setSampleRate (double sampleRate) noexcept
	{
		if (sampleRate > 0.0)
		{
			sampleRate_ = sampleRate;
			updateRates();
		}
	}

	void setParameters (float attack, float decay, float sustain, float release) noexcept
	{
		attackTime_ = std::max (0.0f, attack);
		decayTime_ = std::max (0.0f, decay);
		sustainLevel_ = std::clamp (sustain, 0.0f, 1.0f);
		releaseTime_ = std::max (0.0f, release);
		updateRates();
	}

	void noteOn() noexcept
	{
		state_ = State::Attack;
		if (attackTime_ > 0.0f && sampleRate_ > 0.0)
		{
			attackRate_ = (1.0f - currentLevel_) / static_cast<float> (attackTime_ * sampleRate_);
			if (attackRate_ <= 0.0f)
			{
				currentLevel_ = 1.0f;
				state_ = State::Decay;
			}
		}
		else
		{
			currentLevel_ = 1.0f;
			state_ = State::Decay;
		}
	}

	void noteOff() noexcept
	{
		if (state_ != State::Idle)
		{
			state_ = State::Release;
			if (releaseTime_ > 0.0f && sampleRate_ > 0.0)
			{
				releaseRate_ = currentLevel_ / static_cast<float> (releaseTime_ * sampleRate_);
				if (releaseRate_ <= 0.0f || currentLevel_ <= 0.0001f)
				{
					currentLevel_ = 0.0f;
					state_ = State::Idle;
				}
			}
			else
			{
				currentLevel_ = 0.0f;
				state_ = State::Idle;
			}
		}
	}

	float getNextSample() noexcept
	{
		switch (state_)
		{
		case State::Idle:
			currentLevel_ = 0.0f;
			break;

		case State::Attack:
			currentLevel_ += attackRate_;
			if (currentLevel_ >= 1.0f)
			{
				currentLevel_ = 1.0f;
				state_ = State::Decay;
			}
			break;

		case State::Decay:
			currentLevel_ -= decayRate_;
			if (currentLevel_ <= sustainLevel_)
			{
				currentLevel_ = sustainLevel_;
				state_ = State::Sustain;
			}
			break;

		case State::Sustain:
			currentLevel_ = sustainLevel_;
			break;

		case State::Release:
			currentLevel_ -= releaseRate_;
			if (currentLevel_ <= 0.0001f)
			{
				currentLevel_ = 0.0f;
				state_ = State::Idle;
			}
			break;
		}

		return std::clamp (currentLevel_, 0.0f, 1.0f);
	}

	bool isActive() const noexcept
	{
		return state_ != State::Idle;
	}

	void reset() noexcept
	{
		state_ = State::Idle;
		currentLevel_ = 0.0f;
	}

	State getState() const noexcept { return state_; }
	float getCurrentLevel() const noexcept { return currentLevel_; }

private:
	void updateRates() noexcept
	{
		if (sampleRate_ <= 0.0)
			return;

		decayRate_ = decayTime_ > 0.0f
			? (1.0f - sustainLevel_) / static_cast<float> (decayTime_ * sampleRate_)
			: 1.0f;
	}

	double sampleRate_ { 44100.0 };
	State state_ { State::Idle };

	float attackTime_ { 0.01f };
	float decayTime_ { 0.1f };
	float sustainLevel_ { 0.8f };
	float releaseTime_ { 0.15f };

	float attackRate_ { 0.0f };
	float decayRate_ { 0.0f };
	float releaseRate_ { 0.0f };

	float currentLevel_ { 0.0f };
};
