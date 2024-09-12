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
    private:
        sqlite3* DB;
        void init_database();
};
#endif