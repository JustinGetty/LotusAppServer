#ifndef DATABASE_H
#define DATABASE_H
#include <sqlite3.h>

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
        sqlite3 get_database();
        int generic_insert_function(std::string query);
        int verify_unique_friend_request(const std::string& sender_id, const std::string& receiver_id);

    private:
        sqlite3* DB;
        void init_database();
};
#endif