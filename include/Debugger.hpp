#pragma once
#include <set>
#include <vector>
#include <string>
#include <memory>
#include "Environment.hpp"

// ============================================================================
// Debugger.hpp — an optional, interactive step-debugger for .ton scripts,
// activated with `--debug` or `--break=<line>` (see src/main.cpp).
//
// The Interpreter checks `shouldPause()` before executing every statement
// (see Interpreter::execute in src/Interpreter.cpp) and, if it should, hands
// control to `pauseAndPrompt()` which reads debugger commands from stdin
// (step, continue, breakpoints, inspecting variables, the call stack...).
// It has no effect at all on a script run without those flags.
// ============================================================================

struct CallFrame {
    std::string functionName;
    int callLine;
    std::shared_ptr<Environment> env;
};

enum class DebugMode { RUN, STEP };

class Debugger {
public:
    bool enabled = false;
    DebugMode mode = DebugMode::RUN;
    std::set<int> breakpoints;
    std::vector<CallFrame> callStack;
    std::string sourceFilename;
    std::vector<std::string> sourceLines;

    void addBreakpoint(int line) { breakpoints.insert(line); }
    void removeBreakpoint(int line) { breakpoints.erase(line); }
    bool hasBreakpoint(int line) const { return breakpoints.count(line) > 0; }

    bool shouldPause(int line) const {
        if (!enabled) return false;
        if (mode == DebugMode::STEP) return true;
        if (hasBreakpoint(line)) return true;
        return false;
    }

    void pauseAndPrompt(int currentLine, std::shared_ptr<Environment> currentEnv);
    void printSourceContext(int line, int contextSize = 2) const;
};
