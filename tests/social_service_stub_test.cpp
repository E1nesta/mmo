#include "modules/social/application/social_service.h"
#include "modules/social/infrastructure/in_memory_social_repository.h"

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
    game_server::social::InMemorySocialRepository social_repository;
    game_server::social::SocialService social_service(social_repository);

    const auto friends = social_service.ListFriends(20001);
    if (!Expect(friends.success && friends.items.empty(), "expected social stub friends list to be empty")) {
        return 1;
    }

    const auto conversations = social_service.ListConversations(20001);
    if (!Expect(conversations.success && conversations.items.empty(),
                "expected social stub conversation list to be empty")) {
        return 1;
    }

    const auto history = social_service.GetChatHistory(20001, "conversation-1");
    if (!Expect(history.success && history.items.empty(), "expected social stub chat history to be empty")) {
        return 1;
    }

    return 0;
}
