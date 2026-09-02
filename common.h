/*
** common.h
** Shared definitions between server and client.
*/

#ifndef COMMON_H
#define COMMON_H

#define SERVER_PORT 54321
#define SERVER_ADDRESS "127.0.0.1"
#define BUFFER_SIZE 512

#define ARCHIVE_FILE "prenotazioni.dat"
#define LOCK_FILE    "prenotazioni.lock"
#define USERS_FILE   "utenti.dat"
#define USERS_LOCK_FILE "utenti.lock"

#define MAX_FIELD 64
#define MAX_DATE  11   /* "DD/MM/YYYY" */
#define MAX_TIME  6    /* "HH:MM" */

typedef enum {
    STATUS_PENDING  = 0,
    STATUS_APPROVED = 1,
    STATUS_REJECTED = 2
} Status;

typedef enum {
    ROLE_STANDARD = 0,
    ROLE_ADMIN = 1
} Role;

typedef struct {
    int id;
    char resource[MAX_FIELD];
    char date[MAX_DATE];
    char start_time[MAX_TIME];
    char end_time[MAX_TIME];
    char requester[MAX_FIELD];
    Status status;
} Booking;

#endif /* COMMON_H */