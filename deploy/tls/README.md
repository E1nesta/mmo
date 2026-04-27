Delivery and production TLS assets are expected under this directory.

Required files for the current deployment layout:

- `ca.pem`
- `gateway.crt`
- `gateway.key`
- `online_gateway_server.crt`
- `online_gateway_server.key`
- `auth_server.crt`
- `auth_server.key`
- `player_query_server.crt`
- `player_query_server.key`
- `dungeon_runtime_server.crt`
- `dungeon_runtime_server.key`
- `social_server.crt`
- `social_server.key`

Notes:

- External TCP services use transport TLS certificates named after each service.
- `api_gateway_server` sits behind Nginx and does not require a separate external TLS certificate in this layout.
- Internal player write gRPC is expected to stay on the trusted internal network in the current deployment skeleton.
