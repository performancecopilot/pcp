/*
 * GFS2 gfs2_glock_lock_time tracepoint metrics.
 *
 * Copyright (c) 2013 Red Hat.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "stdio.h"
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "lock_time.h"
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <sys/types.h>

/* global linked list which will contain the locks for other mounted gfs2 
 * filesystems between metric refreshes, this is due to trace_pipe giving
 * all locks each read. We need to keep the locks for the other filesystems
 * in order not to lose any data. The linked list is implemented as a stack
 * on which items are pushed onto the head but values can be removed from
 * anypoint as long as the pointers to the next items in the list are
 * re-attached after the removal of a node. 
 */
linkedList_t* lockList = NULL;

/* Refreshing of the metrics for gsf2.lock_time, some of metrics are of
 * a different typing.
 */
int 
gfs2_locktime_fetch(int item, struct lock_time *glstats, pmAtomValue *atom){
    switch(item){
        case 0:
            atom->ul = glstats->lock_type; /* Glock type number */
            break;
        case 1:
            atom->ull = glstats->number; /* Inode or resource group number */
            break;
        case 2:
            atom->ll = glstats->srtt; /* Non blocking smoothed round trip time */
            break;
        case 3:
            atom->ll = glstats->srttvar; /* Non blocking smoothed variance */
            break;
        case 4:
            atom->ll = glstats->srttb; /* Blocking smoothed round trip time */
            break;
        case 5:
            atom->ll = glstats->srttvarb; /* Blocking smoothed variance */
            break;
        case 6:
            atom->ll = glstats->sirt; /* Smoothed Inter-request time */
            break;
        case 7:
            atom->ll = glstats->sirtvar; /* Smoothed Inter-request variance */
            break;
        case 8:
            atom->ll = glstats->dlm; /* Count of dlm requests */
            break;
        case 9:
            atom->ll = glstats->queue; /* Count of gfs2_holder queues */
            break;
        default:
            return PM_ERR_PMID;
    }
    return 1;
}

/* Gathering of the required data for th gsf2.lock_time metrics. We take all 
 * required data from the trace_pipe and the trace values come from the 
 * gfs2_glock_lock_time tracepoint. Items are read in and stored within the
 * linked list data structure. Locks are later compared in order to find the
 * worse lock this refresh for the given filesystem dev. 
 */
int 
gfs2_refresh_lock_time(const char *sysfs, const char *name, struct lock_time *glstats, dev_t dev){
    static char *TRACE_PIPE = "/sys/kernel/debug/tracing/trace_pipe";
    int fd, buffer_size, buffer_offset, read_count, chars_read;
    char *buffer, *temp_buffer, *p;

    buffer_size = 8196;
    buffer_offset = 0;
    read_count = buffer_size;

    memset(glstats, 0, sizeof(*glstats)); /* reset all counters for this fs */

    /* We allocate an inital amount of memory to hold data from the tracepipe */
    buffer = (char *)calloc(buffer_size, sizeof(char));
    if(buffer == NULL) 
        return -oserror();

    /* We open the pipe in both read-only and non-blocking mode */
    if ((fd = open(TRACE_PIPE, O_RDONLY | O_NONBLOCK)) == -1)
	return -oserror();

    /* We fill our buffer will all the data that exists for this refresh*/
    while((chars_read = read(fd, buffer + buffer_offset, (sizeof(char) * read_count)))){
        if(chars_read <= 0)
            break;

        buffer_offset += chars_read;
        buffer_size += read_count;

        temp_buffer = realloc(buffer, sizeof(char) * buffer_size);
        if (temp_buffer == NULL) 
            return -oserror();
        buffer = temp_buffer;
    }

    /*
     * Read through glocks file accumulating statistics as we go;
     * we bin unwanted glocks which are not inode(2) or resource group(3)
     * and compare to locate the worse lock on this run though.
     */ 
    for(p = strtok(buffer, "."); p != NULL; p = strtok(NULL, ".")){
        struct lock_time temp;
        unsigned int major, minor;
        dev_t dev_id;
        char *start;

        /* Match buffer to start of the glock information */
        if ((start = strstr(p, "gfs2_glock_lock_time: "))){  
            sscanf(start, 
                "gfs2_glock_lock_time: %"SCNu32",%"SCNu32" glock %"SCNu32":%"SCNu64" status:%*d flags:%*x tdiff:%*d srtt:%"SCNd64"/%"SCNd64" srttb:%"SCNd64"/%"SCNd64" sirt:%"SCNd64"/%"SCNd64" dcnt:%"SCNd64" qcnt:%"SCNd64,
                &major,
                &minor, 
                &temp.lock_type,
                &temp.number,
                &temp.srtt, 
                &temp.srttvar, 
                &temp.srttb, 
                &temp.srttvarb, 
                &temp.sirt, 
                &temp.sirtvar, 
                &temp.dlm, 
                &temp.queue
            );

            /* We are only after these lock types, bin the rest */
           if(temp.lock_type == 2 || temp.lock_type == 3){
                /* We take our filesystem identifier and store lock in linked list */
                dev_id = makedev(major, minor);
                list_push(&lockList, temp, dev_id);
            }
        }
    }

    /* We get rid of duplicate values in list, keeping only the most up-to date value */
    list_remove_duplicates(lockList);

    /* We iterate through the remaining locks checking if they are for our
     * requested filesystem and comparing them to find the worst lock to 
     * report back.
     */
    *glstats = check_glocks(lockList, dev);

    /* We only want to keep the locks left for any other filesystems other than
     * the one we are refreshing, so we delete the locks left in the linked list
     * for this current filesystem.
     */
    list_pop(&lockList, dev);

    free(buffer);
    close(fd);
    return 0;
}

/*
 * Runs through our list, starting a comparison between two chosen glstats
 * until a final lock remains which is deemed to be the worst of this run,
 * subsequently this lock is returned to gfs2_refresh_glstats to set as our
 * metric.
 *
 */
struct lock_time
check_glocks(linkedList_t * list, dev_t dev){

    struct lock_time glockA, glockB /*worstGlock */;
    struct lock_time worstGlock = {0};
    linkedList_t* currentNode = list;
    int count = 0;

    while(currentNode != NULL){
        /* Make sure the lock is for the filesystem we are refreshing metrics for */
        if(currentNode->dev_id == dev){
            if(count == 0){
                glockA = currentNode->data;
            } else {
                glockB = currentNode->data;
            }
            if (count >= 1 ){
                /* Call to compare the two given locks, the return value indicates
                 * which lock is the worst, 1 for glockA and 0 for glockB
                 */
                if(lock_compare(&glockA, &glockB) == 1){
                    worstGlock = glockA;   
                } else {
                    worstGlock = glockB;
                    /* If glockB is the worse overwrite glockA with that value
                     * because on the each loop the new "challenger" will be
                     * placed as glockB
                     */
                    glockA = glockB;
                }
            } else {
                worstGlock = glockA;
            }
            count++;
        } 
        currentNode = currentNode->next; /* increase cursor though the list */
    }
    return worstGlock;
}

/*
 * Comparision function to allow the comparision of two different locks, we
 * compare the values; we return A worse than B = 1, 
 *                               B worse than A = 0.
 */
int 
lock_compare(struct lock_time *glockA, struct lock_time *glockB){
    int truecount = 0;

    /* (A sirt (LESS THAN) B sirt = A worse) */
    if(glockA->sirtvar < glockB->sirtvar)
        truecount++;

    /* A srtt (MORE THAN) B srtt = A worse */
    if(glockA->srttvarb > glockB->srttvarb)
        truecount++;

    /* A srttb (MORE THAN) B srttb = A worse */
    if(glockA->srttvar > glockB->srttvar)
        truecount++;

    /* Base return on the number of true counts for A worse than B
     * if there is more than one true out of three A is worse, if 
     * there is one count each way we decide on qucount and dcount
     * else B is worse 
     */
    if(truecount > 1){
        return 1; /* glockA worse */
    /* Tie-break decision case */
    } else if(truecount == 1){
         if(glockA->dlm > glockA->queue) return 1; /* glockA worse */
    }
    return 0; /* glockB worse */
}

/* Allocate heap memory to store our locks in the linked list and assign values
 * using a stack implementation so new locks are placed at the head of the 
 * linked list and we point the new values next to the list. */
void 
list_push(linkedList_t** list, struct lock_time data, dev_t dev_id){
    linkedList_t* newNode = (linkedList_t*) malloc(sizeof(linkedList_t));
    if(!newNode){
        fputs("Error cannot allocate memory for list \n", stderr);
        exit(1);
    }
    newNode->data = data;
    newNode->dev_id = dev_id;
    newNode->next = *list;
    *list = newNode;
}

/* we iterate through the linked list value at a time and remove any locks 
 * which belong to the filesystem given by dev_t dev. The locks are removed
 * by freeing their memory and relinking the pointers of the list. */
void 
list_pop(linkedList_t** list, dev_t dev){
    linkedList_t* currentNode = *list; 
    linkedList_t* head = *list;

    if(currentNode == NULL){
        return; /* Cant perform anything on an empty list */
    }

    /* We iterate though the list, making sure to catch the empty list case */
    while(currentNode != NULL && currentNode->next){
        /* We check to find if the value is for our given filesystem (dev) */
        if(currentNode->next->dev_id == dev){
                /* If it does, we skip forward two values */
                linkedList_t* nextNode = currentNode->next->next;
       
                free(currentNode->next); /* We remove the value we are after */
                currentNode->next = nextNode; /* Re-link the pointers */
        } else {
            currentNode = currentNode->next;
        }
    }

    /* Handle corner case at the end where the head of the list contains a lock
     * that we need to delete, we just re-point the head of the list to the second
     * element and then free the head
     */
    if(head->dev_id == dev){
        *list = head->next;
         free(head);
    }
}

/* We want only the latest set of statistics for the given glock, storing as
 * a stack helps with this as the latest data we have will be the first set of
 * metrics for that glock. We can iterate and remove any glocks that have the
 * same number and type. Again we relink the pointers for the list after deletion.
 */
void 
list_remove_duplicates(linkedList_t* list){
    linkedList_t* iterator = list; 

    /* First iterator through the list*/
    while(iterator != NULL){
        linkedList_t* currentNode = iterator;

        /* Second iterator checks wether each other value in the list matches
         * the value that the first iterator is pointing to, it is done this
         * way because of the stack based pushing, the latest data for the
         * glock will always be towards the head of the list, thus the first
         * we incounter.
         */
        while(currentNode != NULL && currentNode->next != NULL){
            /* If we have the same filesystem, lock_type and the inode/resource
             * group number, its safe to say that this value is an older record
             */
            if(iterator->dev_id == currentNode->next->dev_id &&
               iterator->data.lock_type == currentNode->next->data.lock_type &&
               iterator->data.number == currentNode->next->data.number){
                
                /* same deletion method as pop, we move forward two pointers
                 * skipping the data we wish to remove
                 */
                linkedList_t* nextNode = currentNode->next->next;
       
                free(currentNode->next); /* We remove the duplicate value */
                currentNode->next = nextNode; /* Re-link the pointers */
            } else {
                currentNode = currentNode->next; /* Increase the inner iterator */
            } 
        }
        iterator = iterator->next; /* Increase the outer iterator */
    }
}
