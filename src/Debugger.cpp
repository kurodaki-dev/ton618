#include "Debugger.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

void Debugger::printSourceContext(int line, int contextSize) const {
    int from = std::max(1, line - contextSize);
    int to = std::min((int)sourceLines.size(), line + contextSize);
    for (int i = from; i <= to; i++) {
        std::string marker = (i == line) ? " -> " : "    ";
        std::string bp = hasBreakpoint(i) ? "*" : " ";
        std::cout << bp << marker << i << " | " << sourceLines[i - 1] << "\n";
    }
}

void Debugger::pauseAndPrompt(int currentLine, std::shared_ptr<Environment> currentEnv) {
    std::cout << "\n--- Paused at line " << currentLine << " ---\n";
    printSourceContext(currentLine);

    while (true) {
        std::cout << "(ton618-dbg) ";
        std::string line;
        if (!std::getline(std::cin, line)) { enabled = false; return; }

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "s" || cmd == "step") { mode = DebugMode::STEP; return; }
        else if (cmd == "c" || cmd == "continue") { mode = DebugMode::RUN; return; }
        else if (cmd == "b" || cmd == "break") {
            int ln;
            if (iss >> ln) { addBreakpoint(ln); std::cout << "Breakpoint added at line " << ln << "\n"; }
            else std::cout << "Usage: break <line>\n";
        } else if (cmd == "rb" || cmd == "removebreak") {
            int ln;
            if (iss >> ln) { removeBreakpoint(ln); std::cout << "Breakpoint removed from line " << ln << "\n"; }
        } else if (cmd == "p" || cmd == "print") {
            std::string varName; iss >> varName;
            Value v;
            if (currentEnv->get(varName, v)) std::cout << varName << " = " << v.toString() << " (" << v.typeName() << ")\n";
            else std::cout << "Variable '" << varName << "' not found.\n";
        } else if (cmd == "vars" || cmd == "locals") {
            std::cout << "Local variables:\n";
            for (auto& [name, val] : currentEnv->values) std::cout << "  " << name << " = " << val.toString() << "\n";
        } else if (cmd == "bt" || cmd == "backtrace" || cmd == "stack") {
            std::cout << "Call stack:\n";
            for (auto it = callStack.rbegin(); it != callStack.rend(); ++it)
                std::cout << "  " << it->functionName << "() called from line " << it->callLine << "\n";
            if (callStack.empty()) std::cout << "  (global scope)\n";
        } else if (cmd == "l" || cmd == "list") {
            printSourceContext(currentLine, 5);
        } else if (cmd == "h" || cmd == "help" || cmd == "?") {
            std::cout <<
                "TON618 debugger commands:\n"
                "  s, step        run the next line (steps into function calls)\n"
                "  c, continue    continue until the next breakpoint\n"
                "  b <line>       add a breakpoint\n"
                "  rb <line>      remove a breakpoint\n"
                "  p <var>        print a variable's value\n"
                "  vars           print all local variables\n"
                "  bt             print the call stack\n"
                "  l              show the source around the current line\n"
                "  q              quit the program\n";
        } else if (cmd == "q" || cmd == "quit" || cmd == "exit") {
            std::cout << "Stopping the program.\n";
            std::exit(0);
        } else if (cmd.empty()) {
            continue;
        } else {
            std::cout << "Unknown command. Type 'h' for help.\n";
        }
    }
}
