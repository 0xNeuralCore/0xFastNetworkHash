/*
 * test1.h
 *
 * Deux structures indépendantes :
 *
 *   MemPool     stocke les octets bruts
 *               sait : taille, historique, où écrire
 *               ne sait pas : id, nom, mutex
 *
 *   DataHandler gère la sémantique de chaque donnée
 *               sait : id, nom, mutex, pointeur dans le pool
 *               ne sait pas : où sont les octets en mémoire
 */

#ifndef TEST1_H
#define TEST1_H

#include <stddef.h>
#include <pthread.h>

/* ── Limites ─────────────────────────────────────────────────── */
#define API_POOL_SIZE     256
#define API_MAX_ENTRIES     8
#define API_MAX_HISTORY     8

/* ── Codes de retour ─────────────────────────────────────────── */
#define API_OK            0
#define API_NO_FRESH     -1
#define API_ERR_FULL     -2
#define API_ERR_ID       -3
#define API_ERR_SIZE     -4
#define API_ERR_INIT     -5

/* ================================================================
 *  MEMPOOL
 *  Responsabilité unique : allouer et donner accès aux blocs.
 *  Un bloc = item_size × history_count octets contigus.
 *  Après l'init, le pool ne fait que lire/écrire via les pointeurs.
 * ================================================================ */

typedef struct {
    unsigned char buffer[API_POOL_SIZE];  /* mémoire statique              */
    size_t        bump_index;             /* curseur, avance, jamais recule */
} MemPool;

void *MemPool_alloc        (MemPool *mp, size_t item_size, int history_count);
void  MemPool_write        (void *block, size_t item_size, int history_count,
                            int *ring_head, const void *src);
void  MemPool_read_latest  (const void *block, size_t item_size,
                            int history_count, int ring_head, void *dst);
int   MemPool_read_history (const void *block, size_t item_size,
                            int history_count, int ring_head,
                            int ring_fill, void *dst, int n);

/* ================================================================
 *  DATAHANDLER
 *  Responsabilité unique : référencer et synchroniser les données.
 *  Chaque entrée est un descripteur pointant dans le MemPool.
 *  Un seul mutex global pour toutes les entrées.
 * ================================================================ */

typedef struct {
    int            id;
    char           name[32];

    void          *block;         /* pointeur dans MemPool.buffer           */
    size_t         item_size;
    int            history_count;
    int            ring_head;     /* géré ici, passé à MemPool pour écrire  */
    int            ring_fill;

    int            is_fresh;
    int            used;
} DataEntry;

typedef struct {
    DataEntry       entries[API_MAX_ENTRIES];
    int             count;
    pthread_mutex_t mutex;        /* UN mutex global pour toutes les entries */
} DataHandler;

DataEntry *DataHandler_find     (DataHandler *dh, int id);
int        DataHandler_register (DataHandler *dh, int id, const char *name,
                                 void *block, size_t item_size,
                                 int history_count);

/* ================================================================
 *  API PUBLIQUE
 * ================================================================ */

int  F_api_init      (void);
int  F_api_init_data (int id, const char *name,
                      size_t item_size, int history_count);
int  F_api_get       (int id, void *out);
int  F_api_get_history (int id, void *out, int n);
void F_api_finish    (void);

#endif /* TEST1_H */
