#include "simplequeue.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "nvoid.h"
/*
 * Simple tester for the simplequeue..
 */

void *read_queue(void *arg)
{
    simplequeue_t *q = (simplequeue_t *)arg;
    int len;

    while(1) {
        nvoid_t *s = queue_deq_timeout(q, 2000);
        if (s == NULL)
            printf("Timeout.. \n");
        else
            printf("Read %s.. length %d\n", s->data, s->len);
    }

    return NULL;
}

char buf[64];


int main(void)
{
	simplequeue_t *q = queue_new(true);
    pthread_t tid;

    pthread_create(&tid, NULL, read_queue, (void *)q);

    while (1) {
        printf("Enter string: ");
        scanf("%s", buf);
        printf("Wrote %s.. length %lu\n", buf, strlen(buf));
        queue_enq(q, buf, strlen(buf));
    }

    queue_delete(q);
}
