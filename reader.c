#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "buffer_struct.h"

#define BUF_SEM_NAME "BUF_SEM"

int main(int argc, char **argv){

    int nparams = argc - 1;

    if(nparams < 1) {
        printf( "Wrong number of arguments\n" );
        return -1;
    }
  
    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a leer

    Buffer *buffer = (Buffer*) shmat(shmid, NULL, 0);

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_RDONLY, 0666, 1);

    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }

    if(buffer == (void *) -1){
        perror("Wrong ID");
        exit(1);
    }

    int sts = sem_wait(buf_sem);

    if(sts == 0){
        printf("Data read in buffer %s: %s\n", buffer->key, buffer->msg); 
        shmdt(buffer); //Se desconecta de la memoria compartida

        sem_post(buf_sem);
        printf("Sem locked and released! \n");
    } else
    {
        perror("sem_trywait error");
        exit(1);
    }
    return 0; 
} 
