#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#define BUF_SEM_NAME "BUF_SEM"
  
int main(int argc, char **argv){

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a escribir
    char *str = (char*) shmat(shmid, NULL, 0); 

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_RDONLY, 0666, 1);

    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }

    int sts = sem_trywait(buf_sem);

    if(sts == 0) {
        printf("Write Data : "); 
        fgets(str, 20, stdin);
    
        printf("Data written in memory: %s\n", str); 

        shmdt(str);

        sem_post(buf_sem);

        printf("Sem up succesfully \n");
        
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else
    {
        perror("sem_trywait error \n");
    }
    
    return 0; 
} 
