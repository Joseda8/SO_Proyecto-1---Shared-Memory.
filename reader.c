#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/times.h>
#include "buffer_struct.h"

#define FALSE 0
#define TRUE 1
#define BUF_SEM_NAME "BUF_SEM"
#define AUTO_MODE "auto"
#define MANUAL_MODE "manual"
#define MSG_LEN 60
#define MAX_IN_MSG_LEN 29

typedef struct{
    int pid;
    char exitCause[20];
    int total_msgs_read;       // total de mensajes consumidos.
    double wait_time;    // tiempo esperado para poner un mensaje.
    double time_blocked; // tiempo bloqueado por semáforo.
    double kernel_time;  // tiempo en kernel.
} Stats;

int get_waiting_time(double lambda) {
    double expLambda = exp(-lambda);
    double randU;
    double prodU = 1.0;
    int result = 0;

    do {
        randU = rand() / (RAND_MAX + 1.0);
        prodU = prodU * randU;
        result++;
    }
    while (prodU > expLambda); 
    return result - 1;
}

int find_message(char *buf, int indexes, int current_index) {
    int tmp_ind = indexes - current_index / MSG_LEN;
    int ctr = current_index;
    char start = '(';
    while(tmp_ind > 0) {
        if(buf[ctr] == start) return ctr;
        ctr += MSG_LEN;
        tmp_ind--;
    }
    return -1;
}

int main(int argc, char **argv){

    int nparams = argc - 1;

    if(nparams < 2) {
        printf( "Wrong number of arguments\n" );
        return -1;
    }

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a leer
    double lambda = atof(argv[2]);
    char* mode = argv[3];

    if(!strcmp(mode, MANUAL_MODE)) {
        // Bandera a modo de operacion manual
    } else if (!strcmp(mode, AUTO_MODE)) {
        // Bandera a modo de operacion automatico
    } else {
        perror("Undefined operation mode");
        exit(1);
    }

    srand((unsigned)time(NULL));

    int tics_per_second = sysconf(_SC_CLK_TCK);

    Stats cons_stats = {getpid(), "", 0, 0.0, 0.0, 0.0};

    int magic_number = cons_stats.pid % 6; // calcula número mágico.
    
    Buffer *buffer = (Buffer*) shmat(shmid, NULL, 0);

    int spaces_max = (int)floor(buffer->size / MSG_LEN); // Calcula el número de índices que pueden tener mensajes.

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_RDONLY, 0666, 1);

    int current_buffer_index = 0;  // Posicion del buffer por la que se va leyendo

    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }

    if(buffer == (void *) -1){
        perror("Wrong ID");
        exit(1);
    }

    int stopped;

    // aumenta variables globales.
    if(!sem_wait(buf_sem)) { // Trata de utilizar el semáforo.
        buffer->consumers_current += 1;
        buffer->consumers_total += 1;
        sem_post(buf_sem);  // Libera el semáforo.
    }
   
    while(TRUE) {  // Busy waiting, falta lo de mimir

        double time_until_msg = get_waiting_time(lambda);  // obtiene próximo tiempo de espera aleatorio.

        printf("Waiting for: %f \n", time_until_msg);

        sleep(time_until_msg);  // espera para leer un mensaje.

        cons_stats.wait_time += time_until_msg;

        clock_t begin = clock(); // timer para medir tiempo bloqueado por semáforo.

        char r_msg[MSG_LEN] = {0};

        cons_stats.total_msgs_read += 1; // error

        // trata de leer un mensaje en el buffer.
        if(!sem_wait(buf_sem)) {  // espera por el semáforo.

            clock_t end = clock();

            cons_stats.time_blocked += (double)(end - begin) / CLOCKS_PER_SEC; // calcula tiempo bloqueado por el semáforo.

            int index_available = find_message(buffer->msg, spaces_max, current_buffer_index);

            if (index_available == -1){
                current_buffer_index = 0;
                sem_post(buf_sem);
                continue;    
            }

            current_buffer_index = index_available + MSG_LEN; // Actualiza el indice al siguiente mensaje del leido 

            memcpy(r_msg, buffer->msg + index_available, MSG_LEN);
            printf("Message read from buffer successfully! \n");
            printf("Message: %s \n", r_msg);
            printf("Index: %d \n", index_available);
            printf("Active Producers: %d \n", buffer->producers_current); 
            printf("Active Consumers: %d \n\n", buffer->consumers_current);  

            // TODO: condiciones de terminacion
            char magic_msg_number = r_msg[27];
            int mag_msg_number = magic_msg_number - '0';
            if (magic_number == mag_msg_number) {
                strcat(cons_stats.exitCause, "Magic number matched!");
                sem_post(buf_sem);
                break;
            }

            
            memset(buffer->msg + index_available, 0, MSG_LEN); // Borra el mensaje leido

            sem_post(buf_sem); // libera el semáforo.
     
        } else if(errno == EAGAIN) {
            printf("Semaphore is locked \n");
        } else {
            perror("sem_trywait error \n");
        }
    }

    struct tms times_struct;
    clock_t ptimes = times(&times_struct);

    if(ptimes == -1) {
        perror("Couldn't get times of process");
        exit(1);
    } else {
        double sys_time = ((double)times_struct.tms_stime) / tics_per_second;
        cons_stats.kernel_time = sys_time;
    }

    printf("Consumer about to exit...\n");
    printf("Process ID: %d \nProcess end cause: %s \nTotal msg read: %d \nTime waited: %f \nTime blocked: %f \nKernel Time: %f \n", cons_stats.pid, cons_stats.exitCause, cons_stats.total_msgs_read, cons_stats.wait_time,
    cons_stats.time_blocked, cons_stats.kernel_time);

    // stops the consumer
    if(!sem_wait(buf_sem)) {
        buffer->consumers_current -= 1;

        sem_post(buf_sem);
        printf("Releasing semaphore...\n");

        printf("Deattaching consumer from Buffer... \n");
        shmdt(buffer);

        printf("Consumer ended elegantly! \n");
            
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else {
        perror("sem_trywait error \n");
    }
    
    return 0; 
} 