FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /src

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ccache \
    git \
    ninja-build \
    pkg-config \
    libboost-dev \
    libboost-system-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libprotobuf-dev \
    libgrpc++-dev \
    default-libmysqlclient-dev \
    libhiredis-dev \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN cmake --preset dev-release -DBUILD_TESTING=OFF && cmake --build --preset dev-release --parallel --target \
    api_gateway_server \
    online_gateway_server \
    auth_server \
    player_query_server \
    player_write_grpc_server \
    dungeon_runtime_server \
    social_server \
    demo_client \
    load_client \
    service_check \
    password_tool

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    protobuf-compiler \
    libprotobuf-dev \
    libgrpc++-dev \
    libboost-dev \
    libboost-system-dev \
    default-libmysqlclient-dev \
    libhiredis-dev \
    libssl-dev \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/dev-release/api_gateway_server /app/api_gateway_server
COPY --from=builder /src/build/dev-release/online_gateway_server /app/online_gateway_server
COPY --from=builder /src/build/dev-release/auth_server /app/auth_server
COPY --from=builder /src/build/dev-release/player_query_server /app/player_query_server
COPY --from=builder /src/build/dev-release/player_write_grpc_server /app/player_write_grpc_server
COPY --from=builder /src/build/dev-release/dungeon_runtime_server /app/dungeon_runtime_server
COPY --from=builder /src/build/dev-release/social_server /app/social_server
COPY --from=builder /src/build/dev-release/demo_client /app/demo_client
COPY --from=builder /src/build/dev-release/load_client /app/load_client
COPY --from=builder /src/build/dev-release/service_check /app/service_check
COPY --from=builder /src/build/dev-release/password_tool /app/password_tool
COPY --from=builder /src/configs /app/configs

CMD ["./api_gateway_server", "--config", "configs/api_gateway_server.conf"]
