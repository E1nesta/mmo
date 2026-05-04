#pragma once

#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

namespace modules::aoi {

struct AoiCell {
    int x{};
    int z{};

    bool operator<(const AoiCell& other) const {
        return x == other.x ? z < other.z : x < other.x;
    }
};

struct AoiMoveResult {
    std::vector<std::int64_t> nearby;
    std::vector<std::int64_t> entered;
    std::vector<std::int64_t> left;
};

class AoiGrid {
public:
    explicit AoiGrid(float cell_size);

    AoiMoveResult move(std::int64_t entity_id, float x, float z);

private:
    AoiCell to_cell(float x, float z) const;
    std::set<AoiCell> nearby_cells(const AoiCell& center) const;

    float cell_size_{};
    std::unordered_map<std::int64_t, AoiCell> entity_cells_;
};

}  // namespace modules::aoi
