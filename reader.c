#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#define BUF_SEM_NAME "BUF_SEM"
  
int main(int argc, char **argv){
  
    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a leer
  
    char *str = (char*) shmat(shmid,(void*)0, 0); 

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_RDONLY, 0666, 1);

    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }

    int sts = sem_trywait(buf_sem);

    if(sts == 0){
        printf("Data read from memory: %s\n", str); 
        shmdt(str); //Se desconecta de la memoria compartida

        sem_post(buf_sem);
        printf("Sem deattached succesfully \n");
    } else
    {
        perror("sem_trywait error");
        exit(1);
    }
    
    return 0; 
} 
