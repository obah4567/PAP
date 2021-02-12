#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <sys/time.h>

//#include <collapse.c>

// #include <omp.h>

#define MAX_NBVILLES 22

typedef int DTab_t[MAX_NBVILLES][MAX_NBVILLES];
typedef int chemin_t[MAX_NBVILLES];

/* macro de mesure de temps, retourne une valeur en �secondes */
#define TIME_DIFF(t1, t2) \
  ((t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec))

/* dernier minimum trouv� */
int minimum = INT_MAX;

/* tableau des distances */
DTab_t distance;

/* nombre de villes */
int nbVilles;

/* profondeur du parallélisme */
int grain;

#define MAXX 100
#define MAXY 100
typedef struct
{
  int x, y;
} coor_t;

typedef coor_t coortab_t[MAX_NBVILLES];

void initialisation(int Argc, char *Argv[])
{

  if (Argc < 4 || Argc > 5)
  {
    fprintf(stderr, "Usage: %s  <nbVilles> <seed> [grain] <kernel>\n", Argv[0]);
    exit(1);
  }

 grain = (Argc == 5) ? atoi(Argv[3]) : 0;


  /* initialisation du tableau des distances */
  /* on positionne les villes aléatoirement sur une carte MAXX x MAXY  */

  coortab_t lesVilles;

  int i, j;
  int dx, dy;

  nbVilles = atoi(Argv[1]);
  if (nbVilles > MAX_NBVILLES)
  {
    fprintf(stderr, "trop de villes, augmentez MAX_NBVILLES\n");
    exit(1);
  }

  srand(atoi(Argv[2]));

  for (i = 0; i < nbVilles; i++)
  {
    lesVilles[i].x = rand() % MAXX;
    lesVilles[i].y = rand() % MAXY;
  }

  for (i = 0; i < nbVilles; i++)
    for (j = 0; j < nbVilles; j++)
    {
      dx = lesVilles[i].x - lesVilles[j].x;
      dy = lesVilles[i].y - lesVilles[j].y;
      distance[i][j] = (int)sqrt((double)((dx * dx) + (dy * dy)));
    }
}

/* résolution du problème du voyageur de commerce */

inline int present(int ville, int mask)
{
  return mask & (1 << ville);
}

void verifier_minimum(int lg, chemin_t chemin)
{
  if (lg + distance[0][chemin[nbVilles - 1]] < minimum)
  {
    minimum = lg + distance[0][chemin[nbVilles - 1]];
    printf("%3d :", minimum);
    for (int i = 0; i < nbVilles; i++)
      printf("%2d ", chemin[i]);
    printf("\n");
  }
}

void tsp_seq(int etape, int lg, chemin_t chemin, int mask)
{
  int ici, dist;

  if (etape == nbVilles)
    verifier_minimum(lg, chemin);
  else
  {
    ici = chemin[etape - 1];

    for (int i = 1; i < nbVilles; i++)
    {
      if (!present(i, mask))
      {
        chemin[etape] = i;
        dist = distance[ici][i];
        tsp_seq(etape + 1, lg + dist, chemin, mask | (1 << i));
      }
    }
  }
}

void tsp_ompfor(int etape, int lg, chemin_t chemin, int mask)
{
  //question : 2.1 Optimisation du surcout du parallelisme
  
  //#pragma omp parallel for if (etape <= grain)
  //Ici, en cas de grain trop petit on reste en sequentiel
  if (etape > grain){   
    tsp_seq(etape, lg, chemin, mask);
  }else {
    int ici, dist;

  if (etape == nbVilles)
    verifier_minimum(lg, chemin);
  else
      {
      ici = chemin[etape - 1];

      //pour effectuer des partages en mode dynamique ou static
      #pragma parallel omp for schedule(dynamic, 1) 

        for (int i = 1; i < nbVilles; i++)
        {
          if (!present(i, mask))
          {
            //Ici on ajoute un nouveau tableau de mon_chemin, 
            //pour eviter que le thread actuel ecrase le precedent
            chemin_t mon_chemin;
            memcpy(mon_chemin, chemin, etape * sizeof(int));
            mon_chemin[etape] = i;
            dist = distance[ici][i];
            tsp_ompfor(etape + 1, lg + dist, mon_chemin, mask | (1 << i));
          }
        }
      }
    } 
}


void tsp_task(int etape, int lg, chemin_t chemin, int mask)
{
  //question : 2.1 Optimisation du surcout du parallelisme
  
  //#pragma omp parallel for if (etape <= grain)
  //Ici, en cas de grain trop petit on reste en sequentiel
  if (etape > grain){  
    tsp_seq(etape, lg, chemin, mask);
  }else {
    int ici, dist;

  if (etape == nbVilles)
    verifier_minimum(lg, chemin);
  else
      {

      chemin_t mon_chemin;
      memcpy(mon_chemin, chemin, etape * sizeof(int));

      ici = chemin[etape - 1];

      //#pragma parallel omp for collapse(4) schedule(static, 1) 
      //pour effectuer des partages en mode dynamique ou static
      #pragma parallel omp for schedule(static, 1) 
        for (int i = 1; i < nbVilles; i++)
        {
          if (!present(i, mask))
          {
            //Ici on ajoute un nouveau tableau de mon_chemin, 
            //pour eviter que le thread actuel ecrase le precedent

            // chemin_t mon_chemin;
            // memcpy(mon_chemin, chemin, etape * sizeof(int));
            
            //A revoir
            //#pragma omp task firstprivate (mon_chemin, i, ici, lg)

            mon_chemin[etape] = i;
            dist = distance[ici][i];
            tsp_ompfor(etape + 1, lg + dist, mon_chemin, mask | (1 << i));
          }
        }
      }
    } 
}

int main(int argc, char **argv)
{
  unsigned long temps;
  struct timeval t1, t2;
  chemin_t chemin;

  initialisation(argc, argv);

  printf("nbVilles = %3d - grain %d \n", nbVilles, grain);

  //omp_set_max_active_levels(grain);

  gettimeofday(&t1, NULL);

  chemin[0] = 0;

  if (!strcmp(argv[argc-1],"seq")) 
    tsp_seq(1, 0, chemin, 1);
  else if (!strcmp(argv[argc-1],"ompfor")) 
    tsp_ompfor(1, 0, chemin, 1);

  else if (!strcmp(argv[argc-1],"task")) 
    tsp_task(1, 0, chemin, 1);

  else
  {
      printf("kernel inconnu\n");
      exit(1);
  }
      
  gettimeofday(&t2, NULL);

  temps = TIME_DIFF(t1, t2);
  fprintf(stderr, "Le temps : %ld.%03ld\n", temps / 1000, temps % 1000);

  return 0;
}
