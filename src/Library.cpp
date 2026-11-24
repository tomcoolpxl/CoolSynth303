#include "Library.h"

#include "AppData.h"

namespace cs303
{
    namespace
    {
        juce::File& testFolder()
        {
            static juce::File folder;
            return folder;
        }

        juce::String fileSafe(const juce::String& name)
        {
            const auto cleaned = juce::File::createLegalFileName(name.trim());
            return cleaned.isEmpty() ? juce::String("Untitled") : cleaned;
        }

        /** A free file for a name in a folder: "Name.json", else "Name 2.json" and on. */
        juce::File freeFile(const juce::File& folder, const juce::String& name)
        {
            auto file = folder.getChildFile(fileSafe(name) + ".json");
            for (int n = 2; file.exists() && n < 1000; ++n)
                file = folder.getChildFile(fileSafe(name) + " " + juce::String(n) + ".json");
            return file;
        }

        juce::var lineToVar(const LibraryLine& l)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty("name", l.name);
            o->setProperty("style", l.style);
            o->setProperty("bpm", l.bpm);
            o->setProperty("sound", l.sound);
            o->setProperty("about", l.about);
            auto pattern = toVar(l.pattern);
            pattern.getDynamicObject()->setProperty("name", l.name);
            o->setProperty("pattern", pattern);
            return juce::var(o);
        }

        LibraryLine lineFromVar(const juce::var& v)
        {
            LibraryLine l;
            l.name = v["name"].toString();
            l.style = v["style"].toString();
            l.about = v["about"].toString();
            l.sound = v["sound"].toString();
            l.bpm = v["bpm"].isVoid() ? 125.0 : juce::jlimit(40.0, 300.0, static_cast<double>(v["bpm"]));
            l.pattern = patternFromVar(v["pattern"]);
            l.pattern.name = l.name.toStdString();
            return l;
        }

        juce::var soundToVar(const LibrarySound& s)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty("name", s.name);
            o->setProperty("family", s.family);
            o->setProperty("about", s.about);
            o->setProperty("panel", s.values);
            return juce::var(o);
        }

        LibrarySound soundFromVar(const juce::var& v)
        {
            LibrarySound s;
            s.name = v["name"].toString();
            s.family = v["family"].toString();
            s.about = v["about"].toString();
            s.values = v["panel"];
            return s;
        }

        bool write(const juce::File& file, const juce::var& v)
        {
            file.getParentDirectory().createDirectory();
            return file.replaceWithText(juce::JSON::toString(v));
        }
    }

    Library libraryFromJson(const juce::String& json)
    {
        Library lib;
        const auto v = juce::JSON::parse(json);
        if (const auto* lines = v["lines"].getArray())
            for (const auto& l : *lines)
                lib.lines.push_back(lineFromVar(l));
        if (const auto* sounds = v["sounds"].getArray())
            for (const auto& s : *sounds)
                lib.sounds.push_back(soundFromVar(s));
        return lib;
    }

    Library builtInLibrary()
    {
        int size = 0;
        const char* data = AppData::getNamedResource("library303_json", size);
        if (data == nullptr || size <= 0)
            return {};
        return libraryFromJson(juce::String::fromUTF8(data, size));
    }

    juce::File userLibraryFolder()
    {
        if (testFolder() != juce::File())
            return testFolder();
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("CoolSynth303").getChildFile("Library");
    }

    void setUserLibraryFolderForTesting(const juce::File& folder) { testFolder() = folder; }

    Library userLibrary()
    {
        Library lib;
        const auto root = userLibraryFolder();
        auto byName = [](const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural(b.getFileName()) < 0; };
        auto lineFiles = root.getChildFile("Lines").findChildFiles(juce::File::findFiles, false, "*.json");
        std::sort(lineFiles.begin(), lineFiles.end(), byName);
        for (const auto& f : lineFiles)
        {
            auto l = lineFromVar(juce::JSON::parse(f));
            if (l.name.isEmpty())
                l.name = f.getFileNameWithoutExtension();
            l.style = "My lines";
            l.builtIn = false;
            l.file = f;
            lib.lines.push_back(std::move(l));
        }
        auto soundFiles = root.getChildFile("Sounds").findChildFiles(juce::File::findFiles, false, "*.json");
        std::sort(soundFiles.begin(), soundFiles.end(), byName);
        for (const auto& f : soundFiles)
        {
            auto s = soundFromVar(juce::JSON::parse(f));
            if (s.name.isEmpty())
                s.name = f.getFileNameWithoutExtension();
            s.family = "My sounds";
            s.builtIn = false;
            s.file = f;
            lib.sounds.push_back(std::move(s));
        }
        return lib;
    }

    juce::File saveUserLine(LibraryLine line)
    {
        const auto file = freeFile(userLibraryFolder().getChildFile("Lines"), line.name);
        line.name = file.getFileNameWithoutExtension();
        return write(file, lineToVar(line)) ? file : juce::File();
    }

    juce::File saveUserSound(LibrarySound sound)
    {
        const auto file = freeFile(userLibraryFolder().getChildFile("Sounds"), sound.name);
        sound.name = file.getFileNameWithoutExtension();
        return write(file, soundToVar(sound)) ? file : juce::File();
    }

    juce::File renameUserEntry(const juce::File& file, const juce::String& newName)
    {
        if (! file.existsAsFile() || newName.trim().isEmpty())
            return {};
        auto v = juce::JSON::parse(file);
        if (! v.isObject())
            return {};
        const auto target = fileSafe(newName) == file.getFileNameWithoutExtension() ? file : freeFile(file.getParentDirectory(), newName);
        v.getDynamicObject()->setProperty("name", target.getFileNameWithoutExtension());
        if (auto* pattern = v["pattern"].getDynamicObject())
            pattern->setProperty("name", target.getFileNameWithoutExtension());
        if (! write(target, v))
            return {};
        if (target != file)
            file.deleteFile();
        return target;
    }

    bool recycleUserEntry(const juce::File& file) { return file.existsAsFile() && file.moveToTrash(); }

    Panel applySound(const Panel& panel, const juce::var& values)
    {
        Project p;
        p.panel = panel;
        auto whole = toVar(p);
        if (auto* target = whole["panel"].getDynamicObject())
            if (const auto* given = values.getDynamicObject())
                for (const auto& nv : given->getProperties())
                    if (target->hasProperty(nv.name))
                        target->setProperty(nv.name, nv.value);
        return projectFromVar(whole).panel;
    }

    juce::var soundValuesOf(const Panel& panel)
    {
        Project p;
        p.panel = panel;
        auto values = toVar(p)["panel"];
        if (auto* o = values.getDynamicObject())
            for (const char* kept : { "volume", "tuning", "tm3", "tm4", "tm5", "c11" })
                o->removeProperty(kept);
        return values;
    }
}
