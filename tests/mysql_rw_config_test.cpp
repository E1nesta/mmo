#include "runtime/foundation/config/simple_config.h"
#include "runtime/storage/mysql/mysql_client.h"

#include <fstream>
#include <iostream>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

}  // namespace

int main() {
    {
        common::config::SimpleConfig config;
        const std::string path = "mysql_rw_config_legacy.conf";
        {
            std::ofstream output(path);
            output << "storage.player.mysql.host=legacy-writer\n";
            output << "storage.player.mysql.port=3308\n";
            output << "storage.player.mysql.user=legacy-user\n";
            output << "storage.player.mysql.password=legacy-pass\n";
            output << "storage.player.mysql.database=legacy-db\n";
            output << "storage.player.mysql.pool_size=7\n";
        }
        if (!Expect(config.LoadFromFile(path), "expected legacy mysql config to load")) {
            return 1;
        }

        const auto options = common::mysql::ReadReadWritePoolOptions(config, "storage.player.mysql.", 4);
        if (!Expect(options.writer.connection.host == "legacy-writer" && options.writer.connection.port == 3308 &&
                        options.writer.connection.user == "legacy-user" &&
                        options.writer.connection.password == "legacy-pass" &&
                        options.writer.connection.database == "legacy-db" && options.writer.pool_size == 7 &&
                        !options.reader.has_value(),
                    "expected legacy config to map into writer-only read/write options")) {
            return 1;
        }
    }

    {
        common::config::SimpleConfig config;
        const std::string path = "mysql_rw_config_new.conf";
        {
            std::ofstream output(path);
            output << "storage.player.mysql.writer.host=writer-host\n";
            output << "storage.player.mysql.writer.port=3309\n";
            output << "storage.player.mysql.writer.user=writer-user\n";
            output << "storage.player.mysql.writer.password=writer-pass\n";
            output << "storage.player.mysql.writer.database=writer-db\n";
            output << "storage.player.mysql.writer.pool_size=9\n";
            output << "storage.player.mysql.reader.host=reader-host\n";
            output << "storage.player.mysql.reader.port=3310\n";
            output << "storage.player.mysql.reader.user=reader-user\n";
            output << "storage.player.mysql.reader.password=reader-pass\n";
            output << "storage.player.mysql.reader.database=reader-db\n";
            output << "storage.player.mysql.reader.pool_size=5\n";
        }
        if (!Expect(config.LoadFromFile(path), "expected new mysql config to load")) {
            return 1;
        }

        const auto options = common::mysql::ReadReadWritePoolOptions(config, "storage.player.mysql.", 4);
        if (!Expect(options.writer.connection.host == "writer-host" && options.writer.connection.port == 3309 &&
                        options.writer.pool_size == 9 && options.reader.has_value() &&
                        options.reader->connection.host == "reader-host" &&
                        options.reader->connection.port == 3310 && options.reader->pool_size == 5,
                    "expected new config to expose writer and reader endpoints")) {
            return 1;
        }

        const auto compatibility = common::mysql::ReadConnectionOptions(config, "storage.player.mysql.");
        if (!Expect(compatibility.host == "writer-host" && compatibility.port == 3309,
                    "expected legacy base-prefix reads to fall through to writer config")) {
            return 1;
        }
    }

    return 0;
}
