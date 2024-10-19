#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <string>
#include <mutex>
#include <algorithm>
#include <thread>
#include <vector>
#include "Database.h"
#include <sqlite3.h>
#include "OnlineManager.h"
#include <chrono>
#include <ctime>
#include <iomanip>  // For std::put_time
#include <sstream>
/* 
Compile:
g++ -std=c++11 server.cpp Database.cpp -o server -lsqlite3

TODO
- Remove legacy code, ln curses, chat logs, etc. Implement logs in db
- Restructrue legacy functions
- Turn all message types to ints rather than strings
- add classes to server.cpp
- in db, fix user to have autoincrement id and not manually pulled from server_info
- Friend requests needs to be seperate thread to constantly wait, when launched pull everything from db, 
    then when sent, will update live. If request is sent and user is offline, keep it in db. Same with messages. 
    Threads need to be asynchronous
- Reorg threads to have classes or objects
- For message system, make "active_users_in_chat" variable in message class to be set when switching to that chat to make global
    then can be accessed in the send message and display messages, probably array of user_ids

 */
std::mutex clients_mutex;
std::vector<int> client_sockets;

std::string getCurrentTimestamp() {
    // Get the current time as a time_point
    auto now = std::chrono::system_clock::now();

    // Convert time_point to time_t, which represents calendar time
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // Convert time_t to tm structure for local time
    std::tm* local_time = std::localtime(&now_time);

    // Create a stringstream to format the timestamp as a string
    std::ostringstream timestamp;
    timestamp << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");

    // Return the formatted string
    return timestamp.str();
}
std::string read_header(int client_socket) {
    std::string header;
    char c;
    while (true) {
        ssize_t bytes_received = recv(client_socket, &c, 1, 0);
        if (bytes_received <= 0) {
            return ""; 
        }
        if (c == '\n') {
            break;  
        }
        header += c;
    }
    return header;
}
std::string read_pipe_ended_gen_data(int client_socket) 
{
    std::string data;
    char c;

    while (true) {
        ssize_t bytes_received = recv(client_socket, &c, 1, 0);
        if (bytes_received <= 0) {
            return ""; 
        }
        if (c == '|') {
            break;  
        }
        data += c; 
    }
    return data;
}
//user_id is client's id
void handle_message_client(int client_socket, Database* DB, OnlineManager& user_management_system, int user_id)
{
    while (true)
    {
        std::cout << "Reading MESSAGE Header" << std::endl;
        std::string data_type = read_header(client_socket);
        std::cout << "data type read: " << data_type << std::endl;
        std::cout << "MESSAGE THREAD WORKING" << std::endl;
        
        if (data_type.empty()) {
        std::cerr << "Error: Client Disconnected or No Header Sent" << std::endl;
        break;
        }

	    if (data_type == "image") {
         char buffer[1024];
	    std::vector<char> image_data;
	    size_t bytes_rec;
	    while ((bytes_rec = recv(client_socket, buffer, sizeof(buffer), 0)) > 0)
        {
             image_data.insert(image_data.end(), buffer, buffer + bytes_rec);
	     if (bytes_rec < sizeof(buffer))
             {
		 std::cout << "image recieved" << std::endl;
		 //should prob be break
             }
	    }
        }
    if(data_type == "init_chat")
    {
        
        //recv the id's of everyone in the chat, for now just 1. Later refactor to except arr for gc
        std::string msg_id = read_pipe_ended_gen_data(client_socket);
        std::cout << "ID read to init chat: " << msg_id << std::endl;
        std::vector<int> member_id_list = {stoi(msg_id), user_id};
        std::vector<std::vector<std::string>> chats =  DB->pull_chat_messages(member_id_list);

        for(auto data : chats)
        {
            std::string data_sendable = data[0] + "\\+" + data[1] + "\\-" + data[2] + "\\|";
            ssize_t bytes_sent = send(client_socket, data_sendable.c_str(), data_sendable.size(), 0);
        }
        ssize_t bytes_sent = send(client_socket, "-", 1, 0);
        std::cout << "Finished initializing chat" << std::endl;
        //pull all chat times, sender, and content per id in messages table



    } else if (data_type == "get_all_chats")
    {
    std::vector<std::vector<std::string>> chats = DB->pull_non_exclusive_chat_messages(user_id);

    for (const auto& data : chats)
    {
        std::string data_sendable = data[0] + "\\+" + data[1] + "\\-" + data[2] + "\\]" + data[3] + "\\[" + data[4] + "\\|";
        std::cout << "DATA SENT: " << data_sendable << std::endl;
        ssize_t bytes_sent = send(client_socket, data_sendable.c_str(), data_sendable.size(), 0);
        if (bytes_sent == -1)
        {
            std::cerr << "Failed to send chat message data to client." << std::endl;
            break;
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ssize_t end_signal_sent = send(client_socket, "-", 1, 0);
    if (end_signal_sent == -1)
    {
        std::cerr << "Failed to send termination signal to client." << std::endl;
    }

    std::cout << "Finished sending all chats" << std::endl;
    
    } else if (data_type == "incoming_message") {
    std::string data;
    char c;
    bool found_backslash = false;  // To track when '\' is found
    std::string username;          // Declare username
    std::string message_contents;  // Declare message_contents

    while (true) {
        ssize_t bytes_received = recv(client_socket, &c, 1, 0);  // Declare bytes_received
        if (bytes_received <= 0) {
            data = "";
            std::cerr << "ERROR: no data received in message" << std::endl;
            break;
        }
        data += c;
        if (found_backslash && c == '|') {
            break;
        }
        found_backslash = (c == '\\');
    }

    std::cout << "BROKEN: DATA: " << data << std::endl;
    int receiver_user_id;
    if (!data.empty()) {
        size_t dash_pos = data.find("\\-");
        if (dash_pos != std::string::npos) {
            username = data.substr(0, dash_pos);

            size_t plus_pos = data.find("\\+", dash_pos + 2);
            if (plus_pos != std::string::npos) {
                std::string user_id_str = data.substr(dash_pos + 2, plus_pos - (dash_pos + 2));
                receiver_user_id = std::stoi(user_id_str);  // Declare receiver_user_id

                size_t pipe_pos = data.find("\\|", plus_pos + 2);
                if (pipe_pos != std::string::npos) {
                    message_contents = data.substr(plus_pos + 2, pipe_pos - (plus_pos + 2));

                    std::cout << "Username: " << username << " UserID: " << receiver_user_id << " Message: " << message_contents << std::endl;
                }
            }
        }

        int sender_socket = user_management_system.getSocket(receiver_user_id);
        if (sender_socket != -1) {
            std::string timestamp = getCurrentTimestamp();
            std::string message_complete = timestamp + "\\+" + username + "\\-" + std::to_string(user_id) + "\\]" + std::to_string(receiver_user_id) + "\\[" + message_contents + "\\|";
            ssize_t bytes_sent = send(sender_socket, message_complete.c_str(), message_complete.size(), 0);
            // Log in database
            std::string insert_query = "INSERT INTO messages(sender_id, receiver_1, message_text, sender_username)VALUES('" + std::to_string(user_id) + "', '" + std::to_string(receiver_user_id) + "', '" + message_contents + "', '" + username + "');";
            int insert_status = DB->generic_insert_function(insert_query);
        } else {
            // Log in database
            std::string insert_query = "INSERT INTO messages(sender_id, receiver_1, message_text, sender_username)VALUES('" + std::to_string(user_id) + "', '" + std::to_string(receiver_user_id) + "', '" + message_contents + "', '" + username + "');";
            int insert_status = DB->generic_insert_function(insert_query);
        }
    }



    } else {
         std::cerr << "Unkown message data type: " << data_type << std::endl;
	}
    }
    {
    std::lock_guard<std::mutex> lock(clients_mutex);
	client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
    }
    close(client_socket);
}

void handle_relations_client(int client_socket, Database* DB, int user_id)
{
//TODO - fix friend_requests table (1-username error)
/* 

*/
sqlite3* Database = DB->get_database();
while (true)
{
    std::cout << "Reading RELATIONS Header" << std::endl;
    std::string data_type = read_header(client_socket);
    std::cout << "data type read: " << data_type << std::endl;
    
    std::cout << "RELATIONS THREAD WORKING" << std::endl;

    if (data_type.empty()) {
    std::cerr << "Error: Client Disconnected or No Header Sent" << std::endl;
    break;
    }

    //error handle to send message of already accepted if trying to add current friend
    if (data_type == "new_friend_request")
    {
        std::string data = read_pipe_ended_gen_data(client_socket);
        std::cout << "Friend request data recieved" << data << std::endl;

        std::string receiver_username = data.substr(0, data.find('+')); 
        size_t plus_pos = data.find("+");
        size_t dash_pos = data.find("-", plus_pos + 1);  // Start searching for '-' after '+'
        std::string sender_id = data.substr(plus_pos + 1, dash_pos - plus_pos - 1);
        std::string sender_username = data.substr(dash_pos + 1);

        std::cout << "Receiver Username: " << receiver_username << std::endl;
        std::cout << "Sender ID: " << sender_id << std::endl;
        std::cout << "Sender Username: " << sender_username << std::endl;

        //1. check if the user exists
        int user_exists = DB->check_unique_username(receiver_username); 
        if (user_exists == 0)
        {
            //send back error that it doesnt exists
            std::string error_msg = "User to be added does not exist|";
            std::cout << error_msg << std::endl;
            send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        } else if (user_exists == 1)
        {
            // process request
            std::cout << "User to be added exists" << std::endl;

            //get receiver id
            std::string receiver_id = DB->get_receiver_id(receiver_username);

            //check for duplicatesssssss!!!!!!!!! ADDDd----------------------

            int exists = DB->verify_unique_friend_request(sender_id, receiver_id);

            if (exists == 0){
                //process
                std::string query = "INSERT INTO friend_requests(sender_id, reciever_id, sender_username, receiver_username) VALUES('" + sender_id + "', '" + receiver_id + "', '" + sender_username + "', '" + receiver_username + "');";
                int result_of_insert = DB->generic_insert_function(query);
                std::cout << "result of id inserts: " << result_of_insert << std::endl; 
                std::string error_msg = "Request Sen1!|";
                std::cout << error_msg << std::endl;
                send(client_socket, error_msg.c_str(), error_msg.size(), 0);
            }

            else if (exists == 1){
                //send back duplicat message
                std::string error_msg = "Request to this user already pending|";
                std::cout << error_msg << std::endl;
                send(client_socket, error_msg.c_str(), error_msg.size(), 0);
            }
            else {
                //send back error
                std::string error_msg = "Verification of request failed|";
                std::cout << error_msg << std::endl;
                send(client_socket, error_msg.c_str(), error_msg.size(), 0);
            }

            //sent back success or failure
        } else {
            //send another sort of error
            std::string error_msg = "ERROR: Server error, please try again later.|";
            std::cout << error_msg << std::endl;
            send(client_socket, error_msg.c_str(), error_msg.size(), 0);
        }
    } else if (data_type == "get_incoming_friends")
    {
        sqlite3_stmt *stmt;

        // Prepare the query to retrieve incoming friend requests
        std::string query = "SELECT sender_id, sender_username FROM friend_requests WHERE reciever_id = '" + std::to_string(user_id) + "' AND status = 'pending';";
        const char *query_cstr = query.c_str();
        int rc;

        rc = sqlite3_prepare_v2(Database, query_cstr, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            std::cerr << "Failed to prepare friend retrieval statement: " << sqlite3_errmsg(Database) << std::endl;
            // Optionally send an error message back to the client here
        }
        else
        {
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
            {
                int sender_id = sqlite3_column_int(stmt, 0);
                const unsigned char *sender_username = sqlite3_column_text(stmt, 1);

                if (sender_username == NULL) {
                    std::cerr << "Error: NULL sender username." << std::endl;
                    continue;
                }

                std::string data_to_send = std::to_string(sender_id) + "+" + std::string(reinterpret_cast<const char *>(sender_username)) + "|";
                std::cout << "DATA TO SEND IN get_incoming_friend_requests: " << data_to_send << std::endl;
                ssize_t bytes_sent = send(client_socket, data_to_send.c_str(), data_to_send.size(), 0);

                if (bytes_sent == -1)
                {
                    std::cerr << "Failed to send friend request data to client." << std::endl;
                    break;
                }

                std::cout << "Sender ID: " << sender_id << ", Sender Username: " << sender_username << std::endl;
            }

            if (rc != SQLITE_DONE)
            {
                std::cerr << "Error during friend request iteration: " << sqlite3_errmsg(Database) << std::endl;
            }

            sqlite3_finalize(stmt);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            ssize_t end_signal_sent = send(client_socket, "-", 1, 0);
            if (end_signal_sent == -1)
            {
                std::cerr << "Failed to send termination signal to client." << std::endl;
            }
        }
    

    } else if (data_type == "accept_friend_request")
    {
        std::string sender_id = read_pipe_ended_gen_data(client_socket); 
        int receiver_id = user_id;

        int err_msg = DB->handle_friend_request(std::stoi(sender_id), receiver_id, "accepted");

        if (err_msg != 0)
        {
            //send error
            std::string msg = "accept_failed|";
            std::cerr << msg << std::endl;
            send(client_socket, msg.c_str(), msg.size(), 0);
        } else if (err_msg == 0)
        {
            //send sucess
            std::string msg = "accept_succeeded|";
            std::cerr << msg << std::endl;
            send(client_socket, msg.c_str(), msg.size(), 0);
        }

    } else if (data_type == "decline_friend_request")
    { 
        std::string sender_id = read_pipe_ended_gen_data(client_socket);
        int receiver_id = user_id;

        int err_msg = DB->handle_friend_request(std::stoi(sender_id), receiver_id, "declined");

        if (err_msg !=0)
        {
            std::string msg = "decline_failed|";
            std::cerr << msg << std::endl;
            send(client_socket, msg.c_str(), msg.size(), 0);
        } else if (err_msg == 0)
        {
            std::string msg = "decline_succeeded|";
            std::cerr << msg << std::endl;
            send(client_socket, msg.c_str(), msg.size(), 0);
        }

    } else if (data_type == "get_outbound_friends")
    {
        sqlite3_stmt *stmt;

        // Prepare the query to retrieve incoming friend requests
        std::string query = "SELECT reciever_id, receiver_username FROM friend_requests WHERE sender_id = '" + std::to_string(user_id) + "' AND status = 'pending';";
        const char *query_cstr = query.c_str();
        int rc;

        rc = sqlite3_prepare_v2(Database, query_cstr, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            std::cerr << "Failed to prepare friend retrieval statement: " << sqlite3_errmsg(Database) << std::endl;
            // Optionally send an error message back to the client here
        }
        else
        {
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
            {
                int sender_id = sqlite3_column_int(stmt, 0);
                const unsigned char *sender_username = sqlite3_column_text(stmt, 1);

                if (sender_username == NULL) {
                    std::cerr << "Error: NULL sender username." << std::endl;
                    continue;
                }

                std::string data_to_send = std::to_string(sender_id) + "+" + std::string(reinterpret_cast<const char *>(sender_username)) + "|";
                ssize_t bytes_sent = send(client_socket, data_to_send.c_str(), data_to_send.size(), 0);

                if (bytes_sent == -1)
                {
                    std::cerr << "Failed to send friend request data to client." << std::endl;
                    break;
                }

                std::cout << "Sender ID: " << sender_id << ", Sender Username: " << sender_username << std::endl;
            }

            if (rc != SQLITE_DONE)
            {
                std::cerr << "Error during friend request iteration: " << sqlite3_errmsg(Database) << std::endl;
            }

            sqlite3_finalize(stmt);

            ssize_t end_signal_sent = send(client_socket, "-", 1, 0);
            if (end_signal_sent == -1)
            {
                std::cerr << "Failed to send termination signal to client." << std::endl;
            }
        }
    } else if (data_type == "get_friends")
    {
        sqlite3_stmt *stmt;

        // Prepare the query to retrieve incoming friend requests
        std::string query = "SELECT sender_id, sender_username FROM friend_requests WHERE reciever_id = '" 
                    + std::to_string(user_id) + "' AND status = 'accepted' "
                    "UNION "
                    "SELECT reciever_id, receiver_username FROM friend_requests WHERE sender_id = '" 
                    + std::to_string(user_id) + "' AND status = 'accepted';";
        const char *query_cstr = query.c_str();
        int rc;

        rc = sqlite3_prepare_v2(Database, query_cstr, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            std::cerr << "Failed to prepare friend retrieval statement: " << sqlite3_errmsg(Database) << std::endl;
            // Optionally send an error message back to the client here
        }
        else
        {
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
            {
                int sender_id = sqlite3_column_int(stmt, 0);
                const unsigned char *sender_username = sqlite3_column_text(stmt, 1);

                if (sender_username == NULL) {
                    std::cerr << "Error: NULL sender username." << std::endl;
                    continue;
                }

                std::string data_to_send = std::to_string(sender_id) + "+" + std::string(reinterpret_cast<const char *>(sender_username)) + "|";
                ssize_t bytes_sent = send(client_socket, data_to_send.c_str(), data_to_send.size(), 0);

                if (bytes_sent == -1)
                {
                    std::cerr << "Failed to send friend request data to client." << std::endl;
                    break;
                }

                std::cout << "Sender ID: " << sender_id << ", Sender Username: " << sender_username << std::endl;
            }

            if (rc != SQLITE_DONE)
            {
                std::cerr << "Error during friend request iteration: " << sqlite3_errmsg(Database) << std::endl;
            }

            sqlite3_finalize(stmt);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ssize_t end_signal_sent = send(client_socket, "-", 1, 0);
            if (end_signal_sent == -1)
            {
                std::cerr << "Failed to send termination signal to client." << std::endl;
            }
        }
    } else if (data_type == "new_conversation") 
    {
        std::string buff = read_pipe_ended_gen_data(client_socket);
        buff += "|";
        std::vector<int> id_list;
        int count = 0;
        char i = buff[count];

        while (i != '|') 
        {
            std::string tmp_str = "";
            while (i != '-' && i != '|') {
                tmp_str += i;
                count++;
                
                if (count >= buff.length()) break;
                
                i = buff[count];
            }
            if (!tmp_str.empty()) {
                id_list.push_back(std::stoi(tmp_str));
            }
            count++;
            if (count < buff.length()) {
                i = buff[count];
            } else {
                break;
            }
        }
        int conversation_id = DB->create_conversation("");

        for(auto &id : id_lists)
        {
            bool status = DB->addMemberToConversation(conversation_id, id);
        }
        //send back good status
        //pick up here

    } else {
        std::cerr << "Unkown message data type: " << data_type << std::endl;
}
}
{
std::lock_guard<std::mutex> lock(clients_mutex);
client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
}
close(client_socket);
}
void handle_logic_client(int logic_client_socket, Database* DB)
{
    //once verified, pull user id and assign to this thread
    while (true)
    {
        std::cout << "Reading LOGIC Header" << std::endl;
        std::string data_type = read_header(logic_client_socket);
        std::cout << "data type read: " << data_type << std::endl;
        std::cout << "LOGIC THREAD WORKING" << std::endl;

        if (data_type.empty()) {
        std::cerr << "Error: Client Disconnected or No Header Sent" << std::endl;
        break;
        }

	 if (data_type == "verify_user")
     {
        std::cerr << "HIT VERIFY USER" << std::endl;
        std::string data;
        char buffer[1024];
        size_t bytes_rec = recv(logic_client_socket, buffer, sizeof(buffer), 0);
        if (bytes_rec > 0)
        {
//---Breaking here, not recieving fill buffer, something else is receiving the beginning
            data = std::string(buffer, bytes_rec);
            std::cout << "Recieved user data to verify: " << data << std::endl;
        }
        else 
        {
            std::cerr << "Verification info failed to recieve" << std::endl;
        }
        //comes in format USERNAME|PASSWORD
        std::string username = data.substr(0, data.find('|'));
        std::string password = data.substr(data.find('|') + 1);

        std::cout << "Username: " << username << "\nPassword: " << password << std::endl;

        int verification_status = DB->db_verify_login(username, password);

        if (verification_status == 1){
            std::cerr << "User Verified. Good!" << std::endl;
            std::string msg = "verification_succeeded|";
            send(logic_client_socket, msg.c_str(), msg.size(), 0);
        } else if (verification_status == -1) {
            std::cerr << "Error Verifying User" << std::endl;
            //send to client
            std::string msg = "Error Verifying|";
            send(logic_client_socket, msg.c_str(), msg.size(), 0);
        } else if (verification_status == 0) {
            std::cout << "User Not Found" << std::endl;
            std::string msg = "verification_failed|";
            send(logic_client_socket, msg.c_str(), msg.size(), 0);
        }
        else{
            std::cerr << "Major error verifying" << std::endl;
        }
        std::cout << "Verification completed" << std::endl;

     } else if (data_type == "new_user")
     {
        std::cerr << "New User Process Started -----------" << std::endl;
        //add pfp insert later
        std::string data;
        char buffer[1024];
        size_t bytes_rec = recv(logic_client_socket, buffer, sizeof(buffer), 0);
        if (bytes_rec > 0)
        {
            data = std::string(buffer, bytes_rec);
            std::cout << "Recieved user data to insert: " << data << std::endl;
        }
        else 
        {
            std::cerr << "Insertion info failed to recieve" << std::endl;
        }
        //comes in format USERNAME|PASSWORD
        std::string username = data.substr(0, data.find('|'));
        std::string password = data.substr(data.find('|') + 1);

        std::cout << "New Username: " << username << "\nNew Password: " << password << std::endl;

        //check if username already in use
        int username_in_use = DB->check_unique_username(username);

        if (username_in_use == 0){
            std::cerr << "Username Available!" << std::endl;
            std::string msg = "Username Available|";
            send(logic_client_socket, msg.c_str(), msg.size(), 0);
        } else if (username_in_use == -1) {
            std::cerr << "Error Checking Username" << std::endl;
            //send to client
            std::string msg = "Error Checking Username|";
            send(logic_client_socket, msg.c_str(), msg.size(), 0);
        } else if (username_in_use == 1) {
            std::cout << "Username Taken!" << std::endl;
            std::string msg = "Username Taken|";
            send(logic_client_socket, msg.c_str(), msg.size(), 0);

        }

        if (username_in_use == 0){
            int insert_return = DB->db_new_user(username, password);

            if (insert_return == -1){
                std::cerr << "User not added" << std::endl;
            }
            else if (insert_return == 0){
                std::cout << "User added" << std::endl;
            }
        } 
        else {
            std::cout << "Username not unique so its not added" << std::endl;
            }
        } else if (data_type == "get_user_id")
        {

            std::string username_to_check = read_pipe_ended_gen_data(logic_client_socket);
            
            std::cout << "GETTING ID FOR USERNAME: " << username_to_check;
            std::string id_str = DB->get_receiver_id(username_to_check);
            id_str += "|";

            std::cout << "ID_STR: " << id_str << std::endl;
            send(logic_client_socket, id_str.c_str(), id_str.size(), 0);
            std::cout << "ID SENT TO CLIENT" << std::endl;
        }

    else {
         std::cerr << "Unkown message data type: " << data_type << std::endl;
	}
    }
    {
    std::lock_guard<std::mutex> lock(clients_mutex);
	client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), logic_client_socket), client_sockets.end());
    }
    close(logic_client_socket);
}
int main()
{
	
	Database* DB = new Database;
    OnlineManager user_management_system;

    std::string chat_line;
    std::cout << "Starting the server ..." << std::endl;

    std::cout << "_____Message History Below_____" << std::endl;

    std::fstream chat_logs("chat_logs", std::ios::in | std::ios::out | std::ios::app);
    if (!chat_logs.is_open())
    {
        std::cerr << "Error opening chat log file!" << std::endl;
        return 1;
    }

    while (getline(chat_logs, chat_line))
    {
	if (chat_line != "")
	{
            std::cout << chat_line << std::endl;
	}
    }
    chat_logs.close();

    //create socket (af_inet = ipv4, sock_stream = tcp, 0 is default for something idk
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        std::cerr << "Error creating the socket!" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr;
    //set the server connection family to ipv4
    serverAddr.sin_family = AF_INET;
    //set the server to listen form any incoming ip and not a specific one
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(4000);


    //binds our server socket to aforementioned server address, also casts serverAddr (sockaddr_in)
    //to a type sockadr struct
    if (bind(server_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
    {
        std::cerr << "Error binding the socket!" << std::endl;
        close(server_socket);
        return 1;
    }

    //sets socket up to listen for connections, takes in server socket and max queue
    if (listen(server_socket, 20) == -1)
    {
        std::cerr << "Error listening on the socket!" << std::endl;
        close(server_socket);
        return 1;
    }


    char buff[1024];
    std::fstream chat_logs_append("chat_logs", std::ios::app);

    //handle incoming connections
    while (true)
    {

    int client_socket = accept(server_socket, NULL, NULL);
	if (client_socket == -1)
	{
	std::cerr << "Error accepting connection!" << std::endl;
	close(server_socket);
	return 1;
    	}

	{
	    std::lock_guard<std::mutex> lock(clients_mutex);
	    client_sockets.push_back(client_socket);
	}


    std::string handshake_id_data = read_pipe_ended_gen_data(client_socket);

    std::string client_handshake = handshake_id_data.substr(0, handshake_id_data.find("+"));
    std::string user_id_str = handshake_id_data.substr(handshake_id_data.find("+") + 1);
    int user_id = atoi(user_id_str.c_str());

    std::cout << "HANDSHAKE: " << client_handshake << " USER ID: " << user_id << std::endl;

    if (client_handshake == "LOGIC_MANAGEMENT")
    {
        std::thread(handle_logic_client, client_socket, DB).detach();
    }
    else if (client_handshake == "MESSAGE_MANAGEMENT")
    {
        user_management_system.addUser(user_id, client_socket);
        std::thread(handle_message_client, client_socket, std::ref(DB), std::ref(user_management_system), user_id).detach();
    }
    else if (client_handshake == "RELATION_MANAGEMENT")
    {
        std::thread(handle_relations_client, client_socket, DB, user_id).detach();
    }
    else
    {
        //eventually this should be an error and reject
	    //std::thread(handle_client, client_socket, DB).detach();
    }

    }

    close(server_socket);
    return 0;
}
