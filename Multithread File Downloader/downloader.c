#include "downloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef struct {
    const char *url;
    size_t start;
    size_t end;
    int segment_id;
    DownloadProgress *progress;
} ThreadData;

typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

typedef struct {
    DownloadProgress *progress;
    const char *output_path;
} WriterArgs;

void init_rwlock(RWLock *lock) {
    pthread_mutex_init(&lock->mutex, NULL);
    pthread_cond_init(&lock->can_read, NULL);
    pthread_cond_init(&lock->can_write, NULL);
    lock->readers_count = 0;
    lock->writer_active = 0;
    lock->writers_waiting = 0;
}

void reader_lock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    while (lock->writer_active || lock->writers_waiting > 0) {
        pthread_cond_wait(&lock->can_read, &lock->mutex);
    }
    lock->readers_count++;
    pthread_mutex_unlock(&lock->mutex);
}


void reader_unlock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->readers_count--;
    if (lock->readers_count == 0) {
        pthread_cond_signal(&lock->can_write);
    }
    pthread_mutex_unlock(&lock->mutex);
}

void writer_lock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->writers_waiting++;
    while (lock->writer_active || lock->readers_count > 0) {
        pthread_cond_wait(&lock->can_write, &lock->mutex);
    }
    lock->writers_waiting--;
    lock->writer_active = 1;
    pthread_mutex_unlock(&lock->mutex);
}


void writer_unlock(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->writer_active = 0;
    pthread_cond_broadcast(&lock->can_read);
    pthread_cond_signal(&lock->can_write);
    pthread_mutex_unlock(&lock->mutex);
}


static unsigned long calculate_checksum(const char *data, size_t size) {
    unsigned long checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum = ((checksum << 5) + checksum) + data[i];
    }
    return checksum;
}

static void *verify_segments(void *arg) {
    DownloadProgress *progress = (DownloadProgress *)arg;
    Buffer *buffer = &progress->buffer;
    
    while (!progress->done || buffer->count > 0) {
        reader_lock(&buffer->rwlock);
        
        pthread_mutex_lock(&buffer->mutex);
        if (buffer->count == 0) {
            pthread_mutex_unlock(&buffer->mutex);
            reader_unlock(&buffer->rwlock);
            continue;
        }
        int found = 0;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            Segment *seg = &buffer->buffer[i];
            if (seg->data && !seg->verified) {
                seg->checksum = calculate_checksum(seg->data, seg->size);
                seg->verified = 1;
                found = 1;
                
                pthread_mutex_lock(&progress->mutex);
                progress->segments_verified++;
                pthread_mutex_unlock(&progress->mutex);
                break;
            }
        }
        
        pthread_mutex_unlock(&buffer->mutex);
        reader_unlock(&buffer->rwlock);
        
        if (!found) {
            struct timespec ts = {0, 10000000}; 
            nanosleep(&ts, NULL);
        }
    }
    
    return NULL;
}

static void init_buffer(Buffer *buffer) {
    buffer->in = 0;
    buffer->out = 0;
    buffer->count = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_full, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    init_rwlock(&buffer->rwlock);
    
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer->buffer[i].data = NULL;
        buffer->buffer[i].size = 0;
        buffer->buffer[i].segment_id = -1;
        buffer->buffer[i].is_last = 0;
        buffer->buffer[i].verified = 0;
        buffer->buffer[i].checksum = 0;
    }
}

static void cleanup_buffer(Buffer *buffer) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        free(buffer->buffer[i].data);
    }
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_full);
    pthread_cond_destroy(&buffer->not_empty);
    pthread_mutex_destroy(&buffer->rwlock.mutex);
    pthread_cond_destroy(&buffer->rwlock.can_read);
    pthread_cond_destroy(&buffer->rwlock.can_write);
}

//producer: adding segment to buffer
static void produce_segment(Buffer *buffer, char *data, size_t size, int segment_id, int is_last) {
    pthread_mutex_lock(&buffer->mutex);
    
    while (buffer->count == BUFFER_SIZE) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }
    
    Segment *seg = &buffer->buffer[buffer->in];
    seg->data = data;
    seg->size = size;
    seg->segment_id = segment_id;
    seg->is_last = is_last;
    
    buffer->in = (buffer->in + 1) % BUFFER_SIZE;
    buffer->count++;
    
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;
    
    char *ptr = realloc(mem->memory, mem->size + realsize);
    if (!ptr) return 0;
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    
    return realsize;
}

//producer thread function
static void *download_segment(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    CURL *curl = curl_easy_init();
    if (!curl) {
        pthread_mutex_lock(&data->progress->mutex);
        data->progress->error = 1;
        snprintf(data->progress->error_msg, 256, "Failed to init curl");
        pthread_mutex_unlock(&data->progress->mutex);
        return NULL;
    }
    
    MemoryStruct chunk = {malloc(1), 0};
    
    char range[64];
    snprintf(range, sizeof(range), "%zu-%zu", data->start, data->end);
    
    curl_easy_setopt(curl, CURLOPT_URL, data->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        pthread_mutex_lock(&data->progress->mutex);
        data->progress->error = 1;
        snprintf(data->progress->error_msg, 256, "%s", curl_easy_strerror(res));
        pthread_mutex_unlock(&data->progress->mutex);
        free(chunk.memory);
        curl_easy_cleanup(curl);
        return NULL;
    }
    
    pthread_mutex_lock(&data->progress->mutex);
    data->progress->downloaded += chunk.size;
    pthread_mutex_unlock(&data->progress->mutex);
  
    produce_segment(&data->progress->buffer, chunk.memory, chunk.size, 
                   data->segment_id, data->end == data->progress->total - 1);
    
    curl_easy_cleanup(curl);
    return NULL;
}

static void *write_segments(void *arg) {
    WriterArgs *args = (WriterArgs *)arg;
    DownloadProgress *progress = args->progress;
    const char *output_path = args->output_path;
    Buffer *buffer = &progress->buffer;
    FILE *file = fopen(output_path, "wb");
    if (!file) {
        pthread_mutex_lock(&progress->mutex);
        progress->error = 1;
        snprintf(progress->error_msg, 256, "Could not open output file");
        pthread_mutex_unlock(&progress->mutex);
        free(args);
        return NULL;
    }
    
    int segments_written = 0;
    int total_segments = NUM_THREADS;
    
    while (segments_written < total_segments) {
        pthread_mutex_lock(&buffer->mutex);
        
        while (buffer->count == 0) {
            pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
        }
        
        Segment *seg = &buffer->buffer[buffer->out];
        if (seg->data) {
            fwrite(seg->data, 1, seg->size, file);
            free(seg->data);
            seg->data = NULL;
            segments_written++;
        }
        
        buffer->out = (buffer->out + 1) % BUFFER_SIZE;
        buffer->count--;
        
        pthread_cond_signal(&buffer->not_full);
        pthread_mutex_unlock(&buffer->mutex);
    }
    
    fclose(file);
    free(args);
    return NULL;
}

static size_t get_file_size(const char *url) {
    CURL *curl = curl_easy_init();
    double filesize = 0;
    if (!curl) return 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    if (curl_easy_perform(curl) == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
    }
    
    curl_easy_cleanup(curl);
    return (size_t)filesize;
}

void start_download(const char *url, const char *output_path, DownloadProgress *progress) {
    progress->done = 0;
    progress->error = 0;
    progress->downloaded = 0;
    progress->segments_verified = 0;
    pthread_mutex_init(&progress->mutex, NULL);
    
    init_buffer(&progress->buffer);
    
    size_t filesize = get_file_size(url);
    progress->total = filesize;
    
    if (filesize == 0) {
        pthread_mutex_lock(&progress->mutex);
        progress->error = 1;
        snprintf(progress->error_msg, 256, "Could not get file size");
        pthread_mutex_unlock(&progress->mutex);
        progress->done = 1;
        return;
    }
    
    //creating producer threads (downloaders)
    pthread_t threads[NUM_THREADS];
    ThreadData tdata[NUM_THREADS];
    size_t seg = filesize / NUM_THREADS;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        tdata[i].url = url;
        tdata[i].start = i * seg;
        tdata[i].end = (i == NUM_THREADS - 1) ? filesize - 1 : (i + 1) * seg - 1;
        tdata[i].segment_id = i;
        tdata[i].progress = progress;
        pthread_create(&threads[i], NULL, download_segment, &tdata[i]);
    }
    
    //creating reader threads (verifiers)
    pthread_t reader_threads[NUM_READERS];
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_create(&reader_threads[i], NULL, verify_segments, progress);
    }
    
    //creating consumer thread (writer)
    pthread_t writer_thread;
    WriterArgs *wargs = malloc(sizeof(WriterArgs));
    wargs->progress = progress;
    wargs->output_path = output_path;
    pthread_create(&writer_thread, NULL, write_segments, wargs);
    
    //waitin for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    //waiting for reader threads
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(reader_threads[i], NULL);
    }
    
    pthread_join(writer_thread, NULL);
    
    cleanup_buffer(&progress->buffer);
    pthread_mutex_lock(&progress->mutex);
    progress->done = 1;
    pthread_mutex_unlock(&progress->mutex);
} 
