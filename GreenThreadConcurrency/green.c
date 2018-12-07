#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <assert.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, FALSE};

static green_t *running = &main_green;
static green_t *firstInQueue;
int queueCount = 0;


void *enqueue(struct green_t *thread) {
    if (queueCount == 0) {
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

static void init() __attribute__((constructor));

void init(){
    getcontext(&main_cntx);
}

void green_thread(){
    green_t *this = running;
    printf("Current thread: %p\n",this);
    (*this->fun)(this->arg);

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
    enqueue(susp);
    //printf("Susp:%p\n",susp);
    struct green_t *next = dequeue();
    //printf("next in yield: %p\n",&next);
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
    //enQueue(readyQueue, new);
    enqueue(new);
    return 0;
}

void green_cond_init(green_cond_t* cond){
    //kan byta till pekare till tråd i stället för en kö?
    cond->first = NULL;
    cond->count = 0;
}

void green_cond_wait(green_cond_t* cond){
    //thread says: i want to suspend and wait for this condition to occur
    //
    // not "susp and put me in ready queue", but susp and wait for condition
    // so each cond needs a leeeeeeethreads waiting for it
    green_t *toSuspend = running;
    if(toSuspend == NULL){printf("toSuspend är null");}
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

void green_cond_signal(green_cond_t* cond){
    //release ALL threads

    if (cond->first == NULL)
        return;


    queueCount = queueCount + cond->count -1;
    enqueue(cond->first);
    cond->count = 0;
    cond->first = NULL;
}
