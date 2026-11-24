#pragma once

#include <vector>

#include "McpServer.h"

namespace cs303
{
    class Processor;

    /** The port the standalone app's MCP server listens on, on 127.0.0.1: CoolSynth's is 47613, this one its own. */
    inline constexpr int mcpPort = 47633;

    /**
        CoolSynth 303's tools, so Claude can drive and hear the running app (as CoolSynth's D225): read and set the
        panel (every knob and switch, the Devil Fish's and the trimmers too), run the transport, hold keys, record the
        output into a WAV file, and picture the window. `runner` runs a job on the message thread; the tools that wait
        (play, record) run on the server's thread and post their work through it.
    */
    [[nodiscard]] std::vector<coolsynth::mcp::Tool> tools303(Processor& processor, coolsynth::mcp::Server::Runner runner);
}
