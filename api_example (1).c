/*
 * Implémentation en C d'un exemple d'API
 * Architecture : Thread APP + Thread BS (Background Service)
 * Données partagées protégées par mutex
 * 2 patterns : Polling et Event (condition variable)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/* ============================================================
 *  BRIQUE 1 — Structure de données partagées
 *  Protégée par un mutex (section critique)
 * ============================================================ */

typedef struct {
    int     value;          /* dernière valeur calculée par le BS    */
    int     is_fresh;       /* flag : nouvelle donnée disponible ?   */
    char    label[64];      /* label descriptif de la donnée         */
} SharedData;

/* ============================================================
 *  BRIQUE 2 — Contexte global partagé entre les threads
 * ============================================================ */

typedef struct {
    SharedData      data;           /* données partagées                    */
    pthread_mutex_t mutex;          /* mutex pour la section critique       */
    pthread_cond_t  event;          /* condition variable (pattern EVENT)   */
    int             running;        /* flag d'arrêt des threads             */
} AppContext;

/* ============================================================
 *  BRIQUE 3 — Fonctions de l'API (Thread APP côté utilisateur)
 * ============================================================ */

/*
 * Pattern POLLING
 * L'utilisateur appelle cette fonction quand il veut.
 * Elle retourne la dernière donnée disponible.
 */
int API_GetFreshData(AppContext *ctx, SharedData *out)
{
    pthread_mutex_lock(&ctx->mutex);        /* section critique : début     */

    *out = ctx->data;                       /* copie de la donnée           */
    ctx->data.is_fresh = 0;                 /* on consomme le flag          */

    pthread_mutex_unlock(&ctx->mutex);      /* section critique : fin       */

    return out->is_fresh;                   /* 1 = nouvelle donnée, 0 = déjà lue */
}

/*
 * Pattern EVENT
 * L'utilisateur attend passivement qu'une nouvelle donnée arrive.
 * Le Thread BS va déclencher l'événement (signal).
 */
int API_WaitForData(AppContext *ctx, SharedData *out)
{
    pthread_mutex_lock(&ctx->mutex);

    /* Attente bloquante jusqu'à ce que le BS signale */
    while (!ctx->data.is_fresh && ctx->running) {
        pthread_cond_wait(&ctx->event, &ctx->mutex);    /* EVENT : attente  */
    }

    *out = ctx->data;
    ctx->data.is_fresh = 0;

    pthread_mutex_unlock(&ctx->mutex);

    return ctx->running;    /* 0 = contexte arrêté, 1 = donnée reçue */
}

/* ============================================================
 *  BRIQUE 4 — Thread BS (Background Service)
 *  Fait des traitements non accessibles directement par l'APP
 *  Publie le résultat dans les données partagées
 * ============================================================ */

void *ThreadBS(void *arg)
{
    AppContext *ctx = (AppContext *)arg;
    int counter = 0;

    printf("[BS] Thread Background Service démarré\n");

    while (ctx->running) {

        /* --- Traitement interne (invisible pour l'APP) --- */
        sleep(1);                           /* simule un calcul long        */
        counter++;

        /* --- Mise à jour des données partagées --- */
        pthread_mutex_lock(&ctx->mutex);    /* section critique : début     */

        ctx->data.value    = counter * 10;
        ctx->data.is_fresh = 1;
        snprintf(ctx->data.label, sizeof(ctx->data.label),
                 "Donnée #%d calculée par BS", counter);

        printf("[BS] Nouvelle donnée publiée : value=%d\n", ctx->data.value);

        /* Déclenche l'EVENT pour réveiller les waiters */
        pthread_cond_signal(&ctx->event);

        pthread_mutex_unlock(&ctx->mutex);  /* section critique : fin       */
    }

    printf("[BS] Thread Background Service arrêté\n");
    return NULL;
}

/* ============================================================
 *  BRIQUE 5 — Thread APP (côté utilisateur de l'API)
 *  Démo des 2 patterns : polling puis event
 * ============================================================ */

void *ThreadAPP(void *arg)
{
    AppContext *ctx = (AppContext *)arg;
    SharedData result;
    int i;

    printf("[APP] Thread APP démarré\n");

    /* --- Pattern POLLING : on interroge toutes les 500ms --- */
    printf("\n[APP] === Pattern POLLING ===\n");
    for (i = 0; i < 4; i++) {
        usleep(500000);                     /* 500 ms                       */
        int fresh = API_GetFreshData(ctx, &result);
        if (fresh) {
            printf("[APP] Polling → %s (value=%d)\n",
                   result.label, result.value);
        } else {
            printf("[APP] Polling → pas de nouvelle donnée\n");
        }
    }

    /* --- Pattern EVENT : on attend passivement --- */
    printf("\n[APP] === Pattern EVENT ===\n");
    for (i = 0; i < 3; i++) {
        printf("[APP] En attente d'un événement...\n");
        if (API_WaitForData(ctx, &result)) {
            printf("[APP] Événement reçu → %s (value=%d)\n",
                   result.label, result.value);
        }
    }

    /* Arrêt propre */
    pthread_mutex_lock(&ctx->mutex);
    ctx->running = 0;
    pthread_cond_signal(&ctx->event);       /* débloquer le BS s'il attend  */
    pthread_mutex_unlock(&ctx->mutex);

    printf("\n[APP] Thread APP terminé\n");
    return NULL;
}

/* ============================================================
 *  BRIQUE 6 — Initialisation et lancement
 * ============================================================ */

int main(void)
{
    AppContext ctx;
    pthread_t tid_app, tid_bs;

    /* Initialisation du contexte */
    memset(&ctx, 0, sizeof(ctx));
    ctx.running = 1;
    pthread_mutex_init(&ctx.mutex, NULL);
    pthread_cond_init(&ctx.event, NULL);

    printf("=== Démarrage de l'exemple API ===\n\n");

    /* Lancement des threads */
    pthread_create(&tid_bs,  NULL, ThreadBS,  &ctx);
    pthread_create(&tid_app, NULL, ThreadAPP, &ctx);

    /* Attente de la fin */
    pthread_join(tid_app, NULL);
    pthread_join(tid_bs,  NULL);

    /* Nettoyage */
    pthread_mutex_destroy(&ctx.mutex);
    pthread_cond_destroy(&ctx.event);

    printf("\n=== Fin de l'exemple ===\n");
    return 0;
}

/*
 * Compilation :
 *   gcc api_example.c -o api_example -lpthread
 *
 * Exécution :
 *   ./api_example
 */
