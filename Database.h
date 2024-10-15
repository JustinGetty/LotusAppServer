#ifndef DATABASE_H
#define DATABASE_H
#include <sqlite3.h>
#include <vector>
class Database
{
    public:
        Database();
        ~Database();
        int db_verify_login(const std::string& username, const std::string& password);
        int db_new_user(const std::string& username, const std::string& password, const std::string& path_to_pfp = "");
        int check_unique_username(const std::string& username);
        std::string get_receiver_id(const std::string& reciever_username);
        std::string select_from_database_gen(const std::string& query);
        int generic_insert_function(std::string query);
        int verify_unique_friend_request(const std::string& sender_id, const std::string& receiver_id);
        sqlite3* get_database();
        int handle_friend_request(int sender_user_id, int receiver_user_id, const std::string &status);
        std::vector<std::vector<std::string>> pull_chat_messages(std::vector<int> member_id_list);

    private:
        sqlite3* DB;
        void init_database();
};
#endif