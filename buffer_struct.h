#ifndef BUFFER_STRUCT_H_INCLUDED
#define BUFFER_STRUCT_H_INCLUDED

typedef struct{
    char key[25];
    int buffer_id;
    int size;
    int consumers_current;
    int producers_current;
    int consumers_total;
    int producers_total;
    int consumers_key_removed;
    int time_waited;
    int time_waiting;
    int time_locked;
    int time_usr;
    int time_kernel;
    char finish_reader[4];
    int flag_stop_producer; //1: stop; 0: continue
    int total_msg;
    char msg[];
} Buffer;

#endif

