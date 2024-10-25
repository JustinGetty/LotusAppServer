// user.h
#ifndef USER_H
#define USER_H

class user {
public:
    user();  // Constructor declaration
    int get_user_id();  // Getter for user_id
    void set_user_id(int user_id);  // Setter for user_id

private:
    int user_id_global;  // Member variable to store user ID
};

#endif // USER_H
