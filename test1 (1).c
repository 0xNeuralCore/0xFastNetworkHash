/*
 * test1.c
 */

#include "test1.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define BS_PERIOD_MS  500

/* ── Deux instances statiques globales, clairement séparées ───── */
static MemPool     _mempool;
static DataHandler _handler;

static int        _initialized = 0;
static int        _running     = 0;
static pthread_t  _tid_bs;

/* ================================================================
 *  MEMPOOL — implémentation
 *  Ne connaît que des void*, des tailles et des index.
 *  Aucune notion d'id, de nom ou de mutex ici.
 * ================================================================ */

void *MemPool_alloc(MemPool *mp, size_t item_size, int history_count)
{
    size_t block_size = item_size * (size_t)history_count;
    if (mp->bump_index + block_size > API_POOL_SIZE)
        return NULL;

    void *ptr = &mp->buffer[mp->bump_index];
    mp->bump_index += block_size;
    return ptr;
}

void MemPool_write(void *block, size_t item_size, int history_count,
                   int *ring_head, const void *src)
{
    size_t offset = (size_t)(*ring_head) * item_size;
    memcpy((unsigned char *)block + offset, src, item_size);
    *ring_head = (*ring_head + 1) % history_count;
}

void MemPool_read_latest(const void *block, size_t item_size,
                         int history_count, int ring_head, void *dst)
{
    int    latest = (ring_head - 1 + history_count) % history_count;
    size_t offset = (size_t)latest * item_size;
    memcpy(dst, (const unsigned char *)block + offset, item_size);
}

int MemPool_read_history(const void *block, size_t item_size,
                         int history_count, int ring_head,
                         int ring_fill, void *dst, int n)
{
    int count = n < ring_fill ? n : ring_fill;
    for (int i = 0; i < count; i++) {
        int    idx     = (ring_head - 1 - i + history_count) % history_count;
        size_t src_off = (size_t)idx * item_size;
        size_t dst_off = (size_t)i   * item_size;
        memcpy((unsigned char *)dst + dst_off,
               (const unsigned char *)block + src_off,
               item_size);
    }
    return count;
}

/* ================================================================
 *  DATAHANDLER — implémentation
 *  Ne touche jamais directement aux octets.
 *  Délègue toute lecture/écriture mémoire aux fonctions MemPool_*.
 * ================================================================ */

DataEntry *DataHandler_find(DataHandler *dh, int id)
{
    for (int i = 0; i < dh->count; i++)
        if (dh->entries[i].id == id)
            return &dh->entries[i];
    return NULL;
}

int DataHandler_register(DataHandler *dh, int id, const char *name,
                         void *block, size_t item_size, int history_count)
{
    if (dh->count >= API_MAX_ENTRIES) return API_ERR_FULL;

    DataEntry *e     = &dh->entries[dh->count++];
    e->id            = id;
    e->block         = block;
    e->item_size     = item_size;
    e->history_count = history_count;
    e->ring_head     = 0;
    e->ring_fill     = 0;
    e->is_fresh      = 0;
    e->used          = 1;
    strncpy(e->name, name ? name : "?", sizeof(e->name) - 1);
    return API_OK;
}

/* ================================================================
 *  THREAD BS
 *  Écrit dans le DataHandler via MemPool_write.
 *  Calcul fait hors mutex — seule la copie finale est protégée.
 * ================================================================ */

static void *_ThreadBS(void *arg)
{
    (void)arg;
    int tick = 0;

    printf("[BS] démarré — %d entrée(s)\n", _handler.count);

    while (_running) {
        usleep(BS_PERIOD_MS * 1000);
        tick++;

        pthread_mutex_lock(&_handler.mutex);

        for (int i = 0; i < _handler.count; i++) {
            DataEntry *e = &_handler.entries[i];

            /*
             * Valeur simulée — remplacer par la vraie source :
             *   int val = HAL_Read(e->id);
             */
            int val = e->id * 100 + tick;

            MemPool_write(e->block, e->item_size, e->history_count,
                          &e->ring_head, &val);

            if (e->ring_fill < e->history_count)
                e->ring_fill++;

            e->is_fresh = 1;
        }

        pthread_mutex_unlock(&_handler.mutex);
    }

    printf("[BS] arrêté\n");
    return NULL;
}

/* ================================================================
 *  API PUBLIQUE
 * ================================================================ */

int F_api_init(void)
{
    if (_initialized) return API_OK;

    memset(&_mempool, 0, sizeof(_mempool));
    memset(&_handler, 0, sizeof(_handler));

    if (pthread_mutex_init(&_handler.mutex, NULL) != 0)
        return API_ERR_INIT;

    _running     = 1;
    _initialized = 1;

    if (pthread_create(&_tid_bs, NULL, _ThreadBS, NULL) != 0) {
        pthread_mutex_destroy(&_handler.mutex);
        _initialized = 0;
        return API_ERR_INIT;
    }

    printf("[API] init — pool %d octets / %d slots max\n",
           API_POOL_SIZE, API_MAX_ENTRIES);
    return API_OK;
}

/*
 * F_api_init_data
 * Seul endroit où MemPool et DataHandler se parlent :
 *   1. MemPool_alloc  → réserve le bloc brut
 *   2. DataHandler_register → enregistre le descripteur + pointeur
 */
int F_api_init_data(int id, const char *name,
                    size_t item_size, int history_count)
{
    if (!_initialized)                       return API_ERR_INIT;
    if (history_count < 1
        || history_count > API_MAX_HISTORY)  return API_ERR_SIZE;
    if (DataHandler_find(&_handler, id))     return API_OK;

    void *block = MemPool_alloc(&_mempool, item_size, history_count);
    if (!block) return API_ERR_FULL;

    int rc = DataHandler_register(&_handler, id, name,
                                  block, item_size, history_count);
    if (rc != API_OK) return rc;

    printf("[API] déclaré  id=%-2d  name=%-20s  "
           "size=%zu  hist=%d  pool=%zu/%d\n",
           id, name, item_size, history_count,
           _mempool.bump_index, API_POOL_SIZE);
    return API_OK;
}

int F_api_get(int id, void *out)
{
    if (!_initialized || !out) return API_ERR_INIT;

    pthread_mutex_lock(&_handler.mutex);

    DataEntry *e = DataHandler_find(&_handler, id);
    if (!e) { pthread_mutex_unlock(&_handler.mutex); return API_ERR_ID; }

    int fresh   = e->is_fresh;
    e->is_fresh = 0;

    if (fresh)
        MemPool_read_latest(e->block, e->item_size,
                            e->history_count, e->ring_head, out);

    pthread_mutex_unlock(&_handler.mutex);
    return fresh ? API_OK : API_NO_FRESH;
}

int F_api_get_history(int id, void *out, int n)
{
    if (!_initialized || !out || n <= 0) return API_ERR_INIT;

    pthread_mutex_lock(&_handler.mutex);

    DataEntry *e = DataHandler_find(&_handler, id);
    if (!e) { pthread_mutex_unlock(&_handler.mutex); return API_ERR_ID; }

    int count = MemPool_read_history(e->block, e->item_size,
                                     e->history_count, e->ring_head,
                                     e->ring_fill, out, n);

    pthread_mutex_unlock(&_handler.mutex);
    return count;
}

void F_api_finish(void)
{
    if (!_initialized) return;

    _running = 0;
    pthread_join(_tid_bs, NULL);
    pthread_mutex_destroy(&_handler.mutex);

    memset(&_mempool, 0, sizeof(_mempool));
    memset(&_handler, 0, sizeof(_handler));
    _initialized = 0;

    printf("[API] terminée\n");
}
