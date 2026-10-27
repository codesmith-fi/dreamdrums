#include "DrumPadComponent.hpp"
#include <algorithm>

DrumPadComponent::DrumPadComponent()
{
	setOpaque (false);
}

void DrumPadComponent::setPadInfo (int note, const juce::String& name, const juce::String& noteLabel)
{
	midiNote_ = note;
	padName_ = name;
	noteName_ = noteLabel;
	repaint();
}

void DrumPadComponent::trigger (float velocity)
{
	triggerIntensity_ = std::clamp (velocity, 0.4f, 1.0f);
	isTriggered_ = true;
	repaint();
}

void DrumPadComponent::updateAnimation (float decayFactor)
{
	if (triggerIntensity_ > 0.001f)
	{
		triggerIntensity_ *= decayFactor;
		if (triggerIntensity_ < 0.01f)
		{
			triggerIntensity_ = 0.0f;
			isTriggered_ = false;
		}
		repaint();
	}
}

void DrumPadComponent::setSelected (bool selected)
{
	if (isSelected_ != selected)
	{
		isSelected_ = selected;
		repaint();
	}
}

void DrumPadComponent::mouseDown (const juce::MouseEvent& event)
{
	// Velocity sensitive clicking: clicking towards top yields higher velocity
	const float normalizedY = 1.0f - std::clamp (static_cast<float> (event.y) / static_cast<float> (getHeight()), 0.0f, 1.0f);
	const float velocity = 0.35f + 0.65f * normalizedY;

	trigger (velocity);

	if (onSelect != nullptr)
		onSelect (midiNote_);

	if (onTrigger != nullptr)
		onTrigger (midiNote_, velocity);
}

void DrumPadComponent::paint (juce::Graphics& g)
{
	auto bounds = getLocalBounds().toFloat().reduced (2.0f);
	const float cornerSize = 10.0f;

	// Palette
	const auto baseBg = juce::Colour (0xff212429);
	const auto baseBorder = juce::Colour (0xff323842);
	const auto glowColor = juce::Colour (0xff00d4ff); // Electric cyan

	const auto currentBg = baseBg.interpolatedWith (glowColor.withAlpha (0.45f), triggerIntensity_);
	const auto currentBorder = baseBorder.interpolatedWith (glowColor, triggerIntensity_);

	// 1. Background fill
	g.setColour (currentBg);
	g.fillRoundedRectangle (bounds, cornerSize);

	// 2. Glow effect when triggered
	if (triggerIntensity_ > 0.01f)
	{
		juce::ColourGradient glowGrad (glowColor.withAlpha (0.35f * triggerIntensity_),
		                              bounds.getCentreX(), bounds.getCentreY(),
		                              currentBg,
		                              bounds.getX(), bounds.getY(),
		                              true);
		g.setGradientFill (glowGrad);
		g.fillRoundedRectangle (bounds, cornerSize);
	}

	// 3. Border outline
	const float borderWidth = isSelected_ ? 2.5f : (1.5f + (triggerIntensity_ * 1.5f));
	const auto borderCol = isSelected_ ? glowColor : currentBorder;
	g.setColour (borderCol);
	g.drawRoundedRectangle (bounds.reduced (borderWidth * 0.5f), cornerSize, borderWidth);

	// 3b. Selected pad top marker
	if (isSelected_)
	{
		auto topPill = juce::Rectangle<float> (bounds.getCentreX() - 14.0f, bounds.getY() + 3.0f, 28.0f, 3.0f);
		g.setColour (glowColor);
		g.fillRoundedRectangle (topPill, 1.5f);
	}

	// 4. Note label in top-left
	g.setColour (triggerIntensity_ > 0.2f ? juce::Colours::white.withAlpha (0.9f) : juce::Colour (0x99a0aab8));
	g.setFont (juce::FontOptions (11.0f));
	g.drawText (noteName_,
	            bounds.reduced (8.0f),
	            juce::Justification::topLeft,
	            false);

	// 5. Pad name in center
	g.setColour (triggerIntensity_ > 0.2f ? juce::Colours::white : juce::Colour (0xffeeeff2));
	g.setFont (juce::FontOptions (13.5f, juce::Font::bold));
	g.drawText (padName_,
	            bounds.reduced (4.0f),
	            juce::Justification::centred,
	            false);
}
