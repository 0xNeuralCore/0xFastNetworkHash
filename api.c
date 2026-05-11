/*
 * ============================================================
 *  api.c
 *  Implémentation — pool mémoire statique, Thread BS, polling
 * ============================================================
 */

#include "api.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/* ── Période du Background Service (ms) ── */
#define BS_PERIOD_MS  500

/* ============================================================
 *  POOL STATIQUE — déclaration globale
 *  Toute la mémoire est réservée à la compilation.
 *  Aucun malloc, aucune allocation dynamique.
 * ============================================================ */

static ApiSlot          _pool[API_MAX_SLOTS];   /* pool de slots            */
static int              _pool_count = 0;        /* slots déclarés           */
static int              _initialized = 0;       /* garde-fou init           */

static pthread_mutex_t  _mutex;
static pthread_t        _tid_bs;
static int              _running = 0;

/* ============================================================
 *  INTERNE — _find_slot
 *  Cherche un slot par id dans le pool.
 *  Retourne le pointeur ou NULL si absent.
 *  Appelée mutex déjà locké ou en phase init (single-thread).
 * ============================================================ */

static ApiSlot *_find_slot(int id)
{
    for (int i = 0; i < _pool_count; i++) {
        if (_pool[i].id == id)
            return &_pool[i];
    }
    return NULL;
}

/* ============================================================
 *  INTERNE — _ThreadBS
 *  Tourne en background, peuple tous les slots déclarés.
 *  C'est ici que l'utilisateur branchera ses vraies sources
 *  (lecture registre, capteur, protocole...).
 * ============================================================ */

static void *_ThreadBS(void *arg)
{
    (void)arg;
    int tick = 0;

    printf("[BS] démarré — %d slot(s) à peupler\n", _pool_count);

    while (_running) {
        usleep(BS_PERIOD_MS * 1000);
        tick++;

        pthread_mutex_lock(&_mutex);

        for (int i = 0; i < _pool_count; i++) {
            ApiSlot *s = &_pool[i];

            /*
             * ── Remplacer ici par la vraie lecture de la source ──
             * Exemple : s->value = HAL_ReadSensor(s->id);
             *           s->value = register_read(s->id);
             */
            s->value    = s->id * 100 + tick;   /* valeur simulée           */
            s->is_fresh = 1;
        }

        pthread_mutex_unlock(&_mutex);
    }

    printf("[BS] arrêté\n");
    return NULL;
}

/* ============================================================
 *  F_api_init
 *  Initialise le pool et démarre le Thread BS.
 *  À appeler une seule fois au démarrage.
 *
 *  Retourne : API_OK ou API_ERR_INIT.
 * ============================================================ */

int F_api_init(void)
{
    if (_initialized) return API_OK;    /* idempotent                       */

    memset(_pool, 0, sizeof(_pool));
    _pool_count = 0;

    if (pthread_mutex_init(&_mutex, NULL) != 0) return API_ERR_INIT;

    _running     = 1;
    _initialized = 1;

    if (pthread_create(&_tid_bs, NULL, _ThreadBS, NULL) != 0) {
        pthread_mutex_destroy(&_mutex);
        _initialized = 0;
        return API_ERR_INIT;
    }

    printf("[API] initialisée\n");
    return API_OK;
}

/* ============================================================
 *  F_api_init_data
 *  Déclare un slot de donnée dans le pool.
 *  L'utilisateur appelle cette fonction pour chaque donnée
 *  qu'il veut suivre, avant de lancer le polling.
 *
 *  id    : identifiant unique choisi par l'utilisateur
 *  label : nom lisible (debug, logs)
 *
 *  Retourne : API_OK, API_ERR_FULL, ou API_ERR_INIT.
 * ============================================================ */

int F_api_init_data(int id, const char *label)
{
    if (!_initialized)           return API_ERR_INIT;
    if (_pool_count >= API_MAX_SLOTS) return API_ERR_FULL;

    /* Pas de doublon */
    if (_find_slot(id) != NULL) return API_OK;

    ApiSlot *s  = &_pool[_pool_count++];
    s->id       = id;
    s->value    = 0;
    s->is_fresh = 0;
    s->used     = 1;
    strncpy(s->label, label ? label : "?", sizeof(s->label) - 1);

    printf("[API] slot déclaré — id=%d  label=%s\n", id, s->label);
    return API_OK;
}

/* ============================================================
 *  F_api_get
 *  Récupère la valeur d'un slot (polling).
 *  Consomme le flag is_fresh : retourne API_NO_FRESH si aucune
 *  nouvelle valeur depuis la dernière lecture.
 *
 *  id  : identifiant du slot
 *  out : pointeur vers l'entier à remplir
 *
 *  Retourne : API_OK, API_NO_FRESH, ou API_ERR_ID.
 * ============================================================ */

int F_api_get(int id, int *out)
{
    if (!_initialized || !out) return API_ERR_INIT;

    pthread_mutex_lock(&_mutex);

    ApiSlot *s = _find_slot(id);
    if (!s) {
        pthread_mutex_unlock(&_mutex);
        return API_ERR_ID;
    }

    int fresh  = s->is_fresh;
    *out       = s->value;
    s->is_fresh = 0;            /* consommé                                 */

    pthread_mutex_unlock(&_mutex);

    return fresh ? API_OK : API_NO_FRESH;
}

/* ============================================================
 *  F_api_finish
 *  Arrête le Thread BS et remet le pool à zéro.
 * ============================================================ */

void F_api_finish(void)
{
    if (!_initialized) return;

    _running = 0;
    pthread_join(_tid_bs, NULL);
    pthread_mutex_destroy(&_mutex);

    memset(_pool, 0, sizeof(_pool));
    _pool_count  = 0;
    _initialized = 0;

    printf("[API] terminée — pool remis à zéro\n");
}
