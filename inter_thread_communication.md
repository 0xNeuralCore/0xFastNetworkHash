# Communication inter-threads en Embedded C

## Polling
Le thread va **activement chercher** l'état en boucle.
```c
while (flag == 0); // attend
process();
```
Le thread ne dort jamais.

---

## Sémaphore
Un thread **dort** jusqu'à ce qu'un autre le réveille.
```c
xSemaphoreGive(sem);
xSemaphoreTake(sem, MAX_DELAY);
```
Juste un signal, pas de données.

---

## Event Flags
Comme le sémaphore mais **plusieurs événements combinés**.
```c
xEventGroupSetBits(eg, BIT_RX | BIT_TX);
xEventGroupWaitBits(eg, BIT_RX, ...);
```
Un seul objet, plusieurs signaux possibles.

---

## Queue
Le producteur **dépose**, le consommateur **récupère** quand il veut.
```c
xQueueSend(q, &msg, 0);
xQueueReceive(q, &msg, MAX_DELAY);
```
Découplage total + données transmises.

---

## Mailbox
Comme une queue mais **un seul slot** — le dernier message écrase l'ancien.
```c
xQueueOverwrite(mb, &msg);
xQueuePeek(mb, &msg, MAX_DELAY);
```
Utile pour des données "fraîches" (température, capteur...).

---

## Mutex
Protège une **ressource partagée** contre les accès simultanés.
```c
xSemaphoreTake(mutex, MAX_DELAY); // lock
shared_var = value;
xSemaphoreGive(mutex);            // unlock
```
Pas de communication, juste de la protection.

---

## Callback
Le producteur **appelle directement** une fonction enregistrée.
```c
void on_event(uint8_t val) { ... }
register_cb(on_event);
// producteur :
cb(42);
```
Le consommateur ne fait rien, c'est le producteur qui vient à lui.

---

## Ring Buffer (Pipe)
Un buffer circulaire partagé, souvent **sans RTOS**.
```c
ring_buf_put(&rb, data);
ring_buf_get(&rb, &data);
```
Classique entre une ISR et un thread.

---

## Opération Atomique
Une opération **indivisible**, le CPU ne peut pas être interrompu au milieu.
```c
atomic_store(&shared_var, 42);
uint32_t val = atomic_load(&shared_var);
```
Pas de mutex, pas de RTOS — garanti par le hardware.

---

## Seqlock
Le producteur **écrit librement**, le consommateur vérifie qu'il n'a pas lu pendant une écriture.
```c
// Producteur
seq++;
shared_data = new_value;
seq++;

// Consommateur
do {
    s = seq;
    val = shared_data;
} while (s != seq); // relire si écriture en cours
```
Idéal quand **un seul écrivain, plusieurs lecteurs** — les lecteurs ne bloquent jamais.

---

## Résumé

| Technique        | CPU idle | Données | Sans RTOS | Principe                              |
|------------------|----------|---------|-----------|---------------------------------------|
| Polling          | ❌       | ✅      | ✅        | Je vais chercher                      |
| Sémaphore        | ✅       | ❌      | ❌        | On me prévient (signal)               |
| Event Flags      | ✅       | ❌      | ❌        | On me prévient (plusieurs signaux)    |
| Queue            | ✅       | ✅      | ❌        | On me dépose un message               |
| Mailbox          | ✅       | ✅      | ❌        | On me dépose le dernier message       |
| Mutex            | ✅       | ❌      | ❌        | Je protège, je ne communique pas      |
| Callback         | —        | ✅      | ✅        | Le producteur m'appelle directement   |
| Ring Buffer      | ❌/✅    | ✅      | ✅        | Buffer circulaire léger               |
| Atomique         | ✅       | ✅      | ✅        | Une variable, garanti par le CPU      |
| Seqlock          | ✅       | ✅      | ✅        | Lecteurs relisent si écriture en cours|
