#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <string.h>

/*
Se elimina una memoria compartida
@param shmid ID de la memoria compartida a eliminar
*/
void destroy_sharedMem(int shmid){
    shmctl(shmid, IPC_RMID, NULL); //Elimina una memoria compartida
    printf("Shared memory %i has been removed\n", shmid);  
}

/*
Se crea una memoria compartida
@param key_name Nombre de la memoria compartida a utilizar
@param key_id ID de la memoria compartida a utilizar
@param size Tamaño en bytes de la memoria compartida a utilizar
*/
void create_sharedMem(char* key_name, int key_id, int size){
    key_t key = ftok(key_name, key_id); //Crea una llave única
    int shmid = shmget(key, size, 0666|IPC_CREAT); //Identificador de la memoria compartida
    printf("New shared memory with ID: %i\n", shmid);
}

int main(int argc, char **argv){

    char* option = argv[1];

    if(!strcmp(option, "create")){
	char* key_name = argv[2];
	int key_id = atoi(argv[3]);
	int size = atoi(argv[4]);
        create_sharedMem(key_name, key_id, size);
    }

    else if(!strcmp(option, "destroy")){
	int key_id = atoi(argv[2]);
        destroy_sharedMem(key_id);
    }

    else{
	printf("Unrecognized command\n");
    }

    return 1;
}
