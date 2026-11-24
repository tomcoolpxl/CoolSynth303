// CoolSynth 303's test program: the patterns, the sequencer, the processor playing the circuit, the panel.

#include <cstdio>
#include <iostream>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#if JUCE_WINDOWS
 #include <crtdbg.h>
 #include <cstdlib>
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace
{
    class StdoutLogger : public juce::Logger
    {
    protected:
        void logMessage(const juce::String& message) override { std::cout << message.toRawUTF8() << std::endl; }
    };

    /** A failed check or a crash prints and exits instead of opening a dialog on the user's screen. */
    void keepFailuresOffTheScreen()
    {
       #if JUCE_WINDOWS
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        for (const int kind : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
        {
            _CrtSetReportMode(kind, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(kind, _CRTDBG_FILE_STDERR);
        }
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _set_invalid_parameter_handler([](const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t)
        {
            std::fputs("\nINVALID PARAMETER (an index out of range?)\n", stderr);
            std::fflush(stderr);
            std::_Exit(3);
        });
       #endif
    }
}

int main()
{
    keepFailuresOffTheScreen();
    StdoutLogger logger;
    juce::Logger::setCurrentLogger(&logger);
    juce::ScopedJuceInitialiser_GUI initialiser;
    juce::SystemStats::setApplicationCrashHandler([](void*)
    {
        std::fputs(("\nCRASH\n" + juce::SystemStats::getStackBacktrace() + "\n").toRawUTF8(), stderr);
        std::fflush(stderr);
    });
    juce::UnitTestRunner runner;
    // COOLSYNTH_TEST_ONLY=part of a test's name runs only the tests whose names contain it.
    if (const auto only = juce::SystemStats::getEnvironmentVariable("COOLSYNTH_TEST_ONLY", {}); only.isNotEmpty())
    {
        juce::Array<juce::UnitTest*> chosen;
        for (auto* test : juce::UnitTest::getAllTests())
            if (test->getName().containsIgnoreCase(only))
                chosen.add(test);
        runner.runTests(chosen);
    }
    else
        runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* result = runner.getResult(i))
            failures += result->failures;
    juce::Logger::setCurrentLogger(nullptr);
    return failures == 0 ? 0 : 1;
}
