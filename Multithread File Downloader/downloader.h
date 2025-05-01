#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <stddef.h>
#include <pthread.h>

#define NUM_THREADS 4      //(producer threads)these will perform downloading
#define BUFFER_SIZE 8      //setting the capacity of the buffer holding downloaded segments
#define NUM_READERS 2      //number of reader threads for verification

//we'll define a struct to store a single segment(piece) of file
typedef struct {
    char *data;           
    size_t size;         
    int segment_id;      
    int is_last;        
    int verified;        
    unsigned long checksum; 
} Segment;

//reader-writer structure
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t can_read;
    pthread_cond_t can_write;
    int readers_count;
    int writer_active;
    int writers_waiting;
} RWLock;

//defining a circular buffer to support producer consumer pattern
typedef struct {
    Segment buffer[BUFFER_SIZE];  
    int in;                       
    int out;                      
    int count;                    
    pthread_mutex_t mutex;        
    pthread_cond_t not_full;      
    pthread_cond_t not_empty;     
    RWLock rwlock;               
} Buffer;

//defining a struct to track the progress of the download
typedef struct {
    pthread_mutex_t mutex;
    size_t downloaded;
    size_t total;
    int done;
    int error;
    char error_msg[256];
    Buffer buffer;              
    int segments_verified;      
} DownloadProgress;


void start_download(const char *url, const char *output_path, DownloadProgress *progress);

void init_rwlock(RWLock *lock);

void reader_lock(RWLock *lock);

void reader_unlock(RWLock *lock);

void writer_lock(RWLock *lock);

void writer_unlock(RWLock *lock);

#endif  
