#include <stdio.h>
#include "sfmm.h"

/*
 * Define WEAK_MAGIC during compilation to use MAGIC of 0x0 for debugging purposes.
 * Note that this feature will be disabled during grading.
 */
#ifdef WEAK_MAGIC
int sf_weak_magic = 1;
#endif

int main(int argc, char const *argv[]) {
   	char* a = sf_malloc(12);
    char* b = sf_malloc(102);
    char* c = sf_malloc(500);
    char* d = sf_malloc(2131);
    sf_free(b);
    char* e = sf_malloc(3434);
    sf_realloc(c, 10);
    char* f = sf_malloc(600);
    sf_free(e);
    char* g = sf_malloc(4096);
    a = sf_realloc(a, 2222);
    char* h = sf_malloc(23);
    char* i = sf_malloc(453);
    sf_free(d);
    sf_realloc(f, 353);
    sf_free(g);
    sf_free(h);
    sf_free(i);
    sf_malloc(100);
    sf_malloc(57728-8);
    sf_realloc(a, 1000);
    sf_malloc(12999);
    sf_show_heap();

	return EXIT_SUCCESS;
}
