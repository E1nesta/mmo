#include "modules/aoi/aoi_grid.h"

#include <cmath>

namespace modules::aoi {

AoiGrid::AoiGrid(float cell_size) : cell_size_(cell_size) {}

AoiMoveResult AoiGrid::move(std::int64_t entity_id, float x, float z) {
    const AoiCell next = to_cell(x, z);
    const auto previous_it = entity_cells_.find(entity_id);
    const bool first_enter = previous_it == entity_cells_.end();
    const AoiCell previous = first_enter ? next : previous_it->second;
    entity_cells_[entity_id] = next;

    const auto previous_cells = nearby_cells(previous);
    const auto next_cells = nearby_cells(next);

    AoiMoveResult result;
    for (const auto& [other_entity_id, other_cell] : entity_cells_) {
        if (other_entity_id == entity_id) {
            continue;
        }
        if (next_cells.count(other_cell) > 0) {
            result.nearby.push_back(other_entity_id);
        }
        if (next_cells.count(other_cell) > 0 &&
            (first_enter || previous_cells.count(other_cell) == 0)) {
            result.entered.push_back(other_entity_id);
        }
        if (!first_enter && previous_cells.count(other_cell) > 0 &&
            next_cells.count(other_cell) == 0) {
            result.left.push_back(other_entity_id);
        }
    }

    return result;
}

AoiCell AoiGrid::to_cell(float x, float z) const {
    return AoiCell{
        static_cast<int>(std::floor(x / cell_size_)),
        static_cast<int>(std::floor(z / cell_size_)),
    };
}

std::set<AoiCell> AoiGrid::nearby_cells(const AoiCell& center) const {
    std::set<AoiCell> cells;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            cells.insert(AoiCell{center.x + dx, center.z + dz});
        }
    }
    return cells;
}

}  // namespace modules::aoi
