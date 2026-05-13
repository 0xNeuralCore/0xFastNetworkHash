/*
 * test3.h
 *
 * Deux structures indépendantes :
 *   MemPool     — stocke les octets bruts, alignés
 *   DataHandler — gère la sémantique, un mutex global
 *
 * Alignement : item_size est aligné une seule fois dans
 * F_api_init_data. Aucun champ supplémentaire dans DataEntry.
 */

#ifndef TEST3_H
#define TEST3_H

#include <stddef.h>
#include <pthread.h>

/* ── Alignement (puissance de 2) ─────────────────────────────── */
#define API_ALIGN         4
#define ALIGN_UP(x, a)    (((x) + (a) - 1u) & ~((a) - 1u))

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
 * ================================================================ */

typedef struct {
    unsigned char buffer[API_POOL_SIZE] __attribute__((aligned(API_ALIGN)));
    size_t        bump_index;
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
 * ================================================================ */

typedef struct {
    int             id;
    char            name[32];
    void           *block;
    size_t          item_size;      /* toujours aligné sur API_ALIGN        */
    int             history_count;
    int             ring_head;
    int             ring_fill;
    int             is_fresh;
    int             used;
} DataEntry;

typedef struct {
    DataEntry       entries[API_MAX_ENTRIES];
    int             count;
    pthread_mutex_t mutex;
} DataHandler;

DataEntry *DataHandler_find     (DataHandler *dh, int id);
int        DataHandler_register (DataHandler *dh, int id, const char *name,
                                 void *block, size_t item_size,
                                 int history_count);

/* ── API publique ────────────────────────────────────────────── */
int  F_api_init        (void);
int  F_api_init_data   (int id, const char *name,
                        size_t item_size, int history_count);
int  F_api_get         (int id, void *out);
int  F_api_get_history (int id, void *out, int n);
void F_api_finish      (void);

#endif /* TEST3_H */
