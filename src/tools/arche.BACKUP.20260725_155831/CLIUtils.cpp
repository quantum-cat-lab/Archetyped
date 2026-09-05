#include "CLIUtils.h"

namespace cli {

    static const char* RESET   = "\033[0m";
    static const char* RED     = "\033[31m";
    static const char* GREEN   = "\033[32m";
    static const char* YELLOW  = "\033[33m";
    static const char* BLUE    = "\033[34m";
    static const char* MAGENTA = "\033[35m";
    static const char* CYAN    = "\033[36m";
    static const char* GRAY    = "\033[90m";
    static const char* BOLD    = "\033[1m";

    std::string red(const std::string& text)    { return RED + text + RESET; }
    std::string green(const std::string& text)  { return GREEN + text + RESET; }
    std::string yellow(const std::string& text) { return YELLOW + text + RESET; }
    std::string blue(const std::string& text)   { return BLUE + text + RESET; }
    std::string magenta(const std::string& text){ return MAGENTA + text + RESET; }
    std::string cyan(const std::string& text)   { return CYAN + text + RESET; }
    std::string gray(const std::string& text)   { return GRAY + text + RESET; }
    std::string bold(const std::string& text)   { return BOLD + text + RESET; }

    void printSuccess(const std::string& msg) { std::cout << green("  ✅ ") << msg << std::endl; }
    void printError(const std::string& msg)   { std::cerr << red("  ❌ Error: ") << msg << std::endl; }
    void printWarning(const std::string& msg) { std::cout << yellow("  ⚠️   ") << msg << std::endl; }
    void printInfo(const std::string& msg)    { std::cout << blue("  ℹ️   ") << msg << std::endl; }
    void printHeader(const std::string& msg)  { std::cout << bold(cyan(msg)) << std::endl; }

    // --- Icons ---
    std::string iconOk()        { return green("✔"); }
    std::string iconFail()      { return red("✘"); }
    std::string iconWarn()      { return yellow("⚠"); }
    std::string iconInfo()      { return blue("ℹ"); }
    std::string iconArrow()     { return cyan("➜"); }
    std::string iconBullet()    { return gray("•"); }
    std::string iconStar()      { return yellow("★"); }
    std::string iconGear()      { return cyan("⚙"); }
    std::string iconPackage()   { return magenta("📁"); }
    std::string iconBox()       { return yellow("📋"); }
    std::string iconSDK()       { return blue("🔗"); }
    std::string iconBuild()     { return cyan("🔨"); }
    std::string iconDoctor()    { return green("🏥"); }
    std::string iconModule()    { return magenta("🧩"); }
    std::string iconContainer() { return yellow("📦"); }

    // --- Section header with icon ---
    void printSection(const std::string& icon, const std::string& title) {
        std::cout << "\n  " << icon << "  " << bold(cyan(title)) << "\n";
        std::cout << "  " << gray("────────────────────────────────────────────") << "\n";
    }

    // --- Horizontal rule ---
    void printRule() {
        std::cout << "  " << gray("────────────────────────────────────────────") << "\n";
    }

    // --- Progress meter ---
    void printMeter(int current, int total, const std::string& label) {
        float pct = total > 0 ? (float)current / total : 0;
        int bars = (int)(pct * 20);
        std::string bar;
        bar.reserve(22);
        bar += "[";
        for (int i = 0; i < 20; ++i) {
            if (i < bars) bar += green("■");
            else bar += gray("·");
        }
        bar += "]";
        char pctStr[8];
        snprintf(pctStr, sizeof(pctStr), " %3d%%", (int)(pct * 100));
        std::string out = "  " + bar + cyan(pctStr);
        if (!label.empty()) out += "  " + gray(label);
        std::cout << out << "\n";
    }

    // --- Box with border ---
    void printBox(const std::vector<std::string>& lines) {
        if (lines.empty()) return;
        size_t maxLen = 0;
        for (auto& l : lines)
            if (l.size() > maxLen) maxLen = l.size();
        std::string hor(maxLen + 2, '-');
        std::cout << cyan("  .-") << gray(hor) << cyan("-.") << "\n";
        for (auto& l : lines) {
            std::cout << cyan("  | ") << l;
            if (l.size() < maxLen) std::cout << std::string(maxLen - l.size(), ' ');
            std::cout << cyan(" |") << "\n";
        }
        std::cout << cyan("  '-") << gray(hor) << cyan("-'") << "\n";
    }

    // --- Key-Value with optional status badge ---
    void printKV(const std::string& key, const std::string& value, const std::string& status) {
        std::cout << "    " << gray(key + ":") << " " << bold(value);
        if (!status.empty()) std::cout << "  " << status;
        std::cout << "\n";
    }

    // --- Colored badge ---
    void printBadge(const std::string& text, const std::string& color) {
        std::string c;
        if (color == "red") c = RED;
        else if (color == "green") c = GREEN;
        else if (color == "yellow") c = YELLOW;
        else if (color == "blue") c = BLUE;
        else if (color == "cyan") c = CYAN;
        else if (color == "magenta") c = MAGENTA;
        else c = GRAY;
        std::cout << c << "[" << text << "]" << RESET;
    }

    // --- Logo ---
    void printLogo(const std::string& version) {
        std::cout << "\n";
        std::cout << bold(blue(
            "     ████╗  ██████╗  ██████╗██╗  ██╗███████╗"
        )) << "\n";
        std::cout << bold(blue(
            "    ██╔══██╗██╔══██╗██╔════╝██║  ██║██╔════╝"
        )) << "\n";
        std::cout << bold(blue(
            "    ███████║██████╔╝██║     ███████║█████╗  "
        )) << "\n";
        std::cout << bold(blue(
            "    ██╔══██║██╔══██╗██║     ██╔══██║██╔══╝  "
        )) << "\n";
        std::cout << bold(blue(
            "    ██║  ██║██║  ██║╚██████╗██║  ██║███████╗"
        )) << "\n";
        std::cout << bold(magenta(
            "    ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚══════╝"
        )) << "\n";
        std::cout << "              " << bold(magenta("Archetyped") + " Development " +  cyan("CLI") + "  v" + version) << "\n";
        std::cout << "\n";
    }

    // --- Spinner ---
    Spinner::Spinner(const std::string& message) {
        if (!message.empty()) start(message);
    }

    Spinner::~Spinner() {
        stop();
    }

    void Spinner::start(const std::string& message) {
        if (running_) stop();
        message_ = message;
        running_ = true;
        thread_ = std::thread(&Spinner::spin, this);
    }

    void Spinner::stop() {
        if (running_) {
            running_ = false;
            if (thread_.joinable()) thread_.join();
            std::cout << "\r" << std::string(80, ' ') << "\r" << std::flush;
        }
    }

    void Spinner::message(const std::string& msg) {
        message_ = msg;
    }

    void Spinner::spin() {
        const char* frames = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏";
        int i = 0;
        while (running_) {
            std::cout << "\r  " << cyan(std::string(1, frames[i % 10])) << " " << message_ << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            i++;
        }
    }

    // ===================== Prompt =====================

    void printPrompt() {
        std::cout << gray("[ ") << bold(cyan("ARCHE")) << " " << green("\xe2\x97\x8f") << " "
                  << gray("Ready") << gray(" ]: ") << std::flush;
    }

} // namespace cli
