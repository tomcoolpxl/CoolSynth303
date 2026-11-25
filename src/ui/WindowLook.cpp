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
                                      : colour::screenLabel.withAlpha(down ? 0.22f : 0.14f));
                    g.fillRoundedRectangle(r, 3.0f);
                }
                g.setColour(close && (over || down) ? juce::Colours::white : colour::screenLabel);
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

    WindowLook::WindowLook() { recolour(); }

    void WindowLook::recolour()
    {
        using C = juce::Colour;
        const std::pair<int, C> scheme[] = {
            { juce::PopupMenu::backgroundColourId, palette().dialogTop },
            { juce::PopupMenu::textColourId, colour::screenLabel },
            { juce::PopupMenu::headerTextColourId, colour::screenDim },
            { juce::PopupMenu::highlightedBackgroundColourId, colour::orange },
            { juce::PopupMenu::highlightedTextColourId, colour::orangeInk },
            { juce::ScrollBar::thumbColourId, C(0xff4f5156) },
            { juce::ScrollBar::trackColourId, colour::well },
            { juce::TextEditor::backgroundColourId, colour::well },
            { juce::TextEditor::textColourId, colour::ink },
            { juce::TextEditor::outlineColourId, colour::wellLine },
            { juce::TextEditor::focusedOutlineColourId, colour::orange },
            { juce::TextEditor::highlightColourId, colour::orange.withAlpha(0.35f) },
            { juce::CaretComponent::caretColourId, colour::amber },
            { juce::ListBox::backgroundColourId, colour::face },
            { juce::AlertWindow::backgroundColourId, palette().dialogTop },
            { juce::AlertWindow::textColourId, colour::screenLabel },
            { juce::AlertWindow::outlineColourId, colour::wellLine },
            { juce::TextButton::buttonColourId, C(0xff2b2c30) },
            { juce::TextButton::textColourOffId, colour::capInk },
            { juce::ComboBox::backgroundColourId, colour::well },
            { juce::ComboBox::textColourId, colour::ink },
            { juce::ComboBox::outlineColourId, colour::wellLine },
            { juce::ComboBox::arrowColourId, colour::orange },
            { juce::ResizableWindow::backgroundColourId, colour::face },
        };
        for (const auto& [id, c] : scheme)
            setColour(id, c);
    }

    void WindowLook::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical, int thumbStart, int thumbSize,
                                   bool over, bool down)
    {
        // a slim rounded thumb in a dark slot, lighter under the mouse
        const auto slot = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
        g.setColour(colour::well);
        g.fillRoundedRectangle(slot, std::min(slot.getWidth(), slot.getHeight()) * 0.5f);
        if (thumbSize <= 0)
            return;
        const auto thumb = vertical ? juce::Rectangle<float>(slot.getX(), static_cast<float>(thumbStart) + 2.0f, slot.getWidth(), static_cast<float>(thumbSize) - 4.0f)
                                    : juce::Rectangle<float>(static_cast<float>(thumbStart) + 2.0f, slot.getY(), static_cast<float>(thumbSize) - 4.0f, slot.getHeight());
        g.setColour(down ? colour::orange : over ? juce::Colour(0xff6a6c71) : juce::Colour(0xff4f5156));
        g.fillRoundedRectangle(thumb, std::min(thumb.getWidth(), thumb.getHeight()) * 0.5f);
    }

    void WindowLook::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
    {
        const auto r = juce::Rectangle<int>(width, height).toFloat();
        g.setGradientFill(juce::ColourGradient(palette().dialogTop, 0.0f, 0.0f, palette().dialogBottom, 0.0f, r.getBottom(), false));
        g.fillAll();
        g.setColour(juce::Colours::black);
        g.drawRect(r, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRect(1.0f, 1.0f, r.getWidth() - 2.0f, 1.0f);
    }

    juce::Font WindowLook::getPopupMenuFont() { return bodyFont(14.0f); }

    void WindowLook::drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int w, int h, int, int, const juce::Image*, bool)
    {
        // The top bar's charcoal with its black foot, the title centred in the sections' capitals.
        const auto bar = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
        g.setColour(colour::bar);
        g.fillRect(bar);
        g.setColour(juce::Colours::black);
        g.fillRect(bar.withTop(bar.getBottom() - 1.0f));
        g.setColour(window.isActiveWindow() ? colour::screenCaption : colour::screenDim);
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
