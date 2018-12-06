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
/*
typedef struct Node{
	struct green_t *value;
	struct Node *next;
} Node;
typedef struct Queue {
	struct Node *first;
	struct Node *tail;
	int count;
} Queue;

struct QNode
{
	green_t thread;
	struct QNode *next;
};

struct Queue
{
	struct QNode *front, *rear;
};
*/

struct QNode* newNode(green_t *k)
{
	struct QNode *temp = (struct QNode*)malloc(sizeof(struct QNode));
	temp->thread = *k;
	temp->next = NULL;
	return temp;
}

struct Queue *createQueue()
{
	struct Queue *q = (struct Queue*)malloc(sizeof(struct Queue));
	q->front = q->rear = NULL;
	return q;
}
struct Queue *readyQueue;

void enQueue(struct Queue *q, green_t *k)
{
	struct QNode *temp = newNode(k);

	if (q->rear == NULL)
	{
	q->front = q->rear = temp;
	return;
	}

	q->rear->next = temp;
	q->rear = temp;
}

struct green_t *deQueue(struct Queue *q)
{
	if (q->front == NULL){

	return NULL;
	}

	struct QNode *temp = q->front;
	q->front = q->front->next;

	if (q->front == NULL)
	q->rear = NULL;

	return &(temp->thread);
}

/*
void enqueue(struct green_t *thread){
	struct Node newNode = {thread, NULL};
	printf("In enqueue: %p\n",&newNode);
	if(readyQueue.count == 0){
		readyQueue.first = &newNode;
		readyQueue.tail = &newNode;
		readyQueue.count = readyQueue.count + 1;
		return;
	}else{
		struct Node *last = readyQueue.tail;
		last->next = &newNode;
		readyQueue.tail = &newNode;
		readyQueue.count = readyQueue.count + 1;
		return;
	}
}

struct green_t dequeue(){
	if(readyQueue.count == 0){
	//	return NULL;
	}else{
		struct Node *first = readyQueue.first;
		printf("In dequeue: %p\n", first->value);
		if(readyQueue.count == 1){
			readyQueue.first = NULL;
			readyQueue.tail = NULL;
		}else{
			readyQueue.first = first->next;
		}
		readyQueue.count = readyQueue.count -1;
		return *(first->value);
	}
}
*/
/*
void enqueue(struct green_t *thread){
	if(firstInQueue == NULL){
		firstInQueue = thread;
	}else{
		struct green_t *currentThread = firstInQueue;
		while(currentThread->next != NULL){
			currentThread = currentThread->next;
		} 
		currentThread->next = thread;
	}

}

struct green_t *dequeue(){

	struct green_t *fetchedThread = firstInQueue;
	firstInQueue = firstInQueue->next;
	return fetchedThread;
}
*/
static void init() __attribute__((constructor));

void init(){
	getcontext(&main_cntx);
	readyQueue = createQueue();
}

void green_thread(){
	green_t *this = running;
	printf("Current thread: %p\n",this);
	(*this->fun)(this->arg);
	if(this->join != NULL){
		enQueue(readyQueue, this->join); //fel??
	}
	free(this->context->uc_stack.ss_sp);
	free(this->context);
	this->zombie = TRUE;
	struct green_t *next = deQueue(readyQueue);
	if(next == NULL){
		next = &main_green;
	}
	running = next;
	setcontext(next->context);
}

int green_yield(){
	green_t *susp = running;
	enQueue(readyQueue, susp);
	//printf("Susp:%p\n",susp);
	struct green_t *next = deQueue(readyQueue);
	//printf("next in yield: %p\n",&next);
	running = next;
	swapcontext(susp->context, next->context);
	return 0;
}

int green_join(green_t *thread) {

	if(thread->zombie){
		return 0;
	}

	green_t *susp = running;
	susp->join = thread;
	struct green_t *next = deQueue(readyQueue);
	if(next == NULL){
		next = &main_green;
	}
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

	enQueue(readyQueue, new);
	return 0;
}

void green_cond_init(green_cond_t* cond){
	cond->q = createQueue();
}

void green_cond_wait(green_cond_t* cond){
	green_t *toSuspend = running;
	if(toSuspend == NULL){printf("toSuspend är null");}
	struct green_t *next = deQueue(readyQueue);
	if(next == NULL){
		next = &main_green;
	}
	running = next;
	enQueue(cond->q, toSuspend);
	swapcontext(toSuspend->context, next->context);
}

void green_cond_signal(green_cond_t* cond){

	while(cond->q == NULL){

	}
	enQueue(readyQueue, deQueue(cond->q));
}
