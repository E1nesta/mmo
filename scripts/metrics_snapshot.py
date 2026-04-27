#!/usr/bin/env python3
"""Capture a lightweight metrics snapshot from docker compose services."""

from __future__ import annotations

import argparse
import math
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT_DIR = Path(__file__).resolve().parents[1]
SERVICES = [
    "online_gateway_1",
    "online_gateway_2",
    "api_gateway_server",
    "auth_server",
    "player_query_server",
    "player_write_grpc_server",
    "dungeon_runtime_server",
    "social_server",
]
GATEWAY_FORWARD_EVENTS = {
    "gateway_forward_succeeded",
    "gateway_forward_failed",
    "gateway_forward_rejected",
}
LATENCY_EVENTS = {
    "gateway_forward_succeeded",
    "gateway_forward_failed",
}
EVENT_PATTERN = re.compile(r"(?:^|\s)event=([a-z0-9_]+)(?:\s|$)")
LATENCY_PATTERN = re.compile(r"(?:^|\s)latency_ms=(\d+)(?:\s|$)")
DOCKER_BYTES_PATTERN = re.compile(r"^\s*([0-9]+(?:\.[0-9]+)?)\s*([kmgt]?i?b)?\s*$", re.IGNORECASE)
WINDOW_CHUNK_PATTERN = re.compile(r"(\d+)([smhd])", re.IGNORECASE)


class SnapshotError(RuntimeError):
    """Raised when the snapshot cannot be collected."""


@dataclass(frozen=True)
class GatewayMetrics:
    total_requests: int
    succeeded: int
    failed: int
    rejected: int
    qps: float
    p95_latency_ms: int | None
    error_rate: float | None


@dataclass(frozen=True)
class ServiceStat:
    service: str
    container_id: str | None
    cpu_percent: float | None
    memory_used_bytes: int | None
    memory_used_display: str
    memory_percent: float | None


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--window", default="60s", help="lookback window for compose logs (default: 60s)")
    parser.add_argument("--top", type=int, default=3, help="number of top services to highlight (default: 3)")
    return parser


def docker_cmd() -> str:
    return "/usr/bin/docker" if Path("/usr/bin/docker").exists() else "docker"


def deploy_profile() -> str:
    return os.getenv("DEPLOY_PROFILE", "demo")


def compose_file() -> Path:
    if deploy_profile() == "delivery":
        return ROOT_DIR / "deploy" / "docker-compose.delivery.yml"
    return ROOT_DIR / "deploy" / "docker-compose.yml"


def compose_env_file() -> Path:
    deploy_dir = ROOT_DIR / "deploy"
    if deploy_profile() == "delivery":
        preferred = deploy_dir / ".env.delivery"
        if preferred.exists():
            return preferred
        return deploy_dir / ".env.delivery.example"
    return deploy_dir / ".env.demo"


def compose_base_command() -> list[str]:
    return [
        docker_cmd(),
        "compose",
        "--env-file",
        str(compose_env_file()),
        "-f",
        str(compose_file()),
    ]


def run_command(command: list[str], *, check: bool = True) -> str:
    result = subprocess.run(
        command,
        cwd=ROOT_DIR,
        check=False,
        capture_output=True,
        text=True,
    )
    if check and result.returncode != 0:
        stderr = result.stderr.strip()
        stdout = result.stdout.strip()
        detail = stderr or stdout or f"command exited with status {result.returncode}"
        raise SnapshotError(f"{' '.join(command)} failed: {detail}")
    return result.stdout


def parse_window_seconds(window: str) -> int:
    total_seconds = 0
    consumed = 0
    for match in WINDOW_CHUNK_PATTERN.finditer(window.strip()):
        value = int(match.group(1))
        unit = match.group(2).lower()
        multiplier = {
            "s": 1,
            "m": 60,
            "h": 3600,
            "d": 86400,
        }[unit]
        total_seconds += value * multiplier
        consumed += len(match.group(0))
    if total_seconds <= 0 or consumed != len(window.strip()):
        raise SnapshotError(f"unsupported --window value: {window!r}")
    return total_seconds


def percentile_nearest_rank(values: list[int], percentile: float) -> int | None:
    if not values:
        return None
    sorted_values = sorted(values)
    rank = max(1, math.ceil(percentile * len(sorted_values)))
    return sorted_values[rank - 1]


def parse_gateway_metrics(log_text: str, window_seconds: int) -> GatewayMetrics:
    succeeded = 0
    failed = 0
    rejected = 0
    latencies: list[int] = []

    for raw_line in log_text.splitlines():
        event_match = EVENT_PATTERN.search(raw_line)
        if event_match is None:
            continue
        event = event_match.group(1)
        if event not in GATEWAY_FORWARD_EVENTS:
            continue

        if event == "gateway_forward_succeeded":
            succeeded += 1
        elif event == "gateway_forward_failed":
            failed += 1
        elif event == "gateway_forward_rejected":
            rejected += 1

        if event in LATENCY_EVENTS:
            latency_match = LATENCY_PATTERN.search(raw_line)
            if latency_match is not None:
                latencies.append(int(latency_match.group(1)))

    total_requests = succeeded + failed + rejected
    qps = float(total_requests) / float(window_seconds) if window_seconds > 0 else 0.0
    error_count = failed + rejected
    error_rate = (float(error_count) / float(total_requests)) if total_requests else None
    return GatewayMetrics(
        total_requests=total_requests,
        succeeded=succeeded,
        failed=failed,
        rejected=rejected,
        qps=qps,
        p95_latency_ms=percentile_nearest_rank(latencies, 0.95),
        error_rate=error_rate,
    )


def parse_cpu_percent(value: str) -> float | None:
    stripped = value.strip().rstrip("%")
    if not stripped:
        return None
    try:
        return float(stripped)
    except ValueError:
        return None


def parse_bytes(value: str) -> int | None:
    match = DOCKER_BYTES_PATTERN.match(value)
    if match is None:
        return None

    amount = float(match.group(1))
    unit = (match.group(2) or "b").lower()
    multipliers = {
        "b": 1,
        "kb": 1000,
        "mb": 1000**2,
        "gb": 1000**3,
        "tb": 1000**4,
        "kib": 1024,
        "mib": 1024**2,
        "gib": 1024**3,
        "tib": 1024**4,
    }
    multiplier = multipliers.get(unit)
    if multiplier is None:
        return None
    return int(amount * multiplier)


def resolve_service_name(container_id: str, container_to_service: dict[str, str]) -> str | None:
    direct = container_to_service.get(container_id)
    if direct is not None:
        return direct
    for known_container_id, service in container_to_service.items():
        if known_container_id.startswith(container_id) or container_id.startswith(known_container_id):
            return service
    return None


def parse_stats_output(stats_text: str, container_to_service: dict[str, str]) -> dict[str, ServiceStat]:
    parsed: dict[str, ServiceStat] = {}
    for raw_line in stats_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) != 4:
            continue
        container_id, cpu_raw, mem_usage_raw, mem_percent_raw = parts
        service = resolve_service_name(container_id, container_to_service)
        if service is None:
            continue
        memory_used_display = mem_usage_raw.split("/", 1)[0].strip()
        parsed[service] = ServiceStat(
            service=service,
            container_id=container_id,
            cpu_percent=parse_cpu_percent(cpu_raw),
            memory_used_bytes=parse_bytes(memory_used_display),
            memory_used_display=memory_used_display or "n/a",
            memory_percent=parse_cpu_percent(mem_percent_raw),
        )
    return parsed


def running_container_ids(services: Iterable[str]) -> dict[str, str]:
    service_to_container: dict[str, str] = {}
    for service in services:
        container_id = run_command(compose_base_command() + ["ps", "-q", service], check=False).strip().splitlines()
        if not container_id:
            continue
        candidate = container_id[0].strip()
        if not candidate:
            continue
        running = run_command(
            [docker_cmd(), "inspect", "--format", "{{.State.Running}}", candidate],
            check=False,
        ).strip()
        if running == "true":
            service_to_container[service] = candidate
    return service_to_container


def collect_gateway_logs(window: str) -> str:
    return run_command(
        compose_base_command() + ["logs", "--no-color", "--since", window, "api_gateway_server"],
        check=False,
    )


def collect_service_stats() -> list[ServiceStat]:
    service_to_container = running_container_ids(SERVICES)
    if not service_to_container:
        return [
            ServiceStat(service=service, container_id=None, cpu_percent=None, memory_used_bytes=None, memory_used_display="n/a", memory_percent=None)
            for service in SERVICES
        ]

    stats_output = run_command(
        [
            docker_cmd(),
            "stats",
            "--no-stream",
            "--format",
            "{{.Container}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.MemPerc}}",
            *service_to_container.values(),
        ]
    )
    container_to_service = {container_id: service for service, container_id in service_to_container.items()}
    parsed = parse_stats_output(stats_output, container_to_service)
    stats: list[ServiceStat] = []
    for service in SERVICES:
        if service in parsed:
            stats.append(parsed[service])
        else:
            stats.append(
                ServiceStat(
                    service=service,
                    container_id=service_to_container.get(service),
                    cpu_percent=None,
                    memory_used_bytes=None,
                    memory_used_display="n/a",
                    memory_percent=None,
                )
            )
    return stats


def format_ratio(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value * 100.0:.2f}%"


def format_float(value: float | None, suffix: str = "") -> str:
    if value is None:
        return "n/a"
    return f"{value:.2f}{suffix}"


def format_latency(value: int | None) -> str:
    if value is None:
        return "n/a"
    return f"{value} ms"


def print_summary(window: str, metrics: GatewayMetrics, stats: list[ServiceStat], top: int) -> None:
    print(f"window={window}")
    print(f"gateway_qps={metrics.qps:.2f}")
    print(f"gateway_p95_latency_ms={metrics.p95_latency_ms if metrics.p95_latency_ms is not None else 'n/a'}")
    print(f"gateway_error_rate={format_ratio(metrics.error_rate)}")
    print("cpu_by_service:")
    print("service                       cpu")
    for stat in stats:
        print(f"{stat.service:<28} {format_float(stat.cpu_percent, '%'):>8}")
    print("memory_by_service:")
    print("service                       used            pct")
    for stat in stats:
        memory_pct = format_float(stat.memory_percent, "%")
        print(f"{stat.service:<28} {stat.memory_used_display:>12} {memory_pct:>10}")

    cpu_top = sorted((stat for stat in stats if stat.cpu_percent is not None), key=lambda item: item.cpu_percent, reverse=True)[:top]
    mem_top = sorted(
        (stat for stat in stats if stat.memory_used_bytes is not None),
        key=lambda item: item.memory_used_bytes,
        reverse=True,
    )[:top]
    print("top_cpu_services:")
    if cpu_top:
        for stat in cpu_top:
            print(f"- {stat.service}: {format_float(stat.cpu_percent, '%')}")
    else:
        print("- n/a")
    print("top_memory_services:")
    if mem_top:
        for stat in mem_top:
            memory_pct = f" ({format_float(stat.memory_percent, '%')})" if stat.memory_percent is not None else ""
            print(f"- {stat.service}: {stat.memory_used_display}{memory_pct}")
    else:
        print("- n/a")


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    try:
        window_seconds = parse_window_seconds(args.window)
        gateway_logs = collect_gateway_logs(args.window)
        metrics = parse_gateway_metrics(gateway_logs, window_seconds)
        stats = collect_service_stats()
    except SnapshotError as exc:
        print(f"metrics snapshot failed: {exc}", file=sys.stderr)
        return 1

    print_summary(args.window, metrics, stats, max(1, args.top))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
