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


#define BUF_SEM_NAME "BUF_SEM"

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

char *build_message(int mnumber, char* message) {
    char time_msg[30] = {0};
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    // prepara parte de fecha
    strcat(time_msg, asctime(timeinfo));
    //time_msg[30] = '\0';

    // prepara parte de número mágico.
    char id_msg[2] = {0}; 
    char numbr_buf[2];
    sprintf(numbr_buf, "%d", mnumber);
    strcat(id_msg, numbr_buf);

    char final_msg[60] = {0};
    strcat(final_msg, "(");
    strcat(final_msg, time_msg);
    strcat(final_msg, ", ");
    strcat(final_msg, id_msg);
    strcat(final_msg, ", ");
    strcat(final_msg, message);
    strcat(final_msg, ")");
    final_msg[60] = '\0';

    char *fmsg_ptr = (char*)malloc(60*sizeof(char) + 1);

    strcpy(fmsg_ptr, final_msg); 

    return fmsg_ptr;
}
  
int main(int argc, char **argv){

    clock_t begin = clock();

    int nparams = argc - 1;

    if(nparams < 2) {
        printf( "Wrong number of arguments\n" );
        return -1;
    }

    int shmid = atoi(argv[1]); //ID de la memoria en la que se va a escribir
    double lambda = atof(argv[2]);
    char* msg = argv[3];

    srand((unsigned)time(NULL));

    int tics_per_second = sysconf(_SC_CLK_TCK);

    Stats prod_stats = {getpid(), 0, 0.0, 0.0, 0.0};

    int magic_number = prod_stats.pid % 6;
    
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

    int stopped;

    // aumenta variables globales.
    if(!sem_wait(buf_sem)) { // Trata de utilizar el semáforo.
        buffer->producers_current += 1;
        buffer->producers_total += 1;
        stopped = buffer->flag_stop_producer;
        sem_post(buf_sem);  // Libera el semáforo.
    }
   
    while(!stopped) {

        double time_until_msg = get_waiting_time(lambda);

        printf("Wainting for: %f \n", time_until_msg);

        sleep(time_until_msg);

        prod_stats.wait_time += time_until_msg;

        char *f_msg = build_message(magic_number, msg); 

        printf("Message: %s \n", f_msg);

        // check if flag has changed here

        printf("Message sent! \n"); //upon entry

        prod_stats.total_msgs += 1;

        // trata de colocar un mensaje en el buffer.
        if(!sem_wait(buf_sem)) {

            clock_t end = clock();

            prod_stats.time_blocked += (double)(end - begin) / CLOCKS_PER_SEC;

            strcpy(buffer->msg, f_msg);
            printf("Message written in buffer successfully! \n");
            printf("Message: %s \n ", f_msg);
            printf("Index: %d ", 0);
            printf("Active Producers: %d ", buffer->producers_current); 
            printf("Active Consumers: %d ", buffer->consumers_current);  
            
            free(f_msg);
            sem_post(buf_sem);
     
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
        prod_stats.kernel_time = sys_time;
    }

    printf("Total msg: %d \n Time waited: %f \n Time blocked: %f \n Kernel Time: %f \n", prod_stats.total_msgs, prod_stats.wait_time,
    prod_stats.time_blocked, prod_stats.kernel_time);


    // stops the producer
    if(!sem_wait(buf_sem)) {
        buffer->producers_current -= 1;

        sem_post(buf_sem);
        printf("Releasing semaphore...\n");

        printf("Deattaching producer from Buffer... \n");
        shmdt(buffer);

        printf("Producer ended elegantly!");
            
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else {
        perror("sem_trywait error \n");
    }
    
    return 0; 
} 
