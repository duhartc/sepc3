#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "tsp-types.h"
#include "tsp-genmap.h"
#include "tsp-print.h"
#include "tsp-tsp.h"
#include "tsp-lp.h"

/* dernier minimum trouv� */
int minimum;

/* r�solution du probl�me du voyageur de commerce */
int present (int city, int hops, tsp_path_t path, uint64_t vpres)
{
  (void) hops;
  (void) path;
  return (vpres & (1<<city)) != 0;
}



void tsp (int hops, int len, uint64_t vpres, tsp_path_t path, long long int *cuts, tsp_path_t sol, int *sol_len)
{
    if (len + cutprefix[(nb_towns-hops)] >= minimum) {
      (*cuts)++ ;
      return;
    }
  
    /* un rayon de coupure � 15, pour ne pas lancer la programmation
       lin�aire pour les petits arbres, plus rapide � calculer sans */
    if ((nb_towns - hops) > 15
	&& lower_bound_using_lp(path, hops, len, vpres) >= minimum) {
      (*cuts)++;
      return;
    }

  
    if (hops == nb_towns) {
	    int me = path [hops - 1];
	    int dist = distance[me][0]; // retourner en 0
            if ( len + dist < minimum ) {
		    minimum = len + dist;
		    *sol_len = len + dist;
		    memcpy(sol, path, nb_towns*sizeof(int));
		    print_solution (path, len+dist);
	    }
    } else {
        int me = path [hops - 1];        
        for (int i = 0; i < nb_towns; i++) {
	  if (!present (i, hops, path, vpres)) {
                path[hops] = i;
		vpres |= (1<<i);
                int dist = distance[me][i];
                tsp (hops + 1, len + dist, vpres, path, cuts, sol, sol_len);
		vpres &= (~(1<<i));
            }
        }
    }
}

