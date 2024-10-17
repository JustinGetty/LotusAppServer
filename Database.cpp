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
    int exit = sqlite3_open("/root/infinite/LotusAppServer/Database/ServerDatabase.sqlite", &DB);

    if (exit != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(DB) << std::endl;
        sqlite3_close(DB);
        DB = nullptr;
    } else {
        std::cout << "Opened database successfully!" << std::endl;
    }
}

//first arg us user data to be combined with other shit. Have this return the value and not print it
int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   //loop over all columns in current row(aka argc) 
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");


   return 0;
}


static int verify_callback(void *existsFlag, int argc, char **argv, char **colNames) {
    int *flag = (int*)existsFlag;
    if (argv[0]) {
        *flag = atoi(argv[0]);  // Convert the result ('1' or '0') to an integer and store it in *flag
    }
    return 0;  
}

int update_query(const std::string &query, sqlite3* DB)
{
    char* zErrMsg = 0;
    const char* update_query_c = query.c_str();

    int rc = sqlite3_exec(DB, update_query_c, callback, 0, &zErrMsg);

    if(rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        return -1;
    } else {
        fprintf(stdout, "Successfull friend request status update\n");
        return 0;
    }
}
std::string Database::select_from_database_gen(const std::string& query)
{
    std::string val = "";
    const char* id_query_cstr = query.c_str();
    sqlite3_stmt* statement;

    int result = sqlite3_prepare_v2(DB, id_query_cstr, -1, &statement, 0);

    if (result == SQLITE_OK)
    {
        std::cout << "good gen query" << std::endl;
        int step_result = sqlite3_step(statement);
        if (step_result == SQLITE_ROW){
            val = std::to_string(sqlite3_column_int(statement, 0));
            sqlite3_finalize(statement);
        }
    }
    else {
        return "ERROR IN FUNCTION: select_from_database_gen";
    }
    return val;
}

int Database::generic_insert_function(std::string query)
{

    char* zErrMsg = 0;
    const char* insert_query = query.c_str();

    //int statement_result = sqlite3_prepare_v2(DB, insert_usr_query, -1, &statement, 0);

    int rc = sqlite3_exec(DB, insert_query, callback, 0, &zErrMsg);

    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        return -1;
    } else {
        fprintf(stdout, "Succesful insertion\n");
        return 0;
    }
    
}
int row_exists(sqlite3* DB, const char *sql)
{
    char *errMsg = 0;
    int exists = 0;  

    int rc = sqlite3_exec(DB, sql, verify_callback, &exists, &errMsg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return -1;  
    }

    //1 for exists, 0 for doesnt
    return exists;
}

//call this to test in main
int Database::db_verify_login(const std::string& username, const std::string& password)
{

    if (DB == nullptr) {
        std::cerr << "Database is not open" << std::endl;
        return -1;
    }

    std::string verify_query = "SELECT EXISTS (SELECT 1 FROM users WHERE username = '" + username + "' AND password = '" + password + "');";

    std::cout << verify_query << std::endl;
    const char* query_cstr = verify_query.c_str();

    int exists = row_exists(DB, query_cstr);

    return exists;
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
        return -1;
    } else {
        fprintf(stdout, "Succesful insertion\n");
    }
    //dont forget to update max id in db

    std::string update_max_id_query = "UPDATE server_info SET max_id = " + str_user_id + ";";

    const char* update_query = update_max_id_query.c_str();

    int nc = sqlite3_exec(DB, update_query, callback, 0, &zErrMsg);

    if (nc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        std::cerr << "Max user ID not updated, major problem" << std::endl;
    } else {
        fprintf(stdout, "Successful Update\n");
    }
    return 0;

}

int Database::check_unique_username(const std::string& username)
{
    int exists;
    if (DB == nullptr) {
        std::cerr << "Database is closed, cannot check for username" << std::endl;
        return false;
    }
    std::string check_username_query = "SELECT EXISTS (SELECT 1 FROM users WHERE username = '" + username + "');";
    std::cout << "Check username query: " << check_username_query << std::endl;

    const char* check_username_cquery = check_username_query.c_str();
    exists = row_exists(DB, check_username_cquery);

    return exists;

}

std::string Database::get_receiver_id(const std::string& reciever_username)
{
    /* 
    Server end:
    1. convert recieve username to an id and log in db
    2. user will either accept or decline, sending signal back here which will be processed accordingly. 
    3. if accepted then signal will be send and the friend will be added to both sender and reciever's user table as next available friend

    Client end:
    1. Select all from tables where reciever is (inbound) X or sender id is Y (outbound)
    2. have accept/decline button to update the server, this is where server end starts
    */

    std::string get_id_query = "SELECT id FROM users WHERE USERNAME = '" + reciever_username + "';";
    std::string reciever_id_result = select_from_database_gen(get_id_query);


    //insert
    

    return reciever_id_result;

}

int Database::verify_unique_friend_request(const std::string& sender_id, const std::string& receiver_id)
{

    std::string verify_query = "SELECT EXISTS (SELECT 1 FROM friend_requests WHERE sender_id = '" + sender_id + "' AND reciever_id = '" + receiver_id + "');";
    std::cout << verify_query << std::endl;
    const char* query_cstr = verify_query.c_str();

    //1 for exists 0 for doesnt, -1 for error
    int exists = row_exists(DB, query_cstr);

    return exists;

}

sqlite3* Database::get_database()
{
    return DB;
}


int Database::handle_friend_request(int sender_user_id, int receiver_user_id, const std::string &status)
{

    char* error_msg = 0;  

    std::string accept_request_query = "UPDATE friend_requests SET status = '" + status + "' WHERE sender_id = '" + std::to_string(sender_user_id) + "' AND reciever_id = '" + std::to_string(receiver_user_id) + "';";
    std::cout << "Update Friend Query: " << accept_request_query << std::endl;
    int stat = update_query(accept_request_query, DB);

    if (stat == -1)
    {
        std::cerr << "Error accepting friend request, error running query" << std::endl;
        return -1;

    } else if (stat == 0)
    {
        std::cout << "good friend request update" << std::endl;
        return 0;
    } else {
        std::cerr << "Unknown error handling friend request :(" << std::endl;
        return -1;
    }
}
std::vector<std::vector<std::string>> Database::pull_chat_messages(std::vector<int> member_id_list)
{
    std::string query = "SELECT timestamp, sender_username, message_text FROM messages WHERE sender_id = '" 
                    + std::to_string(member_id_list[0]) + "' AND receiver_1 = '" + std::to_string(member_id_list[1]) + "'"
                    "UNION "
                    "SELECT timestamp, sender_username, message_text FROM messages WHERE sender_id = '" 
                    + std::to_string(member_id_list[1]) + "' AND receiver_1 = '" + std::to_string(member_id_list[0]) + "';";

    std::vector<std::vector<std::string>> result;
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(DB, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(DB) << std::endl;
        return result;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int i = 0; i < 3; ++i) {
            const unsigned char* val = sqlite3_column_text(stmt, i);
            row.emplace_back(val ? reinterpret_cast<const char*>(val) : "");
        }

        result.push_back(row);
    }
    sqlite3_finalize(stmt);
    return result; 
}

std::vector<std::vector<std::string>> Database::pull_non_exclusive_chat_messages(int user_id)
{
    std::string query = "SELECT timestamp, sender_username, sender_id, receiver_1, message_text FROM messages WHERE sender_id = '" 
                    + std::to_string(user_id) + "' "
                    "UNION "
                    "SELECT timestamp, sender_username, sender_id, receiver_1, message_text FROM messages WHERE receiver_1 = '" 
                    + std::to_string(user_id) + "';";

    std::vector<std::vector<std::string>> result;
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(DB, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(DB) << std::endl;
        return result;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int i = 0; i < 5; ++i) {
            const unsigned char* val = sqlite3_column_text(stmt, i);
            row.emplace_back(val ? reinterpret_cast<const char*>(val) : "");
        }
        //std::cout << "MESSAGE CONTENT: " << row[4] << std::endl;
        result.push_back(row);
    }
    sqlite3_finalize(stmt);
    return result; 
}

int Database::insert_message_into_database(int sender_id, int receiver_1, std::string message_text, std::string sender_username)
{
    return -1;
}