#include "ui/WindowLook.h"

#include "ui/Theme.h"

namespace cs303::ui
{
    namespace
    {
        /** Minimise or close, drawn in the labels' grey; close turns red under the mouse, as Windows' own does. */
        class TitleKey final : public juce::Button
        {
        public:
            explicit TitleKey(int kind) : juce::Button(kind == juce::DocumentWindow::closeButton ? "close" : "minimise"), type(kind) {}

            void paintButton(juce::Graphics& g, bool over, bool down) override
            {
                const auto r = getLocalBounds().toFloat().reduced(1.0f, 2.0f);
                const bool close = type == juce::DocumentWindow::closeButton;
                if (over || down)
                {
                    g.setColour(close ? juce::Colour(0xffc8372d).withMultipliedBrightness(down ? 0.85f : 1.0f)
                                      : colour::label.withAlpha(down ? 0.22f : 0.14f));
                    g.fillRoundedRectangle(r, 3.0f);
                }
                g.setColour(close && (over || down) ? juce::Colours::white : colour::label);
                const auto c = r.getCentre();
                const float s = std::min(r.getWidth(), r.getHeight()) * 0.24f;
                if (close)
                {
                    g.drawLine(c.x - s, c.y - s, c.x + s, c.y + s, 1.4f);
                    g.drawLine(c.x - s, c.y + s, c.x + s, c.y - s, 1.4f);
                }
                else
                    g.drawLine(c.x - s, c.y, c.x + s, c.y, 1.4f);
            }

        private:
            int type;
        };

        struct SharedLook final : public juce::DeletedAtShutdown
        {
            WindowLook look;
            ~SharedLook() override { clearSingletonInstance(); }
            JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE(SharedLook, false)
        };
    }

    void WindowLook::drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int w, int h, int, int, const juce::Image*, bool)
    {
        // The top bar's charcoal with its black foot, the title centred in the sections' capitals.
        const auto bar = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
        g.setColour(colour::bar);
        g.fillRect(bar);
        g.setColour(juce::Colours::black);
        g.fillRect(bar.withTop(bar.getBottom() - 1.0f));
        g.setColour(window.isActiveWindow() ? colour::caption : colour::dim);
        g.setFont(labelFont(11.0f).withExtraKerningFactor(0.12f));
        g.drawText(window.getName().toUpperCase(), bar, juce::Justification::centred, true);
    }

    juce::Button* WindowLook::createDocumentWindowButton(int buttonType)
    {
        return new TitleKey(buttonType);
    }

    WindowLook& WindowLook::shared()
    {
        return SharedLook::getInstance()->look;
    }
}
