/*
 * test3_main.c
 *
 * Compilation : gcc test3.c test3_main.c -o demo -lpthread
 */

#include "test3.h"
#include <stdio.h>
#include <unistd.h>

#define DATA_A  1    /* int,   hist=1 */
#define DATA_B  2    /* float, hist=3 */
#define DATA_C  3    /* int,   hist=2 */

int main(void)
{
    F_api_init();

    /*
     * item_size aligné automatiquement dans F_api_init_data :
     *   sizeof(int)   = 4 → ALIGN_UP(4,4) = 4  (inchangé)
     *   sizeof(float) = 4 → ALIGN_UP(4,4) = 4  (inchangé)
     *
     * Layout pool après init :
     *   [ int×1 | float×3  | int×2  | ........... ]
     *     4 oct   12 oct     8 oct    libre
     *     bump = 24 / 256
     */
    F_api_init_data(DATA_A, "sensor_temp",     sizeof(int),   1);
    F_api_init_data(DATA_B, "sensor_pressure", sizeof(float), 3);
    F_api_init_data(DATA_C, "sensor_humidity", sizeof(int),   2);

    printf("\n[APP] polling...\n\n");

    int   a;
    float b;
    int   c;
    float hist_b[3];
    int   hist_c[2];

    for (int t = 0; t < 6; t++) {
        usleep(700000);

        printf("── tick %d ──────────────────────────\n", t + 1);

        if (F_api_get(DATA_A, &a) == API_OK)
            printf("  DATA_A = %d\n", a);
        else
            printf("  DATA_A → no fresh\n");

        if (F_api_get(DATA_B, &b) == API_OK)
            printf("  DATA_B = %.1f\n", b);
        else
            printf("  DATA_B → no fresh\n");

        int n = F_api_get_history(DATA_B, hist_b, 3);
        printf("  DATA_B hist(%d) :", n);
        for (int i = 0; i < n; i++) printf("  %.1f", hist_b[i]);
        printf("\n");

        if (F_api_get(DATA_C, &c) == API_OK)
            printf("  DATA_C = %d\n", c);
        else
            printf("  DATA_C → no fresh\n");

        n = F_api_get_history(DATA_C, hist_c, 2);
        printf("  DATA_C hist(%d) :", n);
        for (int i = 0; i < n; i++) printf("  %d", hist_c[i]);
        printf("\n\n");
    }

    F_api_finish();
    return 0;
}
