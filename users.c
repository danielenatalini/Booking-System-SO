/*
** users.c
** File format: username;password;role
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#include "users.h"

int load_users(User list[], int max_elements) {
    FILE *f = fopen(USERS_FILE, "r");
    if (f == NULL) {
        perror("load_users");
        return -1;
    }

    int n = 0;
    while (n < max_elements) {
        int role;
        int fields = fscanf(f, "%63[^;];%63[^;];%d\n",
                             list[n].username, list[n].password, &role);
        if (fields != 3) break;
        list[n].role = (Role) role;
        n++;
    }
    fclose(f);
    return n;
}

int check_login(User list[], int num_elements,
                 const char *username, const char *password,
                 User *found) {
    int i;
    for (i = 0; i < num_elements; i++) {
        if (strcmp(list[i].username, username) == 0) {
            if (strcmp(list[i].password, password) == 0) {
                *found = list[i];
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

int acquire_users_lock(void) {
    int fd;
    int attempts = 0;
    int tried_stale_cleanup = 0;
    while ((fd = open(USERS_LOCK_FILE, O_CREAT | O_EXCL, 0644)) == -1) {
        if (errno != EEXIST) {
            perror("acquire_users_lock");
            return -1;
        }

        if (!tried_stale_cleanup) {
            struct stat info;
            if (stat(USERS_LOCK_FILE, &info) == 0) {
                if (time(NULL) - info.st_mtime > 10) {
                    unlink(USERS_LOCK_FILE);
                }
            }
            tried_stale_cleanup = 1;
        }

        usleep(10000);
        attempts++;
        if (attempts > 5000) {
            fprintf(stderr, "Timeout acquiring users lock\n");
            return -1;
        }
    }
    close(fd);
    return 0;
}

void release_users_lock(void) {
    unlink(USERS_LOCK_FILE);
}

int username_exists(User list[], int num_elements, const char *username) {
    int i;
    for (i = 0; i < num_elements; i++) {
        if (strcmp(list[i].username, username) == 0) return 1;
    }
    return 0;
}

int register_user(const char *username, const char *password) {
    FILE *f = fopen(USERS_FILE, "a");
    if (f == NULL) {
        perror("register_user");
        return -1;
    }
    fprintf(f, "%s;%s;0\n", username, password);
    fclose(f);
    return 0;
}