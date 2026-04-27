$ErrorActionPreference = "Stop"

cmake --preset dev-debug
cmake --build --preset dev-debug

./build/dev-debug/api_gateway_server.exe --config configs/api_gateway_server.conf --check
./build/dev-debug/online_gateway_server.exe --config configs/online_gateway_server.conf --check
./build/dev-debug/auth_server.exe --config configs/auth_server.conf --check
./build/dev-debug/player_query_server.exe --config configs/player_query_server.conf --check
./build/dev-debug/player_write_grpc_server.exe --config configs/player_write_grpc_server.conf --check
./build/dev-debug/dungeon_runtime_server.exe --config configs/dungeon_runtime_server.conf --check
./build/dev-debug/social_server.exe --config configs/social_server.conf --check
