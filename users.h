/*
** users.h
** Gestione utenti: login, ruoli, registrazione self-service.
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

int load_users(User list[], int max_elements);        /* legge utenti.dat */
int check_login(User list[], int num_elements,
                 const char *username, const char *password,
                 User *found);        /* verifica credenziali */

int acquire_users_lock(void);        /* stesso meccanismo di archive.c */
void release_users_lock(void);
int username_exists(User list[], int num_elements, const char *username);
int register_user(const char *username, const char *password);        /* append a utenti.dat */

#endif /* USERS_H */
