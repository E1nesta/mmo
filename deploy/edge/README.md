# Edge TLS Deployment Notes

Production public traffic terminates TLS at Edge/LB before reaching the MMO
services. The repository services stay focused on protocol and session logic:

- `api_gateway_server` listens on internal HTTP, for example `127.0.0.1:4100`.
- `game_gateway_server` listens on the reliable game protocol, for example
  `127.0.0.1:4102`, behind an L4/L7 edge entry.
- Backend services remain internal-only and must not be exposed by the edge.

Use managed cloud load balancers, Envoy, or Nginx for certificates, TLS
policy, basic rate limits, and forwarding. Certificate private keys must be
mounted by deployment tooling and must not be committed to the repository.

`nginx_api_gateway_tls.conf` is a minimal HTTPS termination sample for local
production-like deployments. Replace hostnames, certificate paths, and upstream
addresses before use.
