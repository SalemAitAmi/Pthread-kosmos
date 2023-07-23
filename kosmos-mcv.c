/*
 * kosmos-mcv.c (mutexes & condition variables)
 *
 * For UVic CSC 360, Summer 2022
 *
 * Here is some code from which to start.
 *
 * PLEASE FOLLOW THE INSTRUCTIONS REGARDING WHERE YOU ARE PERMITTED
 * TO ADD OR CHANGE THIS CODE. Read from line 133 onwards for
 * this information.
 */

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logging.h"


/* Random # below threshold indicates H; otherwise C. */
#define ATOM_THRESHOLD 0.55
#define DEFAULT_NUM_ATOMS 40

#define MAX_ATOM_NAME_LEN 10
#define MAX_KOSMOS_SECONDS 5

/* Global / shared variables */
int  cNum = 0, hNum = 0;
long numAtoms;


/* Function prototypes */
void kosmos_init(void);
void *c_ready(void *);
void *h_ready(void *);
void make_radical(int, int, int, char *);
void wait_to_terminate(int);


/* Needed to pass legit copy of an integer argument to a pthread */
int *dupInt( int i )
{
	int *pi = (int *)malloc(sizeof(int));
	assert( pi != NULL);
	*pi = i;
	return pi;
}


int main(int argc, char *argv[])
{
	long seed;
	numAtoms = DEFAULT_NUM_ATOMS;
	pthread_t **atom;
	int i;
	int status;

	if ( argc < 2 ) {
		fprintf(stderr, "usage: %s <seed> [<num atoms>]\n", argv[0]);
		exit(1);
	}

	if ( argc >= 2) {
		seed = atoi(argv[1]);
	}

	if (argc == 3) {
		numAtoms = atoi(argv[2]);
		if (numAtoms < 0) {
			fprintf(stderr, "%ld is not a valid number of atoms\n",
				numAtoms);
			exit(1);
		}
	}

    kosmos_log_init();
	kosmos_init();

	srand(seed);
	atom = (pthread_t **)malloc(numAtoms * sizeof(pthread_t *));
	assert (atom != NULL);
	for (i = 0; i < numAtoms; i++) {
		atom[i] = (pthread_t *)malloc(sizeof(pthread_t));
		if ( (double)rand()/(double)RAND_MAX < ATOM_THRESHOLD ) {
			hNum++;
			status = pthread_create (
					atom[i], NULL, h_ready,
					(void *)dupInt(hNum)
				);
		} else {
			cNum++;
			status = pthread_create (
					atom[i], NULL, c_ready,
					(void *)dupInt(cNum)
				);
		}
		if (status != 0) {
			fprintf(stderr, "Error creating atom thread\n");
			exit(1);
		}
	}

    /* Determining the maximum number of ethynyl radicals is fairly
     * straightforward -- it will be the minimum of the number of
     * hNum and cNum/2.
     */

    int max_radicals = (hNum < cNum/2 ? hNum : (int)(cNum/2));
#ifdef VERBOSE
    printf("Maximum # of radicals expected: %d\n", max_radicals);
#endif

    wait_to_terminate(max_radicals);
}


/*
* Now the tricky bit begins....  All the atoms are allowed
* to go their own way, but how does the Kosmos ethynyl-radical
* problem terminate? There is a non-zero probability that
* some atoms will not become part of a radical; that is,
* many atoms may be blocked on some condition variable of
* our own devising. How do we ensure the program ends when
* (a) all possible radicals have been created and (b) all
* remaining atoms are blocked (i.e., not on the ready queue)?
*/


/*
 * ^^^^^^^
 * DO NOT MODIFY CODE ABOVE THIS POINT.
 *
 *************************************
 *************************************
 *
 * ALL STUDENT WORK MUST APPEAR BELOW.
 * vvvvvvvv
 */


/* 
 * DECLARE / DEFINE NEEDED VARIABLES IMMEDIATELY BELOW.
 */
pthread_mutex_t radLock; //Mutex
pthread_cond_t need_more; //When overflow of H or C occurs, wait for a signal that more is needed
pthread_cond_t consumed; //Once a radical is made notify the consumed atoms (terminate threads)
int radicals = 0;  //Radical count 
int c1 = 0;  //First c
int c2 = 0;  //Second c
int h = 0;  //h
bool decay = false; //When true: No more reactions can occur; Cause remaining atoms to "decay" (return NULL)
int free_radicals = 0; //Number of atoms currently waiting to react (used to terminate leftover atoms at the end)
/*
 * FUNCTIONS YOU MAY/MUST MODIFY.
 */

//Initialize mutex and condition variables 
void kosmos_init() {
    pthread_mutex_init(&radLock, NULL);
    pthread_cond_init(&need_more, NULL);
    pthread_cond_init(&consumed, NULL);
}


void *h_ready( void *arg )
{
	int id = *((int *)arg);
    char name[MAX_ATOM_NAME_LEN]; 
    sprintf(name, "h%03d", id);
    
#ifdef VERBOSE
	printf("%s now exists\n", name);
#endif
    
    pthread_mutex_lock(&radLock);
    free_radicals++;
    if(h == 0){   //If no H with C overflow, make radical
        if(c1 != 0 && c2 != 0){
            //Make radical; Increment counter
            h = id;
            radicals++;
            make_radical(c1, c2, h, name);
            //Reset reacting atoms
            h = 0;
            c1 = 0;
            c2 = 0;
            //Decrement counter by amount of atoms consumed
            free_radicals -= 3;
            //Terminate all consumed threads
            pthread_cond_broadcast(&consumed);
        }
        else{  //If no H and not enough C; Use for current radical; wait to be consumed
            h = id;
            pthread_cond_wait(&consumed, &radLock);
        }
    }
    else{   //H overflow: Wait until an h is needed; then wait to be consumed
        while(1){
            //Wait for signal; broadcast occurs after a radical is made
            pthread_cond_wait(&need_more, &radLock);

            //Decay == true if max_radicals have been made; terminate leftover atoms
            if(decay){
                return NULL;
            }
            
            //Assign the first signaled h to radical as needed
            if(h == 0){
                h = id;
                //Wait to be consumed
                pthread_cond_wait(&consumed, &radLock);
                //Terminate leftover atoms assigned to radical
                if(decay){
                    return NULL;
                }
                break;
            }
        }
        
    }
    
    pthread_mutex_unlock(&radLock);
    //Notify waiting atoms
    pthread_cond_broadcast(&need_more);
    

	return NULL;
}

void *c_ready( void *arg )
{
	int id = *((int *)arg);
    char name[MAX_ATOM_NAME_LEN];
    sprintf(name, "c%03d", id);
    
#ifdef VERBOSE
	printf("%s now exists\n", name);
#endif
    
    
    pthread_mutex_lock(&radLock);
    free_radicals++;
    if(c1 == 0){   //First C for radical (Never the maker)
        c1 = id;
        //Wait to be consumed
        pthread_cond_wait(&consumed, &radLock);
    }
    else if(c2 == 0){  //If H overflow and already have c1, then make radical
        if(h != 0){
            //Make radical; increment counter
            c2 = id;
            radicals++;
            make_radical(c1, c2, h, name);
            //Reset reacting atoms
            h = 0;
            c1 = 0;
            c2 = 0;
            //Decrement counter by amount of atoms consumed
            free_radicals -= 3;
            //Terminate all consumed threads
            pthread_cond_broadcast(&consumed);
        }
        else{  //If no H and already have c1
            //Set c2
            c2 = id;
            //Wait to be consumed
            pthread_cond_wait(&consumed, &radLock);
        }
    }
    else{ //C overflow: wait until a C is needed; then wait to be consumed
        while(1){
            //Wait for signal; broadcast occurs after a radical is made
            pthread_cond_wait(&need_more, &radLock);
            
            //Decay == true if max_radicals have been made; terminate leftover atoms
            if(decay){
                return NULL;
            }
            
            //Assign the first signaled C to radical as needed
            if(c1 == 0){
                c1 = id;
                //Wait to be consumed
                pthread_cond_wait(&consumed, &radLock);
                //Terminate leftover atoms assigned to radical
                if(decay){
                    return NULL;
                }
                break;
            }
            else if(c2 == 0){
                c2 = id;
                //Wait to be consumed
                pthread_cond_wait(&consumed, &radLock);
                //Terminate leftover atoms assigned to radical
                if(decay){
                    return NULL;
                }
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&radLock);
    //Notify waiting atoms
    pthread_cond_broadcast(&need_more);

	return NULL;
}

void make_radical(int c1, int c2, int h, char *maker)
{
#ifdef VERBOSE
	fprintf(stdout, "A ethynyl radical was made: c%03d  c%03d  h%03d\n",
		c1, c2, h);
#endif
    //Add entry to log
    kosmos_log_add_entry(radicals, c1, c2, h, maker);
}


void wait_to_terminate(int expected_num_radicals) {
    //Sleep to give atoms time to react
    sleep(MAX_KOSMOS_SECONDS);
    
    //No atoms left are able to react so they decay (i.e. numRadicals = maxRadicals)
    decay = true;

    //Simulate atoms decaying by unblocking and returning NULL
    for(int i = 0; i < free_radicals; i++){
        //Terminate unassigned atoms
        pthread_cond_broadcast(&need_more);
        //Terminate atoms assigned to radical
        pthread_cond_broadcast(&consumed);
    }
    
    //Outputs log
    kosmos_log_dump();
    
    //Destroy mutex and condition variables 
    pthread_mutex_destroy(&radLock);
    pthread_cond_destroy(&need_more);
    pthread_cond_destroy(&consumed);
    exit(0);
}
