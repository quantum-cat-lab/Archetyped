#include "LineEditor.h"

#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

namespace {

    struct RawMode {
        termios saved{};
        bool active = false;
        bool enable() {
            if (!isatty(STDIN_FILENO)) return false;
            tcgetattr(STDIN_FILENO, &saved);
            termios raw = saved;
            raw.c_lflag &= ~(ECHO | ICANON);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            active = true;
            return true;
        }
        ~RawMode() { if (active) tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved); }
    };

    void refresh(const std::string& prompt, const std::string& buf, size_t cursor) {
        // \r + prompt + buffer, then clear to EOL; position cursor by re-printing
        fputs("\r\033[K", stdout);
        fputs(prompt.c_str(), stdout);
        fwrite(buf.data(), 1, buf.size(), stdout);
        if (cursor < buf.size())
            fprintf(stdout, "\r\033[%zuC", prompt.size() + cursor);
        fflush(stdout);
    }

} // namespace

bool LineEditor::read(const std::string& prompt, std::string& out) {
    if (!isatty(STDIN_FILENO)) {
        // Non-interactive: plain getline.
        out.clear();
        int c;
        while ((c = fgetc(stdin)) != EOF && c != '\n') out.push_back((char)c);
        return c != EOF || !out.empty();
    }

    RawMode raw;
    raw.enable();
    fputs(prompt.c_str(), stdout);
    fflush(stdout);

    std::string buf;
    size_t cursor = 0;
    int histIdx = -1; // -1 = live buffer
    std::string draft;

    auto moveCursor = [&](int delta) {
        cursor = (delta < 0)
            ? (size_t)std::max(0, (int)cursor + delta)
            : std::min(buf.size(), cursor + (size_t)delta);
    };

    char c;
    while (fread(&c, 1, 1, stdin) == 1) {
        if (c == '\n' || c == '\r') { fputc('\n', stdout); fflush(stdout); break; }

        if (c == 3) { // Ctrl-C: cancel line
            buf.clear(); cursor = 0; histIdx = -1;
            fputs("^C\n", stdout);
            fputs(prompt.c_str(), stdout);
            fflush(stdout);
            continue;
        }
        if (c == 4) { // Ctrl-D on empty line = EOF
            if (buf.empty()) { fputc('\n', stdout); return false; }
            continue;
        }
        if (c == 127 || c == 8) { // Backspace
            if (cursor > 0) { buf.erase(cursor - 1, 1); moveCursor(-1); }
        } else if (c == 21) { // Ctrl-U: clear line
            buf.clear(); cursor = 0;
        } else if (c == 23) { // Ctrl-W: delete word back
            size_t p = cursor;
            while (p > 0 && isspace((unsigned char)buf[p-1])) --p;
            while (p > 0 && !isspace((unsigned char)buf[p-1])) --p;
            buf.erase(p, cursor - p);
            cursor = p;
        } else if (c == 1)  { cursor = 0; }               // Ctrl-A
        else if (c == 5)  { cursor = buf.size(); }         // Ctrl-E
        else if (c == 27) { // escape sequence
            char seq[2];
            if (fread(seq, 1, 2, stdin) != 2) continue;
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'C': moveCursor(1); break;   // Right
                    case 'D': moveCursor(-1); break;  // Left
                    case 'H': cursor = 0; break;      // Home
                    case 'F': cursor = buf.size(); break;
                    case 'A': case 'B': {             // Up / Down — history
                        if (seq[1] == 'A') {
                            if (histIdx + 1 >= (int)history_.size()) continue;
                            if (histIdx == -1) draft = buf;
                            ++histIdx;
                        } else {
                            if (histIdx <= -1) continue;
                            --histIdx;
                        }
                        buf = histIdx == -1 ? draft : history_[histIdx];
                        cursor = buf.size();
                        break;
                    }
                    case '3': {                       // Delete
                        char tilde; fread(&tilde, 1, 1, stdin);
                        if (cursor < buf.size()) buf.erase(cursor, 1);
                        break;
                    }
                }
            }
        } else if (c >= 32) { // printable
            buf.insert(cursor, 1, c);
            ++cursor;
        } else {
            continue;
        }
        refresh(prompt, buf, cursor);
    }

    out = buf;
    if (!out.empty()) addHistory(out);
    return true;
}

void LineEditor::addHistory(const std::string& line) {
    if (line.empty()) return;
    if (!history_.empty() && history_.back() == line) return;
    history_.push_back(line);
}
