#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    The windows' own bar in the 303's look (D349, as CoolSynth 808's D311): the top bar's charcoal, the title in the
    panel's capitals, minimise and close drawn in the labels' grey, close red under the mouse. Used by the standalone
    app's main window and by the options window.
*/
namespace cs303::ui
{
    class WindowLook final : public juce::LookAndFeel_V4
    {
    public:
        void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int w, int h, int titleSpaceX, int titleSpaceW,
                                        const juce::Image* icon, bool drawTitleTextOnLeft) override;
        juce::Button* createDocumentWindowButton(int buttonType) override;

        /** One look for every window, made on first use and kept until JUCE shuts down. */
        static WindowLook& shared();
    };
}
