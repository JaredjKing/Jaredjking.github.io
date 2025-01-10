#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <getopt.h>
#include <errno.h>   
#include "helper.h"
#include <dirent.h>
#include <string.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>


int thread_crawl(bool *waiting_appended){
    //printf("starting 1 pull\n");
    CURL *curl;
    CURLcode res;
    char url[256];
    RECV_BUF recv_buf = {NULL, 0, 0, 0};

    // get URL to search 
    if(sem_trywait(&frontier_available) != 0){
        if(!(*waiting_appended)){
            *waiting_appended = true;
            atomic_fetch_add(&threads_waiting, 1);
        }
        return 0;
    }
    if(*waiting_appended){
        atomic_fetch_sub(&threads_waiting, 1);
        *waiting_appended = false;
    }

    pthread_mutex_lock(&frontier_mutex);
    char* to_visit = pop(&frontiers);
    pthread_mutex_unlock(&frontier_mutex);

    // check that there is a url to check
    if(to_visit == NULL){
        printf("no url to visit\n");
        return 0;
    }

    // check if the url has already been visited
    pthread_mutex_lock(&visited_mutex);
    for(int i = 0; i < (visited.pos+1); i++){
        if (strcmp(to_visit, visited.urls[i]) == 0) {
            //printf("URL already exists\n");
            free(to_visit);
            to_visit = NULL;
            pthread_mutex_unlock(&visited_mutex);
            return 0;
        }
    }

    // let other processes know that this url is beeing visited
    push(&visited, to_visit);
    pthread_mutex_unlock(&visited_mutex);

    // initiate curl session
    //curl = curl_easy_init();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = easy_handle_init(&recv_buf, to_visit);

    if( curl == NULL ){
        curl_global_cleanup();
        printf("error creating curl");
        return 0;
    } 
