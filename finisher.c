#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "buffer_struct.h"
#include "print_color.h"

#define BUF_SEM_NAME "BUF_SEM"
#define MSG_LEN 60
#define TRUE 1


void show_stats(Buffer *buffer) {

    magenta();
    printf("Final Usage Stats of buffer: %s (ID: %i)\n", buffer->key, buffer->buffer_id);
    reset();

    printf("Total consumers:\t");
    blue();
    printf("%d\n", buffer->consumers_total);
    reset();

    printf("Total producers:\t");
    blue();
    printf("%d \n", buffer->producers_total);
    reset();

    printf("Consumers removed by key:\t");
    blue();
    printf("%d \n\n", buffer->consumers_key_removed);
    reset();

    printf("Time waited:\t");
    blue();
    printf("%d \n", buffer->time_waited);
    reset();

    printf("Time locked:\t");
    blue();
    printf("%d \n", buffer->time_locked);
    reset();

    printf("Time user:\t");
    blue();
    printf("%d \n", buffer->time_usr);
    reset();

    printf("Time kernel:\t");
    blue();
    printf("%d \n", buffer->time_kernel);
    reset();

    printf("Total messages:\t");
    blue();
    printf("%d \n", buffer->total_msg);
    reset();
}

/*
Se elimina una memoria compartida
@param shmid ID de la memoria compartida a eliminar
*/
void destroy_shmem(int shmid, Buffer *buffer){
    show_stats(buffer);
    shmctl(shmid, IPC_RMID, NULL); //Elimina una memoria compartida
    printf("Shared memory %i has been removed\n", shmid);  
}
  
int main(int argc, char **argv){

    int nparams = argc - 1;

    if(nparams < 1) {
        printf( "Wrong number of arguments\n" );
        return -1;
    }

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a eliminar
    
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
    if(sts == 0) {
        buffer->flag_stop_producer = 1;
        strcpy(buffer->msg, buffer->finish_reader); 
        printf("Producers and consumers are stopping\n\n");  
        sem_post(buf_sem);  // Libera el semáforo.
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else
    {
        perror("sem_trywait error \n");
    }


    while(TRUE){
        int sts = sem_wait(buf_sem);
        if(sts == 0) { // Trata de utilizar el semáforo.
            if(buffer->consumers_current == 0 && buffer->producers_current==0){
                sem_close(buf_sem);
                sem_unlink(BUF_SEM_NAME);
                destroy_shmem(buffer->buffer_id, buffer);
                break;
            }     
            sem_post(buf_sem);  // Libera el semáforo.  
        }else if(errno == EAGAIN) {
            printf("Semaphore is locked \n");
        } else {
            perror("sem_trywait error \n");
        }
    }

    return 0; 
} 
