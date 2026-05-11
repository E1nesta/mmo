#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <vector>

#include "runtime/entity/entity_executor.h"
#include "runtime/entity/entity_id.h"
#include "runtime/entity/entity_mailbox.h"
#include "runtime/entity/entity_router.h"
#include "runtime/entity/entity_state_store.h"
#include "runtime/scheduler/sharded_executor.h"

namespace {

runtime::entity::EntityTask make_task(
    runtime::entity::EntityId entity_id,
    std::uint32_t message_id) {
    runtime::entity::EntityTask task;
    task.message.entity_id = entity_id;
    task.message.message_id = message_id;
    task.message.request_id = message_id + 1000U;
    task.message.route_key = entity_id.key;
    task.handler = [](const runtime::entity::EntityMessage&) {};
    return task;
}

void verify_bounded_mailbox() {
    runtime::entity::EntityMailbox mailbox({2});
    const auto player = runtime::entity::player_entity(1198216);

    assert(mailbox.try_push(make_task(player, 1)));
    assert(mailbox.try_push(make_task(player, 2)));
    assert(!mailbox.try_push(make_task(player, 3)));
    assert(mailbox.size() == 2);
    assert(mailbox.dropped_count() == 1);
}

void verify_single_writer_gate() {
    runtime::entity::EntityMailbox mailbox;

    assert(mailbox.try_schedule_drain());
    assert(!mailbox.try_schedule_drain());
    assert(mailbox.try_acquire_writer());
    assert(!mailbox.try_acquire_writer());
    assert(!mailbox.try_schedule_drain());
    mailbox.release_writer();
    assert(mailbox.try_schedule_drain());
}

void verify_executor_drains_entity_in_order() {
    runtime::entity::EntityRouter router;
    runtime::entity::EntityExecutor executor(router);
    runtime::scheduler::ShardedExecutor scheduler(
        runtime::scheduler::ShardedExecutorOptions{2, 32});

    const auto player = runtime::entity::player_entity(1198216);
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::uint32_t> handled;

    for (std::uint32_t message_id : {1U, 2U, 3U}) {
        runtime::entity::EntityMessage message;
        message.entity_id = player;
        message.message_id = message_id;
        message.request_id = message_id + 1000U;
        message.route_key = player.key;

        const auto scheduled = executor.submit(
            scheduler,
            message,
            [&mutex, &cv, &handled](const runtime::entity::EntityMessage& item) {
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    handled.push_back(item.message_id);
                }
                cv.notify_one();
            });
        assert(scheduled.accepted());
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        assert(cv.wait_for(lock, std::chrono::seconds(1), [&handled]() {
            return handled.size() == 3;
        }));
        assert((handled == std::vector<std::uint32_t>{1U, 2U, 3U}));
    }

    scheduler.stop();
}

void verify_state_store_reuses_entity_state() {
    struct PlayerState {
        std::int64_t player_id{};
        int level{};
    };

    runtime::entity::EntityStateStore<std::int64_t, PlayerState> store;
    auto first = store.find_or_create(1198216, [](PlayerState& state) {
        state.player_id = 1198216;
        state.level = 7;
    });
    auto second = store.find_or_create(1198216);
    auto third = store.find_or_create(1198217);

    assert(first == second);
    assert(third != first);
    assert(first->level == 7);
    assert(store.size() == 2);
}

}  // namespace

int main() {
    verify_bounded_mailbox();
    verify_single_writer_gate();
    verify_executor_drains_entity_in_order();
    verify_state_store_reuses_entity_state();

    std::cout << "entity mailbox governance probe ok\n";
    return 0;
}
