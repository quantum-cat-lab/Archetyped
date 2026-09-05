#pragma once

#include <string>
#include <vector>

/// Minimal readline-style editor: raw mode via termios, arrow-key history,
/// Ctrl-U / Ctrl-W editing, Home/End/Delete. Falls back to plain getline
/// when stdin is not a TTY.
class LineEditor {
public:
    /// Read one line. Returns false on EOF.
    bool read(const std::string& prompt, std::string& out);

    void addHistory(const std::string& line);
    const std::vector<std::string>& history() const { return history_; }

private:
    std::vector<std::string> history_;
};
