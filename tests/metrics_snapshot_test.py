#!/usr/bin/env python3
"""Tests for the lightweight metrics snapshot parser."""

from __future__ import annotations

import importlib.util
import pathlib
import sys
import unittest


ROOT_DIR = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT_DIR / "scripts" / "metrics_snapshot.py"
SPEC = importlib.util.spec_from_file_location("metrics_snapshot", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"failed to load module from {MODULE_PATH}")
metrics_snapshot = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = metrics_snapshot
SPEC.loader.exec_module(metrics_snapshot)


class MetricsSnapshotTest(unittest.TestCase):
    def test_parse_gateway_metrics_counts_success_failures_and_rejections(self) -> None:
        log_text = """
api_gateway_server  | ts=2026-04-07T00:00:00Z event=gateway_forward_succeeded upstream.service=auth latency_ms=12
api_gateway_server  | ts=2026-04-07T00:00:01Z event=gateway_forward_failed upstream.service=dungeon_runtime latency_ms=150 error_code=upstream_timeout
api_gateway_server  | ts=2026-04-07T00:00:02Z event=gateway_forward_rejected upstream.service=player_query error_code=rate_limit_hit
online_gateway_1    | ts=2026-04-07T00:00:03Z event=session_binding_restored detail="ignored"
"""
        metrics = metrics_snapshot.parse_gateway_metrics(log_text, 60)
        self.assertEqual(metrics.total_requests, 3)
        self.assertEqual(metrics.succeeded, 1)
        self.assertEqual(metrics.failed, 1)
        self.assertEqual(metrics.rejected, 1)
        self.assertAlmostEqual(metrics.qps, 0.05)
        self.assertEqual(metrics.p95_latency_ms, 150)
        self.assertAlmostEqual(metrics.error_rate or 0.0, 2.0 / 3.0)

    def test_parse_gateway_metrics_ignores_missing_or_invalid_latency(self) -> None:
        log_text = """
api_gateway_server  | event=gateway_forward_succeeded upstream.service=auth
api_gateway_server  | event=gateway_forward_failed upstream.service=dungeon_runtime latency_ms=not_a_number
"""
        metrics = metrics_snapshot.parse_gateway_metrics(log_text, 30)
        self.assertEqual(metrics.total_requests, 2)
        self.assertIsNone(metrics.p95_latency_ms)
        self.assertAlmostEqual(metrics.error_rate or 0.0, 0.5)

    def test_parse_stats_output_handles_units_and_missing_services(self) -> None:
        stats_text = "\n".join(
            [
                "abc123\t12.50%\t24.5MiB / 1.0GiB\t2.39%",
                "def456\t0.87%\t512KiB / 1.0GiB\t0.05%",
            ]
        )
        parsed = metrics_snapshot.parse_stats_output(
            stats_text,
            {
                "abc123": "online_gateway_1",
                "def456": "dungeon_runtime_server",
            },
        )
        gateway = parsed["online_gateway_1"]
        battle = parsed["dungeon_runtime_server"]
        self.assertAlmostEqual(gateway.cpu_percent or 0.0, 12.50)
        self.assertEqual(gateway.memory_used_display, "24.5MiB")
        self.assertEqual(gateway.memory_used_bytes, int(24.5 * 1024 * 1024))
        self.assertAlmostEqual(gateway.memory_percent or 0.0, 2.39)
        self.assertEqual(battle.memory_used_bytes, 512 * 1024)

    def test_parse_stats_output_matches_short_container_ids(self) -> None:
        stats_text = "abc123\t9.10%\t10.0MiB / 1.0GiB\t0.98%"
        parsed = metrics_snapshot.parse_stats_output(
            stats_text,
            {
                "abc123456789": "online_gateway_1",
            },
        )
        self.assertIn("online_gateway_1", parsed)
        self.assertAlmostEqual(parsed["online_gateway_1"].cpu_percent or 0.0, 9.10)

    def test_parse_window_seconds_supports_compound_durations(self) -> None:
        self.assertEqual(metrics_snapshot.parse_window_seconds("60s"), 60)
        self.assertEqual(metrics_snapshot.parse_window_seconds("1m30s"), 90)
        self.assertEqual(metrics_snapshot.parse_window_seconds("2h5m"), 7500)
        with self.assertRaises(metrics_snapshot.SnapshotError):
            metrics_snapshot.parse_window_seconds("15")


if __name__ == "__main__":
    unittest.main()
