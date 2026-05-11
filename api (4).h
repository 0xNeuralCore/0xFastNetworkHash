/*
 * ============================================================
 *  api.h
 *  API embarquée — pool mémoire statique
 *
 *  Utilisation :
 *    1. F_api_init()                  initialise le pool + démarre le BS
 *    2. F_api_init_data(id, label)    déclare un slot de donnée
 *    3. F_api_get(id, &out)           poll la valeur (Thread APP)
 *    4. F_api_finish()                arrêt propre
 *
 *  Zéro malloc — tout est statique.
 * ============================================================
 */

#ifndef API_H
#define API_H

/* ── Taille maximale du pool (à ajuster selon la cible) ── */
#define API_MAX_SLOTS    8

/* ── Codes de retour ── */
#define API_OK           0
#define API_NO_FRESH    -1   /* donnée déjà lue, pas de nouvelle valeur  */
#define API_ERR_FULL    -2   /* pool plein, plus de slot disponible       */
#define API_ERR_ID      -3   /* id inconnu ou slot non déclaré            */
#define API_ERR_INIT    -4   /* API non initialisée                       */

/* ── Structure d'un slot de donnée ── */
typedef struct {
    int     id;              /* identifiant unique du slot                 */
    char    label[32];       /* nom lisible (ex: "sensor_temp")            */
    int     value;           /* dernière valeur peuplée par le BS          */
    int     is_fresh;        /* 1 = nouvelle valeur non encore lue         */
    int     used;            /* 1 = slot déclaré par l'utilisateur         */
} ApiSlot;

/* ── Fonctions publiques ── */
int  F_api_init      (void);
int  F_api_init_data (int id, const char *label);
int  F_api_get       (int id, int *out);
void F_api_finish    (void);

#endif /* API_H */
