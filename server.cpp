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

/* 
Compile:
g++ -std=c++11 server.cpp Database.cpp -o server -lsqlite3

TODO
- Remove legacy code, ln curses, chat logs, etc. Implement logs in db
- Restructrue legacy functions
- Critical: New thread per client. Figure out how multiple clients will work, everytime there is 
    a connection, create a new thread.
- Turn all message types to ints rather than strings
- add classes to server.cpp
- in db, fix user to have autoincrement id and not manually pulled from server_info
- Friend requests needs to be seperate thread to constantly wait, when launched pull everything from db, 
    then when sent, will update live. If request is sent and user is offline, keep it in db. Same with messages. 
    Threads need to be asynchronous
- Reorg threads to have classes or objects
 */
std::mutex clients_mutex;
std::vector<int> client_sockets;

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

void handle_message_client(int client_socket, Database* DB, int user_id)
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

    else {
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
                    std::string query = "INSERT INTO friend_requests(sender_id, reciever_id, sender_username) VALUES('" + sender_id + "', '" + receiver_id + "', '" + sender_username + "');";
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
                //insert query here

                //FIXXXXX-------------------------------
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

            sqlite3* Database = DB->get_database();
            std::string query = std::string("SELECT sender_id, sender_username FROM friend_requests WHERE reciever_id = '") + std::to_string(user_id) + "';";
            const char *query_cstr = query.c_str();
            int rc;

            rc = sqlite3_prepare_v2(Database, query_cstr, -1, &stmt, NULL);
            if (rc != SQLITE_OK)
            {
                std::cerr << "Failed to prepare friend retrieval statement" << sqlite3_errmsg(Database) << std::endl;
                //send back error here
            }
            else if (rc == SQLITE_OK)
            {
                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
                {
                    int sender_id = sqlite3_column_int(stmt, 0);
                    const unsigned char *sender_username = sqlite3_column_text(stmt, 1);

                    std::cout << "Sender ID: " << sender_id << ", Sender Username: " << sender_username << std::endl;
                    std::string data_to_send = std::to_string(sender_id) + "+" + std::string(reinterpret_cast<const char*>(sender_username)) + "|";
                    send(client_socket, data_to_send.c_str(), sizeof(data_to_send), 0);
                }
                if (rc != SQLITE_DONE)
                {
                    std::cerr << "Error during friend iteration: " << sqlite3_errmsg(Database) << std::endl;
                }
                
                sqlite3_finalize(stmt);
                send(client_socket, "-", 1, 0);
            }
        }

    else {
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
        std::thread(handle_message_client, client_socket, DB, user_id).detach();
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
