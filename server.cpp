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
#include "user.h"
#include <sstream>
#include <filesystem>
#include <sys/types.h>
#include <sys/stat.h>

/* 
Compile:
g++ -std=c++17 server.cpp OnlineManager.cpp Database.cpp user.cpp -o server -lsqlite3

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
//

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
        //SELECT DISTINCT m.timestamp, m.sender_username, m.sender_id, m.conversation_id, m.message_text, m.image_path, m.message_id 
        std::cout << "Chats pulled successfully" << std::endl;
        for (const auto& data : chats)
        {
            int image_size = 0;
            std::string image_path = data[5];
            std::cout << "Image path: " << image_path << std::endl;
            if(!image_path.empty())
            {
                //pull image and send
                std::ifstream image_file(image_path, std::ios::binary);
                if (image_file) {
                    std::vector<char> image_data((std::istreambuf_iterator<char>(image_file)),
                                                std::istreambuf_iterator<char>());
                    image_file.close();
                    image_size = image_data.size();
                    std::cout << "Image Size: " << image_size << std::endl;
                    // Now image_data contains the contents of the image file
                    // You can process it as needed
                    std::string data_sendable = data[0] + "\\+" + data[1] + "\\-" + data[2] + "\\]" + data[3] + "\\[" + data[4] + "\\$" + data[6] + "\\~" + std::to_string(image_size) + "\\|";
                    std::cout << "Data to send: " << data_sendable << std::endl;
                    ssize_t bytes_sent = send(client_socket, data_sendable.c_str(), data_sendable.size(), 0);
                    if(bytes_sent > 0)
                    {
                        //send image
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        ssize_t image_bytes_sent = send(client_socket, image_data.data(), image_size, 0);

                        if (image_bytes_sent == image_size)
                        {
                            std::cout << "Finished sending image" << std::endl;
                        }

                    }
                    else
                    {
                        std::cout << "Not attempting image send, init message failed to be received" << std::endl;
                    }
                } else 
                {
                    std::cerr << "ERROR: Unable to open image file" << std::endl;
                    std::string data_sendable = data[0] + "\\+" + data[1] + "\\-" + data[2] + "\\]" + data[3] + "\\[" + data[4] + "\\$" + data[6] + "\\~" + std::to_string(image_size) + "\\|";
                    //send without image
                    std::cout << "DATA SENT: " << data_sendable << std::endl;
                    ssize_t bytes_sent = send(client_socket, data_sendable.c_str(), data_sendable.size(), 0);
                    if (bytes_sent == -1)
                    {
                        std::cerr << "Failed to send chat message data to client." << std::endl;
                        break;
                    }
                }
            }
            else
            {
                //send without image
                std::string data_sendable = data[0] + "\\+" + data[1] + "\\-" + data[2] + "\\]" + data[3] + "\\[" + data[4] + "\\$" + data[6] + "\\~" + std::to_string(image_size) + "\\|";
                std::cout << "DATA SENT: " << data_sendable << std::endl;
                ssize_t bytes_sent = send(client_socket, data_sendable.c_str(), data_sendable.size(), 0);
                if (bytes_sent == -1)
                {
                    std::cerr << "Failed to send chat message data to client." << std::endl;
                    break;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::string termination_signal = "#";
        ssize_t end_signal_sent = send(client_socket, termination_signal.c_str(), termination_signal.size(), 0);
        if (end_signal_sent == -1)
        {
            std::cerr << "Failed to send termination signal to client." << std::endl;
        }

        std::cout << "Finished sending all chats" << std::endl;
    
    } else if (data_type == "incoming_message") {
        std::string data;
        char c;
        bool found_backslash = false;
        std::string username;
        std::string message_contents;
        int conversation_id;
        int image_size = 0;

        // Read message metadata until '\\|'
        while (true) {
            ssize_t bytes_received = recv(client_socket, &c, 1, 0);
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

        if (!data.empty()) {
            // Parse data
            size_t dash_pos = data.find("\\-");
            if (dash_pos != std::string::npos) {
                username = data.substr(0, dash_pos);

                size_t plus_pos = data.find("\\+", dash_pos + 2);
                if (plus_pos != std::string::npos) {
                    std::string convo_id_str = data.substr(dash_pos + 2, plus_pos - (dash_pos + 2));
                    conversation_id = std::stoi(convo_id_str);

                    size_t dollar_pos = data.find("\\$", plus_pos + 2);
                    if (dollar_pos != std::string::npos) {
                        message_contents = data.substr(plus_pos + 2, dollar_pos - (plus_pos + 2));

                        size_t pipe_pos = data.find("\\|", dollar_pos + 2);
                        if (pipe_pos != std::string::npos) {
                            std::string image_size_str = data.substr(dollar_pos + 2, pipe_pos - (dollar_pos + 2));
                            image_size = std::stoi(image_size_str);
                        }
                    }
                }
            }

            // Read image data if present
            std::vector<char> image_data;
            if (image_size > 0) {
                image_data.resize(image_size);
                size_t total_received = 0;
                while (total_received < image_size) {
                    ssize_t bytes_received = recv(client_socket, &image_data[total_received], image_size - total_received, 0);
                    if (bytes_received <= 0) {
                        std::cerr << "ERROR: Failed to receive image data" << std::endl;
                        break;
                    }
                    total_received += bytes_received;
                }
            }

            // **Insert the message into the database without image_path**
            std::string insert_query = "INSERT INTO messages(sender_id, conversation_id, message_text, sender_username) VALUES(?, ?, ?, ?);";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(DB->get_database(), insert_query.c_str(), -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_int(stmt, 2, conversation_id);
            sqlite3_bind_text(stmt, 3, message_contents.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, username.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::cerr << "ERROR: Failed to insert message into database." << std::endl;
            }
            sqlite3_finalize(stmt);

            // **Retrieve the last inserted message_id**
            int message_id = sqlite3_last_insert_rowid(DB->get_database());

            std::string image_path = "";
            if (image_size > 0) {
                // **Save image data to file with message_id in filename**
                std::string image_directory = "./IMAGEMESSAGE/";
                std::string image_filename = "image_message" + std::to_string(message_id);
                image_path = image_directory + image_filename;

                // Ensure directory exists
                struct stat st = {0};
                if (stat(image_directory.c_str(), &st) == -1) {
                    mkdir(image_directory.c_str(), 0700);
                }

                std::ofstream image_file(image_path, std::ios::binary);
                if (image_file) {
                    image_file.write(image_data.data(), image_data.size());
                    image_file.close();
                } else {
                    std::cerr << "ERROR: Unable to save image to file" << std::endl;
                }

                // **Update the image_path in the database for this message_id**
                std::string update_query = "UPDATE messages SET image_path = ? WHERE message_id = ?;";
                sqlite3_prepare_v2(DB->get_database(), update_query.c_str(), -1, &stmt, nullptr);
                sqlite3_bind_text(stmt, 1, image_path.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, 2, message_id);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    std::cerr << "ERROR: Failed to update image_path in database." << std::endl;
                }
                sqlite3_finalize(stmt);
            }

            // **Send message to conversation members**
            std::vector<int> conversation_member_ids = DB->getUserIdsInConversation(conversation_id);
            for (auto &id : conversation_member_ids) {
                if (id != user_id) {
                    int sock = user_management_system.getSocket(id);
                    if (sock != -1) {
                        std::string timestamp = getCurrentTimestamp();
                        std::string message_complete = timestamp + "\\+" + username + "\\-" + std::to_string(user_id) + "\\]" + std::to_string(conversation_id) + "\\[" + message_contents + "\\$" + std::to_string(image_size) + "\\#" + std::to_string(message_id) + "\\|";
                        send(sock, message_complete.c_str(), message_complete.size(), 0);
                        if (image_size > 0) {
                            send(sock, image_data.data(), image_size, 0);
                        }
                    }
                }
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
        std::cout << "Buffer: " << buff << std::endl;
        buff += "|";
        std::vector<int> id_list;
        int count = 0;
        char i = buff[count];

        while (i != '|') 
        {
            std::string tmp_str = "";
            while (i != '-' && i != '|') {
                std::cout << "i = " << i << std::endl;
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
        id_list.push_back(user_id);
        for(auto &id : id_list)
        {
            bool status = DB->addMemberToConversation(conversation_id, id);
        }
        //send back good status
        //pick up here

} else if (data_type == "get_user_convos") 
{
    sqlite3_stmt *stmt;

    // Parameterized query to prevent SQL injection
    std::string query = "SELECT c.conversation_id, c.conversation_name, cm.user_id AS member_id, u.username AS member_username "
                        "FROM conversations c "
                        "JOIN conversation_members cm ON c.conversation_id = cm.conversation_id "
                        "JOIN users u ON cm.user_id = u.id "
                        "WHERE c.conversation_id IN ( "
                        "    SELECT conversation_id "
                        "    FROM conversation_members "
                        "    WHERE user_id = ? "
                        ") "
                        "ORDER BY c.conversation_id;";

    int rc = sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(Database) << std::endl;
    }
    else
    {
        // Bind user_id to the query
        sqlite3_bind_int(stmt, 1, user_id);
        
        int prev_convo_id = -1;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            int conversation_id = sqlite3_column_int(stmt, 0);
            const unsigned char *conversation_name = sqlite3_column_text(stmt, 1);
            int conversation_member_id = sqlite3_column_int(stmt, 2);
            const unsigned char *conversation_member_username = sqlite3_column_text(stmt, 3);

            std::string data_to_send;
            if (conversation_id != prev_convo_id)
            {
                // New conversation, send a termination signal for the previous conversation
                if (prev_convo_id != -1)
                {
                    // Send termination signal with delimiter
                    if (send(client_socket, "-|", 2, 0) == -1)
                    {
                        std::cerr << "Failed to send conversation termination signal." << std::endl;
                        break;
                    }
                }

                // Send conversation ID and name, ensure name is included even if empty
                data_to_send = std::to_string(conversation_id) + "+" + 
                               (conversation_name ? reinterpret_cast<const char *>(conversation_name) : "") + "|";
                if (send(client_socket, data_to_send.c_str(), data_to_send.size(), 0) == -1)
                {
                    std::cerr << "Failed to send conversation data." << std::endl;
                    break;
                }
            }

            // Send member ID and username
            data_to_send = std::to_string(conversation_member_id) + "+" + 
                           (conversation_member_username ? reinterpret_cast<const char *>(conversation_member_username) : "") + "|";
            if (send(client_socket, data_to_send.c_str(), data_to_send.size(), 0) == -1)
            {
                std::cerr << "Failed to send member data." << std::endl;
                break;
            }

            prev_convo_id = conversation_id;
        }

        if (rc != SQLITE_DONE)
        {
            std::cerr << "SQLite error: " << sqlite3_errmsg(Database) << std::endl;
        }

        // Finalize statement and send termination signals
        sqlite3_finalize(stmt);
        if (prev_convo_id != -1)
        {
            // Send termination signal for the last conversation
            if (send(client_socket, "-|", 2, 0) == -1)
            {
                std::cerr << "Failed to send final conversation termination signal." << std::endl;
            }
        }

        // Send end-of-data signal
        if (send(client_socket, "--|", 3, 0) == -1)
        {
            std::cerr << "Failed to send end-of-data signal." << std::endl;
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
void handle_logic_client(int logic_client_socket, Database* DB, user* User)
{
    //once verified, pull user id and assign to this thread
    while (true)
    {
        std::cout << "Reading LOGIC Header" << std::endl;
        std::string data_type = read_header(logic_client_socket);
        std::cout << "data type read: " << data_type << std::endl;

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
        User->set_user_id(std::stoi(id_str));
        id_str += "|";

        std::cout << "ID_STR: " << id_str << std::endl;
        send(logic_client_socket, id_str.c_str(), id_str.size(), 0);
        std::cout << "ID SENT TO CLIENT" << std::endl;
    } else if (data_type == "upload_pfp")
    {
        uint64_t image_size;
        if (recv(logic_client_socket, &image_size, sizeof(image_size), 0) == 0)
        {
            std::cout << "Received image size" << std::endl;
        }
        else
        {
            std::cerr << "Failed to receive image size." << std::endl;
        }
        std::vector<char> buffer(image_size);
        size_t total_bytes_rec = 0;
        while (total_bytes_rec < image_size)
        {
            ssize_t bytes_received = recv(logic_client_socket, buffer.data() + total_bytes_rec, image_size - total_bytes_rec, 0);
            if (bytes_received <= 0)
            {
                std::cerr << "Failed to receive image data" << std::endl;
                break;
            }
            total_bytes_rec += bytes_received;
        }
        if(image_size == total_bytes_rec)
        {
            std::cout << "Image data received successfully." << std::endl;
            std::string file_name = "PFP_DATA/profile_picture_user_" + std::to_string(User->get_user_id());
            std::ofstream outFile(file_name, std::ios::binary);
            if (outFile)
            {
                outFile.write(buffer.data(), buffer.size());
                outFile.close();
                  std::string query = "UPDATE users SET profile_picture_path = '" + file_name + "' WHERE id = " + std::to_string(User->get_user_id()) + ";";
                DB->generic_insert_function(query);
                std::cout << "Image saved successfully as " << file_name << std::endl;
            }
            else
            {
                std::cerr << "Failed to open file for writing: " << file_name << std::endl;
            }
        }
        else
        {
            std::cerr << "Mismatch between expected and received image size." << std::endl;
        }
    } else if (data_type == "get_profile_pic")
    {
           // Step 1: Retrieve the user ID
        int user_id = User->get_user_id(); // Assuming you have the user ID stored in the User object

        // Step 2: Query the database to get the profile picture path
        std::string query = "SELECT profile_picture_path FROM users WHERE id = '" + std::to_string(user_id) + "';";
        std::string result = DB->get_profile_pic_from_db(query);
        std::cout << "PROFILE PICTURE PATH: " << result << std::endl;
        
        if (result.empty())
        {
            std::cerr << "No profile picture found for user with ID: " << user_id << std::endl;
            std::string no_pfp_image_size = "0";
            ssize_t size_bytes = send(logic_client_socket, no_pfp_image_size.c_str(), no_pfp_image_size.size(), 0);
            if (size_bytes > 0)
            {
                std::cout << "sent image size" << std::endl;
            }
            
            return;
        }

        std::string file_path = result;
        if (file_path.empty())
        {
            std::cerr << "Profile picture path is empty for user with ID: " << user_id << std::endl;
            return;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            return;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        uint64_t image_size = buffer.size();
        std::cout << "Image size: " << image_size << std::endl;
        if (send(logic_client_socket, &image_size, sizeof(image_size), 0) < 0)
        {
            std::cerr << "Failed to send the image size." << std::endl;
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        size_t total_bytes_sent = 0;
        while (total_bytes_sent < image_size)
        {
            ssize_t bytes_sent = send(logic_client_socket, buffer.data() + total_bytes_sent, image_size - total_bytes_sent, 0);
            if (bytes_sent <= 0)
            {
                std::cerr << "Failed to send image data." << std::endl;
                break;
            }
            total_bytes_sent += bytes_sent;
            std::cout << "Total Bytes Sent: " << total_bytes_sent << std::endl;
        }

        if (total_bytes_sent == image_size)
        {
            std::cout << "Profile picture sent successfully for user with ID: " << user_id << std::endl;
        }
        else
        {
            std::cerr << "Failed to send complete image data for user with ID: " << user_id << std::endl;
        } 
    } else {
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
            user* User = new user;
            std::thread(handle_logic_client, client_socket, std::ref(DB), User).detach();
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
