#include "ui/Theme.h"

#include "AppData.h"

#include <functional>
#include <map>
#include <tuple>

namespace cs303::ui
{
    namespace
    {
        juce::Typeface::Ptr typefaceOf(Face face)
        {
            static const juce::Typeface::Ptr label = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoSemiCondensedSemiBold_ttf, AppData::ArchivoSemiCondensedSemiBold_ttfSize);
            static const juce::Typeface::Ptr labelBold = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoSemiCondensedBold_ttf, AppData::ArchivoSemiCondensedBold_ttfSize);
            static const juce::Typeface::Ptr head = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoExtraBold_ttf, AppData::ArchivoExtraBold_ttfSize);
            static const juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor(AppData::IBMPlexMonoMedium_ttf, AppData::IBMPlexMonoMedium_ttfSize);
            static const juce::Typeface::Ptr monoBold = juce::Typeface::createSystemTypefaceFor(AppData::IBMPlexMonoSemiBold_ttf, AppData::IBMPlexMonoSemiBold_ttfSize);
            static const juce::Typeface::Ptr body = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoMedium_ttf, AppData::ArchivoMedium_ttfSize);
            static const juce::Typeface::Ptr stencil = juce::Typeface::createSystemTypefaceFor(AppData::ArchivoCondensedSemiBold_ttf, AppData::ArchivoCondensedSemiBold_ttfSize);
            switch (face)
            {
                case Face::label: return label;
                case Face::labelBold: return labelBold;
                case Face::head: return head;
                case Face::mono: return mono;
                case Face::monoBold: return monoBold;
                case Face::body: return body;
                case Face::stencil: return stencil;
            }
            return label;
        }
    }

    namespace
    {
        /** Fonts made once and kept (CoolSynth's D306: each made anew cost some 22 us), deleted with JUCE's shutdown. */
        struct KeptFonts final : public juce::DeletedAtShutdown
        {
            std::map<std::tuple<int, int, int>, juce::Font> fonts;
            ~KeptFonts() override { clearSingletonInstance(); }
            JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE(KeptFonts, false)
        };
    }

    juce::Font fontOf(Face face, float height, float tracking)
    {
        auto& kept = KeptFonts::getInstance()->fonts;
        const auto key = std::make_tuple(static_cast<int>(face), juce::roundToInt(height * 100.0f), juce::roundToInt(tracking * 1000.0f));
        if (const auto found = kept.find(key); found != kept.end())
            return found->second;
        const auto font = juce::Font(juce::FontOptions().withTypeface(typefaceOf(face)).withHeight(height)).withExtraKerningFactor(tracking);
        kept.emplace(key, font);
        return font;
    }

    namespace
    {
        juce::ColourGradient vertical(juce::Rectangle<float> r, std::initializer_list<std::pair<float, juce::Colour>> stops)
        {
            juce::ColourGradient grad(stops.begin()->second, r.getX(), r.getY(), (stops.end() - 1)->second, r.getX(), r.getBottom(), false);
            for (const auto& [at, c] : stops)
                if (at > 0.0f && at < 1.0f)
                    grad.addColour(at, c);
            return grad;
        }
    }

    void glowRounded(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c, float spread, float alpha)
    {
        // a few widening, fading copies: enough of a halo at the panel's sizes without a blur
        for (int i = 3; i >= 1; --i)
        {
            const float e = spread * static_cast<float>(i) / 3.0f;
            g.setColour(c.withAlpha(alpha * (0.18f + 0.27f * static_cast<float>(3 - i))));
            g.fillRoundedRectangle(r.expanded(e), corner + e);
        }
    }

    namespace
    {
        void paintGround(juce::Graphics& g, juce::Rectangle<float> area)
        {
            g.setGradientFill(vertical(area, { { 0.0f, colour::faceTop }, { 1.0f, colour::faceBottom } }));
            g.fillRect(area);
            // the brushing: a hairline every other pixel, barely there
            g.setColour(juce::Colours::white.withAlpha(palette().brush));
            for (float x = area.getX(); x < area.getRight(); x += 2.0f)
                g.fillRect(x, area.getY(), 1.0f, area.getHeight());
            // the sheen from above
            juce::ColourGradient sheen(juce::Colours::white.withAlpha(palette().sheen), area.getX() + area.getWidth() * 0.55f, area.getY() - 40.0f,
                                       juce::Colours::white.withAlpha(0.0f), area.getX() + area.getWidth() * 0.55f, area.getY() + 260.0f, true);
            g.setGradientFill(sheen);
            g.fillRect(area);
        }

        /** Still pictures drawn once and kept, at the scale they are shown at (CoolSynth's D306: the panel redrawn
            whole on every press showed it late). Deleted with JUCE's shutdown. */
        struct KeptImages final : public juce::DeletedAtShutdown
        {
            std::map<juce::String, juce::Image> images;
            ~KeptImages() override { clearSingletonInstance(); }
            JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE(KeptImages, false)
        };
    }

    void drawKept(juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, const std::function<void(juce::Graphics&, juce::Rectangle<float>)>& paint)
    {
        const float scale = std::max(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto key = name + "|" + juce::String(area.getWidth(), 1) + "x" + juce::String(area.getHeight(), 1) + "@" + juce::String(scale, 3);
        auto& kept = KeptImages::getInstance()->images;
        auto found = kept.find(key);
        if (found == kept.end())
        {
            if (kept.size() > 200)
                kept.clear(); // a bound, should the window's scale keep changing
            juce::Image image(juce::Image::ARGB, std::max(1, juce::roundToInt(area.getWidth() * scale)), std::max(1, juce::roundToInt(area.getHeight() * scale)), true);
            {
                juce::Graphics c(image);
                c.addTransform(juce::AffineTransform::scale(scale));
                paint(c, area.withPosition(0.0f, 0.0f));
            }
            found = kept.emplace(key, image).first;
        }
        g.drawImageTransformed(found->second, juce::AffineTransform::scale(1.0f / scale).translated(area.getX(), area.getY()));
    }

    void forgetKept() { KeptImages::getInstance()->images.clear(); }

    // ---------------------------------------------------------------- the themes (D352), as the mockup gives them
    namespace
    {
        using C = juce::Colour;

        Palette graphite()
        {
            Palette p;
            p.desk = C(0xff0b0a09);
            p.face = C(0xff1c1d20);
            p.dialogTop = C(0xff26272b);
            p.dialogBottom = C(0xff212226);
            p.bar = C(0xff121315);
            p.barTop = C(0xff141517);
            p.barBottom = C(0xff0f1012);
            p.barDim = C(0xff6f7176);
            p.stencil = C(0xff141517);
            p.stencilRight = C(0xff18191c);
            p.titleInk = C(0xffcfcfcb);
            p.faceTop = C(0xff1f2023);
            p.faceBottom = C(0xff18191b);
            p.plateTop = C(0xff26272b);
            p.plateBottom = C(0xff212226);
            p.plateEdge = C(0xff0c0c0d);
            p.brush = 0.014f;
            p.sheen = 0.07f;
            p.plateLit = 0.045f;
            p.plateShadow = 0.3f;
            p.caption = C(0xffecebe6);
            p.label = C(0xffe2e1dc);
            p.dim = C(0xff7d7f84);
            p.hint = C(0xff8f9196);
            p.rowName = C(0xffd9d7d0);
            p.labelHover = juce::Colours::white;
            p.valueInk = C(0xffffb35c);
            p.screenCaption = C(0xffecebe6);
            p.screenLabel = C(0xffe2e1dc);
            p.screenDim = C(0xff7d7f84);
            p.screenHint = C(0xff8f9196);
            p.gridNumber = C(0xff6d6f74);
            p.gridOff = C(0xff77797e);
            p.gridHalf = C(0xff8d8f94);
            p.slotInk = C(0xffa9aaa6);
            p.slotLabel = C(0xffe2e1dc);
            p.slotDim = C(0xff7d7f84);
            p.slotEdited = C(0xffffb35c);
            p.capTop = C(0xff3d3e43);
            p.capMid = C(0xff2b2c30);
            p.capBottom = C(0xff232427);
            p.capEdge = C(0xff08090a);
            p.capInk = C(0xffe7e5df);
            p.capInkOn = juce::Colours::white;
            p.capEngrave = juce::Colours::black.withAlpha(0.6f);
            p.capLit = 0.14f;
            p.engraveDy = -1.0f;
            p.orange = C(0xfff0782a);
            p.orangeInk = C(0xff1d0f05);
            p.hotTop = C(0xffffa25c);
            p.hotBottom = C(0xffc95a17);
            p.hotDeep = C(0xffcc5c18);
            p.amber = C(0xffffb35c);
            p.accentNote = C(0xffffc489);
            p.light = C(0xfffff1e0);
            p.lensLit = C(0xffffb067);
            p.lensCore = C(0xfffff4e6);
            p.lensGlow = C(0xffff8c32);
            p.tieTop = C(0xffb2581f);
            p.tieBottom = C(0xff8e4416);
            p.screenTop = C(0xff0f1012);
            p.screenBottom = C(0xff141518);
            p.screenEdge = C(0xff050506);
            p.displayTop = C(0xff0a0b0b);
            p.displayBottom = C(0xff121314);
            p.well = C(0xff0d0e0f);
            p.wellLine = C(0xff2a2b2f);
            p.outline = C(0xff3a3c41);
            p.cell = C(0xff2a2b2f);
            p.cellOff = C(0xff222326);
            p.litTop = C(0xff55575c);
            p.litBottom = C(0xff44464b);
            p.halfTop = C(0xff36373b);
            p.halfBottom = C(0xff2e2f33);
            p.slotHasTop = C(0xff36373b);
            p.slotHasBottom = C(0xff2b2c30);
            p.slotEmptyTop = C(0xff26272b);
            p.slotEmptyBottom = C(0xff1f2023);
            p.key = C(0xff4f5156);
            p.keyInk = C(0xfff2efe8);
            p.lampOff = C(0xff5a2a20);
            p.lampOn = C(0xffff3b28);
            p.ink = C(0xfff1ebde);
            p.laneHi = 0.035f;
            p.laneLo = 0.012f;
            p.motionFill = 0.03f;
            p.hoverFill = 0.08f;
            p.hoverEdge = 0.22f;
            p.wellRim = 0.05f;
            return p;
        }

        /** Silver: the TB-303's face. The plates brushed aluminium printed in black, the keys light grey; the frame,
            the screens, the bays and the dialogs smoked mid grey (black was too hard a step from the silver), the
            displays a little darker for their amber. */
        Palette silver()
        {
            auto p = graphite();
            // the frame (the name's strip, the top bar, the desk round the board) a mid grey, not graphite's black:
            // against the light plates black was too hard a step (the user, after Steel)
            p.desk = C(0xff464a4f);
            p.bar = C(0xff50545a);
            p.barTop = C(0xff5b5f65);
            p.barBottom = C(0xff4d5157);
            p.barDim = C(0xffbcc0c5);
            p.stencil = C(0xff50545a);
            p.stencilRight = C(0xff595d63);
            p.titleInk = C(0xffeff0f1);
            p.faceTop = C(0xffd1d4d7);
            p.faceBottom = C(0xffb5b9bd);
            p.plateTop = C(0xffe7e9eb);
            p.plateBottom = C(0xffcbced2);
            p.plateEdge = C(0xff868a90);
            p.brush = 0.10f;
            p.sheen = 0.45f;
            p.plateLit = 0.5f;
            p.plateShadow = 0.2f;
            p.caption = C(0xff18191b);
            p.label = C(0xff1c1d20);
            p.dim = C(0xff4b4e54);
            p.hint = C(0xff44474c);
            p.rowName = C(0xff1c1d20);
            p.labelHover = juce::Colours::black;
            p.valueInk = C(0xffb4501a);
            p.capTop = C(0xfff4f5f6);
            p.capMid = C(0xffdcdee1);
            p.capBottom = C(0xffc2c5c9);
            p.capEdge = C(0xff5f6368);
            p.capInk = C(0xff1b1c1e);
            p.capInkOn = juce::Colours::black;
            p.capEngrave = juce::Colours::white.withAlpha(0.65f);
            p.capLit = 0.9f;
            p.engraveDy = 1.0f;
            // the glass (the grid, the side column, the bays, the dialogs) smoked mid grey, not graphite's black
            // (the user: too black against the silver); the displays a little darker, for their amber
            p.face = C(0xff46494e);
            p.dialogTop = C(0xff55595e);
            p.dialogBottom = C(0xff4d5156);
            p.screenTop = C(0xff383c41);
            p.screenBottom = C(0xff3d4146);
            p.screenEdge = C(0xff26292d);
            p.displayTop = C(0xff2c2f33);
            p.displayBottom = C(0xff33363a);
            p.well = C(0xff2e3135);
            p.wellLine = C(0xff62666c);
            p.outline = C(0xff6e7278);
            // the grid's and the bays' keys light grey with dark ink, as the TB-303's own step keys (the user's pick
            // after measuring: text 4.5:1 or more against its ground, every key clearly off the screen)
            p.cell = C(0xffa3a7ad);
            p.cellOff = C(0xff989ca2);
            p.litTop = C(0xffe9ebed);
            p.litBottom = C(0xffdadde0);
            p.halfTop = C(0xffc9ccd0);
            p.halfBottom = C(0xffbcbfc3);
            p.slotHasTop = C(0xffc6c9cd);
            p.slotHasBottom = C(0xffb9bcc0);
            p.slotEmptyTop = C(0xffa3a7ad);
            p.slotEmptyBottom = C(0xff989ca2);
            p.key = C(0xffdcdee1);
            p.keyInk = C(0xff16171a);
            p.screenDim = C(0xffc6c9cd);
            p.screenHint = C(0xffd0d3d6);
            p.gridNumber = C(0xffc0c4c8);
            p.gridOff = C(0xff26282c);
            p.gridHalf = C(0xff26282c);
            p.slotInk = C(0xff26282c);
            p.slotLabel = C(0xff16171a);
            p.slotDim = C(0xff2e3135);
            p.slotEdited = C(0xff76300c);
            return p;
        }

        /** Steel: the 808's blue-grey anodised aluminium, white printing, the glass tinted blue, its orange. */
        Palette steel()
        {
            auto p = graphite();
            p.face = C(0xff1c2127);
            p.dialogTop = C(0xff2a3038);
            p.dialogBottom = C(0xff242a31);
            p.bar = C(0xff171b20);
            p.barTop = C(0xff1a1f24);
            p.barBottom = C(0xff14181c);
            p.barDim = C(0xff7f8a94);
            p.stencil = C(0xff15191d);
            p.stencilRight = C(0xff1b2026);
            p.faceTop = C(0xff56606a);
            p.faceBottom = C(0xff454e58);
            p.plateTop = C(0xff636d78);
            p.plateBottom = C(0xff56606a);
            p.plateEdge = C(0xff262c32);
            p.brush = 0.015f;
            p.sheen = 0.08f;
            p.plateLit = 0.06f;
            p.plateShadow = 0.35f;
            p.caption = C(0xfff5f8fa);
            p.label = C(0xffe8eef3);
            p.dim = C(0xffa3aeb8);
            p.hint = C(0xffb8c2cb);
            p.rowName = C(0xffe8eef3);
            p.valueInk = C(0xffffa04a);
            p.screenCaption = C(0xfff0f4f7);
            p.screenDim = C(0xff8f9aa5);
            p.screenHint = C(0xff9aa5af);
            p.capTop = C(0xff6e7883);
            p.capMid = C(0xff56606a);
            p.capBottom = C(0xff47505a);
            p.capEdge = C(0xff20252a);
            p.capInk = C(0xfff3f6f8);
            p.capLit = 0.18f;
            p.orange = C(0xffe8742a);
            p.orangeInk = C(0xff1a120c);
            p.hotTop = C(0xfff5a066);
            p.hotBottom = C(0xffc3581b);
            p.hotDeep = C(0xffc3581b);
            p.amber = C(0xffffa04a);
            p.screenTop = C(0xff10151a);
            p.screenBottom = C(0xff151b21);
            p.screenEdge = C(0xff040607);
            p.displayTop = C(0xff0b0f13);
            p.displayBottom = C(0xff12171c);
            p.well = C(0xff0d1115);
            p.wellLine = C(0xff2a3038);
            p.outline = C(0xff3d4650);
            p.cell = C(0xff2b333c);
            p.cellOff = C(0xff232a32);
            p.litTop = C(0xff5b6773);
            p.litBottom = C(0xff4b5661);
            p.halfTop = C(0xff38414b);
            p.halfBottom = C(0xff2f3740);
            p.slotHasTop = C(0xff3a434d);
            p.slotHasBottom = C(0xff2e363f);
            p.slotEmptyTop = C(0xff262d35);
            p.slotEmptyBottom = C(0xff1f252c);
            p.key = C(0xff56606b);
            p.slotLabel = p.screenLabel; // the slots print as the screen does
            p.slotDim = p.screenDim;
            p.slotEdited = p.amber;
            return p;
        }

        /** Olive: the 808's olive drab, cream printing, the glass tinted green, signal red in place of orange. */
        Palette olive()
        {
            auto p = graphite();
            p.face = C(0xff1f2018);
            p.dialogTop = C(0xff2d2e24);
            p.dialogBottom = C(0xff27281f);
            p.bar = C(0xff1a1b14);
            p.barTop = C(0xff1d1e16);
            p.barBottom = C(0xff171812);
            p.barDim = C(0xff8a8873);
            p.stencil = C(0xff17180f);
            p.stencilRight = C(0xff1e1f16);
            p.faceTop = C(0xff5c5e49);
            p.faceBottom = C(0xff4a4c3a);
            p.plateTop = C(0xff686a55);
            p.plateBottom = C(0xff5a5c48);
            p.plateEdge = C(0xff24251b);
            p.brush = 0.015f;
            p.sheen = 0.07f;
            p.plateLit = 0.06f;
            p.plateShadow = 0.35f;
            p.caption = C(0xfff7f4e8);
            p.label = C(0xffeeead6);
            p.dim = C(0xffaaa78f);
            p.hint = C(0xffc4c0a8);
            p.rowName = C(0xffeeead6);
            p.valueInk = C(0xffff7a55);
            p.screenCaption = C(0xfff4f1e4);
            p.screenDim = C(0xff9a977f);
            p.screenHint = C(0xffa9a58d);
            p.capTop = C(0xff74765e);
            p.capMid = C(0xff5d5f49);
            p.capBottom = C(0xff4c4e3b);
            p.capEdge = C(0xff1f2017);
            p.capInk = C(0xfff7f4e8);
            p.capLit = 0.17f;
            p.orange = C(0xffe2512b);
            p.orangeInk = C(0xff230b04);
            p.hotTop = C(0xfff68866);
            p.hotBottom = C(0xffb33a1a);
            p.hotDeep = C(0xffb33a1a);
            p.amber = C(0xffff7a55);
            p.accentNote = C(0xffffb59c);
            p.light = C(0xfffff0ea);
            p.lensLit = C(0xffff8a66);
            p.lensCore = C(0xfffff0ea);
            p.lensGlow = C(0xffff6a43);
            p.tieTop = C(0xffa83c1c);
            p.tieBottom = C(0xff7e2c14);
            p.screenTop = C(0xff12130d);
            p.screenBottom = C(0xff171810);
            p.screenEdge = C(0xff050604);
            p.displayTop = C(0xff0e0f0a);
            p.displayBottom = C(0xff14150f);
            p.well = C(0xff10110c);
            p.wellLine = C(0xff2d2e24);
            p.outline = C(0xff45473a);
            p.cell = C(0xff2d2f24);
            p.cellOff = C(0xff25271d);
            p.litTop = C(0xff5f6250);
            p.litBottom = C(0xff4f5242);
            p.halfTop = C(0xff3a3c2f);
            p.halfBottom = C(0xff313327);
            p.slotHasTop = C(0xff3c3e31);
            p.slotHasBottom = C(0xff313327);
            p.slotEmptyTop = C(0xff292a20);
            p.slotEmptyBottom = C(0xff222319);
            p.key = C(0xff5a5d4b);
            p.slotLabel = p.screenLabel; // the slots print as the screen does
            p.slotDim = p.screenDim;
            p.slotEdited = p.amber;
            return p;
        }

        int themeInUse = 0;
    }

    Palette& palette()
    {
        static Palette p = graphite();
        return p;
    }

    juce::StringArray themeNames() { return { "Graphite", "Silver", "Steel", "Olive" }; }
    int currentTheme() { return themeInUse; }

    namespace
    {
        // WCAG 2's relative luminance and contrast ratio
        float luminanceOf(juce::Colour c)
        {
            const auto lin = [](float u) { return u <= 0.04045f ? u / 12.92f : std::pow((u + 0.055f) / 1.055f, 2.4f); };
            return 0.2126f * lin(c.getFloatRed()) + 0.7152f * lin(c.getFloatGreen()) + 0.0722f * lin(c.getFloatBlue());
        }
        float ratioOf(juce::Colour a, juce::Colour b)
        {
            const float x = luminanceOf(a), y = luminanceOf(b);
            return (std::max(x, y) + 0.05f) / (std::min(x, y) + 0.05f);
        }
        juce::Colour brighter(juce::Colour c, float add) { return c.withBrightness(std::min(1.0f, c.getBrightness() + add)); }
        /** How much brighter c must be, its hue and saturation kept, to stand `target` off the darker `against`. */
        float liftFor(juce::Colour c, juce::Colour against, float target)
        {
            float add = 0.0f;
            while (add < 1.0f && ratioOf(brighter(c, add), against) < target)
                add += 0.004f;
            return add;
        }
        /** A gradient's two ends lifted together, judged by their middle. */
        void liftPair(juce::Colour& top, juce::Colour& bottom, juce::Colour against, float target)
        {
            const float add = liftFor(top.interpolatedWith(bottom, 0.5f), against, target);
            top = brighter(top, add);
            bottom = brighter(bottom, add);
        }
        /** An ink brought to 4.5:1 on its ground (aimed a hair over, for rounding): lighter on a dark ground, darker on a
            light one; where its own hue cannot get there, pure white or near black, whichever stands further off. */
        juce::Colour inkOn(juce::Colour ink, juce::Colour ground)
        {
            constexpr float need = 4.6f;
            if (ratioOf(ink, ground) >= need)
                return ink;
            const bool dark = luminanceOf(ground) < 0.18f;
            for (float step = 0.0f; step <= 1.0f; step += 0.01f)
            {
                const auto c = dark ? brighter(ink, step) : ink.withBrightness(std::max(0.0f, ink.getBrightness() - step));
                if (ratioOf(c, ground) >= need)
                    return c;
            }
            const juce::Colour white(0xffffffff), black(0xff101113);
            return ratioOf(white, ground) > ratioOf(black, ground) ? white : black;
        }

        /** Every theme's contrast (D352, the user's pick of three steps drawn and measured): the grid's empty keys 3:1
            off the screen (WCAG's floor for a control), half-lit 4:1, lit 5.5:1, the screens' keys 3:1, each lifted in
            its own hue; the lanes, the hover and the displays' rims plainly there; every ink 4.5:1 on what it prints on. */
        void strengthen(Palette& p)
        {
            struct Targets { float off, half, lit, cap, laneHi, laneLo, motion, hoverFill, hoverEdge, rim; };
            constexpr Targets t { 3.05f, 4.05f, 5.55f, 3.05f, 0.14f, 0.06f, 0.14f, 0.28f, 0.75f, 0.42f }; // a hair over 3, 4, 5.5, for rounding
            const auto screen = p.screenTop.interpolatedWith(p.screenBottom, 0.5f);
            liftPair(p.cell, p.cellOff, screen, t.off);
            liftPair(p.slotEmptyTop, p.slotEmptyBottom, screen, t.off);
            liftPair(p.halfTop, p.halfBottom, screen, t.half);
            liftPair(p.slotHasTop, p.slotHasBottom, screen, t.half);
            liftPair(p.litTop, p.litBottom, screen, t.lit);
            p.key = brighter(p.key, liftFor(p.key, screen, t.lit));
            const float capAdd = liftFor(p.capMid, screen, t.cap);
            p.capTop = brighter(p.capTop, capAdd);
            p.capMid = brighter(p.capMid, capAdd);
            p.capBottom = brighter(p.capBottom, capAdd);
            p.laneHi = std::max(p.laneHi, t.laneHi);
            p.laneLo = std::max(p.laneLo, t.laneLo);
            p.motionFill = std::max(p.motionFill, t.motion);
            p.hoverFill = std::max(p.hoverFill, t.hoverFill);
            p.hoverEdge = std::max(p.hoverEdge, t.hoverEdge);
            p.wellRim = std::max(p.wellRim, t.rim);
            const auto mid = [](juce::Colour a, juce::Colour b) { return a.interpolatedWith(b, 0.5f); };
            const auto off = mid(p.cell, p.cellOff), half = mid(p.halfTop, p.halfBottom), lit = mid(p.litTop, p.litBottom);
            p.keyInk = inkOn(p.keyInk, lit);
            p.gridHalf = inkOn(p.gridHalf, half);
            p.gridOff = inkOn(p.gridOff, off);
            p.slotInk = inkOn(p.slotInk, off);
            p.slotLabel = inkOn(p.slotLabel, mid(p.slotHasTop, p.slotHasBottom));
            p.slotDim = inkOn(p.slotDim, mid(p.slotEmptyTop, p.slotEmptyBottom));
            p.slotEdited = inkOn(p.slotEdited, mid(p.slotHasTop, p.slotHasBottom));
            p.gridNumber = inkOn(p.gridNumber, screen);
            p.capInk = inkOn(p.capInk, p.capMid);
        }
    }

    void applyTheme(int i)
    {
        themeInUse = juce::jlimit(0, 3, i);
        palette() = themeInUse == 1 ? silver() : themeInUse == 2 ? steel() : themeInUse == 3 ? olive() : graphite();
        strengthen(palette());
        forgetKept();
    }

    void drawGround(juce::Graphics& g, juce::Rectangle<float> area)
    {
        drawKept(g, "ground" + juce::String(currentTheme()), area, paintGround);
    }

    float drawSection(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& name, juce::Colour, int number)
    {
        const auto plate = area.withTop(area.getY() - 8.0f);
        constexpr float corner = 7.0f;
        g.setColour(juce::Colours::black.withAlpha(palette().plateShadow));
        g.fillRoundedRectangle(plate.translated(0.0f, 3.0f).expanded(1.0f), corner + 1.0f);
        g.setColour(palette().plateEdge);
        g.fillRoundedRectangle(plate.expanded(1.0f), corner + 1.0f);
        g.setGradientFill(vertical(plate, { { 0.0f, colour::plateTop }, { 1.0f, colour::plateBottom } }));
        g.fillRoundedRectangle(plate, corner);
        // the head band
        const auto band = plate.withHeight(22.0f);
        {
            juce::Graphics::ScopedSaveState s(g);
            juce::Path clip;
            clip.addRoundedRectangle(band.getX(), band.getY(), band.getWidth(), band.getHeight(), corner, corner, true, true, false, false);
            g.reduceClipRegion(clip);
            g.setGradientFill(vertical(band, { { 0.0f, juce::Colours::white.withAlpha(palette().plateLit) }, { 1.0f, juce::Colours::white.withAlpha(palette().plateLit / 3.0f) } }));
            g.fillRect(band);
        }
        g.setColour(juce::Colours::black.withAlpha(palette().plateShadow * 1.2f));
        g.fillRect(band.getX(), band.getBottom() - 1.0f, band.getWidth(), 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.fillRect(band.getX(), band.getBottom(), band.getWidth(), 1.0f);
        // the plate's lit top edge
        g.setColour(juce::Colours::white.withAlpha(juce::jmin(0.9f, palette().plateLit * 1.6f)));
        g.fillRect(plate.getX() + corner, plate.getY(), plate.getWidth() - 2.0f * corner, 1.0f);

        float x = plate.getX() + 12.0f;
        if (number > 0)
        {
            const auto font = fontOf(Face::monoBold, 10.5f);
            const auto t = juce::String(number).paddedLeft('0', 2);
            g.setColour(colour::orange);
            g.setFont(font);
            shapedText(g, t, juce::Rectangle<float>(x, band.getY(), 30.0f, band.getHeight()), juce::Justification::centredLeft, false);
            x += shapedWidth(font, t) + 7.0f;
        }
        if (name.isEmpty())
            return x;
        const auto font = fontOf(Face::head, 10.0f, 0.15f);
        const auto text = name.toUpperCase();
        g.setColour(colour::caption);
        g.setFont(font);
        shapedText(g, text, juce::Rectangle<float>(x, band.getY(), plate.getRight() - x - 8.0f, band.getHeight()), juce::Justification::centredLeft, false);
        return x + shapedWidth(font, text);
    }

    void drawLens(juce::Graphics& g, juce::Point<float> centre, bool on, float d)
    {
        const auto r = juce::Rectangle<float>(d, d).withCentre(centre);
        if (on)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour(palette().lensGlow.withAlpha(0.12f + 0.1f * static_cast<float>(3 - i)));
                g.fillEllipse(r.expanded(static_cast<float>(i) * 1.6f));
            }
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillEllipse(r.expanded(1.0f));
            juce::ColourGradient grad(palette().lensCore, r.getX() + d * 0.38f, r.getY() + d * 0.32f, colour::orange, r.getRight(), r.getBottom(), true);
            grad.addColour(0.45, palette().lensLit);
            g.setGradientFill(grad);
            g.fillEllipse(r);
        }
        else
        {
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(r.expanded(1.0f));
            juce::ColourGradient grad(colour::lampOff, r.getX() + d * 0.35f, r.getY() + d * 0.3f, juce::Colour(0xff1c0d09), r.getRight(), r.getBottom(), true);
            g.setGradientFill(grad);
            g.fillEllipse(r);
        }
    }

    void drawHover(juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        g.setColour(juce::Colours::white.withAlpha(palette().hoverFill));
        g.fillRoundedRectangle(r, corner);
        g.setColour(juce::Colours::white.withAlpha(palette().hoverEdge));
        g.drawRoundedRectangle(r.reduced(0.5f), corner, 1.0f);
    }

    void drawKey(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, bool on, bool lens, bool enabled, bool hot, bool hover, bool down)
    {
        juce::Graphics::ScopedSaveState s(g);
        if (! enabled)
        {
            g.beginTransparencyLayer(0.4f);
            hover = down = false;
        }
        if (down)
            area = area.translated(0.0f, 1.0f); // pressed: the cap goes down a pixel, its shadow shorter
        constexpr float corner = 4.0f;
        g.setColour(juce::Colours::black.withAlpha(hot ? 0.0f : 0.5f));
        g.fillRoundedRectangle(area.translated(0.0f, 2.0f), corner);
        if (hot)
            glowRounded(g, area, corner, colour::orange, 4.0f, 0.3f);
        g.setColour(palette().capEdge);
        g.fillRoundedRectangle(area.expanded(1.0f), corner + 1.0f);
        if (hot)
            g.setGradientFill(vertical(area, { { 0.0f, palette().hotTop }, { 0.52f, colour::orange }, { 1.0f, palette().hotBottom } }));
        else
            g.setGradientFill(vertical(area, { { 0.0f, palette().capTop }, { 0.55f, palette().capMid }, { 1.0f, palette().capBottom } }));
        g.fillRoundedRectangle(area, corner);
        if (hover || down)
        {
            // under the mouse the cap lights a little; held, it darkens
            g.setColour(down ? juce::Colours::black.withAlpha(0.18f) : juce::Colours::white.withAlpha(hot ? 0.12f : 0.07f));
            g.fillRoundedRectangle(area, corner);
        }
        g.setColour(juce::Colours::white.withAlpha(hot ? 0.4f : juce::jmin(1.0f, palette().capLit + (hover ? 0.1f : 0.0f))));
        g.fillRect(area.getX() + corner, area.getY(), area.getWidth() - 2.0f * corner, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(hot ? 0.15f : 0.35f));
        g.fillRect(area.getX() + corner * 0.5f, area.getBottom() - 2.0f, area.getWidth() - corner, 2.0f);

        const auto font = labelFont(10.5f);
        const auto t = text.toUpperCase();
        const float tw = t.isEmpty() ? 0.0f : shapedWidth(font, t);
        const float total = (lens ? 6.0f + (tw > 0.0f ? 5.0f : 0.0f) : 0.0f) + tw;
        float x = area.getCentreX() - total * 0.5f;
        if (lens)
        {
            drawLens(g, { x + 3.0f, area.getCentreY() }, on);
            x += 11.0f;
        }
        if (t.isNotEmpty())
        {
            g.setFont(font);
            // the engraving's shadow, then the name
            g.setColour(hot ? juce::Colours::white.withAlpha(0.25f) : palette().capEngrave);
            shapedText(g, t, juce::Rectangle<float>(x, area.getY() + (hot ? 1.0f : palette().engraveDy), tw + 2.0f, area.getHeight()), juce::Justification::centredLeft, false);
            g.setColour(hot ? colour::orangeInk : on ? palette().capInkOn : colour::capInk);
            shapedText(g, t, juce::Rectangle<float>(x, area.getY(), tw + 2.0f, area.getHeight()), juce::Justification::centredLeft, false);
        }
        if (! enabled)
            g.endTransparencyLayer();
    }

    void drawDisplay(juce::Graphics& g, juce::Rectangle<float> area)
    {
        constexpr float corner = 4.0f;
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(area.translated(0.0f, 1.0f), corner);
        g.setGradientFill(vertical(area, { { 0.0f, palette().displayTop }, { 1.0f, palette().displayBottom } }));
        g.fillRoundedRectangle(area, corner);
        g.setColour(juce::Colours::black);
        g.fillRect(area.getX() + corner, area.getY(), area.getWidth() - 2.0f * corner, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(palette().wellRim));
        g.drawRoundedRectangle(area.reduced(0.5f), corner, 1.0f);
        // the glass's sheen
        const auto glass = area.reduced(1.0f);
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.07f), glass.getX(), glass.getY(),
                                   juce::Colours::white.withAlpha(0.0f), glass.getX() + glass.getWidth() * 0.25f, glass.getY() + glass.getHeight() * 1.2f, false);
        g.setGradientFill(sheen);
        g.fillRoundedRectangle(glass, corner - 1.0f);
    }

    void drawWell(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour ink)
    {
        drawDisplay(g, area);
        const auto font = fontOf(Face::monoBold, 11.5f);
        g.setFont(font);
        g.setColour(ink.withAlpha(0.22f));
        shapedText(g, text, area.translated(0.0f, 0.5f).expanded(0.6f, 0.0f), juce::Justification::centred, false);
        g.setColour(ink);
        shapedText(g, text, area, juce::Justification::centred, false);
    }

    void drawScreen(juce::Graphics& g, juce::Rectangle<float> area, float corner)
    {
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(area.translated(0.0f, 1.0f), corner);
        g.setColour(palette().screenEdge);
        g.fillRoundedRectangle(area, corner);
        g.setGradientFill(vertical(area, { { 0.0f, palette().screenTop }, { 1.0f, palette().screenBottom } }));
        g.fillRoundedRectangle(area.reduced(1.0f), corner - 1.0f);
        // the shadow its rim casts inside
        g.setGradientFill(vertical(area.withHeight(8.0f), { { 0.0f, juce::Colours::black.withAlpha(0.7f) }, { 1.0f, juce::Colours::black.withAlpha(0.0f) } }));
        g.fillRoundedRectangle(area.reduced(1.0f).withHeight(8.0f), corner - 1.0f);
    }

    void drawCell(juce::Graphics& g, juce::Rectangle<float> r, Cell look, float alpha)
    {
        constexpr float corner = 3.0f;
        juce::Graphics::ScopedSaveState s(g);
        if (alpha < 0.999f)
            g.beginTransparencyLayer(alpha);
        if (look == Cell::hot)
        {
            glowRounded(g, r, corner, colour::orange, 4.0f, 0.35f);
            juce::ColourGradient grad(palette().accentNote, r.getCentreX(), r.getY() + r.getHeight() * 0.3f, palette().hotDeep,
                                      r.getRight(), r.getBottom() + r.getHeight() * 0.4f, true);
            grad.addColour(0.6, colour::orange);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(r, corner);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.fillRect(r.getX() + corner, r.getY(), r.getWidth() - 2.0f * corner, 1.0f);
        }
        else
        {
            g.setColour(juce::Colours::black);
            g.fillRoundedRectangle(r.translated(0.0f, 1.0f), corner);
            const auto [a, b] = look == Cell::lit ? std::pair { palette().litTop, palette().litBottom }
                              : look == Cell::half ? std::pair { palette().halfTop, palette().halfBottom }
                                                   : std::pair { colour::cell, colour::cellOff };
            g.setGradientFill(vertical(r, { { 0.0f, a }, { 1.0f, b } }));
            g.fillRoundedRectangle(r, corner);
            g.setColour(juce::Colours::white.withAlpha(look == Cell::lit ? 0.12f : 0.05f));
            g.fillRect(r.getX() + corner, r.getY(), r.getWidth() - 2.0f * corner, 1.0f);
        }
        if (alpha < 0.999f)
            g.endTransparencyLayer();
    }
}
