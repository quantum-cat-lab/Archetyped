#pragma once

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

class Workspace;

/// CLI argument parser — positional args + named flags.
struct Args {
    std::vector<std::string> positional;
    std::vector<std::string> flags;

    static Args parse(const std::vector<std::string>& raw) {
        Args a;
        for (auto& s : raw) {
            if (s.size() > 1 && s[0] == '-')
                a.flags.push_back(s);
            else
                a.positional.push_back(s);
        }
        return a;
    }

    bool has(const std::string& flag) const {
        return std::find(flags.begin(), flags.end(), flag) != flags.end();
    }

    std::string value(const std::string& opt, const std::string& def = "") const {
        for (size_t i = 0; i + 1 < flags.size(); ++i)
            if (flags[i] == opt) return flags[i + 1];
        return def;
    }

    /// Indexed-positional accessors.
    std::string pos(size_t i) const { return i < positional.size() ? positional[i] : ""; }
    bool empty() const { return positional.empty() && flags.empty(); }
};

/// Abstract interface every arche command implements.
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual int execute(const Args& args, Workspace& ws) = 0;
};
