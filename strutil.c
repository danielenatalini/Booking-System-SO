/*
** strutil.c
*/

#include <stdio.h>
#include <string.h>

#include "strutil.h"

int is_valid_date_format(const char *date) {
    if (strlen(date) != 10) return 0;
    if (date[2] != '/' || date[5] != '/') return 0;

    int day, month, year;
    if (sscanf(date, "%2d/%2d/%4d", &day, &month, &year) != 3) return 0;
    if (month < 1 || month > 12) return 0;

    int days_in_month[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_day = days_in_month[month];
    if (month == 2) {
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
    int hours, minutes;
    sscanf(time_str, "%d:%d", &hours, &minutes);
    return hours * 60 + minutes;
}