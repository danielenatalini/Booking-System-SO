/*
** server.c
** AF_INET socket server, one child process per client (fork),
** same accept/fork scheme as server1_INET.c (module 6 - IPC).
** SIGCHLD handling follows limit.c (module 5 - Signals).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "archive.h"
#include "users.h"
#include "strutil.h"

typedef struct {
    int authenticated;
    char username[MAX_FIELD];
    Role role;
} Session;

static void handle_sigchld(int sig) {
    (void) sig;
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        ;
    }
}

static void send_framed(int sock, const char *msg) {
    char header[16];
    int len = (int) strlen(msg);
    snprintf(header, sizeof(header), "%d\n", len);
    write(sock, header, strlen(header));
    write(sock, msg, len);
}

static void send_ok(int sock, const char *detail) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "OK;%s\n", detail);
    send_framed(sock, buffer);
}

static void send_error(int sock, const char *code, const char *detail) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "ERR;%s;%s\n", code, detail);
    send_framed(sock, buffer);
}

static const char *status_to_string(Status s) {
    switch (s) {
        case STATUS_PENDING:  return "pending";
        case STATUS_APPROVED: return "approved";
        case STATUS_REJECTED: return "rejected";
    }
    return "unknown";
}

static int is_date_in_past(const char *date) {
    int day, month, year;
    sscanf(date, "%2d/%2d/%4d", &day, &month, &year);

    time_t now = time(NULL);
    struct tm *today = localtime(&now);
    int today_year  = today->tm_year + 1900;
    int today_month = today->tm_mon + 1;
    int today_day   = today->tm_mday;

    int requested_value = year * 10000 + month * 100 + day;
    int today_value      = today_year * 10000 + today_month * 100 + today_day;

    return requested_value < today_value;
}

static void handle_login(int sock, const char *username, const char *password, Session *session) {
    if (username == NULL || password == NULL) {
        send_error(sock, "SYNTAX", "Usage: LOGIN;username;password");
        return;
    }

    User user_list[MAX_USERS];
    int num_users = load_users(user_list, MAX_USERS);
    if (num_users < 0) {
        send_error(sock, "SERVER", "Error reading users file");
        return;
    }

    User found_user;
    if (check_login(user_list, num_users, username, password, &found_user) != 0) {
        send_error(sock, "LOGIN", "Invalid credentials");
        return;
    }

    session->authenticated = 1;
    session->role = found_user.role;
    strncpy(session->username, found_user.username, MAX_FIELD - 1);
    session->username[MAX_FIELD - 1] = '\0';

    char detail[128];
    snprintf(detail, sizeof(detail), "role=%s", session->role == ROLE_ADMIN ? "admin" : "standard");
    send_ok(sock, detail);
}

static void handle_register(int sock, const char *username, const char *password) {
    if (username == NULL || password == NULL) {
        send_error(sock, "SYNTAX", "Usage: REGISTER;username;password");
        return;
    }

    if (acquire_users_lock() != 0) {
        send_error(sock, "SERVER", "Cannot access users file (lock)");
        return;
    }

    User user_list[MAX_USERS];
    int num_users = load_users(user_list, MAX_USERS);
    if (num_users < 0) {
        release_users_lock();
        send_error(sock, "SERVER", "Error reading users file");
        return;
    }

    if (username_exists(user_list, num_users, username)) {
        release_users_lock();
        send_error(sock, "REGISTER", "Username already exists");
        return;
    }

    if (num_users >= MAX_USERS) {
        release_users_lock();
        send_error(sock, "SERVER", "Maximum number of users reached");
        return;
    }

    int result = register_user(username, password);
    release_users_lock();

    if (result != 0) {
        send_error(sock, "SERVER", "Error during registration");
        return;
    }
    send_ok(sock, "Registration completed, you can now log in");
}

static void handle_new(int sock, char *resource, char *date,
                        char *start_time, char *end_time, Session *session) {
    if (!session->authenticated) {
        send_error(sock, "AUTH", "You must log in first");
        return;
    }
        if (resource == NULL || date == NULL || start_time == NULL || end_time == NULL) {
        send_error(sock, "SYNTAX", "Usage: NEW;resource;date;start_time;end_time");
        return;
    }
    if (!is_valid_resource(resource)) {
        send_error(sock, "SYNTAX", "Resource must be Room1 to Room10");
        return;
    }
    if (!is_valid_date_format(date)) {
        send_error(sock, "SYNTAX", "Invalid date format: use DD/MM/YYYY");
        return;
    }
    if (!is_valid_time_format(start_time) || !is_valid_time_format(end_time)) {
        send_error(sock, "SYNTAX", "Invalid time format: use HH:MM");
        return;
    }
    if (is_date_in_past(date)) {
        send_error(sock, "SYNTAX", "Cannot book a date in the past");
        return;
    }
        if (time_to_minutes(start_time) >= time_to_minutes(end_time)) {
        send_error(sock, "SYNTAX", "Start time must be earlier than end time");
        return;
    }
    if (time_to_minutes(start_time) < 8 * 60 || time_to_minutes(end_time) > 20 * 60) {
        send_error(sock, "SYNTAX", "Bookings are only allowed between 08:00 and 20:00");
        return;
    }

    if (acquire_lock() != 0) {
        send_error(sock, "SERVER", "Cannot access archive (lock)");
        return;
    }

    Booking list[MAX_BOOKINGS];
    int n = load_archive(list, MAX_BOOKINGS);
    if (n < 0) {
        release_lock();
        send_error(sock, "SERVER", "Error reading archive");
        return;
    }

    if (find_conflict(list, n, resource, date, start_time, end_time) != -1) {
        release_lock();
        send_error(sock, "CONFLICT", "A conflicting booking already exists for this resource");
        return;
    }

    if (n >= MAX_BOOKINGS) {
        release_lock();
        send_error(sock, "SERVER", "Archive full");
        return;
    }

    Booking new_booking;
    new_booking.id = next_id(list, n);
    strncpy(new_booking.resource, resource, MAX_FIELD - 1);
    new_booking.resource[MAX_FIELD - 1] = '\0';
    /* is_valid_resource() already guaranteed the format "Room" + a
       number 1-10, so we can rebuild it in a canonical, consistent
       capitalization ("Room1") instead of just lowercasing it. */
    {
        int room_number = atoi(new_booking.resource + 4);
        snprintf(new_booking.resource, MAX_FIELD, "Room%d", room_number);
    }
    strncpy(new_booking.date, date, MAX_DATE - 1);
    new_booking.date[MAX_DATE - 1] = '\0';
    strncpy(new_booking.start_time, start_time, MAX_TIME - 1);
    new_booking.start_time[MAX_TIME - 1] = '\0';
    strncpy(new_booking.end_time, end_time, MAX_TIME - 1);
    new_booking.end_time[MAX_TIME - 1] = '\0';
    strncpy(new_booking.requester, session->username, MAX_FIELD - 1);
    new_booking.requester[MAX_FIELD - 1] = '\0';
    new_booking.status = STATUS_PENDING;

    list[n] = new_booking;
    n++;

    int result = save_archive(list, n);
    release_lock();

    if (result != 0) {
        send_error(sock, "SERVER", "Error saving archive");
        return;
    }

    char detail[64];
    snprintf(detail, sizeof(detail), "id=%d status=pending", new_booking.id);
    send_ok(sock, detail);
}

static void append_list_row(char *out, int out_size, Booking *b) {
    char row[256];
    snprintf(row, sizeof(row), "%d|%s|%s|%s|%s|%s|%s\n",
             b->id, b->resource, b->date, b->start_time, b->end_time,
             b->requester, status_to_string(b->status));
    strncat(out, row, out_size - strlen(out) - 1);
}

static void handle_list_mine(int sock, Session *session) {
    if (!session->authenticated) {
        send_error(sock, "AUTH", "You must log in first");
        return;
    }

    if (acquire_lock() != 0) {
        send_error(sock, "SERVER", "Cannot access archive (lock)");
        return;
    }
    Booking list[MAX_BOOKINGS];
    int n = load_archive(list, MAX_BOOKINGS);
    release_lock();

    if (n < 0) {
        send_error(sock, "SERVER", "Error reading archive");
        return;
    }

    char body[BUFFER_SIZE];
    body[0] = '\0';
    int found_count = 0;
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(list[i].requester, session->username) != 0) continue;
        append_list_row(body, sizeof(body), &list[i]);
        found_count++;
    }
    if (found_count == 0) strncat(body, "(no bookings found)\n", sizeof(body) - strlen(body) - 1);

    char response[BUFFER_SIZE + 16];
    snprintf(response, sizeof(response), "OK;LIST\n%s", body);
    send_framed(sock, response);
}

static int matches_filter(const char *filter, const char *value) {
    if (filter[0] == '\0' || strcmp(filter, "-") == 0) return 1;
    return strcmp(filter, value) == 0;
}

static int matches_resource_filter(const char *filter, const char *value) {
    if (filter[0] == '\0' || strcmp(filter, "-") == 0) return 1;
    return strcasecmp(filter, value) == 0;
}

static void handle_search_mine(int sock, char *resource, char *date, char *status, Session *session) {
    if (!session->authenticated) {
        send_error(sock, "AUTH", "You must log in first");
        return;
    }
    if (resource == NULL) resource = "-";
    if (date == NULL) date = "-";
    if (status == NULL) status = "-";

    if (acquire_lock() != 0) {
        send_error(sock, "SERVER", "Cannot access archive (lock)");
        return;
    }
    Booking list[MAX_BOOKINGS];
    int n = load_archive(list, MAX_BOOKINGS);
    release_lock();

    if (n < 0) {
        send_error(sock, "SERVER", "Error reading archive");
        return;
    }

    char body[BUFFER_SIZE];
    body[0] = '\0';
    int found_count = 0;
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(list[i].requester, session->username) != 0) continue;
        if (!matches_resource_filter(resource, list[i].resource)) continue;
        if (!matches_filter(date, list[i].date)) continue;
        if (!matches_filter(status, status_to_string(list[i].status))) continue;
        append_list_row(body, sizeof(body), &list[i]);
        found_count++;
    }
    if (found_count == 0) strncat(body, "(no bookings found with these filters)\n", sizeof(body) - strlen(body) - 1);

    char response[BUFFER_SIZE + 16];
    snprintf(response, sizeof(response), "OK;LIST\n%s", body);
    send_framed(sock, response);
}

static void handle_list_all(int sock, Session *session) {
    if (!session->authenticated) {
        send_error(sock, "AUTH", "You must log in first");
        return;
    }
    if (session->role != ROLE_ADMIN) {
        send_error(sock, "AUTH", "Operation restricted to the administrator");
        return;
    }

    if (acquire_lock() != 0) {
        send_error(sock, "SERVER", "Cannot access archive (lock)");
        return;
    }
    Booking list[MAX_BOOKINGS];
    int n = load_archive(list, MAX_BOOKINGS);
    release_lock();

    if (n < 0) {
        send_error(sock, "SERVER", "Error reading archive");
        return;
    }

    char body[BUFFER_SIZE];
    body[0] = '\0';
    int i;
    for (i = 0; i < n; i++) append_list_row(body, sizeof(body), &list[i]);
    if (n == 0) strncat(body, "(no bookings found)\n", sizeof(body) - strlen(body) - 1);

    char response[BUFFER_SIZE + 16];
    snprintf(response, sizeof(response), "OK;LIST\n%s", body);
    send_framed(sock, response);
}

static void handle_status_change(int sock, char *id_text, Status new_status, Session *session) {
    if (!session->authenticated) {
        send_error(sock, "AUTH", "You must log in first");
        return;
    }
    if (session->role != ROLE_ADMIN) {
        send_error(sock, "AUTH", "Operation restricted to the administrator");
        return;
    }
    if (id_text == NULL) {
        send_error(sock, "SYNTAX", "Usage: APPROVE;id or REJECT;id");
        return;
    }

    int id = atoi(id_text);

    if (acquire_lock() != 0) {
        send_error(sock, "SERVER", "Cannot access archive (lock)");
        return;
    }
    Booking list[MAX_BOOKINGS];
    int n = load_archive(list, MAX_BOOKINGS);
    if (n < 0) {
        release_lock();
        send_error(sock, "SERVER", "Error reading archive");
        return;
    }

    int i, found_index = -1;
    for (i = 0; i < n; i++) {
        if (list[i].id == id) { found_index = i; break; }
    }
    if (found_index == -1) {
        release_lock();
        send_error(sock, "NOTFOUND", "No booking with this ID exists");
        return;
    }

    /* Before approving, verify it doesn't conflict with another
       active (non-rejected) booking on the same resource/time. */
    if (new_status == STATUS_APPROVED) {
        int j;
        for (j = 0; j < n; j++) {
            if (j == found_index) continue;
            if (list[j].status == STATUS_REJECTED) continue;
            if (strcasecmp(list[j].resource, list[found_index].resource) != 0) continue;
            if (intervals_overlap(list[j].date, list[j].start_time, list[j].end_time,
                                   list[found_index].date, list[found_index].start_time,
                                   list[found_index].end_time)) {
                release_lock();
                send_error(sock, "CONFLICT", "Approving this booking would conflict with another active booking");
                return;
            }
        }
    }

    list[found_index].status = new_status;
    int result = save_archive(list, n);
    release_lock();

    if (result != 0) {
        send_error(sock, "SERVER", "Error saving archive");
        return;
    }

    char detail[64];
    snprintf(detail, sizeof(detail), "id=%d newStatus=%s", id, status_to_string(new_status));
    send_ok(sock, detail);
}

static int handle_command(int sock, char *buffer, Session *session) {
    char *command = strtok(buffer, ";");
    if (command == NULL) {
        send_error(sock, "SYNTAX", "Empty command");
        return 0;
    }

    if (strcmp(command, "QUIT") == 0) return -1;

    if (strcmp(command, "LOGIN") == 0) {
        char *username = strtok(NULL, ";");
        char *password = strtok(NULL, ";");
        handle_login(sock, username, password, session);
        return 0;
    }
    if (strcmp(command, "REGISTER") == 0) {
        char *username = strtok(NULL, ";");
        char *password = strtok(NULL, ";");
        handle_register(sock, username, password);
        return 0;
    }
    if (strcmp(command, "NEW") == 0) {
        char *resource = strtok(NULL, ";");
        char *date = strtok(NULL, ";");
        char *start_time = strtok(NULL, ";");
        char *end_time = strtok(NULL, ";");
        handle_new(sock, resource, date, start_time, end_time, session);
        return 0;
    }
    if (strcmp(command, "LIST_MINE") == 0) {
        handle_list_mine(sock, session);
        return 0;
    }
    if (strcmp(command, "SEARCH_MINE") == 0) {
        char *resource = strtok(NULL, ";");
        char *date = strtok(NULL, ";");
        char *status = strtok(NULL, ";");
        handle_search_mine(sock, resource, date, status, session);
        return 0;
    }
    if (strcmp(command, "LIST_ALL") == 0) {
        handle_list_all(sock, session);
        return 0;
    }
    if (strcmp(command, "APPROVE") == 0) {
        char *id_text = strtok(NULL, ";");
        handle_status_change(sock, id_text, STATUS_APPROVED, session);
        return 0;
    }
    if (strcmp(command, "REJECT") == 0) {
        char *id_text = strtok(NULL, ";");
        handle_status_change(sock, id_text, STATUS_REJECTED, session);
        return 0;
    }
    if (strcmp(command, "SET_STATUS") == 0) {
        char *id_text = strtok(NULL, ";");
        char *status_text = strtok(NULL, ";");
        Status new_status;
        if (status_text != NULL && strcmp(status_text, "approved") == 0) new_status = STATUS_APPROVED;
        else if (status_text != NULL && strcmp(status_text, "rejected") == 0) new_status = STATUS_REJECTED;
        else if (status_text != NULL && strcmp(status_text, "pending") == 0) new_status = STATUS_PENDING;
        else { send_error(sock, "SYNTAX", "Status must be: pending, approved or rejected"); return 0; }
        handle_status_change(sock, id_text, new_status, session);
        return 0;
    }

    send_error(sock, "SYNTAX", "Unknown command");
    return 0;
}

static void handle_client(int connect_socket) {
    char buffer[BUFFER_SIZE];
    Session session;
    session.authenticated = 0;
    session.role = ROLE_STANDARD;
    session.username[0] = '\0';

    int return_code;
    while ((return_code = read(connect_socket, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[return_code] = '\0';
        int len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';

        if (handle_command(connect_socket, buffer, &session) == -1) break;
    }
}

int main(void) {
    int server_socket, connect_socket, return_code;
    socklen_t client_address_len;
    struct sockaddr_in server_address, client_address;
    char *client_ip;

    User initial_user_list[MAX_USERS];
    int initial_user_count = load_users(initial_user_list, MAX_USERS);
    if (initial_user_count < 0) {
        fprintf(stderr, "Cannot load %s\n", USERS_FILE);
        exit(EXIT_FAILURE);
    }
    printf("Loaded %d users from %s\n", initial_user_count, USERS_FILE);

    signal(SIGCHLD, handle_sigchld);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);

    if ((return_code = bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address))) == -1) {
        perror("bind (wait a few seconds if the server was just closed)");
        exit(EXIT_FAILURE);
    }

    if ((return_code = listen(server_socket, 5)) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Booking System server ready on %s:%d\n", SERVER_ADDRESS, SERVER_PORT);

    client_address_len = sizeof(client_address);

    while (1) {
        connect_socket = accept(server_socket, (struct sockaddr *) &client_address, &client_address_len);
        if (connect_socket == -1) {
            perror("accept");
            continue;
        }

        if (fork() == 0) {
            close(server_socket);
            client_ip = inet_ntoa(client_address.sin_addr);
            printf("Client connected from %s\n", client_ip);

            handle_client(connect_socket);

            printf("Client %s disconnected\n", client_ip);
            close(connect_socket);
            exit(0);
        }

        close(connect_socket);
    }

    close(server_socket);
    return 0;
}
