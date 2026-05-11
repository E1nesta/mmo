#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "runtime/foundation/readiness.h"
#include "runtime/foundation/server_config.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/observability/metrics_exporter.h"
#include "runtime/storage/schema_migration.h"
#include "runtime/storage/storage_runtime.h"

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::int64_t now_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string unique_name(const std::string& prefix) {
    const auto now = now_millis();
    return prefix + std::to_string(now);
}

void verify_metrics_exporter() {
    runtime::observability::MetricsRegistry metrics;
    metrics.record_connection_open();
    metrics.record_request();
    metrics.record_error();
    metrics.record_login_success();
    metrics.record_gateway_ticket_issued();
    metrics.record_gateway_login_failed();
    metrics.set_executor_queue_depth(7);
    metrics.set_rpc_pending_count(3);

    const auto text =
        runtime::observability::render_prometheus_metrics(metrics.snapshot());
    assert(contains(text, "# TYPE mmo_active_connections gauge"));
    assert(contains(text, "mmo_active_connections 1"));
    assert(contains(text, "# TYPE mmo_requests_total counter"));
    assert(contains(text, "mmo_requests_total 1"));
    assert(contains(text, "mmo_login_success_total 1"));
    assert(contains(text, "mmo_gateway_ticket_issued_total 1"));
    assert(contains(text, "# TYPE mmo_rpc_pending_count gauge"));
}

void verify_readiness() {
    const auto not_ready = runtime::foundation::check_tcp_dependency(
        "missing_probe_dependency",
        "127.0.0.1",
        1,
        std::chrono::milliseconds(20));
    assert(!not_ready.ready);
    assert(not_ready.error_code == "dependency_unreachable");
    assert(contains(not_ready.message, "missing_probe_dependency"));
}

void verify_logging() {
    runtime::observability::LogContext context("production_probe");
    context.request_id = 1001;
    context.route_key = 3003;
    context.gateway_id = "game_gateway_server";
    context.session_id = 4004;
    context.upstream = "auth_server";
    context.status = "failed";
    context.error_code = 401;
    context.latency_ms = 12;

    const auto line =
        runtime::observability::format_log_line(context, "probe_event");
    assert(contains(line, "service=production_probe"));
    assert(contains(line, "event=probe_event"));
    assert(contains(line, "route_key=3003"));
    assert(contains(line, "session_id=4004"));
    assert(contains(line, "upstream=auth_server"));
    assert(contains(line, "status=failed"));
    assert(!contains(line, "password"));
    assert(!contains(line, "access_token"));
    assert(!contains(line, "gateway_ticket"));
    assert(!contains(line, "session_token"));
    assert(!contains(line, "internal_signature"));
    assert(!contains(line, "shared_secret"));
}

void verify_migrations(
    runtime::storage::MysqlConnectionPool& mysql_pool) {
    std::string error;
    auto lease = mysql_pool.acquire();

    runtime::storage::SchemaMigration foundation;
    assert(runtime::storage::load_sql_migration_file(
        1,
        "foundation_schema",
        "deploy/mysql/migrations/0001_foundation_schema.sql",
        &foundation,
        &error));
    auto foundation_result =
        runtime::storage::run_schema_migrations(*lease, {foundation}, &error);
    assert(foundation_result.success);

    const int probe_version =
        100000000 + static_cast<int>(now_millis() % 100000000);
    const std::string table_name = unique_name("migration_probe_");
    const std::string migration_name = unique_name("probe_migration_");
    const std::string sql =
        "CREATE TABLE IF NOT EXISTS " + table_name +
        " (id BIGINT NOT NULL PRIMARY KEY) ENGINE=InnoDB";

    const auto migration =
        runtime::storage::make_sql_migration(probe_version, migration_name, sql);
    const std::vector<runtime::storage::SchemaMigration> migrations = {
        migration,
    };

    auto first =
        runtime::storage::run_schema_migrations(*lease, migrations, &error);
    assert(first.success);
    assert(first.applied_count == 1);
    assert(first.skipped_count == 0);

    auto second =
        runtime::storage::run_schema_migrations(*lease, migrations, &error);
    assert(second.success);
    assert(second.applied_count == 0);
    assert(second.skipped_count == 1);

    auto changed = runtime::storage::make_sql_migration(
        probe_version,
        migration_name,
        "CREATE TABLE IF NOT EXISTS " + table_name +
            " (id BIGINT NOT NULL PRIMARY KEY, value BIGINT NULL) ENGINE=InnoDB");
    const std::vector<runtime::storage::SchemaMigration> changed_migrations = {
        changed,
    };
    auto mismatch = runtime::storage::run_schema_migrations(
        *lease, changed_migrations, &error);
    assert(!mismatch.success);
    assert(contains(mismatch.error_message, "checksum mismatch"));
}

}  // namespace

int main() {
    verify_metrics_exporter();
    verify_readiness();
    verify_logging();

    const auto config = runtime::foundation::load_server_config_from_env();
    std::string error;
    std::shared_ptr<runtime::storage::MysqlConnectionPool> mysql_pool;
    assert(runtime::storage::initialize_mysql_pool(
        config, &mysql_pool, &error));
    verify_migrations(*mysql_pool);

    std::cout << "production runtime governance probe ok\n";
    return 0;
}
