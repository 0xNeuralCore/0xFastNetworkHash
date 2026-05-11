/*
 * ============================================================
 *  main.c
 *  Application exemple — déclare dataA, dataB, dataC
 *  et fait du polling pour récupérer les valeurs.
 * ============================================================
 */

#include "api.h"
#include <stdio.h>
#include <unistd.h>

/* ── IDs des données — définis par l'utilisateur ── */
#define DATA_A  1
#define DATA_B  2
#define DATA_C  3

int main(void)
{
    int val;

    /* ── 1. Init de l'API ── */
    F_api_init();

    /* ── 2. Déclaration des données par l'utilisateur ── */
    F_api_init_data(DATA_A, "sensor_temp");
    F_api_init_data(DATA_B, "sensor_pressure");
    F_api_init_data(DATA_C, "sensor_humidity");

    printf("\n[APP] polling démarré...\n\n");

    /* ── 3. Boucle de polling ── */
    for (int tick = 0; tick < 8; tick++) {
        usleep(600000);     /* 600 ms — légèrement plus rapide que le BS   */

        if (F_api_get(DATA_A, &val) == API_OK)
            printf("[APP] DATA_A (temp)     = %d\n", val);
        else
            printf("[APP] DATA_A → pas de nouvelle valeur\n");

        if (F_api_get(DATA_B, &val) == API_OK)
            printf("[APP] DATA_B (pressure) = %d\n", val);
        else
            printf("[APP] DATA_B → pas de nouvelle valeur\n");

        if (F_api_get(DATA_C, &val) == API_OK)
            printf("[APP] DATA_C (humidity) = %d\n", val);
        else
            printf("[APP] DATA_C → pas de nouvelle valeur\n");

        printf("\n");
    }

    /* ── 4. Fin propre ── */
    F_api_finish();
    return 0;
}

/*
 * Compilation :
 *   gcc api.c main.c -o demo -lpthread
 *
 * Sortie attendue :
 *   [API] initialisée
 *   [API] slot déclaré — id=1  label=sensor_temp
 *   [API] slot déclaré — id=2  label=sensor_pressure
 *   [API] slot déclaré — id=3  label=sensor_humidity
 *   [BS]  démarré — 3 slot(s) à peupler
 *   [APP] polling démarré...
 *   [APP] DATA_A (temp)     = 101
 *   [APP] DATA_B (pressure) = 201
 *   [APP] DATA_C (humidity) = 301
 *   ...
 */
