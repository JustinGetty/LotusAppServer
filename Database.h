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
        std::vector<std::vector<std::string>> pull_non_exclusive_chat_messages(int user_id);
        int insert_message_into_database(int sender_id, int receiver_1, std::string message_test, std::string sender_username);
        int create_conversation(const std::string& conversation_name);
        bool addMemberToConversation(int conversation_id, int user_id);

    private:
        sqlite3* DB;
        void init_database();
};
#endif