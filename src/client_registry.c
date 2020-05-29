#include "client_registry.h"
#include "csapp.h"
#include "debug.h"

#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>


typedef struct client_registry{
    int file_descriptors[FD_SETSIZE - 4];      //same as number of possible connections, is this ok?
    volatile int cnt;               //keep track of spot in file descriptors 
    pthread_mutex_t lock;           //mutex
    sem_t mutex;                    //semaphore
} CLIENT_REGISTRY;


CLIENT_REGISTRY *creg_init(){
    //CLIENT_REGISTRY client_reg;
    CLIENT_REGISTRY* client_reg_ptr = malloc(sizeof(CLIENT_REGISTRY));
    client_reg_ptr -> cnt = 0;
    if(pthread_mutex_init(&client_reg_ptr -> lock, NULL) != 0){     //init mutex
        return NULL;
    }
    if(sem_init(&client_reg_ptr -> mutex, 0, 0) != 0){      //init semaphore
        return NULL;
    }
    debug("initialized client registry");
    return  client_reg_ptr;
}


void creg_fini(CLIENT_REGISTRY *cr){
    //destroy mutex?
    free(cr);
}


int creg_register(CLIENT_REGISTRY *cr, int fd){
    //identify the mutex
    if(pthread_mutex_lock(&cr -> lock) != 0){
        return -1;
    }
    //add the file descriptor to the proper place
    cr -> file_descriptors[cr -> cnt] = fd;
    //set login bit to false
    cr -> cnt = cr -> cnt + 1;
    //release the mutex
    if(pthread_mutex_unlock(&cr -> lock) != 0){
        return -1;
    }
    debug("register file descriptor %d", fd);
    return 0;
}


int creg_unregister(CLIENT_REGISTRY *cr, int fd){
    //identify the mutex
    if(pthread_mutex_lock(&cr -> lock) != 0){
        return -1;
    }
    //get fd in question
    for(int i = 0; i < cr -> cnt; i++){
        if(cr -> file_descriptors[i] == fd){
            //remove fd
            cr -> file_descriptors[i] = 0;
            //set login bit to 0
            //shift everything in the struct back one position
            for(int j = i + 1; j < cr -> cnt; j++){                 //make sure this works correctly!!!!
                cr -> file_descriptors[j] = cr -> file_descriptors[j + 1];
            }
            cr -> cnt = cr -> cnt - 1;
            //sem post wrapper from csapp
            if(cr -> cnt == 0){
                V(&cr -> mutex);
            }
            //release the mutex
            if(pthread_mutex_unlock(&cr -> lock) != 0){
                return -1;
            }
            debug("Unregiser file descriptor %d", fd);
            return 0;
        }
    }
    return -1;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    //sem wait wrapper from csapp
    P(&cr -> mutex);
}

void creg_shutdown_all(CLIENT_REGISTRY *cr){
    //identify the mutex
    pthread_mutex_lock(&cr -> lock);
    for(int i = 0; i < cr -> cnt; i++){
        //call shutdown
        shutdown(cr -> file_descriptors[i], SHUT_RDWR);           //disable further send and recieve operations 
    }
    //release the mutex
    pthread_mutex_unlock(&cr -> lock);
}