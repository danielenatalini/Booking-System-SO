/*
** client.c
** Same connect scheme as client1_INET.c (module 6 - IPC).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

static void read_line(char *dest, int size) {
    if (fgets(dest, size, stdin) != NULL) {
        int len = strlen(dest);
        if (len > 0 && dest[len - 1] == '\n') dest[len - 1] = '\0';
    } else {
        dest[0] = '\0';
    }
}

static int recv_framed(int sock, char *dest, int max_dest) {
    char header[16];
    int header_len = 0;
    char c;

    /* read the length prefix, one byte at a time, up to the '\n' */
    while (header_len < (int) sizeof(header) - 1) {
        int n = read(sock, &c, 1);
        if (n <= 0) return -1;
        if (c == '\n') break;
        header[header_len++] = c;
    }
    header[header_len] = '\0';

    int total = atoi(header);
    if (total < 0) return -1;
    if (total >= max_dest) total = max_dest - 1;

    /* keep reading until we actually have the whole message: a
       single read() over TCP is not guaranteed to return it all
       at once. */
    int received = 0;
    while (received < total) {
        int n = read(sock, dest + received, total - received);
        if (n <= 0) return -1;
        received += n;
    }
    dest[received] = '\0';
    return received;
}

static int send_command(int sock, const char *command, char *response, int max_response) {
    char to_send[BUFFER_SIZE];
    snprintf(to_send, sizeof(to_send), "%s\n", command);

    if (write(sock, to_send, strlen(to_send)) == -1) {
        perror("write");
        return -1;
    }

    int n = recv_framed(sock, response, max_response);
    if (n < 0) {
        printf("Connection with the server lost.\n");
        return -1;
    }
    return n;
}

static void print_list(const char *response) {
    char copy[BUFFER_SIZE];
    strncpy(copy, response, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *line = strtok(copy, "\n");
    line = strtok(NULL, "\n"); /* skip "OK;LIST" */

    printf("%-4s %-15s %-12s %-8s %-8s %-12s %-10s\n",
           "ID", "Resource", "Date", "Start", "End", "Requester", "Status");
    printf("--------------------------------------------------------------------\n");

    while (line != NULL) {
        char id[16], resource[MAX_FIELD], date[MAX_DATE], start_time[MAX_TIME];
        char end_time[MAX_TIME], requester[MAX_FIELD], status[16];

        int fields = sscanf(line, "%15[^|]|%63[^|]|%10[^|]|%5[^|]|%5[^|]|%63[^|]|%15[^\n]",
                             id, resource, date, start_time, end_time, requester, status);
        if (fields == 7) {
            printf("%-4s %-15s %-12s %-8s %-8s %-12s %-10s\n",
                   id, resource, date, start_time, end_time, requester, status);
        } else {
            printf("%s\n", line);
        }
        line = strtok(NULL, "\n");
    }
}

static void standard_menu(int sock) {
    char response[BUFFER_SIZE + 32];
    int choice;
    char buffer[16];

    while (1) {
        printf("\n--- Standard User Menu ---\n");
        printf("1) New booking\n");
        printf("2) View all my bookings\n");
        printf("3) Search my bookings by field\n");
        printf("4) Exit\n");
        printf("Choice: ");
        read_line(buffer, sizeof(buffer));
        choice = atoi(buffer);

        if (choice == 1) {
            /* Buffers larger than MAX_DATE/MAX_TIME: fgets needs room
               to also consume the trailing '\n'. */
            char resource[MAX_FIELD], date[32], start_time[16], end_time[16];
            printf("Resource (Room1 to Room10): ");
            read_line(resource, sizeof(resource));
            printf("Date (DD/MM/YYYY): ");
            read_line(date, sizeof(date));
            printf("Start time (HH:MM): ");
            read_line(start_time, sizeof(start_time));
            printf("End time (HH:MM): ");
            read_line(end_time, sizeof(end_time));

            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "NEW;%s;%s;%s;%s", resource, date, start_time, end_time);

            if (send_command(sock, command, response, sizeof(response)) == -1) return;
            printf("%s\n", response);

        } else if (choice == 2) {
            if (send_command(sock, "LIST_MINE", response, sizeof(response)) == -1) return;
            if (strncmp(response, "OK", 2) == 0) print_list(response);
            else printf("%s\n", response);

        } else if (choice == 3) {
            char resource[32], date[32], status[32];
            printf("Leave empty or type '-' to ignore a field.\n");
            printf("Resource: ");
            read_line(resource, sizeof(resource));
            printf("Date (DD/MM/YYYY): ");
            read_line(date, sizeof(date));
            printf("Status (pending/approved/rejected): ");
            read_line(status, sizeof(status));

            if (resource[0] == '\0') strcpy(resource, "-");
            if (date[0] == '\0') strcpy(date, "-");
            if (status[0] == '\0') strcpy(status, "-");

            char command[BUFFER_SIZE];
            snprintf(command, sizeof(command), "SEARCH_MINE;%s;%s;%s", resource, date, status);

            if (send_command(sock, command, response, sizeof(response)) == -1) return;
            if (strncmp(response, "OK", 2) == 0) print_list(response);
            else printf("%s\n", response);

        } else if (choice == 4) {
            write(sock, "QUIT\n", 5);
            printf("Goodbye!\n");
            return;

        } else {
            printf("Invalid choice.\n");
        }
    }
}

static void admin_menu(int sock) {
    char response[BUFFER_SIZE + 32];
    int choice;
    char buffer[16];

    while (1) {
        printf("\n--- Administrator Menu ---\n");
        printf("1) View all bookings\n");
        printf("2) Approve a booking\n");
        printf("3) Reject a booking\n");
        printf("4) Manually change a booking's status\n");
        printf("5) Exit\n");
        printf("Choice: ");
        read_line(buffer, sizeof(buffer));
        choice = atoi(buffer);

        if (choice == 1) {
            if (send_command(sock, "LIST_ALL", response, sizeof(response)) == -1) return;
            if (strncmp(response, "OK", 2) == 0) print_list(response);
            else printf("%s\n", response);

        } else if (choice == 2 || choice == 3) {
            char id_text[16];
            printf("Booking ID: ");
            read_line(id_text, sizeof(id_text));

            char command[64];
            snprintf(command, sizeof(command), "%s;%s", (choice == 2) ? "APPROVE" : "REJECT", id_text);
            if (send_command(sock, command, response, sizeof(response)) == -1) return;
            printf("%s\n", response);

        } else if (choice == 4) {
            char id_text[16], status_text[16];
            printf("Booking ID: ");
            read_line(id_text, sizeof(id_text));
            printf("New status (pending/approved/rejected): ");
            read_line(status_text, sizeof(status_text));

            char command[64];
            snprintf(command, sizeof(command), "SET_STATUS;%s;%s", id_text, status_text);
            if (send_command(sock, command, response, sizeof(response)) == -1) return;
            printf("%s\n", response);

        } else if (choice == 5) {
            write(sock, "QUIT\n", 5);
            printf("Goodbye!\n");
            return;

        } else {
            printf("Invalid choice.\n");
        }
    }
}

int main(void) {
    int client_socket;
    struct sockaddr_in server_address;

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);

    if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("connect (is the server running?)");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the Booking System server (%s:%d)\n", SERVER_ADDRESS, SERVER_PORT);

    char response[BUFFER_SIZE + 32];
    int login_success = 0;
    int logged_as_admin = 0;

    while (!login_success) {
        char initial_choice[8];
        printf("\n1) Log in\n2) Register (new user)\nChoice: ");
        read_line(initial_choice, sizeof(initial_choice));

        if (atoi(initial_choice) == 2) {
            char new_username[MAX_FIELD], new_password[MAX_FIELD];
            printf("Choose a username (format: name.surname): ");
            read_line(new_username, sizeof(new_username));
            printf("Choose a password: ");
            read_line(new_password, sizeof(new_password));

            char register_command[BUFFER_SIZE];
            snprintf(register_command, sizeof(register_command), "REGISTER;%s;%s", new_username, new_password);

            if (send_command(client_socket, register_command, response, sizeof(response)) == -1) {
                close(client_socket);
                exit(EXIT_FAILURE);
            }
            printf("%s", response);
            continue;
        }

        char username[MAX_FIELD], password[MAX_FIELD];
        printf("Username: ");
        read_line(username, sizeof(username));
        printf("Password: ");
        read_line(password, sizeof(password));

        char command[BUFFER_SIZE];
        snprintf(command, sizeof(command), "LOGIN;%s;%s", username, password);

        if (send_command(client_socket, command, response, sizeof(response)) == -1) {
            close(client_socket);
            exit(EXIT_FAILURE);
        }

        if (strncmp(response, "OK", 2) == 0) {
            login_success = 1;
            if (strstr(response, "role=admin") != NULL) logged_as_admin = 1;
            printf("Login successful.\n");
        } else {
            printf("%s", response);
            printf("Please try again.\n");
        }
    }

    if (logged_as_admin) admin_menu(client_socket);
    else standard_menu(client_socket);

    close(client_socket);
    return 0;
}