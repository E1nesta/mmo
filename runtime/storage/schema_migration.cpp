#include "runtime/storage/schema_migration.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace runtime::storage {
namespace {

constexpr const char* kSchemaMigrationsTableSql =
    "CREATE TABLE IF NOT EXISTS schema_migrations ("
    "version INT NOT NULL,"
    "name VARCHAR(255) NOT NULL,"
    "checksum CHAR(64) NOT NULL,"
    "applied_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "PRIMARY KEY (version)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci";

std::vector<std::string> split_sql_statements(const std::string& sql) {
    std::vector<std::string> statements;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;

    for (const char ch : sql) {
        current.push_back(ch);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }
        if (ch == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }
        if (ch == ';' && !in_single_quote && !in_double_quote) {
            current.pop_back();
            const auto first = current.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) {
                const auto last = current.find_last_not_of(" \t\r\n");
                statements.push_back(current.substr(first, last - first + 1U));
            }
            current.clear();
        }
    }

    const auto first = current.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
        const auto last = current.find_last_not_of(" \t\r\n");
        statements.push_back(current.substr(first, last - first + 1U));
    }
    return statements;
}

SchemaMigrationResult failed_result(
    const std::string& message,
    std::string* error_message) {
    if (error_message != nullptr) {
        *error_message = message;
    }
    SchemaMigrationResult result;
    result.success = false;
    result.error_message = message;
    return result;
}

bool has_duplicate_versions(const std::vector<SchemaMigration>& migrations) {
    int previous = -1;
    bool first = true;
    for (const auto& migration : migrations) {
        if (!first && migration.version == previous) {
            return true;
        }
        previous = migration.version;
        first = false;
    }
    return false;
}

}  // namespace

std::string schema_migration_checksum(const std::string& sql) {
    unsigned char digest[SHA256_DIGEST_LENGTH]{};
    SHA256(
        reinterpret_cast<const unsigned char*>(sql.data()),
        sql.size(),
        digest);

    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const unsigned char value : digest) {
        output << std::setw(2) << static_cast<int>(value);
    }
    return output.str();
}

SchemaMigration make_sql_migration(
    int version,
    std::string name,
    std::string sql) {
    SchemaMigration migration;
    migration.version = version;
    migration.name = std::move(name);
    migration.sql = std::move(sql);
    migration.checksum = schema_migration_checksum(migration.sql);
    return migration;
}

bool load_sql_migration_file(
    int version,
    const std::string& name,
    const std::string& path,
    SchemaMigration* migration,
    std::string* error_message) {
    if (migration == nullptr) {
        if (error_message != nullptr) {
            *error_message = "schema migration output is null";
        }
        return false;
    }

    std::ifstream input(path);
    if (!input) {
        if (error_message != nullptr) {
            *error_message = "failed to open schema migration file: " + path;
        }
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    *migration = make_sql_migration(version, name, buffer.str());
    return true;
}

SchemaMigrationResult run_schema_migrations(
    MysqlClient& client,
    const std::vector<SchemaMigration>& migrations,
    std::string* error_message) {
    std::string mysql_error;
    if (!client.execute(kSchemaMigrationsTableSql, &mysql_error)) {
        return failed_result(mysql_error, error_message);
    }

    auto ordered = migrations;
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const SchemaMigration& left, const SchemaMigration& right) {
            return left.version < right.version;
        });
    if (has_duplicate_versions(ordered)) {
        return failed_result("duplicate schema migration version", error_message);
    }

    SchemaMigrationResult result;
    result.success = true;
    for (const auto& migration : ordered) {
        if (migration.version <= 0 || migration.name.empty() ||
            migration.checksum.empty()) {
            return failed_result("invalid schema migration definition", error_message);
        }

        std::vector<std::vector<std::string>> rows;
        const std::string select_sql =
            "SELECT checksum FROM schema_migrations WHERE version=" +
            std::to_string(migration.version) + " LIMIT 1";
        if (!client.query(select_sql, &rows, &mysql_error)) {
            return failed_result(mysql_error, error_message);
        }
        if (!rows.empty()) {
            if (rows.front().empty() || rows.front().front() != migration.checksum) {
                return failed_result(
                    "schema migration checksum mismatch for version " +
                        std::to_string(migration.version),
                    error_message);
            }
            ++result.skipped_count;
            continue;
        }

        for (const auto& statement : split_sql_statements(migration.sql)) {
            if (!client.execute(statement, &mysql_error)) {
                return failed_result(mysql_error, error_message);
            }
        }

        const std::string insert_sql =
            "INSERT INTO schema_migrations(version, name, checksum) VALUES (" +
            std::to_string(migration.version) + ", '" +
            client.escape_string(migration.name) + "', '" +
            client.escape_string(migration.checksum) + "')";
        if (!client.execute(insert_sql, &mysql_error)) {
            return failed_result(mysql_error, error_message);
        }
        ++result.applied_count;
    }
    return result;
}

}  // namespace runtime::storage
