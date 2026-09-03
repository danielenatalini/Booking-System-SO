/*
** strutil.h
** Date/time format validation and helpers.
*/

#ifndef STRUTIL_H
#define STRUTIL_H

int is_valid_date_format(const char *date);     /* DD/MM/YYYY */
int is_valid_time_format(const char *time_str);  /* HH:MM */
int time_to_minutes(const char *time_str);
int is_valid_resource(const char *resource);     /* "Room1".."Room10" */

#endif /* STRUTIL_H */