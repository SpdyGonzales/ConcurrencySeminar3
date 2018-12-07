#include <ucontext.h>

typedef struct green_t {
	ucontext_t *context;
	void *(*fun)(void*);
	void *arg;
	struct green_t *next;
	struct green_t *join;
	int zombie;
} green_t;

typedef struct green_cond_t{
	struct green_t *first;
    int count;
}green_cond_t;

typedef struct Queue 
{ 
        struct QNode *front, *rear; 
}Queue;

typedef struct QNode 
{ 
        green_t thread; 
        struct QNode *next; 
}QNode;


int green_create(green_t *thread, void *(*fun)(void*), void *arg);
int green_yield();
int green_join(green_t *thread);
void green_cond_init(green_cond_t *cond);
void green_cond_wait(green_cond_t *cond);
void green_cond_signal(green_cond_t *cond);
