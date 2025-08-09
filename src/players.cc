#include "players.h"

#include <charconv>
#include <map>
#include <ranges>

namespace {

std::vector<std::string_view> SplitBy(std::string_view str, char delim) {
    std::vector<std::string_view> parts;
    for (auto part : std::views::split(str, delim)) {
        parts.emplace_back(std::string_view(part.begin(), part.end()));
    }
    return parts;
}

using param_map_t = std::map<std::string_view, std::string_view>;

std::optional<param_map_t> ParseParams(std::span<std::string_view> parts) {
    param_map_t res;
    for (const auto &part : parts) {
        std::string_view::size_type sep = part.find('=');
        std::string_view key, val;
        if (sep == std::string_view::npos) {
            key = part;
            val = "";
        } else {
            key = part.substr(0, sep);
            val = part.substr(sep + 1);
        }
        if (key.empty() || res.contains(key)) return {};
        res[key] = val;
    }
    return res;
}

std::optional<RandomPlayerOpts> ParseRandomOpts(const param_map_t &params) {
    if (!params.empty()) return {};
    return RandomPlayerOpts{};
}

std::optional<CliPlayerOpts> ParseCliOpts(const param_map_t &params) {
    if (!params.empty()) return {};
    return CliPlayerOpts{};
}

std::optional<MinimaxPlayerOpts> ParseMinimaxOpts(const param_map_t &params) {
    MinimaxPlayerOpts res = {};
    for (const auto &[key, val] : params) {
        if (key == "max_depth") {
            if (std::from_chars(val.begin(), val.end(), res.max_depth).ec != std::errc{}) return {};
        } else {
            return {};  // Unknown key
        }
    }
    return res;
}

std::optional<MctsPlayerOpts> ParseMctsOpts(const param_map_t &params) {
    if (!params.empty()) return {};
    return MctsPlayerOpts{};
}

}  // namespace

std::optional<PlayerDesc> ParsePlayerDesc(std::string_view sv) {
    std::vector<std::string_view> parts = SplitBy(sv, ',');
    if (parts.empty()) return {};

    std::string_view type = parts[0];
    param_map_t params;
    if (auto opt_params = ParseParams(std::span(parts).subspan(1)); !opt_params) {
        return {};
    } else {
        params = std::move(*opt_params);
    }

    if (type == "rand") {
        if (auto opts = ParseRandomOpts(params); !opts) {
            return {};
        } else {
            return PlayerDesc{
                .type = PLAY_RAND,
                .opts = {
                    .random = std::move(*opts),
                }
            };
        }
    }
    if (type == "cli") {
        if (auto opts = ParseCliOpts(params); !opts) {
            return {};
        } else {
            return PlayerDesc{
                .type = PLAY_CLI,
                .opts = {
                    .cli = std::move(*opts),
                }
            };
        }
    }
    if (type == "minimax") {
        if (auto opts = ParseMinimaxOpts(params); !opts) {
            return {};
        } else {
            return PlayerDesc{
                .type = PLAY_MINIMAX,
                .opts = {
                    .minimax = std::move(*opts),
                }
            };
        }
    }
    if (type == "mcts") {
        if (auto opts = ParseMctsOpts(params); !opts) {
            return {};
        } else {
            return PlayerDesc{
                .type = PLAY_MCTS,
                .opts = {
                    .mcts = std::move(*opts),
                }
            };
        }
    }
    return {};
}

GamePlayer *CreatePlayerFromDesc(const PlayerDesc &desc) {
    switch (desc.type) {
        case PLAY_RAND:     return CreateRandomPlayer   (desc.opts.random);
        case PLAY_CLI:      return CreateCliPlayer      (desc.opts.cli);
        case PLAY_MINIMAX:  return CreateMinimaxPlayer  (desc.opts.minimax);
        case PLAY_MCTS:     return CreateMctsPlayer     (desc.opts.mcts);
    }
    return nullptr;
}
