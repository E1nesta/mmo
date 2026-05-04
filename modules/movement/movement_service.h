#pragma once

namespace modules::movement {

struct Position {
    float x{};
    float y{};
    float z{};
};

struct MoveCommand {
    Position from;
    Position to;
    float max_distance{};
};

class MovementService {
public:
    bool validate(const MoveCommand& command) const;
};

}  // namespace modules::movement
