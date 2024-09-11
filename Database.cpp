#include <iostream>
#include <sqlite3.h>
#include "Database.h"

Database::Database() : DB(nullptr) {
    init_database();
}

Database::~Database() {
    if (DB != nullptr) {
        sqlite3_close(DB);
        std::cout << "Database closed" << std::endl;
    }
}

void Database::init_database()
{
    
    int exit = sqlite3_open("/home/ubuntu/Server/Database/ServerDatabase.sqlite", &DB);

    if (exit != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(DB) << std::endl;
        sqlite3_close(DB);
        DB = nullptr;
    } else {
        std::cout << "Opened database successfully!" << std::endl;
    }
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}

//call this to test in main
bool Database::db_verify_login(const std::string& username, const std::string& password)
{

    if (DB == nullptr) {
        std::cerr << "Database is not open" << std::endl;
        return -1;
    }

    std::string verify_query = "SELECT EXISTS (SELECT 1 FROM users WHERE username = '" + username + "' AND password = '" + password + "');";

    std::cout << verify_query << std::endl;
    const char* query_cstr = verify_query.c_str();
    sqlite3_stmt* statement;

    int result = sqlite3_prepare_v2(DB, query_cstr, -1, &statement, 0);

    std::cout << "result: " << result << std::endl;
    if (result == SQLITE_OK)
    {
        std::cout << "good query" << std::endl;
        int step_result = sqlite3_step(statement);
        if (step_result == SQLITE_ROW) {
            int exists = sqlite3_column_int(statement, 0);
            sqlite3_finalize(statement);
            return exists;
        }
    } else {
        std::cerr << "Failed to execute ID verification query: " << sqlite3_errmsg(DB) << std::endl;
    }

    sqlite3_finalize(statement);
    return false;
}

int Database::db_new_user(const std::string& username, const std::string& password, const std::string& path_to_pfp)
{

    char* zErrMsg = 0;

/* table: id, username, email, password, profile_pic  */
    if (DB == nullptr) {
        std::cerr << "Database is closed, cannot add user" << std::endl;
        return -1;
    }

    int max_id = -1;

    std::string get_max_id = "SELECT max_id FROM server_info;";

    const char* id_query_cstr = get_max_id.c_str();
    sqlite3_stmt* statement;

    int result = sqlite3_prepare_v2(DB, id_query_cstr, -1, &statement, 0);

    if (result == SQLITE_OK)
    {
        std::cout << "good max id query" << std::endl;
        int step_result = sqlite3_step(statement);
        if (step_result == SQLITE_ROW){
            max_id = sqlite3_column_int(statement, 0);
            sqlite3_finalize(statement);
        }
    }

    std::cout << "Max ID: " << max_id << std::endl;

    int unique_user_id = max_id + 1;
    std::string str_user_id = std::to_string(unique_user_id);

    std::string insert_usr_query;
    if (path_to_pfp == "")
    {

        insert_usr_query = "INSERT INTO users(id, username, password) VALUES(" + str_user_id + ", '" + username + "', '" + password + "');";
        std::cout << "No pfp query: " << insert_usr_query << std::endl;

    }else if (path_to_pfp != ""){

        insert_usr_query = "INSERT INTO users(id, username, password, profile_pic) VALUES(" + str_user_id + ", '" + username + "', '" + password + "', '" + path_to_pfp + "');";
        std::cout << "Pfp query: " << insert_usr_query << std::endl;
    }

    const char* insert_query = insert_usr_query.c_str();

    //int statement_result = sqlite3_prepare_v2(DB, insert_usr_query, -1, &statement, 0);

    int rc = sqlite3_exec(DB, insert_query, callback, 0, &zErrMsg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Succesfull insertion\n");
    }
    //dont forget to update max id in db

    std::string update_max_id_query = "UPDATE server_info SET max_id = " + str_user_id + ";";

    const char* update_query = update_max_id_query.c_str();

    int nc = sqlite3_exec(DB, update_query, callback, 0, &zErrMsg);

    if (nc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Successfull Update\n");
    }
    return 0;

}
