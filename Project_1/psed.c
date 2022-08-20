/*  Paralelní SED 
 *  
 *  Řešení IPS-DU1
 *  Autor:  Josef Oškera, FIT (xosker03)
 *          Radek Duchoň, FIT (xducho07)
 *  Přeloženo: gcc 5.4.0
 *  Datum: 17.10.2018
 */


#include <regex>
#include <string>
#include <iostream>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>


using namespace std;

//GLOBAL
char *gdata;
int print_counter = -1;
int counters;
int ende = 0;
int *ready;


//MUTEXY
pthread_mutex_t (*mt_podm)[][2]; 
pthread_cond_t (*cd_podm)[][2];


//STRUKTURA
typedef struct {
    int id;
    char *regex;
    char *replace;
} vl_arg_t;


//PROTOTYPY
void vlakno(vl_arg_t *d);
char *read_line(void);
    

int main(int argc, char *argv[]) {
    /*******************************
     * Inicializace threadu a zamku
     * *****************************/ 
    if ((argc - 1) % 2 || argc < 3) {
        printf("CHYBA: argumenty\n");
        return 1;
    }
    
    //ARGUMENTY
    int counters = (argc - 1) / 2; 
    vl_arg_t ar[counters];
    
    for (int i = 0; i < counters; ++i) {
        ar[i].id = i;
        ar[i].regex = argv[2 * i + 1];
        ar[i].replace = argv[2 * i + 2];
    }
    
    //MUTEXY
    pthread_mutex_t mt_podm_pole[counters+1][2];
    pthread_cond_t cd_podm_pole[counters+1][2];
    mt_podm = (pthread_mutex_t (*)[][2]) &mt_podm_pole;
    cd_podm = (pthread_cond_t (*)[][2]) &cd_podm_pole;
    
    int ready_pole[counters+1];
    ready = ready_pole;
    memset(ready_pole, -1, sizeof(int) * (counters + 1));
   
    //MUTEX AND COND INIT
    for(int i = 0; i < counters + 1; ++i){
        pthread_mutex_init(&(*mt_podm)[i][0], NULL);
        pthread_mutex_init(&(*mt_podm)[i][1], NULL);
        pthread_cond_init(&(*cd_podm)[i][0], NULL);
        pthread_cond_init(&(*cd_podm)[i][1], NULL);
    }
    
    //VLAKNA
    pthread_t vlakna[counters];
    for (int i = 0; i < counters; ++i)
        pthread_create(&vlakna[i], NULL, (void* (*)(void*)) vlakno, (void*)&ar[i]);
    
    //START///////////////////////////////////////////////////////////////////////////////////////////
    gdata = NULL;
    
    while (1) {
        gdata = read_line();
        
        if (!gdata) break;

        for (int i = 0; i < counters; ++i) {
            pthread_mutex_lock(&(*mt_podm)[i][0]);
            ready[i] = 0;
        }

        print_counter = 0;

        for (int i = 0; i < counters; ++i) {
            pthread_cond_signal(&(*cd_podm)[i][0]);
            pthread_mutex_unlock(&(*mt_podm)[i][0]);
        }
        ///////////////////////////////////////////////
        pthread_mutex_lock(&(*mt_podm)[0][1]);
        pthread_cond_signal(&(*cd_podm)[0][1]);
        pthread_mutex_unlock(&(*mt_podm)[0][1]);
        pthread_mutex_lock(&(*mt_podm)[counters][1]);

        while (print_counter != counters)
            pthread_cond_wait(&(*cd_podm)[counters][1], &(*mt_podm)[counters][1]);
        
        pthread_mutex_unlock(&(*mt_podm)[counters][1]);    
        free(gdata);
    }
    
    //KONEC ///////////////////////////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < counters; ++i) 
        pthread_mutex_lock(&(*mt_podm)[i][0]);
    
    ende = 1;
    
    for (int i = 0; i < counters; ++i) {
        pthread_cond_signal(&(*cd_podm)[i][0]);
        pthread_mutex_unlock(&(*mt_podm)[i][0]);
    }

    for (int i = 0; i < counters; ++i)
        pthread_join(vlakna[i], NULL);

    for (int i = 0; i < counters + 1; ++i) {
        pthread_mutex_destroy(&(*mt_podm)[i][0]);
        pthread_mutex_destroy(&(*mt_podm)[i][1]);
        pthread_cond_destroy(&(*cd_podm)[i][0]);
        pthread_cond_destroy(&(*cd_podm)[i][1]);
    }

    if (gdata) free(gdata);
    
    return 0;
}


void vlakno(vl_arg_t *d) {
    regex r(d->regex);
    
    while (1) {
        while (ready[d->id] || ende) { //čekací podmínka
            if (ende) {
                ++ready;
                return;
            }
        }

        ready[d->id] += 1;    
        string data(gdata);
        string vystup = regex_replace(data, r, d->replace);
        pthread_mutex_lock(&(*mt_podm)[d->id][1]);

        while (print_counter != d->id) //čekací podmínka
            pthread_cond_wait(&(*cd_podm)[d->id][1], &(*mt_podm)[d->id][1]);
        
        printf("%s\n", vystup.c_str());
        fflush(stdout);
        ++print_counter;

        pthread_mutex_lock(&(*mt_podm)[d->id + 1][1]);
        pthread_cond_signal(&(*cd_podm)[d->id + 1][1]);
        pthread_mutex_unlock(&(*mt_podm)[d->id + 1][1]);
        pthread_mutex_unlock(&(*mt_podm)[d->id][1]);
    }
}


char *to_cstr(std::string a) {
    // prevede retezec v c++ do retezce v "c" (char *)
    char *str = (char *) malloc(sizeof(char)*(a.length()+1));
    strcpy(str,a.c_str());
    return str;
}

char *read_line(void) {
    std::string line;
    if (std::getline(std::cin, line)) {
        return to_cstr(line);
    } else {
        return NULL;
    }
}
