/*
** archive.h
** Bookings archive: file storage, locking, conflict detection.
*/

#ifndef ARCHIVE_H
#define ARCHIVE_H

#include "common.h"

#define MAX_BOOKINGS 1000

int acquire_lock(void);
void release_lock(void);

int load_archive(Booking list[], int max_elements);
int save_archive(Booking list[], int num_elements);
int next_id(Booking list[], int num_elements);

int intervals_overlap(const char *date1, const char *start_time1, const char *end_time1,
                       const char *date2, const char *start_time2, const char *end_time2);

int find_conflict(Booking list[], int num_elements,
                   const char *resource, const char *date,
                   const char *start_time, const char *end_time);

#endif /* ARCHIVE_H */