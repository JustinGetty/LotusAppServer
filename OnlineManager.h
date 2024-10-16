#ifndef ONLINEMANAGER_H
#define ONLINEMANAGER_H

#include <unordered_map>
#include <shared_mutex>  // For std::shared_mutex

class OnlineManager {
public:
    OnlineManager();  // Constructor

    void addUser(int user_id, int user_socket);
    void removeUser(int user_id);
    bool isOnline(int user_id) const;
    int getSocket(int user_id) const;

private:
    std::unordered_map<int, int> online_users;  // Maps user ID to socket
    mutable std::shared_mutex map_mutex;  // Shared mutex for concurrent reads
};

#endif  // ONLINEMANAGER_H
