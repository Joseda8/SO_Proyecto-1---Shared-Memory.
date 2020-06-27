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
#include "print_color.h"

#define BUF_SEM_NAME "BUF_SEM"
#define MSG_LEN 60
#define MAX_IN_MSG_LEN 29
#define SLEEP_TIME 2

typedef struct{
    int pid;
    int total_msgs;       // total de mensajes.
    int wait_time;    // tiempo esperado para poner un mensaje.
    int time_blocked; // tiempo bloqueado por semáforo.
    int kernel_time;  // tiempo en kernel.
} Stats;

double get_waiting_time(double lambda) {
    double u;
    u = rand() / (RAND_MAX + 1.0);
    return -log(1 - u) / lambda;
}

void show_stats(Stats *stats) {
    magenta();
    printf("Final Usage Stats:\n");

    reset();
    printf("Process ID:         ");
    blue();
    printf("%d \n", stats->pid);
    reset();

    printf("Total msg produced: ");
    blue();
    printf("%d \n", stats->total_msgs);
    reset();

    printf("Time waited:        ");
    blue();
    printf("%d seconds\n", stats->wait_time);
    reset();

    printf("Time blocked:       ");
    blue();
    printf("%d microseconds\n", stats->time_blocked);
    reset();

    printf("Time in kernel:     ");
    blue();
    printf("%d microseconds\n\n", stats->kernel_time);
    reset();
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

    if(buffer == (void *) -1){
        perror("Wrong ID");
        exit(1);
    }

    int spaces_max = (int)floor(buffer->size / MSG_LEN); // Calcula el número de índices que pueden tener los mensajes.

    struct timeval begin, end;  // Timers para medir tiempo de bloqueo por semáforo.

    sem_t *buf_sem = sem_open(BUF_SEM_NAME, O_RDONLY);

    if(buf_sem == (void*) -1) {
        perror("sem_open error");
        exit(1);
    }

    gettimeofday(&begin, NULL);

    // aumenta variables globales.
    if(!sem_wait(buf_sem)) { // Trata de utilizar el semáforo.
        gettimeofday(&end, NULL);
        prod_stats.time_blocked += end.tv_usec - begin.tv_usec;

        buffer->producers_current += 1;
        buffer->producers_total += 1;
        sem_post(buf_sem);  // Libera el semáforo.
    }
   
    while(1) {

        gettimeofday(&begin, NULL);

        if(!sem_wait(buf_sem)) {
            gettimeofday(&end, NULL);
            prod_stats.time_blocked += end.tv_usec - begin.tv_usec;
            
            if(buffer->flag_stop_producer) { // verifica que no se hayan finalizado los servicios.
                sem_post(buf_sem);
                break;
            }          
            sem_post(buf_sem);
        }

        double time_until_msg = get_waiting_time(lambda);  // obtiene próximo tiempo de espera aleatorio.

        printf("Waiting for: ");
        blue();
        printf("%f seconds\n\n", time_until_msg);
        reset();

        sleep(time_until_msg);  // espera para enviar el mensaje.

        prod_stats.wait_time += time_until_msg;

        gettimeofday(&begin, NULL);

        // trata de colocar un mensaje en el buffer.
        if(!sem_wait(buf_sem)) {  // espera por el semáforo.

            gettimeofday(&end, NULL);
            prod_stats.time_blocked += end.tv_usec - begin.tv_usec;

            int index_available = find_space(buffer->msg, spaces_max);

            if(index_available >= 0) {
                char *f_msg = build_message(magic_number, msg);  // construye el mensaje nuevo.
                strcpy(buffer->msg + index_available, f_msg);

                green();
                printf("Message written in buffer successfully! \n");
                reset();

                printf("Message:          ");
                blue();
                printf("%s \n", f_msg);
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
                
                free(f_msg);               
                printf("Message sent! \n\n"); 
                sem_post(buf_sem); // libera el semáforo.
                prod_stats.total_msgs += 1;
            } else
            {
                blue();
                printf("Buffer is full... \n");
                reset();
                //free(f_msg);
                sem_post(buf_sem); // libera el semáforo.
                sleep(SLEEP_TIME);
            }
        } else if(errno == EAGAIN) {
            printf("Semaphore is locked \n");
        } else {
            perror("sem_trywait error \n");
        }
    }

    gettimeofday(&begin, NULL);

    // stops the producer
    if(!sem_wait(buf_sem)) {

        gettimeofday(&end, NULL);
        prod_stats.time_blocked += end.tv_usec - begin.tv_usec;

        buffer->producers_current -= 1;
        buffer->total_msg += prod_stats.total_msgs;

        getrusage(RUSAGE_SELF, &usage);

        prod_stats.kernel_time += usage.ru_stime.tv_usec;

        buffer->time_kernel += prod_stats.kernel_time;
        buffer->time_locked += prod_stats.time_blocked;
        buffer->time_waiting += prod_stats.wait_time;
        buffer->time_usr += usage.ru_utime.tv_usec;

        magenta();

        printf("Releasing semaphore...\n");
        sem_post(buf_sem);

        printf("Deattaching consumer from Buffer... \n\n");
        shmdt(buffer);

        green();
        printf("Producer ended successfully! Take a look to the stats:\n\n");
        reset();   
            
    } else if(errno == EAGAIN) {
        printf("Semaphore is locked \n");
    } else {
        perror("sem_trywait error \n");
    }

    show_stats(&prod_stats);
    
    return 0; 
} 
