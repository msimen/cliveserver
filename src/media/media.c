#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "util.h"
#include "media.h"
#include "log.h"
#include "list.h"
#include "ts2ts.h"

static List_t tasks;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/*
create a media repack task
*/
sMedia * clive_media_create(int pack_type, int input_media_type)
{
    sMedia *temp;

    temp = clive_calloc(1, sizeof(sMedia));
    ASSERT(temp != NULL);
    temp->pack_type = pack_type;
    temp->input_media_type = input_media_type;
    temp->output_pads.count = 0;
    temp->output_pads.head = NULL;
    temp->output_pads.tail = NULL;
    pthread_mutex_init(&temp->olock, NULL);
    return temp;
}


int clive_media_add_output(sMedia *media, struct kfifo *buffer)
{
    bool val;
    ASSERT(media != NULL);

    pthread_mutex_lock(&media->olock);
    val = ListAdd(&media->output_pads, buffer);
    pthread_mutex_unlock(&media->olock);
    return val ? 1 : 0;
}

/*
   stop the media repacking task
*/
int clive_media_stop(sMedia *media)
{
    if (media != NULL) {
        clive_free(media);
    }
}

/*
   release the media
*/
int clive_media_release(sMedia *media)
{
}

typedef struct {
    struct kfifo * buffer;
    sMedia * flv_media;
    sMedia * ts_media;
}Task ;
/*
   DO NOT call it manually,
   chanel core will cat it automaticly
*/
int clive_media_attach(sMedia *flv_media, sMedia *ts_media,\
                      struct kfifo *buffer)
{
   Task * temp  = clive_calloc(1, sizeof(Task));
   if (temp == NULL)
       return -1;
   temp->buffer = buffer;
   temp->flv_media = flv_media;
   temp->ts_media = ts_media;
   pthread_mutex_lock(&lock);
   ListAdd(&tasks, temp);
   pthread_mutex_unlock(&lock);
   return 0;
}

static void * Entry(void *p)
{
    Task * task;
    ListEntry_t * current = NULL;
    uint8_t *buffer;
    uint32_t len;
    buffer = clive_calloc(1, 1024); 

    do {
        if (current == NULL) {
            pthread_mutex_lock(&lock);
            current = tasks.head ;
            pthread_mutex_unlock(&lock);
            sched_yield();
            continue;
        }
        task = current->data;
        if (kfifo_len(task->buffer) > 0) {
           len = kfifo_get(task->buffer, buffer, 1024);
           //log_debug(LOG_INFO, "media task got %d data from channel's buffer", len);
           clive_media_consume_data(task->flv_media, buffer, len);
           clive_media_consume_data(task->ts_media, buffer, len);
       }
       
        //do something
        //read from buffer, write to flv_media and ts_media

        pthread_mutex_lock(&lock);
        current = current ? current->next : NULL;
        pthread_mutex_unlock(&lock);
    }while(1);
}
/*
  start the repacking task thread
*/

int clive_media_task_thread_start(void)
{
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr); 
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    return pthread_create(&tid, &attr, Entry, NULL);
}


int clive_media_setype(sMedia * media, int pack_type, int input_media_type)
{
    if (media != NULL) {
        media->pack_type = pack_type;
        media->input_media_type = input_media_type;
    }
    return 0;
}

int clive_media_consume_data(sMedia *media, uint8_t *data, uint32_t len)
{
    if (media != NULL) {
        if (media->pack_type == TS2TS)
            do_pack_ts2ts(media, data, len);
    }
    //////
}
