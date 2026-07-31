#ifndef NEZHAGUARD_TOR_CHECKER_H
#define NEZHAGUARD_TOR_CHECKER_H

#include <string>
#include <unordered_set>

namespace Nezha::Core {

class TorChecker {
public:
    TorChecker() = default;

    bool initialize();

    [[nodiscard]] bool is_tor_exit(const std::string &ip) const;

    [[nodiscard]] std::size_t total_nodes() const noexcept { return nodes_.size(); }

    void refresh();

private:
    void load_from_cache();
    void save_to_cache();
    bool fetch_from_tor_project();

    std::unordered_set<std::string> nodes_;
};

}

#endif //NEZHAGUARD_TOR_CHECKER_H
