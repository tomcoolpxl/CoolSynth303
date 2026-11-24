#include "ui/LidView.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "AppData.h"
#include "Processor.h"
#include "synth/v3/Tb303Lid.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace cs303::ui
{
    namespace
    {
        // the mockup's colours and scales
        const juce::Colour sheetColour { 0xff232323 }, flowColour { 0xffffa04a }, vpos { 0xff8fd694 }, vneg { 0xffe8685a };
        const juce::Colour barColour { 0xff1b1b1b }, dimColour { 0xffa49b87 }, inkColour { 0xffece4d2 }, lineColour { 0xff3a3732 };
        constexpr double currentLow = 3.0e-9, currentHigh = 6.0e-3; // the dots' log scale, as the mockup's
        constexpr float dotSpacing = 6.5f, headerHeight = 38.0f, barHeight = 34.0f, footHeight = 20.0f;
        constexpr double replaySlower = 50.0;

        juce::Colour colourOf(const juce::var& v)
        {
            if (! v.isString())
                return juce::Colours::transparentBlack;
            return juce::Colour::fromString(v.toString().removeCharacters("#"));
        }

        juce::AffineTransform matrixOf(const juce::var& m)
        {
            const auto* a = m.getArray();
            if (a == nullptr || a->size() != 6)
                return {};
            auto f = [a](int i) { return static_cast<float>(static_cast<double>(a->getReference(i))); };
            // SVG's matrix(a b c d e f): x' = a x + c y + e, y' = b x + d y + f
            return { f(0), f(2), f(4), f(1), f(3), f(5) };
        }

        float level(double amps) noexcept
        {
            const double a = std::abs(amps);
            return a <= currentLow ? 0.0f : static_cast<float>(std::min(1.0, std::log10(a / currentLow) / std::log10(currentHigh / currentLow)));
        }

        /** A strip the view paints itself: the header and the time bar. */
        class Bar final : public juce::Component
        {
        public:
            std::function<void(juce::Graphics&)> painter;
            void paint(juce::Graphics& g) override
            {
                if (painter)
                    painter(g);
            }
        };

        juce::String amps(double i)
        {
            const double a = std::abs(i);
            if (a < 1.0e-9) return "0";
            if (a < 1.0e-6) return juce::String(i * 1.0e9, 1) + " nA";
            if (a < 1.0e-3) return juce::String(i * 1.0e6, a < 1.0e-5 ? 2 : 1) + " uA";
            return juce::String(i * 1.0e3, 2) + " mA";
        }
    }

    // ---------------------------------------------------------------- the sheet, read once from its data
    struct LidView::Impl
    {
        struct Shape
        {
            juce::Path path;
            juce::Colour stroke, fill;
            float width = 1.0f;
            juce::Array<float> dash;
            int part = -1;
            bool blade = false;
            juce::Point<float> from, to; // a blade's ends in its own frame (its far end swings)
            juce::AffineTransform m;
        };
        struct Text
        {
            juce::String text, cls;
            float x = 0, y = 0, size = 10, tracking = 0;
            int anchor = 0; // 0 start, 1 middle, 2 end
            juce::Colour colour;
            juce::AffineTransform m;
        };
        struct Net
        {
            juce::String name;
            int series = -1;
            juce::Path path;
        };
        struct Flow
        {
            std::vector<juce::Point<float>> pts;
            std::vector<float> at; // the length up to each point
            std::vector<std::pair<int, float>> coef;
            juce::Rectangle<float> box;
            float g = 0.0f, offset = 0.0f, sign = 1.0f;
        };
        struct Part
        {
            juce::String el, pair, name, value, role, kind;
            juce::StringArray nodes;
            juce::Rectangle<float> box;
            int ctl = -1;
            float vt = 0.0f, s = 1.0f;
        };

        std::vector<Shape> shapes;
        std::vector<Text> texts;
        std::vector<Net> nets;
        std::vector<Flow> flows;
        std::vector<Part> parts;
        juce::StringArray seriesNames; // the drawing's
        std::map<juce::String, int> engineIndex; // a series' place in a frame
        juce::StringArray unserved;
        juce::String check;
        float sheetW = 1350.0f, sheetH = 702.0f;

        // the view: s screen pixels a sheet unit, (x, y) the sheet point at the stage's top left
        float s = 1.0f, fit = 1.0f, x = 0.0f, y = 0.0f;
        bool atFit = true;
        juce::Rectangle<int> stage;
        juce::Image cache;
        bool cacheDirty = true;
        float cacheScale = 1.0f;

        // the values: the live frame (and the afterglow), or the held seconds
        std::vector<float> now; // one frame's values
        std::int64_t lastRead = 0;
        double lastFrameTime = 0.0, lastTick = 0.0;
        bool replay = false, replayPlaying = true;
        std::vector<float> held; // the held frames
        int heldCount = 0;
        double pos = 0.0; // 0..1 of the held frames
        int selected = -1;
        juce::Point<float> dragFrom, dragView;
        bool dragged = false;

        // the header and the time bar
        std::unique_ptr<Bar> header, footer;
        std::vector<std::unique_ptr<Key>> keys;
        std::unique_ptr<juce::Slider> time;

        explicit Impl(int stride) : now(static_cast<size_t>(stride), 0.0f) {}

        void read()
        {
            int size = 0;
            const char* data = AppData::getNamedResource("tb303sheet_json", size);
            const auto json = juce::JSON::parse(juce::String::fromUTF8(data, size));
            if (const auto* sh = json["sheet"].getDynamicObject())
            {
                sheetW = static_cast<float>(static_cast<double>(sh->getProperty("w")));
                sheetH = static_cast<float>(static_cast<double>(sh->getProperty("h")));
            }
            for (int i = 0; i < coolsynth::synth::v3::Tb303Lid::count(); ++i)
                engineIndex[juce::String(coolsynth::synth::v3::Tb303Lid::name(i))] = i;
            if (const auto* a = json["series"].getArray())
                for (const auto& n : *a)
                {
                    seriesNames.add(n.toString());
                    if (engineIndex.find(n.toString()) == engineIndex.end())
                        unserved.add(n.toString());
                }
            const auto series = [this](const juce::String& name) {
                const auto it = engineIndex.find(name);
                return it == engineIndex.end() ? -1 : it->second;
            };
            check = json["check"].isString() ? json["check"].toString() : juce::JSON::toString(json["check"], true);

            if (const auto* a = json["shapes"].getArray())
                for (const auto& v : *a)
                {
                    Shape sh;
                    sh.m = matrixOf(v["m"]);
                    sh.stroke = colourOf(v["st"]);
                    sh.fill = colourOf(v["fi"]);
                    sh.width = static_cast<float>(static_cast<double>(v["sw"]));
                    sh.part = static_cast<int>(v["part"]);
                    sh.blade = static_cast<bool>(v["blade"]);
                    if (const auto* d = v["da"].getArray())
                        for (const auto& dl : *d)
                            sh.dash.add(static_cast<float>(static_cast<double>(dl)));
                    const auto local = juce::Drawable::parseSVGPath(v["d"].toString());
                    if (sh.blade)
                    {
                        juce::Path::Iterator it(local);
                        int k = 0;
                        while (it.next())
                        {
                            (k == 0 ? sh.from : sh.to) = { it.x1, it.y1 };
                            ++k;
                        }
                    }
                    sh.path = local;
                    sh.path.applyTransform(sh.m);
                    shapes.push_back(std::move(sh));
                }
            if (const auto* a = json["texts"].getArray())
                for (const auto& v : *a)
                {
                    Text t;
                    t.text = v["t"].toString();
                    t.cls = v["cls"].toString();
                    t.x = static_cast<float>(static_cast<double>(v["x"]));
                    t.y = static_cast<float>(static_cast<double>(v["y"]));
                    t.size = static_cast<float>(static_cast<double>(v["s"]));
                    t.tracking = static_cast<float>(static_cast<double>(v["ls"]));
                    const auto anchor = v["a"].toString();
                    t.anchor = anchor == "middle" ? 1 : anchor == "end" ? 2 : 0;
                    t.colour = colourOf(v["c"]);
                    t.m = matrixOf(v["m"]);
                    texts.push_back(std::move(t));
                }
            if (const auto* a = json["nets"].getArray())
                for (const auto& v : *a)
                {
                    Net n;
                    n.name = v["name"].toString();
                    n.series = series(n.name);
                    if (const auto* segs = v["segs"].getArray())
                        for (const auto& sg : *segs)
                            if (const auto* q = sg.getArray(); q != nullptr && q->size() == 4)
                            {
                                auto f = [q](int i) { return static_cast<float>(static_cast<double>(q->getReference(i))); };
                                n.path.startNewSubPath(f(0), f(1));
                                n.path.lineTo(f(2), f(3));
                            }
                    nets.push_back(std::move(n));
                }
            if (const auto* a = json["flows"].getArray())
                for (const auto& v : *a)
                {
                    Flow f;
                    if (const auto* pts = v["pts"].getArray())
                        for (const auto& p : *pts)
                            f.pts.emplace_back(static_cast<float>(static_cast<double>(p[0])), static_cast<float>(static_cast<double>(p[1])));
                    if (const auto* cs = v["coef"].getArray())
                        for (const auto& c : *cs)
                            if (const int i = series(c[0].toString()); i >= 0)
                                f.coef.emplace_back(i, static_cast<float>(static_cast<double>(c[1])));
                    if (f.pts.size() < 2 || f.coef.empty())
                        continue;
                    float len = 0.0f;
                    f.at.push_back(0.0f);
                    for (size_t i = 1; i < f.pts.size(); ++i)
                        f.at.push_back(len += f.pts[i - 1].getDistanceFrom(f.pts[i]));
                    f.box = juce::Rectangle<float>::findAreaContainingPoints(f.pts.data(), static_cast<int>(f.pts.size())).expanded(3.0f);
                    f.offset = juce::Random::getSystemRandom().nextFloat() * dotSpacing;
                    flows.push_back(std::move(f));
                }
            if (const auto* a = json["parts"].getArray())
                for (const auto& v : *a)
                {
                    Part p;
                    p.el = v["el"].toString();
                    p.pair = v["pair"].isString() ? v["pair"].toString() : juce::String();
                    p.name = v["name"].toString();
                    p.value = v["value"].toString();
                    p.role = v["role"].toString();
                    p.kind = v["kind"].toString();
                    if (const auto* ns = v["nodes"].getArray())
                        for (const auto& n : *ns)
                            p.nodes.add(n.toString());
                    if (const auto* b = v["box"].getArray(); b != nullptr && b->size() == 4)
                    {
                        auto f = [b](int i) { return static_cast<float>(static_cast<double>(b->getReference(i))); };
                        p.box = { f(0), f(1), f(2) - f(0), f(3) - f(1) };
                    }
                    if (v["sw"].isObject())
                    {
                        p.ctl = series(v["sw"]["ctl"].toString());
                        p.vt = static_cast<float>(static_cast<double>(v["sw"]["vt"]));
                        p.s = static_cast<float>(static_cast<double>(v["sw"]["s"]));
                    }
                    parts.push_back(std::move(p));
                }
        }

        [[nodiscard]] float value(int series) const noexcept
        {
            return series >= 0 && series < static_cast<int>(now.size()) ? now[static_cast<size_t>(series)] : 0.0f;
        }
        [[nodiscard]] float valueOf(const juce::String& name) const
        {
            const auto it = engineIndex.find(name);
            return it == engineIndex.end() ? 0.0f : value(it->second);
        }
        [[nodiscard]] static double current(const Flow& f, const float* frame) noexcept
        {
            double i = 0.0;
            for (const auto& [k, c] : f.coef)
                i += static_cast<double>(c) * frame[k];
            return i;
        }

        juce::AffineTransform view() const noexcept
        {
            return juce::AffineTransform::translation(-x, -y).scaled(s).translated(static_cast<float>(stage.getX()), static_cast<float>(stage.getY()));
        }

        void fitNow()
        {
            if (stage.isEmpty())
                return;
            fit = std::min((stage.getWidth() - 12.0f) / sheetW, (stage.getHeight() - 8.0f) / sheetH);
            s = fit;
            x = -(stage.getWidth() / s - sheetW) * 0.5f;
            y = -(stage.getHeight() / s - sheetH) * 0.5f;
            atFit = true;
            cacheDirty = true;
        }

        void zoomAt(float factor, juce::Point<float> at)
        {
            const float ns = juce::jlimit(fit * 0.8f, fit * 8.0f, s * factor);
            const auto p = at - stage.getPosition().toFloat();
            x += p.x / s - p.x / ns;
            y += p.y / s - p.y / ns;
            s = ns;
            atFit = false;
            cacheDirty = true;
        }

        /** The sheet's still parts (symbols, wires, names) drawn once for this view, at the screen's own pixels. */
        void drawCache(float physical)
        {
            const int w = std::max(1, juce::roundToInt(stage.getWidth() * physical)), h = std::max(1, juce::roundToInt(stage.getHeight() * physical));
            cache = juce::Image(juce::Image::ARGB, w, h, true);
            juce::Graphics g(cache);
            g.addTransform(juce::AffineTransform::translation(-x, -y).scaled(s * physical));
            const bool values = s >= fit * 1.25f;
            for (const auto& sh : shapes)
            {
                if (sh.blade)
                    continue;
                if (! sh.fill.isTransparent())
                {
                    g.setColour(sh.fill);
                    g.fillPath(sh.path);
                }
                if (! sh.stroke.isTransparent())
                {
                    g.setColour(sh.stroke);
                    const juce::PathStrokeType type(sh.width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
                    if (sh.dash.isEmpty())
                        g.strokePath(sh.path, type);
                    else
                    {
                        juce::Path dashed;
                        type.createDashedStroke(dashed, sh.path, sh.dash.getRawDataPointer(), sh.dash.size());
                        g.fillPath(dashed);
                    }
                }
            }
            for (const auto& t : texts)
            {
                if (t.cls == "vl" && ! values)
                    continue;
                juce::Graphics::ScopedSaveState state(g);
                g.addTransform(t.m);
                g.setColour(t.colour);
                g.setFont(fontOf(Face::stencil, t.size, t.size > 0.0f ? t.tracking / t.size : 0.0f));
                const auto just = t.anchor == 1 ? juce::Justification::horizontallyCentred : t.anchor == 2 ? juce::Justification::right : juce::Justification::left;
                g.drawSingleLineText(t.text, juce::roundToInt(t.x), juce::roundToInt(t.y), just);
            }
            cacheScale = physical;
            cacheDirty = false;
        }

        /** Takes the frames written since the last tick: the newest is the one shown, each wire's dots keep the
            strongest current any of them carried (the afterglow) and move with the newest. */
        void takeLive(const Processor& p, double dt, double nowSeconds)
        {
            const std::int64_t written = p.lidWritten();
            const std::int64_t first = std::max(lastRead, written - Processor::lidFrames + 64);
            const float glow = static_cast<float>(std::exp(-dt / 0.06));
            for (auto& f : flows)
                f.g *= glow;
            if (written > first)
            {
                const int stride = Processor::lidStride();
                for (std::int64_t i = first; i < written; ++i)
                {
                    const float* frame = p.lidFrame(i);
                    for (auto& f : flows)
                        f.g = std::max(f.g, level(current(f, frame)));
                }
                const float* last = p.lidFrame(written - 1);
                std::copy(last, last + stride, now.begin());
                for (auto& f : flows)
                {
                    const double i = current(f, last);
                    f.sign = i >= 0.0 ? 1.0f : -1.0f;
                }
                lastFrameTime = nowSeconds;
            }
            lastRead = written;
            move(dt);
        }

        /** The held seconds at pos: each value between its two frames. */
        void takeReplay(double dt)
        {
            if (heldCount < 2)
                return;
            if (replayPlaying)
            {
                pos += dt / (heldCount / static_cast<double>(Processor::lidRate)) / replaySlower;
                if (pos >= 1.0)
                {
                    pos = 1.0;
                    replayPlaying = false;
                }
            }
            const int stride = Processor::lidStride();
            const double at = pos * (heldCount - 1);
            const int i = std::min(heldCount - 2, static_cast<int>(at));
            const float fr = static_cast<float>(at - i);
            const float* a = held.data() + static_cast<size_t>(i) * static_cast<size_t>(stride);
            const float* b = a + stride;
            for (int k = 0; k < stride; ++k)
                now[static_cast<size_t>(k)] = a[k] + (b[k] - a[k]) * fr;
            for (auto& f : flows)
            {
                const double c = current(f, now.data());
                f.g = level(c);
                f.sign = c >= 0.0 ? 1.0f : -1.0f;
            }
            move(dt);
        }

        void move(double dt)
        {
            for (auto& f : flows)
                if (f.g >= 0.03f)
                    f.offset -= f.sign * (6.0f + 64.0f * f.g) * static_cast<float>(dt);
        }

        void hold(const Processor& p)
        {
            const int stride = Processor::lidStride();
            const std::int64_t written = p.lidWritten();
            heldCount = static_cast<int>(std::min<std::int64_t>(written, Processor::lidFrames - 64));
            held.assign(static_cast<size_t>(heldCount) * static_cast<size_t>(stride), 0.0f);
            for (int i = 0; i < heldCount; ++i)
            {
                const float* f = p.lidFrame(written - heldCount + i);
                std::copy(f, f + stride, held.begin() + static_cast<std::ptrdiff_t>(i) * stride);
            }
            pos = 0.0;
            replayPlaying = true;
        }

        [[nodiscard]] juce::String caption(double nowSeconds) const
        {
            const int s0 = Processor::lidSeries();
            const bool gate = value(s0) > 0.5f, accent = value(s0 + 1) > 0.5f, slide = value(s0 + 2) > 0.5f;
            const double volts = value(s0 + 3);
            juce::String what;
            if (! replay && nowSeconds - lastFrameTime > 0.3)
                what = "at rest: stopped and silent, the circuit is not solved until the next note";
            else if (gate)
                what = juce::String("a note, ") + juce::String(volts, 2) + " V on the CV" + (accent ? ", accented: the accent sweep opens the filter" : "")
                       + (slide ? ", sliding: C35 takes the CV slowly" : "");
            else
                what = "between notes: the gate is shut, the envelopes fall back";
            if (replay)
            {
                const double ms = pos * heldCount * 1000.0 / Processor::lidRate;
                return juce::String(ms, 0) + " ms of " + juce::String(heldCount * 1000.0 / Processor::lidRate, 0) + " ms  " + what;
            }
            return "Live  " + what;
        }
    };

    // ---------------------------------------------------------------- the view
    LidView::LidView(Processor& p) : impl(std::make_unique<Impl>(Processor::lidStride())), processor(p)
    {
        impl->read();
        setWantsKeyboardFocus(true);
        auto& c = *impl;
        c.header = std::make_unique<Bar>();
        c.footer = std::make_unique<Bar>();
        addAndMakeVisible(*c.header);
        addAndMakeVisible(*c.footer);
        auto key = [&c](Bar& bar, const juce::String& text) -> Key& {
            c.keys.push_back(std::make_unique<Key>(text, false));
            bar.addAndMakeVisible(*c.keys.back());
            return *c.keys.back();
        };
        auto& back = key(*c.header, "Back to the panel");
        back.onClick = [this] { if (onClose) onClose(); };
        auto& play = key(*c.header, "Play");
        play.textOf = [this] { return processor.isPlaying() ? juce::String("Stop") : juce::String("Play"); };
        play.isOn = [this] { return processor.isPlaying(); };
        play.onClick = [this] { processor.setPlaying(! processor.isPlaying()); };
        auto& live = key(*c.header, "Live");
        live.isOn = [this] { return ! impl->replay; };
        live.onClick = [this] { setReplay(false); };
        auto& slow = key(*c.header, "Slow replay");
        slow.isOn = [this] { return impl->replay; };
        slow.onClick = [this] { setReplay(true); };
        auto& fitKey = key(*c.header, "Fit");
        fitKey.onClick = [this] { impl->fitNow(); repaint(); };
        auto& pause = key(*c.footer, "Pause");
        pause.textOf = [this] { return impl->replay ? (impl->replayPlaying ? juce::String("Pause") : juce::String("Play slowly")) : juce::String("Hold"); };
        pause.onClick = [this] {
            if (! impl->replay)
                setReplay(true);
            else
            {
                if (! impl->replayPlaying && impl->pos >= 1.0)
                    impl->pos = 0.0;
                impl->replayPlaying = ! impl->replayPlaying;
            }
        };
        c.time = std::make_unique<juce::Slider>(juce::Slider::LinearBar, juce::Slider::NoTextBox);
        c.time->setRange(0.0, 1.0);
        c.time->setColour(juce::Slider::trackColourId, colour::orange.withAlpha(0.8f));
        c.time->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff34332f));
        c.time->onValueChange = [this, &c] {
            if (! impl->replay)
                setReplay(true);
            impl->pos = c.time->getValue();
            impl->replayPlaying = false;
        };
        c.footer->addChildComponent(*c.time);

        c.header->painter = [](juce::Graphics& g) {
            g.fillAll(barColour);
            g.setColour(inkColour);
            g.setFont(fontOf(Face::labelBold, 15.0f, 0.12f));
            g.drawText("UNDER THE LID", 14, 0, 160, static_cast<int>(headerHeight), juce::Justification::centredLeft, false);
            g.setColour(dimColour);
            g.setFont(fontOf(Face::stencil, 13.0f));
            g.drawText("the TB-303's voice, every part, solved live", 176, 0, 300, static_cast<int>(headerHeight), juce::Justification::centredLeft, false);
        };
        c.footer->painter = [this](juce::Graphics& g) {
            g.fillAll(barColour);
            g.setColour(inkColour);
            g.setFont(fontOf(Face::stencil, 13.5f));
            const auto& ch = *impl;
            const int left = (ch.time->isVisible() ? ch.time->getRight() : ch.keys[5]->getRight()) + 14;
            g.drawText(impl->caption(impl->lastTick), left, 0, ch.header->getWidth() - left - 12,
                       static_cast<int>(barHeight), juce::Justification::centredLeft, true);
            g.setColour(dimColour);
            g.setFont(fontOf(Face::stencil, 11.5f));
            const juce::String fish = processor.isDevilFishPlaying() ? " The Devil Fish plays: its own extra parts are not drawn." : "";
            g.drawText("Dot size and speed follow the logarithm of the current, from 3 nA to 6 mA; the tint, a wire's voltage, green above 0 V. "
                           "The sheet was checked against the netlist: " + impl->check + "." + fish,
                       12, static_cast<int>(barHeight), ch.header->getWidth() - 24, static_cast<int>(footHeight), juce::Justification::centredLeft, true);
        };
    }

    LidView::~LidView()
    {
        setOpen(false);
    }

    void LidView::setOpen(bool shouldOpen)
    {
        if (open == shouldOpen)
            return;
        open = shouldOpen;
        processor.setLidOpen(open);
        if (open)
        {
            impl->lastRead = processor.lidWritten();
            impl->lastTick = juce::Time::getMillisecondCounterHiRes() / 1000.0;
            startTimerHz(30);
            grabKeyboardFocus();
        }
        else
            stopTimer();
    }

    void LidView::resized()
    {
        auto& c = *impl;
        // the header and the time bar at the panel's scale, the sheet at the screen's own pixels
        const float u = std::min(getWidth() / 1280.0f, getHeight() / 800.0f);
        const int w = juce::roundToInt(getWidth() / u);
        c.header->setBounds(0, 0, w, static_cast<int>(headerHeight));
        c.header->setTransform(juce::AffineTransform::scale(u));
        const int footH = static_cast<int>(barHeight + footHeight);
        c.footer->setBounds(0, 0, w, footH);
        c.footer->setTransform(juce::AffineTransform::scale(u).translated(0.0f, getHeight() - footH * u));
        int r = w - 14;
        const int ky = 6, kh = 26;
        c.keys[0]->setBounds(r - 150, ky, 150, kh); // Back to the panel
        r -= 166;
        c.keys[4]->setBounds(r - 50, ky, 50, kh);   // Fit
        r -= 62;
        c.keys[3]->setBounds(r - 110, ky, 110, kh); // Slow replay
        r -= 112;
        c.keys[2]->setBounds(r - 60, ky, 60, kh);   // Live
        r -= 76;
        c.keys[1]->setBounds(r - 64, ky, 64, kh);   // Play
        c.keys[5]->setBounds(14, 4, 110, kh);       // Hold / Pause / Play slowly
        c.time->setBounds(136, 9, juce::roundToInt(w * 0.42f), 16);
        impl->stage = { 0, juce::roundToInt(headerHeight * u), getWidth(), getHeight() - juce::roundToInt((headerHeight + footH) * u) };
        impl->fitNow();
    }

    void LidView::paint(juce::Graphics& g)
    {
        auto& im = *impl;
        g.fillAll(sheetColour);
        if (im.stage.isEmpty())
            return;
        const float physical = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (im.cacheDirty || im.cache.isNull() || std::abs(physical - im.cacheScale) > 0.01f)
            im.drawCache(physical);
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(im.stage);
        const auto view = im.view();
        // the voltage's tint under the wires
        for (const auto& n : im.nets)
        {
            if (n.series < 0)
                continue;
            const float v = im.value(n.series), m = std::min(1.0f, std::sqrt(std::abs(v) / 12.0f));
            if (m < 0.06f)
                continue;
            g.setColour((v >= 0.0f ? vpos : vneg).withAlpha(m * 0.42f));
            g.strokePath(n.path, juce::PathStrokeType(3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), view);
        }
        g.setOpacity(1.0f); // an image takes the last colour's opacity: the tints' would dim the whole sheet
        g.drawImage(im.cache, im.stage.toFloat());
        // the switches' blades, open or shut by their control lines; the part picked in the current's colour
        for (const auto& sh : im.shapes)
        {
            const bool picked = sh.part >= 0 && sh.part == im.selected;
            if (! sh.blade && ! picked)
                continue;
            juce::Path p;
            if (sh.blade)
            {
                const auto& part = im.parts[static_cast<size_t>(sh.part)];
                const bool on = part.ctl >= 0 && im.value(part.ctl) > part.vt;
                p.startNewSubPath(sh.from);
                p.lineTo(sh.to.x, on ? 0.0f : -5.0f * part.s);
                p.applyTransform(sh.m);
            }
            else
                p = sh.path;
            g.setColour(picked ? flowColour : sh.stroke);
            g.strokePath(p, juce::PathStrokeType(sh.width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), view);
        }
        // the current: dots along each wire, bigger and faster for more
        const juce::Rectangle<float> seen(im.x, im.y, im.stage.getWidth() / im.s, im.stage.getHeight() / im.s);
        for (const auto& f : im.flows)
        {
            if (f.g < 0.03f || ! seen.intersects(f.box))
                continue;
            const float d = (0.7f + 2.6f * f.g) * im.s, total = f.at.back();
            g.setColour(flowColour.withAlpha(0.35f + 0.65f * f.g));
            float along = std::fmod(f.offset, dotSpacing);
            if (along < 0.0f)
                along += dotSpacing;
            size_t seg = 1;
            for (; along <= total; along += dotSpacing)
            {
                while (seg + 1 < f.at.size() && f.at[seg] < along)
                    ++seg;
                const float a0 = f.at[seg - 1], a1 = f.at[seg], t = a1 > a0 ? (along - a0) / (a1 - a0) : 0.0f;
                const auto pt = (f.pts[seg - 1] + (f.pts[seg] - f.pts[seg - 1]) * t).transformedBy(view);
                g.fillEllipse(pt.x - d * 0.5f, pt.y - d * 0.5f, d, d);
            }
        }
        // the part picked: its name, value, role and what it does now
        if (im.selected >= 0)
        {
            const auto& p = im.parts[static_cast<size_t>(im.selected)];
            juce::String nowText;
            if (p.kind == "q")
                nowText = "IC " + amps(im.valueOf(p.el + ".ic")) + "  IB " + amps(im.valueOf(p.el + ".ib"));
            else if (p.kind == "j" || p.kind == "d")
                nowText = "ID " + amps(im.valueOf(p.el + ".id"));
            else
            {
                nowText = amps(im.valueOf(p.el + ".i"));
                if (p.pair.isNotEmpty())
                    nowText << "  " << amps(im.valueOf(p.pair + ".i"));
            }
            juce::StringArray volts;
            for (const auto& n : p.nodes)
                if (n != "0" && volts.size() < 4 && im.engineIndex.count(n) > 0)
                    volts.add(juce::String(im.valueOf(n), 2) + " V");
            const float u = std::min(getWidth() / 1280.0f, getHeight() / 800.0f);
            const auto box = juce::Rectangle<float>(14.0f * u, im.stage.getBottom() - 92.0f * u, 430.0f * u, 80.0f * u);
            g.setColour(barColour.withAlpha(0.94f));
            g.fillRoundedRectangle(box, 8.0f * u);
            g.setColour(lineColour);
            g.drawRoundedRectangle(box, 8.0f * u, 1.0f);
            auto r = box.reduced(12.0f * u, 9.0f * u);
            g.setColour(inkColour);
            g.setFont(fontOf(Face::labelBold, 15.0f * u));
            g.drawText(p.name, r.removeFromTop(20.0f * u), juce::Justification::centredLeft, false);
            g.setColour(flowColour);
            g.drawText(p.value, r.withTrimmedTop(-20.0f * u).withHeight(20.0f * u).withTrimmedLeft(90.0f * u), juce::Justification::centredLeft, false);
            g.setColour(dimColour);
            g.setFont(fontOf(Face::stencil, 13.0f * u));
            g.drawText(p.role, r.removeFromTop(19.0f * u), juce::Justification::centredLeft, true);
            g.setColour(inkColour);
            g.drawText("Now: " + nowText + (volts.isEmpty() ? juce::String() : "  at its leads " + volts.joinIntoString(" ")), r.removeFromTop(19.0f * u),
                       juce::Justification::centredLeft, true);
        }
        // the zoom, top right of the sheet
        g.setColour(dimColour);
        g.setFont(fontOf(Face::mono, 12.0f));
        g.drawText(juce::String(im.s / im.fit, 1) + "x", im.stage.getRight() - 70, im.stage.getY() + 6, 60, 16, juce::Justification::centredRight, false);
    }

    void LidView::tickNow(double seconds)
    {
        auto& im = *impl;
        const double dt = std::clamp(seconds - im.lastTick, 0.0, 0.1);
        im.lastTick = seconds;
        if (im.replay)
            im.takeReplay(dt);
        else
            im.takeLive(processor, dt, seconds);
        auto& c = *impl;
        if (im.replay)
            c.time->setValue(im.pos, juce::dontSendNotification);
        c.footer->repaint();
        c.header->repaint();
        for (auto& k : c.keys)
            k->repaint();
        repaint(im.stage);
    }

    void LidView::timerCallback() { tickNow(juce::Time::getMillisecondCounterHiRes() / 1000.0); }

    void LidView::setReplay(bool replay)
    {
        auto& im = *impl;
        if (replay == im.replay)
            return;
        im.replay = replay;
        if (replay)
            im.hold(processor);
        else
            im.lastRead = processor.lidWritten() - 1;
        im.time->setVisible(replay); // the time bar walks the held seconds; live has none
        impl->time->setValue(im.pos, juce::dontSendNotification);
        repaint();
    }

    void LidView::mouseDown(const juce::MouseEvent& e)
    {
        impl->dragFrom = e.position;
        impl->dragView = { impl->x, impl->y };
        impl->dragged = false;
    }

    void LidView::mouseDrag(const juce::MouseEvent& e)
    {
        auto& im = *impl;
        const auto d = e.position - im.dragFrom;
        if (! im.dragged && d.getDistanceFromOrigin() < 3.0f)
            return;
        im.dragged = true;
        im.x = im.dragView.x - d.x / im.s;
        im.y = im.dragView.y - d.y / im.s;
        im.atFit = false;
        im.cacheDirty = true;
        repaint(im.stage);
    }

    void LidView::mouseUp(const juce::MouseEvent& e)
    {
        auto& im = *impl;
        if (im.dragged || ! im.stage.contains(e.position.toInt()))
            return;
        // the smallest part under the pointer
        const auto at = e.position.transformedBy(im.view().inverted());
        int best = -1;
        float bestArea = 1.0e9f;
        for (size_t i = 0; i < im.parts.size(); ++i)
        {
            const auto b = im.parts[i].box.expanded(2.0f);
            if (b.contains(at) && b.getWidth() * b.getHeight() < bestArea)
            {
                best = static_cast<int>(i);
                bestArea = b.getWidth() * b.getHeight();
            }
        }
        im.selected = best;
        repaint(im.stage);
    }

    void LidView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        impl->zoomAt(std::exp(w.deltaY * 1.2f), e.position);
        repaint(impl->stage);
    }

    bool LidView::keyPressed(const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose)
                onClose();
            return true;
        }
        if (key == juce::KeyPress::spaceKey)
        {
            processor.setPlaying(! processor.isPlaying());
            return true;
        }
        return false;
    }

    int LidView::partCount() const noexcept { return static_cast<int>(impl->parts.size()); }
    int LidView::flowCount() const noexcept { return static_cast<int>(impl->flows.size()); }
    juce::String LidView::drawingCheck() const { return impl->check; }
    juce::StringArray LidView::unservedSeries() const { return impl->unserved; }
    int LidView::flowsLit() const noexcept
    {
        return static_cast<int>(std::count_if(impl->flows.begin(), impl->flows.end(), [](const auto& f) { return f.g >= 0.03f; }));
    }
    void LidView::select(const juce::String& element)
    {
        for (size_t i = 0; i < impl->parts.size(); ++i)
            if (impl->parts[i].el == element)
                impl->selected = static_cast<int>(i);
        repaint();
    }
}
