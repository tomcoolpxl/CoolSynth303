#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    The windows' own bar in the 303's look, and JUCE's own pieces in the panel's (D348) (D349, as CoolSynth 808's D311): the top bar's charcoal, the title in the
    panel's capitals, minimise and close drawn in the labels' grey, close red under the mouse. Used by the standalone
    app's main window and by the options window.
*/
namespace cs303::ui
{
    class WindowLook final : public juce::LookAndFeel_V4
    {
    public:
        /** The panel's colours for JUCE's own pieces: pop-up menus, scroll bars, text fields, lists, alert windows. */
        WindowLook();
        /** Takes the colours of the theme in use (again after a theme is chosen). */
        void recolour();
        void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical, int thumbStartPosition,
                           int thumbSize, bool isMouseOver, bool isMouseDown) override;
        int getDefaultScrollbarWidth() override { return 10; }
        void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
        juce::Font getPopupMenuFont() override;
        void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int w, int h, int titleSpaceX, int titleSpaceW,
                                        const juce::Image* icon, bool drawTitleTextOnLeft) override;
        juce::Button* createDocumentWindowButton(int buttonType) override;

        /** One look for every window, made on first use and kept until JUCE shuts down. */
        static WindowLook& shared();
    };
}
