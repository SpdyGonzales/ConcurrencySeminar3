#include <stdio.h>
#include "green.h"

int flag = 0;
green_cond_t cond;
green_mutex_t lock;

// first test program
/* void *test(void *arg) { */
/* 	int i = *(int*)arg; */
/* 	int loop = 4; */

/* 	while(loop > 0){ */
/* 		printf("thread %d: %d\n", i, loop); */
/* 		loop--; */
/* 		green_yield(); */
/* 	} */
/* } */

//second test program, conditionals
/* void *test(void *arg){ */
/* 	int id = *(int*) arg; */
/* 	int loop = 10000; */
/* 	while(loop > 0){ */
/* 		if(flag == id) { //one of the two threads go here */
/* 		printf("thread %d: %d\n", id, loop); */
/* 		loop--; */
/* 		flag = (id + 1) % 2; //toggle to next thread */
/* 		green_cond_signal(&cond); //if someone is waiting for this condition, wake up */
/* 		}else{ //one of the two threads go here */
/* 		green_cond_wait(&cond); //i want to wait for this cond */
/* 		} */
/* 	} */
/* } */

//fourth test program to show mutex
int global = 0;

void *test(void *arg){
    int id = *(int*) arg;
    int loop = 10000;
    green_mutex_lock(&lock);
    while(loop > 0){
        if(flag == id) { //one of the two threads go here
            printf("thread %d: %d\n", id, loop);
            loop--;
            global++;
            flag = (id + 1) % 2; //toggle to next thread
            green_cond_signal(&cond); //if someone is waiting for this condition, wake up
            green_mutex_unlock(&lock);
        }else{ //one of the two threads go here
            green_mutex_unlock(&lock);
            green_cond_wait(&cond,&lock); //i want to wait for this cond
        }
    }
}

//third test program, our library falls short
//when writing to the wait list queue in condition, if several threads go there at the same time, there is a race condition and they overwrite eachother
/* int main() { */
/*     green_cond_init(&cond); */
/*     green_mutex_init(&lock); */
/*     green_t g0,g1,g2,g3,g4; */
/*     int a0 = 0; */
/*     int a1 = 1; */
/*     int a2 = 0; */
/*     int a3 = 1; */
/*     int a4 = 0; */
/*     green_create(&g0, test, &a0); */
/*     green_create(&g1, test, &a1); */
/*     green_create(&g2, test, &a2); */
/*     green_create(&g3, test, &a3); */
/*     green_create(&g4, test, &a4); */

/*     green_join(&g0); */
/*     green_join(&g1); */
/*     green_join(&g2); */
/*     green_join(&g3); */
/*     green_join(&g4); */

/*     printf("GLOBAL IS %d\n", global); */
/*     printf("done\n"); */
/*     return 0; */
/* } */


//main for test1 and test2, shows our time interrupts work
int main(){
    green_cond_init(&cond);
    green_mutex_init(&lock);
    green_t g0, g1;
    int a0 = 0;
    int a1 = 1;
    green_create(&g0, test, &a0);
    green_create(&g1, test, &a1);
    green_join(&g0);
    green_join(&g1);
    printf("GLOBAL IS %d\n", global);
    printf("done\n");
    return 0;
}
