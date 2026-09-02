/*
** users.h
** User management: login, roles, self-registration.
*/

#ifndef USERS_H
#define USERS_H

#include "common.h"

#define MAX_USERS 100

typedef struct {
    char username[MAX_FIELD];
    char password[MAX_FIELD];
    Role role;
} User;

int load_users(User list[], int max_elements);
int check_login(User list[], int num_elements,
                 const char *username, const char *password,
                 User *found);

int acquire_users_lock(void);
void release_users_lock(void);
int username_exists(User list[], int num_elements, const char *username);
int register_user(const char *username, const char *password);

#endif /* USERS_H */