#pragma once

#include <string>
#include <vector>

#include "runtime/storage/mysql_client.h"

namespace runtime::storage {

struct SchemaMigration {
    int version{};
    std::string name;
    std::string checksum;
    std::string sql;
};

struct SchemaMigrationResult {
    bool success{};
    int applied_count{};
    int skipped_count{};
    std::string error_message;
};

std::string schema_migration_checksum(const std::string& sql);

SchemaMigration make_sql_migration(
    int version,
    std::string name,
    std::string sql);

bool load_sql_migration_file(
    int version,
    const std::string& name,
    const std::string& path,
    SchemaMigration* migration,
    std::string* error_message);

SchemaMigrationResult run_schema_migrations(
    MysqlClient& client,
    const std::vector<SchemaMigration>& migrations,
    std::string* error_message);

}  // namespace runtime::storage
