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
std::string read_pipe_ended_gen_data(int client_socket) {
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
void handle_client(int client_socket, Database* DB)
{

    std::cout << "here" <<std::endl;
    std::string chat_line;
    std::fstream chat_logs("chat_logs", std::ios::in | std::ios::out | std::ios::app);
    while (getline(chat_logs, chat_line))
    {
	if (!chat_line.empty())
	{
	    chat_line += "\n";
	    send(client_socket, chat_line.c_str(), chat_line.size(), 0);
	}
    }
    chat_logs.close();
    
    char buff[1024];
    std::fstream chat_logs_append("chat_logs", std::ios::app);
    /* --------- CLIENT RELATED TESTS AND HANDLING PLAYGROUND ---------------- */








    /* ---------- END PLAYGROUND ------------------------------------------------ */
    while (true)
    {
    
    std::cout << "Reading Header" << std::endl;
    std::string data_type = read_header(client_socket);
    std::cout << "data type read: " << data_type << std::endl;
    if (data_type.empty()) {
        std::cerr << "Error: Client Disconnected or No Header Sent" << std::endl;
        break;
    }
	if (data_type == "text")
	{
	    char buffer[1024];
	    size_t bytes_rec = recv(client_socket, buffer, sizeof(buffer), 0);
	    if (bytes_rec > 0)
	    {
		std::string text(buffer, bytes_rec);
		std::cout << text << std::endl;;

		//write to chat logs
        chat_logs_append << text << std::endl;

    //next two lines are stack overflow code, idk how it works at all 
	{
	    std::lock_guard<std::mutex> lock(clients_mutex);
	    //iterate through all connected sockets(clients) and send the message to each except the og sender
	    for (int sock : client_sockets)
        {
         if (sock != client_socket)

		{
		    send(sock, text.c_str(), text.size(), 0);
		}
	    }
	}
             }
	 } else if (data_type == "image") {
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

	 //we have image here now in image_data to save or display
	 } else if (data_type == "verify_user")
     {
        std::string data;
        char buffer[1024];
        size_t bytes_rec = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_rec > 0)
        {
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
            send(client_socket, msg.c_str(), msg.size(), 0);
        } else if (verification_status == -1) {
            std::cerr << "Error Verifying User" << std::endl;
            //send to client
            std::string msg = "Error Verifying|";
            send(client_socket, msg.c_str(), msg.size(), 0);
        } else if (verification_status == 0) {
            std::cout << "User Not Found" << std::endl;
            std::string msg = "verification_failed|";
            send(client_socket, msg.c_str(), msg.size(), 0);
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
        size_t bytes_rec = recv(client_socket, buffer, sizeof(buffer), 0);
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
            send(client_socket, msg.c_str(), msg.size(), 0);
        } else if (username_in_use == -1) {
            std::cerr << "Error Checking Username" << std::endl;
            //send to client
            std::string msg = "Error Checking Username|";
            send(client_socket, msg.c_str(), msg.size(), 0);
        } else if (username_in_use == 1) {
            std::cout << "Username Taken!" << std::endl;
            std::string msg = "Username Taken|";
            send(client_socket, msg.c_str(), msg.size(), 0);

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
     } else if (data_type == "new_friend_request"){


        std::string data = read_pipe_ended_gen_data(client_socket);
        std::string receiver_username = data.substr(0, data.find('+')); 
        //cast to int
        std::string sender_id = data.substr(data.find('+') + 1); 
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
                std::string query = "INSERT INTO friend_requests(sender_id, reciever_id) VALUES('" + sender_id + "', '" + receiver_id + "');";
                int result_of_insert = DB->generic_insert_function(query);
                std::cout << "result of id inserts: " << result_of_insert << std::endl; 
                std::string error_msg = "Request Send!|";
                std::cout << error_msg << std::endl;
                send(client_socket, error_msg.c_str(), error_msg.size(), 0);
            }

            if (exists == 1){
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
    if (listen(server_socket, 5) == -1)
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

	std::thread(handle_client, client_socket, DB).detach();
    }

    close(server_socket);
    return 0;
}
