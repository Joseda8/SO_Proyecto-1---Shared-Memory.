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
#include <sys/times.h>
#include "buffer_struct.h"


#define BUF_SEM_NAME "BUF_SEM"
#define MSG_LEN 60
#define MAX_IN_MSG_LEN 29
#define SLEEP_TIME 2

typedef struct{
    int pid;
    int total_msgs;       // total de mensajes.
    double wait_time;    // tiempo esperado para poner un mensaje.
    double time_blocked; // tiempo bloqueado por semáforo.
    double kernel_time;  // tiempo en kernel.
} Stats;

double get_waiting_time(double lambda) {
    double u;
    u = rand() / (RAND_MAX + 1.0);
    return -log(1 - u) / lambda;
}

void show_stats(Stats *stats) {
    printf("PID: %d \n", stats->pid);
    printf("Total messages produced: %d \n", stats->total_msgs);
    printf("Wait time: %f \n", stats->wait_time);
    printf("Blocked time: %f \n", stats->time_blocked);
    printf("Kernel time: %f \n", stats->kernel_time);
}

char *build_message(int mnumber, char* message) {
    char time_msg[30] = {0};
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime (&rawtime);

    // prepara parte de fecha
    strcat(time_msg, asctime(timeinfo));
    time_msg[24] = 0;  // Gabriel: perro para quitarle el \n que no me cuadra como se ve en el printf

    // prepara parte de número mágico.
    char id_msg[2] = {0}; 
    char numbr_buf[2];
    sprintf(numbr_buf, "%d", mnumber);
    strcat(id_msg, numbr_buf);

    char final_msg[MSG_LEN] = {0};
    strcat(final_msg, "(");
    strcat(final_msg, time_msg);
    strcat(final_msg, ", ");
    strcat(final_msg, id_msg);
    strcat(final_msg, ", ");
    strcat(final_msg, message);
    strcat(final_msg, ")");
    final_msg[MSG_LEN] = '\0';

    char *fmsg_ptr = (char*)malloc(MSG_LEN*sizeof(char) + 1);

    strcpy(fmsg_ptr, final_msg); 

    return fmsg_ptr;
}

// trata de encontrar un espacio en el buffer para poner el mensaje. Si no se encuentra retorna -1.
int find_space(char *buf, int indexes) {
    int tmp_ind = indexes;
    int ctr = 0;
    char start = '(';
    while(tmp_ind > 0) {
        if(buf[ctr] != start) return ctr;
        ctr += MSG_LEN;
        tmp_ind--;
    }
    return -1;
}
  
int main(int argc, char **argv){

    struct rusage usage;

    int nparams = argc - 1;

    if(nparams < 2) {
        printf( "Wrong number of arguments\n" );
        return -1;
    }

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a escribir
    double lambda = atof(argv[2]);
    char* msg = argv[3];

    if(strlen(msg) >= MAX_IN_MSG_LEN) {
        perror("Tamaño de mensaje no puede exceder o ser igual a 29 caracteres");
        exit(1);
    }

    srand((unsigned)time(NULL));

    Stats prod_stats = {getpid(), 0, 0.0, 0.0, 0.0};

    int magic_number = prod_stats.pid % 6; // calcula número mágico.
    
    Buffer *buffer = (Buffer*) shmat(shmid, NULL, 0);

    int spaces_max = (int)floor(buffer->size / MSG_LEN); // Calcula el número de índices que pueden tener los mensajes.

    clock_t begin;  // Timers para medir tiempo bloqueado por semáforos.
    clock_t end;

    /*int ctr = 0;

    while(ctr < 1024) {
        printf("curr pos %d: %c \n", ctr, buffer->msg[ctr]); 
        //buffer->msg[ctr] = ' ';
        ctr++;
    }*/

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_RDONLY);

    
    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }

    if(buffer == (void *) -1){
        perror("Wrong ID");
        exit(1);
    }

    begin = clock();

    // aumenta variables globales.
    if(!sem_wait(buf_sem)) { // Trata de utilizar el semáforo.
        end = clock();
        prod_stats.time_blocked += (double)(end - begin) / CLOCKS_PER_SEC;

        buffer->producers_current += 1;
        buffer->producers_total += 1;
        sem_post(buf_sem);  // Libera el semáforo.
    }
   
    while(1) {

        begin = clock(); // timer para medir tiempo bloqueado por semáforo.

        if(!sem_wait(buf_sem)) {
            end = clock();
            prod_stats.time_blocked += (double)(end - begin) / CLOCKS_PER_SEC; // calcula tiempo bloqueado por el semáforo.
            
            if(buffer->flag_stop_producer) { // verifica que no se hayan finalizado los servicios.
                sem_post(buf_sem);
                break;
            }
            
            sem_post(buf_sem);
        }

        double time_until_msg = get_waiting_time(lambda);  // obtiene próximo tiempo de espera aleatorio.

        printf("Waiting for: %f \n", time_until_msg);

        sleep(time_until_msg);  // espera para enviar el mensaje.

        prod_stats.wait_time += time_until_msg;

        char *f_msg = build_message(magic_number, msg);  // construye el mensaje nuevo.

        begin = clock(); // timer para medir tiempo bloqueado por semáforo.

        // trata de colocar un mensaje en el buffer.
        if(!sem_wait(buf_sem)) {  // espera por el semáforo.

            end = clock();

            prod_stats.time_blocked += (double)(end - begin) / CLOCKS_PER_SEC; // calcula tiempo bloqueado por el semáforo.

            int index_available = find_space(buffer->msg, spaces_max);

            if(index_available >= 0) {
                strcpy(buffer->msg + index_available, f_msg);
                printf("Message written in buffer successfully! \n");
                printf("Message: %s \n ", f_msg);
                printf("Index: %d \n", index_available);
                printf("Active Producers: %d \n", buffer->producers_current); 
                printf("Active Consumers: %d \n", buffer->consumers_current);  
                
                free(f_msg);               
                printf("Message sent! \n\n"); 
                sem_post(buf_sem); // libera el semáforo.
                prod_stats.total_msgs += 1;
            } else
            {
                printf("Buffer is full... \n");
                free(f_msg);
                sem_post(buf_sem); // libera el semáforo.
                sleep(SLEEP_TIME);
            }
        } else if(errno == EAGAIN) {
            printf("Semaphore is locked \n");
        } else {
            perror("sem_trywait error \n");
        }
    }

    begin = clock(); // timer para medir tiempo bloqueado por semáforo.

    // stops the producer
    if(!sem_wait(buf_sem)) {

        end = clock();
        prod_stats.time_blocked += (double)(end - begin) / CLOCKS_PER_SEC;

        buffer->producers_current -= 1;
        buffer->total_msg += prod_stats.total_msgs;

        sem_post(buf_sem);
        printf("Releasing semaphore...\n");

        printf("Deattaching producer from Buffer... \n");
        shmdt(buffer);

        printf("Producer has ended elegantly! \n");
            
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else {
        perror("sem_trywait error \n");
    }

    getrusage(RUSAGE_SELF, &usage);

    prod_stats.kernel_time += usage.ru_stime.tv_usec;

    show_stats(&prod_stats);
    
    return 0; 
} 
