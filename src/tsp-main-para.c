#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <complex.h>
#include <stdbool.h>
#include <unistd.h>

#include "tsp-types.h"
#include "tsp-job.h"
#include "tsp-genmap.h"
#include "tsp-print.h"
#include "tsp-tsp.h"
#include "tsp-lp.h"
#include "tsp-hkbound.h"

#include <pthread.h>

/* macro de mesure de temps, retourne une valeur en nanosecondes */
#define TIME_DIFF(t1, t2) \
  ((t2.tv_sec - t1.tv_sec) * 1000000000ll + (long long int) (t2.tv_nsec - t1.tv_nsec))


/* tableau des distances */
tsp_distance_matrix_t tsp_distance ={};

/** Param�tres **/

/* nombre de villes */
int nb_towns=10;
/* graine */
long int myseed= 0;
/* nombre de threads */
int nb_threads=1;

/* affichage SVG */
bool affiche_sol= false;
bool affiche_progress=false;
bool quiet=false;

/* variable locale du thread */
__thread int minimum_local = INT_MAX;

/* pour passer des arguments multiples en paramètre de la
 start routine de pthread_create */
struct arg_struct {
    struct tsp_queue q; 
    int sol_len; 
    long long int cuts;
    tsp_path_t sol;
};

static void generate_tsp_jobs (struct tsp_queue *q, int hops, int len, uint64_t vpres, tsp_path_t path, long long int *cuts, tsp_path_t sol, int *sol_len, int depth)
{
    if (len >= minimum) {
        (*cuts)++ ;
        return;
    }
    
    if (hops == depth) {
        /* On enregistre du travail � faire plus tard... */
      add_job (q, path, hops, len, vpres);
    } else {
        int me = path [hops - 1];        
        for (int i = 0; i < nb_towns; i++) {
	  if (!present (i, hops, path, vpres)) {
                path[hops] = i;
		vpres |= (1<<i);
                int dist = tsp_distance[me][i];
                generate_tsp_jobs (q, hops + 1, len + dist, vpres, path, cuts, sol, sol_len, depth);
		vpres &= (~(1<<i));
            }
        }
    }
}

static void usage(const char *name) {
  fprintf (stderr, "Usage: %s [-s] <ncities> <seed> <nthreads>\n", name);
  exit (-1);
}

void * fonction_thread(struct arg_struct * arg) {
    pthread_mutex_t mutex_maj_min;   
    pthread_mutex_init(&mutex_maj_min, NULL);
    
    // calculer chacun des travaux 
    tsp_path_t solution;
    uint64_t vpres=0;
    memset (solution, -1, MAX_TOWNS * sizeof (int));
    solution[0] = 0;
    while (!empty_queue (&(arg->q))) {
        int hops = 0, len = 0;
        get_job (&(arg->q), solution, &hops, &len, &vpres);
         
	// le noeud est moins bon que la solution courante
	if (minimum_local < INT_MAX
	    && (nb_towns - hops) > 10
	    && ( (lower_bound_using_hk(solution, hops, len, vpres)) >= minimum_local
		 || (lower_bound_using_lp(solution, hops, len, vpres)) >= minimum_local)
	    )

	  continue;
	tsp (hops, len, vpres, solution, &(arg->cuts), arg->sol, &(arg->sol_len), &mutex_maj_min);
        
        pthread_mutex_lock(&mutex_maj_min);
        // mise à jour du minimum 
        if (arg->sol_len > minimum_local)
            arg->sol_len = minimum_local;
        else
            minimum_local = arg->sol_len;
        pthread_mutex_unlock(&mutex_maj_min);
        
    }
    
    return 0;
}

int main (int argc, char **argv)
{
    unsigned long long perf;
    tsp_path_t path;
    uint64_t vpres=0;
    tsp_path_t sol;
    int sol_len;
    long long int cuts = 0;
    struct tsp_queue q;
    struct timespec t1, t2;
    void * status;

    /* lire les arguments */
    int opt;
    while ((opt = getopt(argc, argv, "spq")) != -1) {
      switch (opt) {
      case 's':
	affiche_sol = true;
	break;
      case 'p':
	affiche_progress = true;
	break;
      case 'q':
	quiet = true;
	break;
      default:
	usage(argv[0]);
	break;
      }
    }

    if (optind != argc-3)
      usage(argv[0]);

    nb_towns = atoi(argv[optind]);
    myseed = atol(argv[optind+1]);
    nb_threads = atoi(argv[optind+2]);
    assert(nb_towns > 0);
    assert(nb_threads > 0);
    pthread_t threads_pid[nb_threads];
   
    minimum = INT_MAX;
      
    /* generer la carte et la matrice de distance */
    if (! quiet)
      fprintf (stderr, "ncities = %3d\n", nb_towns);
    genmap ();

    init_queue (&q);

    clock_gettime (CLOCK_REALTIME, &t1);

    memset (path, -1, MAX_TOWNS * sizeof (int));
    path[0] = 0;
    vpres=1;

    /* mettre les travaux dans la file d'attente */
    generate_tsp_jobs (&q, 1, 0, vpres, path, &cuts, sol, & sol_len, 3);
    no_more_jobs (&q);
   
    /* on crée les threads */
    struct arg_struct *arg = malloc(sizeof(struct arg_struct));
    arg->q = q;
    arg->cuts = cuts;
    arg->sol_len = minimum;
    int return_value;
    for (int i = 0; i < nb_threads; i++) {
        return_value = pthread_create(&threads_pid[i], NULL, (void *)fonction_thread, arg);
        if (return_value != 0) {
            printf("Echec créaton des threads");
        }
    }
    
    /* on attend les threads */
    int return_value_att;
    for (int i = 0;  i < nb_threads; i++) {
        return_value_att = pthread_join(threads_pid[i], &status);   
        //printf("Thread %lx completed ok .\n", threads_pid[i]);
        if (return_value_att != 0) {
            printf("Echec attente des threads");
        }
    }
    
    clock_gettime (CLOCK_REALTIME, &t2);
    

    if (affiche_sol)
      print_solution_svg (arg->sol, arg->sol_len);

    perf = TIME_DIFF (t1,t2);
    printf("<!-- # = %d seed = %ld len = %d threads = %d time = %lld.%03lld ms ( %lld coupures ) -->\n",
	   nb_towns, myseed, arg->sol_len, nb_threads,
	   perf/1000000ll, perf%1000000ll, arg->cuts);
    
    /*on libère la structure crée pour les arguments */
    free(arg);
    return 0 ;
}
