#include <stdint.h>
#include <math.h>

#include "debug.h"
#include "goertzel.h"

void goertzel_init(GOERTZEL_STATE *gp, uint32_t N, double k) {
    gp -> N = N;
    gp -> k = k;
    gp -> A = 2 * M_PI * (gp -> k) / N;
    gp -> B = 2 * cos(gp -> A);
    gp -> s0 = 0;
    gp -> s1 = 0;
    gp -> s2 = 0;
}

void goertzel_step(GOERTZEL_STATE *gp, double x) {
    gp -> s0 = x + (gp -> B) * (gp -> s1) - (gp -> s2);
    gp -> s2 = gp -> s1;
    gp -> s1 = gp -> s0;
}

double goertzel_strength(GOERTZEL_STATE *gp, double x) {
    gp -> s0 = x + (gp -> B) * (gp -> s1) - (gp -> s2);
    double G = (gp -> A) * ((gp -> N) - 1);
    double Re = pow((gp -> s0) * cos(G) - (gp -> s1) * cos((gp -> A) + G), 2);
    //printf("Re^2 is %f\n",Re);
    double Im = pow((gp -> s1) * sin((gp -> A) + G) - (gp -> s0) * sin(G), 2);
    //printf("Im^2 is %f\n",Im);
    double y = 2 * (Re + Im) / pow(gp -> N, 2);
    //printf("result of y is %f\n", y);
    return y;
}
