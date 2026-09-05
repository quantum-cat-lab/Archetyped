#pragma once
#include <string>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>

namespace cli {

    std::string red(const std::string& text);
    std::string green(const std::string& text);
    std::string yellow(const std::string& text);
    std::string blue(const std::string& text);
    std::string magenta(const std::string& text);
    std::string cyan(const std::string& text);
    std::string gray(const std::string& text);
    std::string bold(const std::string& text);

    void printSuccess(const std::string& msg);
    void printError(const std::string& msg);
    void printWarning(const std::string& msg);
    void printInfo(const std::string& msg);
    void printHeader(const std::string& msg);

    // Visual helpers
    void printLogo(const std::string& version);
    void printSection(const std::string& icon, const std::string& title);
    void printRule();
    void printMeter(int current, int total, const std::string& label = "");
    void printBox(const std::vector<std::string>& lines);
    void printKV(const std::string& key, const std::string& value, const std::string& status = "");
    void printBadge(const std::string& text, const std::string& color);
    void printPrompt();

    std::string iconOk();
    std::string iconFail();
    std::string iconWarn();
    std::string iconInfo();
    std::string iconArrow();
    std::string iconBullet();
    std::string iconStar();
    std::string iconGear();
    std::string iconPackage();
    std::string iconBox();
    std::string iconSDK();
    std::string iconBuild();
    std::string iconDoctor();
    std::string iconModule();
    std::string iconContainer();

    class Spinner {
    public:
        Spinner(const std::string& message = "");
        ~Spinner();
        void start(const std::string& message);
        void stop();
        void message(const std::string& msg);
    private:
        std::thread thread_;
        std::atomic<bool> running_{false};
        std::string message_;
        void spin();
    };

} // namespace cli
