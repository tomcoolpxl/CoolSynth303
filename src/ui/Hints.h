#pragma once

#include <map>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/Theme.h"

/*
    The hint line (CoolSynth's D353): one plain sentence at the panel's foot on what the control under the mouse does
    and how to use it. A knob or key takes its hint from this table by its kind and name when it is made; where a name
    means two things (the synth's Accent knob, the keyboard's Accent key, a lock's Accent) the panel sets the hint
    itself. Painted places that are not components of their own (the grid's rows, the pattern keys, the tempo) answer
    through HintSource.
*/
namespace cs303::ui
{
    /** A component whose parts are painted, not components: the hint for the place under the mouse. */
    struct HintSource
    {
        virtual ~HintSource() = default;
        [[nodiscard]] virtual juce::String hintAt(juce::Point<int> local) const = 0;
    };

    /** The place under the mouse in a painted component, lit by drawHover; moving it repaints only the two places. */
    struct HoverPlace
    {
        juce::Rectangle<int> area;
        void moveTo(juce::Component& c, juce::Rectangle<int> next)
        {
            if (next == area)
                return;
            c.repaint(area.expanded(3));
            area = next;
            c.repaint(area.expanded(3));
        }
    };

    inline constexpr const char* knobHelp = " Drag or wheel; Shift for fine; double-click resets.";

    /** The hint for a knob ("knob:") or a key ("key:") by its name, or nothing. */
    inline juce::String hintFor(const juce::String& kindAndName)
    {
        static const std::map<juce::String, juce::String> hints {
            // the top bar
            { "key:<", "Edit the previous pattern." },
            { "key:>", "Edit the next pattern." },
            { "key:New", "A new project: the default patterns and panel. One undo step." },
            { "key:Open", "Open a saved project (.cs303)." },
            { "key:Save", "Save the project to a file." },
            { "key:Library", "The library: lines and sounds to audition and load, and the ones you keep." },
            { "key:Options", "Audio and MIDI devices, accuracy, the window's size, the theme, export." },
            { "key:Init", "The sound back to its defaults: every knob and switch, the Devil Fish off, the trimmers stock. The patterns stay. One undo step." },
            { "key:Play", "Start or stop the sequencer. Space does the same." },
            { "key:Tap", "Tap in time to set the tempo." },
            { "key:Rec", "Record knob moves into the pattern while it plays." },
            { "key:Lid", "Under the Lid: the TB-303's circuit, live as it plays." },
            { "key:Mods", "The trimmers inside the case, and the Devil Fish mod." },
            // the synth
            { "knob:Tuning", "Fine tuning of the oscillator's pitch." },
            { "key:Saw", "The oscillator's waveform: saw." },
            { "key:Square", "The oscillator's waveform: square." },
            { "knob:Cutoff", "The filter's cutoff frequency." },
            { "knob:Resonance", "The filter's resonance: the squelch around the cutoff." },
            { "knob:Env mod", "How far the envelope sweeps the cutoff on each note." },
            { "knob:Decay", "How long the filter's envelope takes to close; accented notes stay short." },
            { "knob:Accent", "How much louder and brighter an accented note is." },
            { "knob:Volume", "The output level." },
            { "knob:Amount", "How hard the drive saturates." },
            { "knob:Tone", "The drive's tone, darker to brighter." },
            { "knob:Time", "The delay's time in note values: 1/16, 1/8, dotted 1/8, 1/4." },
            { "fader:Feedback", "How many repeats the delay makes. Drag up or down." },
            { "fader:Mix", "How loud the repeats are against the dry sound. Drag up or down." },
            // the keyboard
            { "key:Back", "Pick the previous step." },
            { "key:Step", "Pick the next step." },
            { "key:Tie", "Make the picked step a tie: the note before it holds on." },
            { "key:Rest", "Make the picked step a rest." },
            { "key:Write", "The piano writes: a key sets the picked step's note and octave, then moves on." },
            { "key:Transpose", "The piano transposes: a key shifts the running pattern. Right-click to reset it." },
            // the sequencer's head band and Line page
            { "key:Undo", "Undo the last change (Ctrl+Z)." },
            { "key:Redo", "Redo what was undone (Ctrl+Shift+Z)." },
            { "key:Generate", "Write a new random line in the key below over the pattern you edit. One undo step; Generate again for another." },
            { "key:Mutate", "Change four steps of the line at random; the arrow beside it says what may change." },
            { "key:Snap to key", "Move every note of the pattern onto the key." },
            { "key:Clear", "Remove the picked step's locks." },
            // the Mods panel
            { "knob:TM3", "Trimmer TM3, inside the case: how far the cutoff reaches." },
            { "knob:TM4", "Trimmer TM4, inside the case: the tuning." },
            { "knob:TM5", "Trimmer TM5, inside the case: an octave's width." },
            { "knob:C11", "C11, inside the case: the capacitor that shapes the square." },
            { "knob:Overdrive", "Devil Fish: how hard the overdrive drives the filter." },
            { "knob:Slide Time", "Devil Fish: how long a slide takes." },
            { "knob:Soft Attack", "Devil Fish: the attack of notes without accent." },
            { "knob:Norm Decay", "Devil Fish: the volume's decay for notes without accent." },
            { "knob:Acc Decay", "Devil Fish: the decay of accented notes." },
            { "knob:Tracking", "Devil Fish: how far the cutoff follows the note's pitch." },
            { "knob:Filter FM", "Devil Fish: how much the oscillator moves the cutoff." },
            { "key:Sweep Speed", "Devil Fish: how fast the accent's sweep charges. Click for the next, right-click for the one before." },
            { "key:Acc Sweep", "Devil Fish: the accent's sweep circuit. Click for the next, right-click for the one before." },
            { "key:Muffler", "Devil Fish: the output's soft clipper, off, soft or hard. Click for the next, right-click for the one before." },
            { "key:Close", "Back to the synth's sections." },
        };
        if (const auto found = hints.find(kindAndName); found != hints.end())
            return kindAndName.startsWith("knob:") ? found->second + knobHelp : found->second;
        return {};
    }

    /** The hint for a place in a component: a painted place's (HintSource), else the nearest component's own hint,
        walking out from the one under it. */
    inline juce::String hintOnBoard(juce::Component& root, juce::Point<int> at)
    {
        for (auto* c = root.getComponentAt(at); c != nullptr && c != &root; c = c->getParentComponent())
        {
            juce::String t;
            if (const auto* h = dynamic_cast<const HintSource*>(c))
                t = h->hintAt(c->getLocalPoint(&root, at));
            if (t.isEmpty())
                t = c->getHelpText();
            if (t.isNotEmpty())
                return t;
        }
        if (const auto* h = dynamic_cast<const HintSource*>(&root))
            return h->hintAt(at);
        return {};
    }

    /** The hint line (D353): one sentence on what is under the mouse; on dark glass (the dialogs) in their light ink. */
    class HintLine final : public juce::Component
    {
    public:
        explicit HintLine(bool onGlass) : glass(onGlass) { setInterceptsMouseClicks(false, false); }
        juce::String text;
        void paint(juce::Graphics& g) override
        {
            g.setFont(bodyFont(12.0f));
            const auto quiet = glass ? colour::screenDim : colour::dim, said = glass ? colour::screenHint : colour::hint;
            g.setColour(text.isEmpty() ? quiet : said);
            shapedText(g, text.isEmpty() ? juce::String("Point at a control to see what it does.") : text, getLocalBounds().toFloat(),
                       juce::Justification::centredLeft, true);
        }

    private:
        bool glass;
    };

    /** Follows the mouse over a component and its children and puts the hint of what is under it in the line. */
    class HintWatcher final : public juce::MouseListener
    {
    public:
        HintWatcher(juce::Component& r, HintLine& l) : root(r), line(l) {}
        void mouseMove(const juce::MouseEvent& e) override { show(e); }
        void mouseEnter(const juce::MouseEvent& e) override { show(e); }
        void mouseUp(const juce::MouseEvent& e) override { show(e); }
        void mouseExit(const juce::MouseEvent&) override
        {
            if (! root.isMouseOver(true))
                set({});
        }

    private:
        juce::Component& root;
        HintLine& line;
        void set(const juce::String& t)
        {
            if (t != line.text)
            {
                line.text = t;
                line.repaint();
            }
        }
        void show(const juce::MouseEvent& e) { set(hintOnBoard(root, e.getEventRelativeTo(&root).getPosition())); }
    };
}
