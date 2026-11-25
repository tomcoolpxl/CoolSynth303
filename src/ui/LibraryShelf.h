#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Library.h"
#include "ui/Widgets.h"

/*
    The library as a shelf over the panel (CoolSynth's D338, the mockup the user picked, as CoolSynth 808's D273): a
    column of shelves (lines by style, sounds by family, your own of each), a search, the list with a small picture of
    each, and the chosen one in full. Audition plays a line in place of the pattern you edit, or your pattern with a
    sound, until you load or stop; Load puts it in, one undo step. The built-in ones stay as they are: Keep a copy
    puts one among yours, which you can rename or send to the Recycle Bin.

    Drawn at the board's own size (1280 by 800) over the whole board, and scaled with it.
*/
namespace cs303::ui
{
    class LibraryShelf final : public juce::Component, private juce::ListBoxModel
    {
    public:
        LibraryShelf();
        ~LibraryShelf() override;

        std::function<void(const LibraryLine&)> onLoadLine;
        std::function<void(const LibrarySound&)> onLoadSound;
        std::function<void(const LibraryLine*)> onAuditionLine;   // null: back to yours
        std::function<void(const LibrarySound*)> onAuditionSound; // null: back to yours
        std::function<void()> onClose;
        std::function<LibraryLine()> currentLine;   // the pattern you edit, as a line, for Keep on your shelf
        std::function<LibrarySound()> currentSound; // the panel's sound
        juce::String loadTarget = "01A";

        /** Reads the library again, keeping the chosen entry when it is still there. */
        void reload();
        /** An audition ended elsewhere. */
        void auditionEnded();

        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseUp(const juce::MouseEvent&) override;
        bool keyPressed(const juce::KeyPress&) override;

        // ---- tests ----------------------------------------------------------------------------------------------
        void showShelf(const juce::String& key);
        void searchFor(const juce::String& text);
        void selectRow(int row);
        [[nodiscard]] int shownCount() const noexcept { return static_cast<int>(shown.size()); }
        [[nodiscard]] const LibraryLine* chosenLine() const;
        [[nodiscard]] const LibrarySound* chosenSound() const;
        /** Whether an audition is sounding: asked of the editor, which alone holds it (D354: the shelf's own copy of
            this went out of step with the transport). */
        std::function<bool()> auditionActive;
        [[nodiscard]] bool isAuditioning() const { return auditionActive && auditionActive(); }
        void toggleAudition();
        /** Auditions the entry chosen, in place of whatever is sounding. */
        void startAudition();
        void load();
        void keepCopy();
        [[nodiscard]] juce::StringArray shelfKeys() const;

    private:
        struct Shown
        {
            bool line = true;
            int index = 0; // in lines or sounds
        };
        struct SideRow
        {
            juce::String key, label;
            int count = 0;
            bool heading = false;
            juce::Rectangle<float> area;
        };

        int getNumRows() override { return static_cast<int>(shown.size()); }
        void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged(int lastRowSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;

        void refill();
        void layOutSide();
        void updateKeys();
        void renameChosen();
        void recycleChosen();
        [[nodiscard]] juce::Rectangle<int> box() const noexcept { return { 70, 64, 1150, 690 }; }
        [[nodiscard]] juce::Rectangle<int> sideArea() const noexcept;
        [[nodiscard]] juce::Rectangle<int> listArea() const noexcept;
        [[nodiscard]] juce::Rectangle<int> detailArea() const noexcept;
        void paintDetail(juce::Graphics&);

        Library lib;
        std::vector<Shown> shown;
        std::vector<SideRow> side;
        juce::String shelf = "lines:all", query;
        int chosen = -1;
        juce::ListBox list;
        juce::TextEditor search;
        std::unique_ptr<Key> close, audition, loadKey, keep, rename, recycle, keepMine;
    };
}
