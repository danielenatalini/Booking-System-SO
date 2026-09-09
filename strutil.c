/*
** strutil.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "strutil.h"

int is_valid_date_format(const char *date) {
    if (strlen(date) != 10) return 0;
    if (date[2] != '/' || date[5] != '/') return 0;

    int day, month, year;
    if (sscanf(date, "%2d/%2d/%4d", &day, &month, &year) != 3) return 0;
    if (month < 1 || month > 12) return 0;

    int days_in_month[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    /* indice 0 inutilizzato apposta: così days_in_month[month] usa
       direttamente il numero del mese (1-12) senza convertirlo */
    int max_day = days_in_month[month];
    if (month == 2) {
    /* bisestile: divisibile per 4, tranne i secoli, a meno che
        non siano divisibili per 400 */
        int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        if (leap) max_day = 29;
    }
    if (day < 1 || day > max_day) return 0;

    return 1;
}

int is_valid_time_format(const char *time_str) {
    if (strlen(time_str) != 5) return 0;
    if (time_str[2] != ':') return 0;

    int hours, minutes;
    if (sscanf(time_str, "%2d:%2d", &hours, &minutes) != 2) return 0;
    if (hours < 0 || hours > 23) return 0;
    if (minutes < 0 || minutes > 59) return 0;
    return 1;
}

int time_to_minutes(const char *time_str) {
/* non rivalidiamo il formato poichè chi chiama ha già usato
    is_valid_time_format prima */
    int hours, minutes;
    sscanf(time_str, "%d:%d", &hours, &minutes);
    return hours * 60 + minutes;
}

int is_valid_resource(const char *resource) {
/* deve iniziare con "room" (case-insensitive) */
    if (strncasecmp(resource, "room", 4) != 0) return 0;

    const char *num_part = resource + 4;
    if (num_part[0] == '\0') return 0;

    int i;
    for (i = 0; num_part[i] != '\0'; i++) {
        if (num_part[i] < '0' || num_part[i] > '9') return 0;
    }

    int n = atoi(num_part);
    if (n < 1 || n > 10) return 0;
    return 1;
}
