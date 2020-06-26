#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "buffer_struct.h"

#define BUF_SEM_NAME "BUF_SEM"

/*
Se elimina una memoria compartida
@param shmid ID de la memoria compartida a eliminar
*/
void destroy_shmem(int shmid){
    shmctl(shmid, IPC_RMID, NULL); //Elimina una memoria compartida
    printf("Shared memory %i has been removed\n", shmid);  
}

/*
Se crea una memoria compartida
@param key_name Nombre de la memoria compartida a utilizar
@param key_id ID de la memoria compartida a utilizar
@param size Tamaño en bytes de la memoria compartida a utilizar
*/
int create_shmem(char* key_name, int size){
    key_t key = 5678;
    int shmid = shmget(key, sizeof(Buffer), IPC_CREAT | 0666);

    Buffer *buffer = shmat(shmid, NULL, 0);

    strcpy(buffer->key, key_name);
    buffer->size=size;
    buffer->buffer_id=shmid;
    buffer->consumers_current=0;
    buffer->producers_current=0;
    buffer->consumers_total=0;
    buffer->producers_total=0;
    buffer->consumers_key_removed=0;
    buffer->time_waited=0;
    buffer->time_waiting=0;
    buffer->time_locked=0;
    buffer->time_usr=0;
    buffer->time_kernel=0;
    buffer->flag_stop_producer = 0;
    buffer->total_msg=0;
    strcpy(buffer->finish_reader, "(EXIT)");

    char aux[buffer->size];
    strcpy(buffer->msg, aux);
    printf("New shared memory called %s and ID: %i\n", buffer->key, buffer->buffer_id);
    shmdt(buffer);
    return shmid;
}

void create_semaphore() {

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_CREAT, 0666, 1);

    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }
}

int main(int argc, char **argv){

    int nparams = argc - 1;

    if(nparams < 1) {
        printf( "Wrong number of arguments\n" );
        return -1;
    }

    char* option = argv[1];

    sem_t *buf_sem;

    if(!strcmp(option, "create")){

        if( nparams != 3) {
            printf( "Wrong number of arguments\n" );
            return -1;
        }

        char* key_name = argv[2];
        int size = atoi(argv[3]);
        int shmem_id = create_shmem(key_name, size);

        if(shmem_id < 0) {
            perror("Error creating shared memory");
            exit(1);
        }

        int *shmem_ptr = (int*)shmat(shmem_id, NULL, 0);

        if((int*) shmem_ptr < 0) {
            perror("Error attaching pointer");
            exit(1);
        } 

        buf_sem = sem_open(BUF_SEM_NAME, O_CREAT, 0666, 1);

        if(buf_sem == (void*) -1) {
            perror("sem_open error");
            exit(1);
        }
    }

    else if(!strcmp(option, "destroy")){
        if( nparams != 2) {
            printf( "Wrong number of arguments\n" );
            return -1;
        }

	    int key_id = atoi(argv[2]);
        sem_close(buf_sem);
        sem_unlink(BUF_SEM_NAME);
        destroy_shmem(key_id);
    }

    else{
	    printf("Unrecognized command\n");
    }

    return 1;
}
