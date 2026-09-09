/*
** archive.c
** Formato file: id;risorsa;data;ora_inizio;ora_fine;richiedente;stato
*/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#include "archive.h"
#include "strutil.h"

int acquire_lock(void) {
    int fd;
    int attempts = 0;
    int tried_stale_cleanup = 0;
    while ((fd = open(LOCK_FILE, O_CREAT | O_EXCL, 0644)) == -1) {
        if (errno != EEXIST) {
            perror("acquire_lock");
            return -1;
        }
/* Se il lock è vecchio (un processo è crashato mentre lo
deteneva), lo rimuoviamo invece di aspettare per sempre.
Ci proviamo UNA SOLA VOLTA per chiamata poichè ripetere il
controllo ad ogni iterazione permetterebbe a due processi
in attesa di decidere entrambi che il lock è obsoleto e
di cancellarlo in corsa, rischiando di eliminare un lock
appena ricreato legittimamente da un terzo processo. */
        if (!tried_stale_cleanup) {
            struct stat info;
            if (stat(LOCK_FILE, &info) == 0) {
                if (time(NULL) - info.st_mtime > 10) {
                    unlink(LOCK_FILE);
                }
            }
            tried_stale_cleanup = 1;
        }

        usleep(10000);        /* attesa attiva: il lock si tiene solo pochi ms */
        attempts++;
        if (attempts > 5000) {
            fprintf(stderr, "Timeout acquiring archive lock\n");
            return -1;
        }
    }
    close(fd);
    return 0;
}

void release_lock(void) {
    unlink(LOCK_FILE);        /* cancellare il file = rilasciare il lock */
}

int load_archive(Booking list[], int max_elements) {
    FILE *f = fopen(ARCHIVE_FILE, "r");
    if (f == NULL) {
        if (errno == ENOENT) return 0;
        perror("load_archive");
        return -1;
    }

    int n = 0;
    while (n < max_elements) {
        int id, status;
        int fields = fscanf(f, "%d;%63[^;];%10[^;];%5[^;];%5[^;];%63[^;];%d\n",
                             &id, list[n].resource, list[n].date,
                             list[n].start_time, list[n].end_time,
                             list[n].requester, &status);
        if (fields != 7) break;
        list[n].id = id;
        list[n].status = (Status) status;
        n++;
    }
    fclose(f);
    return n;
}

int save_archive(Booking list[], int num_elements) {
    FILE *f = fopen(ARCHIVE_FILE, "w");
    if (f == NULL) {
        perror("save_archive");
        return -1;
    }

    int i;
    for (i = 0; i < num_elements; i++) {
        fprintf(f, "%d;%s;%s;%s;%s;%s;%d\n",
                list[i].id, list[i].resource, list[i].date,
                list[i].start_time, list[i].end_time, list[i].requester,
                (int) list[i].status);
    }
    fclose(f);
    return 0;
}

int next_id(Booking list[], int num_elements) {
    int max_id = 0;
    int i;
    for (i = 0; i < num_elements; i++) {
        if (list[i].id > max_id) max_id = list[i].id;
    }
    return max_id + 1;
}

int intervals_overlap(const char *date1, const char *start_time1, const char *end_time1,
                       const char *date2, const char *start_time2, const char *end_time2) {
    if (strcmp(date1, date2) != 0) return 0;

    int start1 = time_to_minutes(start_time1);
    int end1   = time_to_minutes(end_time1);
    int start2 = time_to_minutes(start_time2);
    int end2   = time_to_minutes(end_time2);
    
    /* si sovrappongono se e solo se ciascuno inizia prima che
       l'altro finisca */

    return (start1 < end2) && (start2 < end1);
}
int find_conflict(Booking list[], int num_elements,
                   const char *resource, const char *date,
                   const char *start_time, const char *end_time) {
    int i;
    for (i = 0; i < num_elements; i++) {
        if (list[i].status == STATUS_REJECTED) continue;
        if (strcasecmp(list[i].resource, resource) != 0) continue;
        if (intervals_overlap(list[i].date, list[i].start_time, list[i].end_time,
                               date, start_time, end_time)) {
            return i;
        }
    }
    return -1;
}
