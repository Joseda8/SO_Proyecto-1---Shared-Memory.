#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include "buffer_struct.h"

#define FALSE 0
#define TRUE 1
#define BUF_SEM_NAME "BUF_SEM"
#define AUTO_MODE "auto"
#define MANUAL_MODE "manual"
#define MSG_LEN 60
#define SLEEP_TIME 5

typedef struct{
    int pid;
    char exitCause[30];
    int total_msgs_read;       // total de mensajes consumidos.
    int wait_time;    // tiempo esperado para poner un mensaje.
    int time_blocked; // tiempo bloqueado por semáforo.
    int kernel_time;  // tiempo en kernel.
    int mode_flag; // bandera de modo de operacion
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

void blue() {
    printf("\033[1;34m");
}

void green() {
    printf("\033[1;32m");
}

void magenta() {
    printf("\033[1;35m");
}

void reset() {
    printf("\033[0m");
}

int main(int argc, char **argv){

    struct rusage usage;

    int nparams = argc - 1;

    if(nparams < 2) {
        printf( "Wrong number of arguments\n" );
        return -1;
    }

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a leer
    double lambda = atof(argv[2]);
    char* mode = argv[3];

    srand((unsigned)time(NULL));

    Stats cons_stats = {getpid(), "", 0, 0, 0, 0, 1};

    int magic_number = cons_stats.pid % 6; // calcula número mágico.
    
    Buffer *buffer = (Buffer*) shmat(shmid, NULL, 0);

    int spaces_max = (int)floor(buffer->size / MSG_LEN); // Calcula el número de índices que pueden tener mensajes.

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_RDONLY);

    int current_buffer_index = 0;  // Posicion del buffer por la que se va leyendo

    struct timeval begin;  // Timers para medir tiempo bloqueado por semáforos.
    struct timeval end;

    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }

    if(buffer == (void *) -1){
        perror("Wrong ID");
        exit(1);
    }

    if(!strcmp(mode, MANUAL_MODE)) {
        // Bandera a modo de operacion manual
        cons_stats.mode_flag = 0;
    } else if (!strcmp(mode, AUTO_MODE)) {
        // Bandera a modo de operacion automatico
        cons_stats.mode_flag = 1;
    } else {
        perror("Undefined operation mode");
        exit(1);
    }

    gettimeofday(&begin, NULL);

    // aumenta variables globales.
    if(!sem_wait(buf_sem)) { // Trata de utilizar el semáforo.
        gettimeofday(&end, NULL);
        cons_stats.time_blocked += end.tv_usec - begin.tv_usec;

        buffer->consumers_current += 1;
        buffer->consumers_total += 1;
        sem_post(buf_sem);  // Libera el semáforo.
    }
   
    while(TRUE) { 

        if (cons_stats.mode_flag) {

            int time_until_msg = get_waiting_time(lambda);  // obtiene próximo tiempo de espera aleatorio.

            printf("Waiting for: ");
            blue();
            printf("%d seconds\n\n", time_until_msg);
            reset();

            sleep(time_until_msg);  // espera para leer un mensaje.

            cons_stats.wait_time += time_until_msg;

        } else {

            time_t begint = time(NULL);

            printf("Press ");
            blue();
            printf("[Enter]");
            reset();
            printf(" key to read a message \n");
            getchar();

            time_t endt = time(NULL);

            cons_stats.wait_time += endt - begint;
        }

        gettimeofday(&begin, NULL); // timer para medir tiempo bloqueado por semáforo.

        char r_msg[MSG_LEN] = {0};

        // trata de leer un mensaje en el buffer.
        if(!sem_wait(buf_sem)) {  // espera por el semáforo.

            gettimeofday(&end, NULL);

            cons_stats.time_blocked += end.tv_usec - begin.tv_usec; // calcula tiempo bloqueado por el semáforo.

            int index_available = find_message(buffer->msg, spaces_max, current_buffer_index);

            if (index_available == -1){
                magenta();
                printf("Waiting for a message...\n");
                reset();
                current_buffer_index = 0;
                sem_post(buf_sem);
                sleep(SLEEP_TIME);
                continue;    
            }

            current_buffer_index = index_available + MSG_LEN; // Actualiza el indice al siguiente mensaje del leido 
            if (current_buffer_index == (spaces_max * MSG_LEN)) {
                current_buffer_index = 0;     // Recorrido circular del buffer
            }


            memcpy(r_msg, buffer->msg + index_available, MSG_LEN);
            green();
            printf("Message read from buffer successfully! \n");
            reset();
            printf("Message:          ");
            blue();
            printf("%s \n", r_msg);
            reset();
            printf("Index:            ");
            blue();
            printf("%d \n", index_available);
            reset();
            printf("Active Producers: ");
            blue();
            printf("%d \n", buffer->producers_current);
            reset(); 
            printf("Active Consumers: ");
            blue();
            printf("%d \n\n", buffer->consumers_current);
            reset();  

            // Condiciones de terminacion
            char magic_msg_number = r_msg[27];
            int mag_msg_number = magic_msg_number - '0';
            if (magic_number == mag_msg_number) {
                strcat(cons_stats.exitCause, "Magic number matched!");
                cons_stats.total_msgs_read += 1;
                sem_post(buf_sem);
                break;
            }

            if (!strcmp(r_msg, "(EXIT)")){
                strcat(cons_stats.exitCause, "EXIT command recibed!");
                cons_stats.total_msgs_read += 1;
                sem_post(buf_sem);
                break;
            }

            cons_stats.total_msgs_read += 1;
            
            memset(buffer->msg + index_available, 0, MSG_LEN); // Borra el mensaje leido

            sem_post(buf_sem); // libera el semáforo.
     
        } else if(errno == EAGAIN) {
            printf("Semaphore is locked \n");
        } else {
            perror("sem_trywait error \n");
        }
    }

    getrusage(RUSAGE_SELF, &usage);

    cons_stats.kernel_time += usage.ru_stime.tv_usec;

    magenta();
    printf("Consumer about to exit...\n\n");

    printf("Final Usage Stats:\n");
    reset();
    printf("Process ID:         ");
    blue();
    printf("%d \n", cons_stats.pid);
    reset();
    printf("Process end cause:  ");
    blue();
    printf("%s \n", cons_stats.exitCause);
    reset(); 
    printf("Total msg read:     ");
    blue();
    printf("%d \n", cons_stats.total_msgs_read);
    reset();
    printf("Time waited:        ");
    blue();
    printf("%d seconds\n", cons_stats.wait_time);
    reset();
    printf("Time blocked:       ");
    blue();
    printf("%d microseconds\n", cons_stats.time_blocked);
    reset();
    printf("Time in kernel:     ");
    blue();
    printf("%d microseconds\n\n", cons_stats.kernel_time);
    reset();

    // stops the consumer
    if(!sem_wait(buf_sem)) {
        buffer->consumers_current -= 1;

        buffer->time_kernel += cons_stats.kernel_time;
        buffer->time_waiting += cons_stats.wait_time;
        buffer->time_locked += cons_stats.time_blocked;

        magenta();

        printf("Releasing semaphore...\n");
        sem_post(buf_sem);

        printf("Deattaching consumer from Buffer... \n\n");
        shmdt(buffer);

        green();
        printf("Consumer ended successfully! \n\n");
        reset();    
        
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else {
        perror("sem_trywait error \n");
    }
    
    return 0; 
} 