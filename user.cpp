// user.cpp
#include "user.h"

user::user() {
    // You can initialize user_id_global if needed
    user_id_global = 0; // Default initialization
}

int user::get_user_id() {
    return user_id_global;
}

void user::set_user_id(int user_id) {
    user_id_global = user_id;
}
