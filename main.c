#include "gb.h"

int main() {
     struct gb gb;

     gb_cpu_reset(&gb);

     while (1) {
          gb_cpu_run_instruction(&gb);
     }

     return 0;
}
