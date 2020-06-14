#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
  
int main(int argc, char **argv){

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a escribir
    char *str = (char*) shmat(shmid,(void*)0,0); 

    printf("Write Data : "); 
    fgets(str, 20, stdin);
  
    printf("Data written in memory: %s\n", str); 

    shmdt(str); 
  
    return 0; 
} 
