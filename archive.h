/*
** archive.h
** Archivio prenotazioni: persistenza su file, lock, controllo conflitti.
*/

#ifndef ARCHIVE_H
#define ARCHIVE_H

#include "common.h"

#define MAX_BOOKINGS 1000 
/* ampiamente sufficiente per l'uso previsto */
int acquire_lock(void);    /* mutua esclusione (acquisisce il lock) */
void release_lock(void);    /* rilascia il lock */

int load_archive(Booking list[], int max_elements);    /* legge da file */
int save_archive(Booking list[], int num_elements);    /* scrive su file */
int next_id(Booking list[], int num_elements);    /* max id + 1 */

int intervals_overlap(const char *date1, const char *start_time1, const char *end_time1,
                       const char *date2, const char *start_time2, const char *end_time2);

int find_conflict(Booking list[], int num_elements,
                   const char *resource, const char *date,
                   const char *start_time, const char *end_time);

#endif /* ARCHIVE_H */
