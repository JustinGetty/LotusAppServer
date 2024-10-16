#include "OnlineManager.h"
#include <mutex>
OnlineManager::OnlineManager() : online_users() {}

void OnlineManager::addUser(int user_id, int user_socket) {
    std::unique_lock<std::shared_mutex> lock(map_mutex);  // Exclusive lock
    online_users[user_id] = user_socket;
}

void OnlineManager::removeUser(int user_id) {
    std::unique_lock<std::shared_mutex> lock(map_mutex);  // Exclusive lock
    online_users.erase(user_id);
}

bool OnlineManager::isOnline(int user_id) const {
    std::shared_lock<std::shared_mutex> lock(map_mutex);  // Shared lock
    return online_users.find(user_id) != online_users.end();
}

int OnlineManager::getSocket(int user_id) const {
    std::shared_lock<std::shared_mutex> lock(map_mutex);  // Shared lock
    auto it = online_users.find(user_id);
    if (it != online_users.end()) {
        return it->second;
    }
    return -1;  // Return -1 if the user is not found
}
