# Architecture API C — Thread APP / Thread BS

> Implémentation d'une API multi-thread en C avec données partagées,
> mutex, ring buffer, polling et événements.

---

## 1. Vue d'ensemble

```
┌──────────────────────────────────────────────────────────────┐
│                         PROCESSUS                            │
│                                                              │
│   Utilisateur                                                │
│       │                                                      │
│       │  API_Init() / API_Finish()                           │
│       │  API_GetFreshData()                                  │
│       │  API_GetLastNData()                                  │
│       │  API_WaitForData()                                   │
│       ▼                                                      │
│   ┌──────────────┐                                           │
│   │  Thread APP  │  ← façade publique                       │
│   └──────┬───────┘                                           │
│          │  lecture protégée par mutex                       │
│          ▼                                                    │
│   ┌──────────────────────────────────────┐                  │
│   │            AppContext                │                  │
│   │                                      │                  │
│   │  data_fresh     → dernière valeur    │  Data 1          │
│   │  data_history[] → ring buffer N      │  Data 2          │
│   │  data_event     → slot événement     │  Data 3          │
│   │                                      │                  │
│   │  mutex + cond_event                  │                  │
│   └──────────────┬───────────────────────┘                  │
│          ▲       │  écriture + signal                        │
│          │       ▼                                            │
│   ┌──────────────┐                                           │
│   │  Thread BS   │  ← traitements internes                  │
│   │ (background) │     non visibles par l'utilisateur       │
│   └──────────────┘                                           │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Diagramme de classes

```
┌──────────────────────────────┐
│          SharedData          │
├──────────────────────────────┤
│ + value     : int            │
│ + is_fresh  : int            │
│ + label     : char[64]       │
│ + timestamp : time_t         │
└──────────────────────────────┘
              ▲
              │ contient (3 instances différentes)
              │
┌──────────────────────────────────────────┐
│               AppContext                 │
├──────────────────────────────────────────┤
│ - data_fresh        : SharedData         │  ← Data 1
│ - data_history[N]   : SharedData         │  ← Data 2
│ - history_head      : int                │
│ - history_count     : int                │
│ - data_event        : SharedData         │  ← Data 3
│ - mutex             : pthread_mutex_t    │
│ - cond_event        : pthread_cond_t     │
│ - running           : int                │
│ - tid_bs            : pthread_t          │
├──────────────────────────────────────────┤
│ + API_Init()            → AppContext*    │
│ + API_Finish()          → void           │
│ + API_GetFreshData()    → int            │
│ + API_GetLastNData()    → int            │
│ + API_WaitForData()     → int            │
│ - _ThreadBS()           → void*          │  ← interne
│ - _RingBuffer_Push()    → void           │  ← interne
└──────────────────────────────────────────┘
        ▲                        ▲
        │ appelle                │ écrit dans
┌───────────────┐       ┌────────────────┐
│  Thread APP   │       │   Thread BS    │
│  (côté user)  │       │ (background)   │
└───────────────┘       └────────────────┘
```

---

## 3. Cycle de vie : Init / Finish

```
main()
  │
  ├─► API_Init()
  │     ├── malloc(AppContext)
  │     ├── memset → tout à zéro
  │     ├── pthread_mutex_init()
  │     ├── pthread_cond_init()
  │     ├── running = 1
  │     └── pthread_create(ThreadBS)  ──► [BS démarre]
  │
  ├─► API_GetFreshData()   ┐
  ├─► API_GetLastNData()   ├── utilisation par l'utilisateur
  ├─► API_WaitForData()    ┘
  │
  └─► API_Finish()
        ├── mutex_lock()
        ├── running = 0
        ├── cond_broadcast()           ──► [tous les waiters réveillés]
        ├── mutex_unlock()
        ├── pthread_join(ThreadBS)     ──► [BS terminé]
        ├── pthread_mutex_destroy()
        ├── pthread_cond_destroy()
        └── free(ctx)
```

---

## 4. Pattern 1 — Polling : donnée fraîche (Data 1)

```
Thread APP                             Thread BS
    │                                      │
    │                              [calcul interne]
    │                              mutex_lock()
    │                              data_fresh.value = X
    │                              data_fresh.is_fresh = 1
    │                              mutex_unlock()
    │                                      │
    ├─► API_GetFreshData()                 │
    │     mutex_lock()                     │
    │     copie data_fresh → out           │
    │     is_fresh = 0  (consommé)         │
    │     mutex_unlock()                   │
    │◄── retourne API_OK ─────────────────-┤
    │                                      │
    ├─► API_GetFreshData()  (trop tôt)     │
    │     is_fresh == 0                    │
    │◄── retourne API_NO_FRESH ────────────┤
```

---

## 5. Pattern 2 — Polling : N dernières valeurs / Ring Buffer (Data 2)

```
Ring buffer circulaire — taille HISTORY_SIZE = 3

Après 5 écritures du BS :

  Écriture 1 → [ v1 |    |    ]  head=1  count=1
  Écriture 2 → [ v1 | v2 |    ]  head=2  count=2
  Écriture 3 → [ v1 | v2 | v3 ]  head=0  count=3
  Écriture 4 → [ v4 | v2 | v3 ]  head=1  count=3  ← v1 écrasé
  Écriture 5 → [ v4 | v5 | v3 ]  head=2  count=3  ← v2 écrasé

Lecture API_GetLastNData(n=3) retourne : [ v5, v4, v3 ]
                                           ↑ plus récent  ↑ plus ancien

Thread APP                             Thread BS
    │                                      │
    │                              [push v5 dans ring buffer]
    │                                      │
    ├─► API_GetLastNData(n=3)              │
    │     mutex_lock()                     │
    │     lecture circulaire               │
    │     out[0]=v5, [1]=v4, [2]=v3        │
    │     mutex_unlock()                   │
    │◄── retourne 3 ───────────────────────┤
```

---

## 6. Pattern 3 — Event : notification push (Data 3)

```
Thread APP                             Thread BS
    │                                      │
    ├─► API_WaitForData()                  │
    │     mutex_lock()                     │
    │     cond_timedwait()  ← bloqué       │
    │          │                           │
    │          │                   [calcul → counter % 3 == 0]
    │          │                   mutex_lock()
    │          │                   data_event = résultat
    │          │                   cond_signal() ────────────┐
    │          │                   mutex_unlock()             │
    │          │                                             │
    │     ◄────┘ réveillé                                    │
    │     copie data_event                                   │
    │     is_fresh = 0                                       │
    │     mutex_unlock()                                     │
    │◄── retourne API_OK ────────────────────────────────────┘
    │
    │   [si pas de signal avant deadline]
    │◄── retourne API_ERR_TIMEOUT
```

---

## 7. Codes de retour

| Code             | Valeur | Signification                              |
|------------------|--------|--------------------------------------------|
| `API_OK`         |  0     | Succès                                     |
| `API_ERR_ALLOC`  | -1     | Pointeur NULL ou malloc échoué             |
| `API_ERR_MUTEX`  | -2     | Erreur d'initialisation du mutex           |
| `API_ERR_THREAD` | -3     | Erreur de création du thread               |
| `API_ERR_TIMEOUT`| -4     | Timeout dépassé sur WaitForData            |
| `API_ERR_STOPPED`| -5     | API arrêtée pendant l'attente              |
| `API_NO_FRESH`   | -6     | Aucune nouvelle donnée depuis la last lect |

---

## 8. Exemple d'utilisation minimal

```c
/* Init */
AppContext *ctx = API_Init();
if (!ctx) return EXIT_FAILURE;

SharedData d;
SharedData hist[3];

/* Pattern 1 — donnée fraîche */
if (API_GetFreshData(ctx, &d) == API_OK)
    printf("value = %d\n", d.value);

/* Pattern 2 — 3 dernières valeurs */
int n = API_GetLastNData(ctx, hist, 3);
for (int i = 0; i < n; i++)
    printf("[%d] %d\n", i, hist[i].value);

/* Pattern 3 — attente événement */
if (API_WaitForData(ctx, &d) == API_OK)
    printf("event = %d\n", d.value);

/* Finish */
API_Finish(ctx);
```

---

## 9. Points de vigilance en production

| Sujet | Risque | Solution |
|---|---|---|
| Plusieurs consumers sur Event | `cond_signal` ne réveille qu'un seul waiter | Utiliser `cond_broadcast` |
| BS produit trop vite | Données perdues en polling | Passer à une vraie queue |
| BS plante | `WaitForData` bloqué indéfiniment | `cond_timedwait` ✅ déjà en place |
| Arrêt en urgence | Waiters bloqués | `cond_broadcast` dans `API_Finish` ✅ déjà en place |
| `HISTORY_SIZE` trop petit | Historique incomplet | Ajuster la constante selon les besoins |
