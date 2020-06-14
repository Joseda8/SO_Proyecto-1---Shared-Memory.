#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
  
int main(int argc, char **argv){
  
    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a leer
  
    char *str = (char*) shmat(shmid,(void*)0,0); 

    printf("Data read from memory: %s\n",str); 
    shmdt(str); //Se desconecta de la memoria compartida
     
    return 0; 
} 
