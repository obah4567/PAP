#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <omp.h>

int f(int i){
  sleep(1);
  return 2*i;
}

int g(int i){
  sleep(1);
  return 2 * i + 1;
}

int main()
{
  int x;
  int y;
  #pragma omp parallel shared(x)
  {   
      #pragma omp master //single
      {
        { 
          #pragma omp task shared(x)

          x = f(2);
          y = g(3);
        }
      }
  }
  printf("résultat %d\n", x+y);

  return 0;
}
