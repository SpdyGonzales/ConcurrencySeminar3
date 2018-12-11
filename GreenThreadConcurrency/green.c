#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include <assert.h>
#include "green.h"

#define PERIOD 100

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

static sigset_t block;

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, FALSE};

static green_t *running = &main_green;
static green_t *firstInQueue;
int queueCount = 0;

int blocked = FALSE;

int countQueue(struct green_t *first) {

    if (first != NULL) {
        struct green_t *node = first;
        int threads = 1;

        while (node->next != NULL) {
            node = node->next;
            threads++;
        }
        return threads;
    }
    return 0;
}

void *enqueue(struct green_t *thread) {
    /* if (thread == NULL) */
    /*     return; */
    /* if (queueCount == 0) { */
    if (firstInQueue == NULL) {
        firstInQueue = thread;
        queueCount++;
        return;
    }
    struct green_t *node = firstInQueue;
    int i = 1;
    while (i < queueCount) {
        node = node->next;
        i++;
    }
    /* while (node->next != NULL) { */
    /*     node = node->next; */
    /* } */
    node->next = thread;
    queueCount++;
    return;
}
struct green_t *dequeue() {
    struct green_t *node = firstInQueue;
    firstInQueue = node->next;
    queueCount--;
    return node;
}

void timer_handler(int sig) {
    green_t *susp = running;

    //add running to ready q
    enqueue(susp);
    //
    ////find next thread for exec
    struct green_t *next = dequeue();
    running = next;

    swapcontext(susp->context, next->context);
}


static void init() __attribute__((constructor));

void init(){
    getcontext(&main_cntx);

    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);

    struct sigaction act = {0};
    struct timeval interval;
    struct itimerval period;

    act.sa_handler = timer_handler;
    assert(sigaction(SIGVTALRM, &act, NULL) == 0);

    interval.tv_sec = 0;
    interval.tv_usec = PERIOD;
    period.it_interval=interval;
    period.it_value = interval;
    setitimer(ITIMER_VIRTUAL, &period, NULL);
}

void green_thread(){
    green_t *this = running;
    printf("Current thread: %p\n",this);
    (*this->fun)(this->arg);

    sigprocmask(SIG_BLOCK, &block, NULL);

    //place waiting (joining) thread in ready queue
    enqueue(this->join);

    //free allocated memory
    free(this->context->uc_stack.ss_sp);
    free(this->context);

    //we are zombie
    this->zombie = TRUE;

    //find next thread to run
    struct green_t *next = dequeue();

    //start run next thread
    running = next;
    setcontext(next->context);
}

int green_yield(){
    green_t *susp = running;
    //enQueue(readyQueue, susp);
    //
    sigprocmask(SIG_BLOCK, &block, NULL);

    enqueue(susp);
    struct green_t *next = dequeue();
    running = next;
    swapcontext(susp->context, next->context);
    return 0;
}

int green_join(green_t *thread) {
    printf("Current thread from join: %p\n",running);

    //if thread already finished, we can continue to run ourselves
    if(thread->zombie == 1){
        return 0;
    }

    //this is us
    green_t *susp = running;

    sigprocmask(SIG_BLOCK, &block, NULL);

    //we are waiting on that thread
    thread->join = susp;

    //find next thread to run
    struct green_t *next = dequeue();
    running = next;
    swapcontext(susp->context, next->context);

    return 0;
}

int green_create(green_t *new, void *(*fun)(void*), void *arg) {

    ucontext_t *cntx = (ucontext_t *)malloc(sizeof(ucontext_t));
    getcontext(cntx);

    void *stack = malloc(STACK_SIZE);

    cntx->uc_stack.ss_sp = stack;
    cntx->uc_stack.ss_size = STACK_SIZE;

    makecontext(cntx, green_thread, 0);
    new->context = cntx;
    new->fun = fun;
    new->arg = arg;
    new->next = NULL;
    new->join = NULL;
    new->zombie = FALSE;

    //add to readyQ
    enqueue(new);
    return 0;
}

void green_cond_init(green_cond_t* cond){
    //kan byta till pekare till tråd i stället för en kö?
    cond->first = NULL;
    cond->count = 0;
}

void green_cond_simple_wait(green_cond_t* cond){
    //thread says: i want to suspend and wait for this condition to occur
    //
    // not "susp and put me in ready queue", but susp and wait for condition
    // so each cond needs a leeeeeeethreads waiting for it
    green_t *toSuspend = running;


    sigprocmask(SIG_BLOCK, &block, NULL);

    struct green_t *next = dequeue();
    running = next;

    if (cond->count == 0) {
        cond->first = toSuspend;
    } else {
        struct green_t *node = cond->first;
        int i = 0;
        while (i < cond->count) {
            node = node->next;
            i++;
        }
        node->next = toSuspend;
    }
        cond->count++;

    swapcontext(toSuspend->context, next->context);
}

void green_cond_wait(green_cond_t* cond, green_mutex_t *mutex) {
    sigprocmask(SIG_BLOCK, &block, NULL);
    struct green_t* suspend = running;
    if (cond->first != NULL) {
        suspend->next = cond->first;
    }
    cond->first = suspend;

    if (mutex != NULL) {
        mutex->taken = FALSE;
        enqueue(mutex->susp);
        mutex->susp = NULL;
        mutex->count = 0;
    }

    struct green_t* next = dequeue();
    running = next;
    swapcontext(suspend->context, next->context);

    if(mutex != NULL) {
        //green_mutex_lock(mutex);
        while (mutex->taken) {
            //take cond instead?
            suspend->next = mutex->susp;
            mutex->susp = suspend;
            /* suspend->next = cond->first; */
            /* cond->first = suspend; */
        }
        mutex->taken = TRUE;
    }

    sigprocmask(SIG_UNBLOCK, &block, NULL);
}
/* void green_cond_wait(green_cond_t* cond, green_mutex_t *mutex){ */
/*     sigprocmask(SIG_BLOCK, &block, NULL); */

/*     green_t *toSuspend = running; */

/*     if (cond->count == 0) { */
/*         cond->first = toSuspend; */
/*     } else { */
/*         struct green_t *node = cond->first; */
/*         int i = 0; */
/*         while (i < cond->count) { */
/*             node = node->next; */
/*             i++; */
/*         } */
/*         node->next = toSuspend; */
/*     } */
/*         cond->count++; */

/*         if (mutex != NULL) { */
/*             //release lock if we have mutex */
/*             mutex->taken = FALSE; */

/*             //schedule suspended threads */
/*             enqueue(mutex->susp); */
/*             mutex->susp = NULL; */
/*             //count? */
/*             queueCount += countQueue(firstInQueue); */
/*             //queueCount += countQueue(mutex->susp); */
/*             queueCount--; */
/*             queueCount--; */
/*         } */

/*     struct green_t *next = dequeue(); */
/*     running = next; */

/*     swapcontext(toSuspend->context, next->context); */

/*     if(mutex != NULL) { */
/*         while (mutex->taken) { */
/*             toSuspend->next = mutex->susp; */
/*             mutex->susp = toSuspend; */
/*         } */
/*         //take lock */
/*         mutex->taken = TRUE; */
/*     } */
/*     sigprocmask(SIG_UNBLOCK, &block, NULL); */
/*     return 0; */
/* } */

void green_cond_signal(green_cond_t* cond){
    //release ALL threads
    sigprocmask(SIG_BLOCK, &block, NULL);

    if (cond->first == NULL)
        return;

    queueCount = queueCount + cond->count -1;
    enqueue(cond->first);
    cond->count = 0;
    cond->first = NULL;

    //sigprocmask(SIG_UNBLOCK, &block, NULL);
}

int green_mutex_init(green_mutex_t *mutex) {
    mutex->taken = FALSE;
    mutex->susp = NULL;
    mutex->count = 0;
}
int green_mutex_lock(green_mutex_t *mutex) {
    sigprocmask(SIG_BLOCK, &block, NULL);

    green_t *susp = running;
    while(mutex->taken) {
        //suspend running thread
        if (mutex->count == 0) {
            printf("ISNULL");
            mutex->susp = susp;
        } else {
            struct green_t *node = mutex->susp;
            while (!(node->next == NULL)) {
                node = node->next;
            }
            node->next = susp;
        }


        //find next thread
        struct green_t *next = dequeue();

        running = next;
        swapcontext(susp->context,next->context);
    }
    //take lock
    mutex->taken = TRUE;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

int green_mutex_unlock(green_mutex_t *mutex) {
    //block timer
    sigprocmask(SIG_BLOCK, &block, NULL);

    //move suspended threads to ready q
    if (mutex->susp != NULL) {
        struct green_t *node = mutex->susp;
        int threads = 1;

        while (node->next != NULL) {
            node = node->next;
            threads++;
        }
        queueCount = queueCount + threads -1;
        enqueue(mutex->susp);
    }

    //release lock
    mutex->taken = FALSE;
    mutex->count = 0;
    mutex->susp = NULL;

    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}
