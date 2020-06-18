#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SEM_NAME "BUF_SEM"

typedef struct{
    char* key;
    int buffer_id;
    int size;
    int consumers_current;
    int producers_current;
    char msg[];
} Buffer;

  
int main(int argc, char **argv){

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a escribir
    char* msg = argv[2];
    
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

        strcpy(buffer->msg, msg);
        printf("Data written in buffer: %s\n", buffer->msg); 

        shmdt(buffer);

        sem_post(buf_sem);

        printf("Sem locked and released! \n");
        
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else
    {
        perror("sem_trywait error \n");
    }
    
    return 0; 
} 
