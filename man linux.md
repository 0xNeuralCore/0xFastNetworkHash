# Documentation de Spécification - Stack TCP/IP et API Socket

## Table des matières
1. [Vue d'ensemble](#vue-densemble)
2. [Architecture et structures de données](#architecture-et-structures-de-données)
3. [Spécifications de l'API Socket](#spécifications-de-lapi-socket)
4. [Protocole TCP - Spécifications](#protocole-tcp---spécifications)
5. [Protocole UDP - Spécifications](#protocole-udp---spécifications)
6. [Architecture multi-thread](#architecture-multi-thread)
7. [Gestion des timeouts](#gestion-des-timeouts)
8. [Gestion des erreurs](#gestion-des-erreurs)

---

## Vue d'ensemble

### Objectif du système

Ce document spécifie le comportement attendu d'une implémentation complète d'une stack TCP/IP avec son API socket. L'objectif est de définir clairement :
- Les mécanismes internes requis
- Les comportements attendus pour chaque fonction
- Les transitions d'état
- Les conditions d'erreur
- Les exigences de synchronisation

### Architecture en couches

```
┌─────────────────────────────────────────────────────────┐
│              Applications utilisateur                    │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│                 API Socket                               │
│  Fonctions : socket, bind, listen, connect, accept,     │
│             send, recv, sendto, recvfrom, close          │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│            Couche de gestion                             │
│  - Table des sockets (multiplexage)                     │
│  - Table TCP (état des connexions)                      │
│  - Table UDP (files de datagrammes)                     │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│            Couche Transport                              │
│  TCP : Contrôle de connexion, fiabilité, flux           │
│  UDP : Sans connexion, sans garantie                    │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│            Couche Réseau (IP)                           │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────┐
│         Interface matérielle réseau                     │
└─────────────────────────────────────────────────────────┘
```

---

## Architecture et structures de données

### Table des sockets

**Rôle** : Structure centrale permettant de gérer tous les sockets actifs du système.

**Caractéristiques** :
- Taille fixe prédéfinie (exemple : 1024 entrées)
- Chaque entrée représente un socket potentiel
- Attribution séquentielle des descripteurs de fichiers

**États possibles d'une entrée** :
1. **LIBRE** : Emplacement disponible pour un nouveau socket
2. **ALLOUÉ** : Socket créé mais pas encore lié à une adresse
3. **LIÉ** : Socket associé à une adresse IP et un port local
4. **EN ÉCOUTE** : Socket TCP en mode passif (serveur)
5. **CONNECTÉ** : Socket avec connexion établie
6. **EN FERMETURE** : Socket en cours de fermeture gracieuse

**Informations stockées par entrée** :
- Descripteur de fichier (identifiant unique)
- État actuel du socket
- Type de protocole (TCP ou UDP)
- Adresse locale (IP + port)
- Adresse distante (IP + port)
- Pointeur vers les données spécifiques au protocole
- Mécanismes de synchronisation thread-safe

**Exigence d'allocation** :
Lors de la création d'un socket, le système DOIT parcourir séquentiellement la table jusqu'à trouver la première entrée libre. Si aucune entrée n'est disponible, une erreur "trop de fichiers ouverts" DOIT être retournée.

### Table TCP

**Rôle** : Gérer l'état et les buffers de chaque connexion TCP.

**Machine à états TCP requis** :
```
CLOSED → LISTEN (serveur)
CLOSED → SYN_SENT → ESTABLISHED (client)
LISTEN → SYN_RECEIVED → ESTABLISHED (serveur acceptant)

ESTABLISHED → FIN_WAIT_1 → FIN_WAIT_2 → TIME_WAIT → CLOSED
ESTABLISHED → CLOSE_WAIT → LAST_ACK → CLOSED
```

**Données obligatoires par connexion** :
- **État TCP actuel**
- **Numéros de séquence** :
  - `SND.UNA` : Plus ancien octet non acquitté
  - `SND.NXT` : Prochain numéro de séquence à envoyer
  - `RCV.NXT` : Prochain numéro de séquence attendu
- **Fenêtres** :
  - Fenêtre d'émission (annoncée par le pair)
  - Fenêtre de réception (notre capacité)
- **Buffers** :
  - Buffer de réception (données reçues non lues)
  - Buffer d'émission (données à envoyer)
- **Pour les sockets en LISTEN** :
  - Backlog maximum (nombre de connexions en attente)
  - File des connexions semi-établies (SYN reçu)
  - File des connexions établies en attente d'accept()

### Table UDP

**Rôle** : Gérer les datagrammes UDP reçus en attente de lecture.

**Caractéristiques** :
- Pas de notion d'état de connexion
- File d'attente FIFO des datagrammes reçus
- Chaque datagramme conserve l'adresse source

**Informations stockées** :
- File chaînée de datagrammes
- Pour chaque datagramme :
  - Adresse IP source
  - Port source
  - Données complètes
  - Taille
- Taille actuelle de la file

---

## Spécifications de l'API Socket

### Fonction `socket()`

**Objectif** : Créer un nouveau point de communication.

**Comportement attendu** :
1. Rechercher dans la table des sockets un emplacement libre
2. Allouer cet emplacement et le marquer comme "ALLOUÉ"
3. Initialiser les structures selon le protocole :
   - **TCP** : Créer un bloc de contrôle TCP à l'état CLOSED
   - **UDP** : Créer un bloc de contrôle UDP avec file vide
4. Retourner le descripteur de fichier (index dans la table)

**Conditions d'erreur** :
- Domaine non supporté → Erreur "famille d'adresses non supportée"
- Table des sockets pleine → Erreur "trop de fichiers ouverts"
- Échec d'allocation mémoire → Erreur système

**Exigences spécifiques** :
- Le socket créé NE DOIT PAS encore être lié à une adresse
- Les verrous de synchronisation DOIVENT être initialisés
- Le socket DOIT être utilisable immédiatement après création

---

### Fonction `bind()`

**Objectif** : Associer une adresse locale (IP + port) au socket.

**Comportement attendu** :
1. Vérifier que le socket existe et est dans l'état "ALLOUÉ"
2. Vérifier que l'adresse demandée n'est pas déjà utilisée
3. Associer l'adresse au socket
4. Changer l'état du socket vers "LIÉ"

**Conditions d'erreur** :
- Socket invalide → Erreur "mauvais descripteur"
- Socket déjà lié → Erreur "opération invalide"
- Port déjà utilisé → Erreur "adresse déjà utilisée"
- Permission insuffisante (ports < 1024) → Erreur "permission refusée"

**Règles de validation** :
- Le numéro de port DOIT être vérifié pour unicité
- L'IP 0.0.0.0 DOIT être acceptée (écoute sur toutes interfaces)
- Le port 0 PEUT être accepté (attribution automatique)

---

### Fonction `listen()` - TCP uniquement

**Objectif** : Passer un socket TCP en mode passif pour accepter des connexions.

**Applicabilité** :
- ✅ **TCP** : Fonction essentielle pour les serveurs
- ❌ **UDP** : Non applicable (erreur "opération non supportée")

**Comportement attendu** :
1. Vérifier que le socket est TCP
2. Vérifier que le socket est dans l'état "LIÉ"
3. Allouer la file d'attente des connexions (taille = backlog)
4. Changer l'état TCP vers "LISTEN"
5. Changer l'état du socket vers "EN ÉCOUTE"

**Conditions d'erreur** :
- Socket UDP → Erreur "opération non supportée"
- Socket non lié → Erreur "adresse de destination requise"
- Déjà en écoute → Peut être autorisé (changement de backlog) ou refusé

**Exigences de comportement** :
- Le socket NE DOIT PLUS pouvoir initier de connexion sortante
- Les SYN entrants DOIVENT être acceptés (dans la limite du backlog)
- La file d'attente DOIT gérer les connexions partielles (SYN reçu) et complètes (ESTABLISHED)

**Schéma du changement d'état** :
```
Socket TCP initialement LIÉ
         │
         │ listen(sockfd, backlog)
         ▼
  État: EN ÉCOUTE
  État TCP: LISTEN
  Capacité: backlog connexions
```

---

### Fonction `connect()` - Comportement différencié

#### TCP : Établissement actif de connexion

**Objectif** : Initier une connexion TCP vers un serveur distant.

**Comportement attendu (Three-Way Handshake)** :

```
État initial: CLOSED
         │
         │ connect() appelé
         ▼
Envoyer: SYN (seq=x)
État: SYN_SENT
         │
         │ Réception: SYN-ACK (seq=y, ack=x+1)
         ▼
Envoyer: ACK (ack=y+1)
État: ESTABLISHED
         │
         ▼
Retour de connect()
```

**Exigences du handshake** :
1. Générer un numéro de séquence initial (ISN) aléatoire
2. Envoyer un segment SYN avec ce numéro
3. Attendre la réponse SYN-ACK du serveur
4. Envoyer l'ACK final
5. Passer à l'état ESTABLISHED

**Gestion du timeout** :
- Un timeout DOIT être configuré (généralement 60-120 secondes)
- Si le SYN-ACK n'arrive pas, des retransmissions du SYN DOIVENT être effectuées
- Après expiration du timeout global, retourner une erreur "timeout"

**Conditions d'erreur** :
- Timeout → Erreur "connexion expirée"
- RST reçu → Erreur "connexion refusée"
- Socket déjà connecté → Erreur "déjà connecté"
- Réseau inatteignable → Erreur "réseau inatteignable"

#### UDP : Pseudo-connexion

**Objectif** : Mémoriser l'adresse de destination par défaut.

**Comportement attendu** :
1. Sauvegarder l'adresse distante dans le socket
2. Les futurs `send()` utiliseront cette adresse
3. AUCUN paquet n'est envoyé sur le réseau
4. Retour immédiat (pas d'attente)

**Différence fondamentale** :
- TCP : Connexion réelle avec handshake réseau
- UDP : Simple mémorisation locale, pas d'échange réseau

---

### Fonction `accept()` - TCP uniquement

**Objectif** : Extraire une connexion établie de la file d'attente d'un socket en écoute.

**Comportement attendu** :
1. Vérifier que le socket est en état "EN ÉCOUTE"
2. Consulter la file des connexions établies
3. Si la file est vide :
   - **Mode bloquant** : Attendre qu'une connexion arrive
   - **Mode non-bloquant** : Retourner immédiatement avec erreur
4. Extraire la première connexion de la file
5. Créer un nouveau socket pour cette connexion
6. Retourner le nouveau descripteur de fichier

**Propriétés du nouveau socket** :
- État : "CONNECTÉ"
- État TCP : "ESTABLISHED"
- Adresse locale : Héritée du socket en écoute
- Adresse distante : Celle du client
- Buffers : Initialisés et prêts à l'usage

**Conditions d'erreur** :
- Socket non en écoute → Erreur "opération invalide"
- Mode non-bloquant et file vide → Erreur "opération bloquerait"

---

### Fonctions d'envoi : `send()` / `sendto()`

#### `send()` - Socket connecté

**Comportement TCP** :
1. Vérifier que la connexion est établie
2. Copier les données dans le buffer d'émission
3. Si le buffer est plein, attendre de l'espace disponible
4. Découper les données en segments selon MSS
5. Appliquer le contrôle de fenêtre glissante
6. Envoyer les segments avec numéros de séquence appropriés
7. Démarrer les timers de retransmission
8. Retourner le nombre d'octets acceptés

**Comportement UDP** :
1. Utiliser l'adresse mémorisée par connect()
2. Encapsuler les données dans un datagramme UDP
3. Calculer le checksum
4. Envoyer immédiatement (pas de buffer)
5. Retourner le nombre d'octets envoyés (ou erreur)

#### `sendto()` - Spécification d'adresse explicite

**Comportement UDP** :
1. Ignorer toute adresse mémorisée par connect()
2. Utiliser l'adresse fournie en paramètre
3. Créer et envoyer le datagramme immédiatement

**Comportement TCP** :
- Généralement NON PERMIS sur un socket connecté
- Peut retourner une erreur "déjà connecté"

**Conditions d'erreur communes** :
- Buffer plein et non-bloquant → Erreur "opération bloquerait"
- Connexion fermée → Erreur "connexion fermée par le pair"
- Message trop grand (UDP) → Erreur "message trop long"

---

### Fonctions de réception : `recv()` / `recvfrom()`

#### `recv()` - TCP

**Comportement attendu** :
1. Consulter le buffer de réception
2. Si le buffer est vide :
   - **Mode bloquant** : Attendre que des données arrivent
   - **Mode non-bloquant** : Retourner immédiatement
3. Copier les données du buffer vers le buffer utilisateur
4. Supprimer les données copiées du buffer
5. Mettre à jour la fenêtre de réception
6. Envoyer un ACK si nécessaire

**Sémantique de flux** :
- TCP est orienté flux : les frontières de messages NE SONT PAS préservées
- Un `recv()` PEUT retourner moins d'octets que demandé
- Un `recv()` PEUT fusionner plusieurs `send()` du pair

#### `recvfrom()` - UDP

**Comportement attendu** :
1. Consulter la file des datagrammes reçus
2. Si la file est vide :
   - **Mode bloquant** : Attendre un datagramme
   - **Mode non-bloquant** : Retourner erreur
3. Extraire le premier datagramme complet
4. Copier les données vers le buffer utilisateur
5. Retourner l'adresse source du datagramme
6. Supprimer le datagramme de la file

**Sémantique de message** :
- UDP préserve les frontières de datagrammes
- Un `recvfrom()` retourne EXACTEMENT un datagramme complet
- Si le buffer est trop petit, les données en excès sont PERDUES

**Conditions d'erreur** :
- File vide en non-bloquant → Erreur "opération bloquerait"
- Connexion fermée (TCP) → Retour de 0 (fin de flux)
- Buffer trop petit (UDP) → Données tronquées

---

### Fonction `close()`

**Objectif** : Fermer un socket et libérer ses ressources.

#### Fermeture TCP : Four-Way Handshake

**Principe** : TCP permet une fermeture bidirectionnelle indépendante (chaque direction peut être fermée séparément).

**Schéma de fermeture normale** :

```
Côté initiateur (A)                    Côté récepteur (B)
État: ESTABLISHED                      État: ESTABLISHED
       │                                      │
       │ close() appelé                      │
       │                                      │
       │────────── FIN (seq=x) ────────────►│
       │                                      │
État: FIN_WAIT_1                       État: CLOSE_WAIT
       │                                      │
       │◄────────── ACK (ack=x+1) ──────────│
       │                                      │
État: FIN_WAIT_2                       │ (Peut continuer à envoyer)
       │                                      │
       │                                      │ close() appelé
       │                                      │
       │◄────────── FIN (seq=y) ────────────│
       │                                      │
État: TIME_WAIT                        État: LAST_ACK
       │                                      │
       │────────── ACK (ack=y+1) ──────────►│
       │                                      │
       │ Attendre 2*MSL                État: CLOSED
       │                                      │
État: CLOSED
```

**Exigences de fermeture** :

1. **Fermeture active (initiateur)** :
   - Envoyer un segment FIN
   - Passer à l'état FIN_WAIT_1
   - Attendre l'ACK du FIN → FIN_WAIT_2
   - Attendre le FIN du pair → TIME_WAIT
   - Envoyer l'ACK final
   - Rester en TIME_WAIT pendant 2*MSL (2 × Maximum Segment Lifetime)
   - Raison du TIME_WAIT : Garantir que le dernier ACK arrive et gérer les segments retardés

2. **Fermeture passive (récepteur)** :
   - Recevoir un FIN → passer en CLOSE_WAIT
   - Envoyer un ACK immédiatement
   - L'application peut continuer à envoyer des données
   - Quand l'application appelle close() → envoyer FIN
   - Passer à LAST_ACK
   - Attendre l'ACK final → CLOSED

3. **Fermeture simultanée** :
   - Les deux côtés envoient FIN en même temps
   - Chaque côté passe en CLOSING
   - Attendre l'ACK croisé
   - Passer en TIME_WAIT puis CLOSED

**Cas particuliers** :

**RST (Reset)** : Fermeture abrupte
- Utilisé en cas d'erreur ou d'état invalide
- Pas de handshake, fermeture immédiate
- Les données en transit sont perdues

**Données en attente** :
- TCP DOIT tenter d'envoyer toutes les données bufferisées avant FIN
- Si impossible (timeout), peut générer un RST

#### Fermeture UDP

**Comportement attendu** :
1. Libérer tous les datagrammes en file d'attente
2. Libérer les structures de contrôle
3. Marquer l'entrée de la table des sockets comme LIBRE
4. Aucun paquet n'est envoyé sur le réseau

**Différence avec TCP** :
- Immédiat, pas d'état de fermeture
- Pas de handshake réseau
- Pas de TIME_WAIT

---

## Protocole TCP - Spécifications

### Three-Way Handshake (Établissement)

**Objectif** : Établir une connexion fiable avec synchronisation des numéros de séquence.

**Étapes obligatoires** :

```
Client                                    Serveur
État: CLOSED                              État: LISTEN
    │                                         │
    │ 1. Envoyer SYN                         │
    │    - seq = ISN_client (aléatoire)     │
    │    - flags = SYN                       │
    │───────────────────────────────────────►│
    │                                         │
État: SYN_SENT                          État: SYN_RECEIVED
    │                                         │
    │                     2. Envoyer SYN-ACK │
    │    - seq = ISN_serveur (aléatoire)    │
    │    - ack = ISN_client + 1              │
    │    - flags = SYN + ACK                 │
    │◄───────────────────────────────────────│
    │                                         │
    │ 3. Envoyer ACK                         │
    │    - seq = ISN_client + 1              │
    │    - ack = ISN_serveur + 1             │
    │    - flags = ACK                       │
    │───────────────────────────────────────►│
    │                                         │
État: ESTABLISHED                       État: ESTABLISHED
```

**Exigences impératives** :

1. **Génération de l'ISN (Initial Sequence Number)** :
   - DOIT être aléatoire pour sécurité
   - DOIT être différent à chaque connexion
   - Éviter la prédictibilité (attaques)

2. **Gestion du SYN par le serveur** :
   - Vérifier que le backlog n'est pas plein
   - Si plein : ignorer le SYN (ou envoyer RST selon stratégie)
   - Créer une entrée semi-connexion
   - Allouer les ressources minimales

3. **Timeout du handshake** :
   - Le client DOIT retransmettre le SYN si pas de réponse
   - Retransmissions exponentielles (1s, 2s, 4s, 8s, ...)
   - Abandon après N tentatives (généralement 5-6)

4. **Protection contre SYN flood** :
   - Implémenter SYN cookies (optionnel)
   - Limiter les ressources par connexion partielle
   - Nettoyer les connexions SYN_RECEIVED expirées

### Transfert de données

**Numérotation de séquence** :

- Chaque octet de données est numéroté
- Le numéro de séquence dans le segment indique le premier octet
- L'acquittement indique le prochain octet attendu (cumulatif)

**Fenêtre glissante** :

```
Côté émetteur:
│◄────── Fenêtre d'émission ──────►│
├───────┬───────┬───────┬───────┬───────┬───────┬───────┤
│ Envoyé│ Envoyé│  En   │  En   │  Futur │ Futur│  Futur│
│  et   │  non  │ cours │ cours │        │       │       │
│ ACKé  │ ACKé  │       │       │        │       │       │
├───────┴───────┴───────┴───────┴───────┴───────┴───────┤
▲               ▲                ▲
SND.UNA         SND.NXT          SND.UNA + SND.WND
```

**Exigences d'émission** :
- NE DOIT PAS envoyer au-delà de la fenêtre annoncée
- DOIT respecter le MSS (Maximum Segment Size)
- DOIT conserver les segments non acquittés pour retransmission

**Exigences de réception** :
- DOIT accepter les segments dans l'ordre
- DOIT bufferiser les segments hors séquence (optionnel mais recommandé)
- DOIT envoyer des ACK cumulatifs
- PEUT envoyer des ACK immédiats ou différés (delayed ACK)

### Contrôle de flux

**Mécanisme** :
- Le récepteur annonce sa fenêtre disponible dans chaque segment
- L'émetteur DOIT respecter cette fenêtre
- La fenêtre représente l'espace libre dans le buffer de réception

**Fenêtre nulle** :
- Si la fenêtre annoncée = 0, l'émetteur DOIT arrêter d'envoyer
- L'émetteur DOIT envoyer des "window probes" périodiques
- Le récepteur DOIT renvoyer des window updates quand l'espace se libère

### Retransmission

**Déclenchement** :
1. **Timeout** : RTO (Retransmission Timeout) expiré
2. **Fast retransmit** : Réception de 3 ACK dupliqués

**Calcul du RTO** :
- DOIT être basé sur le RTT (Round Trip Time) mesuré
- DOIT utiliser un algorithme comme Karn ou Jacobson
- Valeur initiale typique : 3 secondes
- Doit s'adapter dynamiquement aux conditions réseau

**Exigences** :
- Retransmettre le segment non acquitté le plus ancien
- Doubler le RTO après chaque retransmission (backoff exponentiel)
- Limiter le nombre de tentatives avant d'abandonner la connexion

### Gestion des ACK

**ACK immédiats requis** :
- Segment hors séquence reçu
- Segment avec données ET déjà un segment en attente d'ACK
- Fermeture de la fenêtre (changement de taille)

**ACK différés permis** :
- Segment dans l'ordre reçu
- Délai maximum : 200-500 ms
- Permet de grouper plusieurs ACK

**ACK cumulatif** :
- Acquitte tous les octets jusqu'à seq-1
- Exemple : ACK=1000 signifie "reçu jusqu'à 999, j'attends 1000"

---

## Protocole UDP - Spécifications

### Caractéristiques fondamentales

**Sans connexion** :
- Pas de handshake avant envoi
- Chaque datagramme est indépendant
- Pas de notion d'état entre émetteur et récepteur

**Sans garantie** :
- Pas de confirmation de livraison
- Pas de retransmission automatique
- Pas de garantie d'ordre
- Les datagrammes peuvent être :
  - Perdus
  - Dupliqués
  - Réordonnés
  - Corrompus (détecté par checksum)

**Préservation des frontières** :
- Chaque `sendto()` crée UN datagramme
- Chaque `recvfrom()` reçoit UN datagramme complet
- Pas de fragmentation au niveau UDP (gérée par IP si nécessaire)

### Format et validation

**En-tête UDP** :
- Port source (16 bits)
- Port destination (16 bits)
- Longueur totale (16 bits)
- Checksum (16 bits)

**Exigences du checksum** :
- DOIT être calculé sur :
  - Pseudo-en-tête IP (IP source, IP dest, protocole)
  - En-tête UDP complet
  - Données
- Si checksum = 0, validation désactivée (déconseillé)
- Si checksum invalide → datagramme DOIT être ignoré silencieusement

### Différences avec TCP

| Aspect | TCP | UDP |
|--------|-----|-----|
| Connexion | Requise (3-way handshake) | Aucune |
| `listen()` | Obligatoire pour serveur | NON utilisé |
| `accept()` | Requis pour accepter | N'existe pas |
| `connect()` | Établit connexion | Optionnel (mémorise destination) |
| Ordre | Garanti | Non garanti |
| Fiabilité | Garantie | Non garantie |
| Contrôle de flux | Oui (fenêtre) | Non |
| Overhead | Important | Minimal |
| Usage typique | HTTP, SSH, FTP | DNS, streaming, jeux |

### Comportement des serveurs UDP

**Différence fondamentale** :
- Un serveur UDP utilise UN SEUL socket pour tous les clients
- Pas besoin de `listen()` ni `accept()`
- Le même socket reçoit de multiples sources

**Schéma d'un serveur UDP** :
```
1. socket(AF_INET, SOCK_DGRAM, 0)
2. bind(sock, adresse_locale)
3. Boucle infinie :
   - recvfrom(sock, buffer, ..., &adresse_client, ...)
   - Traiter la requête
   - sendto(sock, réponse, ..., &adresse_client, ...)
```

**Pas de table de connexions** :
- Chaque datagramme est traité indépendamment
- L'adresse source est extraite de chaque datagramme
- Aucun état n'est maintenu entre les datagrammes

---

## Architecture multi-thread

### Thread de réception (Thread caché)

**Rôle** : Dépiler continuellement les paquets arrivant de la couche IP et les distribuer.

**Responsabilités** :

1. **Récupération des paquets** :
   - Lire en continu depuis la couche IP/interface réseau
   - Extraire le protocole (TCP ou UDP)
   - Décoder les en-têtes transport

2. **Démultiplexage** :
   - Identifier le socket destinataire basé sur :
     - Port destination
     - Port source (pour TCP connecté)
     - Adresses IP (si nécessaire)
   - Rechercher dans la table des sockets

3. **Distribution TCP** :
   - Analyser les flags TCP (SYN, ACK, FIN, RST)
   - Vérifier l'état actuel de la connexion
   - Appeler le gestionnaire approprié selon l'état :
     - **LISTEN** + SYN → Créer nouvelle connexion semi-établie
     - **SYN_SENT** + SYN-ACK → Compléter handshake client
     - **SYN_RECEIVED** + ACK → Passer en ESTABLISHED
     - **ESTABLISHED** + données → Copier dans buffer de réception
     - **ESTABLISHED** + FIN → Initier fermeture
     - **FIN_WAIT_1** + ACK → Transition vers FIN_WAIT_2
     - Etc.
   - Mettre à jour les numéros de séquence/acquittement
   - Gérer la fenêtre de réception
   - Signaler les threads applicatifs en attente

4. **Distribution UDP** :
   - Créer une entrée dans la file de datagrammes
   - Copier les données complètes
   - Sauvegarder l'adresse source
   - Ajouter en fin de file (FIFO)
   - Signaler les threads en attente de `recvfrom()`

5. **Gestion des cas d'erreur** :
   - Socket destinataire inexistant :
     - **TCP** : Envoyer RST
     - **UDP** : Envoyer ICMP Port Unreachable (optionnel) ou ignorer
   - Checksum invalide : Ignorer silencieusement
   - Segment hors fenêtre (TCP) : Ignorer ou envoyer ACK
   - État TCP invalide pour le segment reçu : Envoyer RST

**Exigences de synchronisation** :
- DOIT verrouiller chaque socket avant modification
- DOIT signaler les variables de condition appropriées
- NE DOIT PAS bloquer indéfiniment (polling ou select/epoll)
- DOIT gérer la concurrence avec les appels API

**Schéma de fonctionnement** :
```
┌──────────────────────────────────────────────────────┐
│         Boucle infinie du thread de réception        │
└───────────────────┬──────────────────────────────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │ Dépiler paquet de la  │
        │   couche IP/réseau    │
        └───────────┬───────────┘
                    │
                    ▼
        ┌───────────────────────┐
        │ Extraire protocole    │
        │   TCP ou UDP ?        │
        └───────┬───────┬───────┘
                │       │
        ┌───────┘       └────────┐
        ▼                        ▼
┌───────────────┐        ┌──────────────┐
│  Traitement   │        │  Traitement  │
│     TCP       │        │     UDP      │
└───────┬───────┘        └──────┬───────┘
        │                       │
        ▼                       ▼
┌──────────────────┐    ┌──────────────┐
│ Trouver socket   │    │ Trouver      │
│ (port + état)    │    │ socket (port)│
└───────┬──────────┘    └──────┬───────┘
        │                      │
        ▼                      ▼
┌──────────────────┐    ┌─────────────────┐
│ Machine à états  │    │ Ajouter à file  │
│ TCP (LISTEN,     │    │ de datagrammes  │
│ ESTABLISHED...)  │    └─────────┬───────┘
└───────┬──────────┘              │
        │                         │
        ▼                         ▼
┌──────────────────┐    ┌─────────────────┐
│ Copier données   │    │ Signaler thread │
│ Buffer RX        │    │ en attente      │
└───────┬──────────┘    └─────────────────┘
        │
        ▼
┌──────────────────┐
│ Signaler threads │
│ bloqués (recv)   │
└──────────────────┘
```

### Thread de retransmission TCP

**Rôle** : Gérer les timeouts et retransmissions des segments TCP non acquittés.

**Responsabilités** :

1. **Surveillance des timers** :
   - Parcourir périodiquement toutes les connexions TCP actives
   - Vérifier les timers de retransmission (RTO)
   - Vérifier les timers de fermeture (TIME_WAIT, FIN_WAIT_2)
   - Vérifier les timers de keepalive (optionnel)

2. **Retransmission automatique** :
   - Identifier les segments expirés (RTO dépassé)
   - Retransmettre le segment le plus ancien non acquitté
   - Doubler le RTO (backoff exponentiel)
   - Incrémenter un compteur de retransmissions
   - Abandonner après un nombre maximum de tentatives

3. **Gestion des états temporaires** :
   - **TIME_WAIT** : Attendre 2*MSL puis fermer définitivement
   - **FIN_WAIT_2** : Timeout si le FIN du pair n'arrive jamais
   - Nettoyer les connexions mortes

4. **Keepalive** (optionnel) :
   - Envoyer des sondes périodiques sur connexions inactives
   - Détecter les pairs disparus
   - Fermer les connexions zombies

**Exigences** :
- Fréquence de vérification : Toutes les 100-500 ms typiquement
- DOIT verrouiller les sockets avant accès
- NE DOIT PAS bloquer le thread de réception
- DOIT gérer l'abandon de connexion (trop de retransmissions)

**Algorithme de base** :
```
Tant que vrai :
    Attendre INTERVALLE_TIMER (ex: 200ms)
    
    Pour chaque socket TCP dans la table :
        Verrouiller le socket
        
        Si état = ESTABLISHED ou FIN_WAIT_1 :
            Pour chaque segment non acquitté :
                Si (temps_actuel - temps_envoi) > RTO :
                    Retransmettre le segment
                    RTO = RTO × 2
                    compteur_retrans++
                    
                    Si compteur_retrans > MAX_RETRANS :
                        Fermer la connexion (RST)
                        Signaler erreur à l'application
        
        Si état = TIME_WAIT :
            Si timer_2MSL expiré :
                État = CLOSED
                Libérer les ressources
        
        Si état = FIN_WAIT_2 :
            Si timeout :
                État = CLOSED
                Libérer les ressources
        
        Déverrouiller le socket
```

### Thread applicatif (threads utilisateurs)

**Rôle** : Exécuter les appels API de l'application.

**Interactions avec le thread de réception** :

1. **Appels bloquants** :
   - `recv()` : Attend des données dans le buffer
   - `accept()` : Attend une connexion dans la file
   - `connect()` : Attend la complétion du handshake
   - `recvfrom()` : Attend un datagramme

2. **Mécanisme d'attente** :
   - Utiliser des variables de condition (`pthread_cond_wait`)
   - Le thread de réception signale via `pthread_cond_signal`
   - Vérifier la condition dans une boucle (spurious wakeups)

3. **Timeouts utilisateur** :
   - Option `SO_RCVTIMEO` / `SO_SNDTIMEO`
   - Utiliser `pthread_cond_timedwait`
   - Retourner erreur si timeout

**Schéma d'interaction recv()** :
```
Thread applicatif              Thread de réception
       │                              │
       │ recv() appelé                │
       ▼                              │
 ┌─────────────┐                     │
 │ Verrouiller │                     │
 │   socket    │                     │
 └──────┬──────┘                     │
        │                             │
        ▼                             │
  Buffer vide ?                       │
    Oui │                             │
        ▼                             │
 ┌─────────────────┐                 │
 │ pthread_cond_   │                 │
 │ wait() sur      │◄────────────────┤ Données reçues
 │ data_available  │                 │
 └──────┬──────────┘                 │
        │ Réveil                      ▼
        ▼                     ┌──────────────┐
 ┌─────────────┐             │ Copier dans  │
 │ Copier vers │             │ buffer RX    │
 │ buffer user │             └──────┬───────┘
 └──────┬──────┘                    │
        │                            ▼
        ▼                     ┌──────────────┐
 ┌─────────────┐             │ pthread_cond_│
 │ Déverrouiller│            │ signal()     │
 │   socket    │             └──────────────┘
 └──────┬──────┘
        │
        ▼
    Retourner
```

### Synchronisation et protection

**Verrous requis** :

1. **Verrou global de la table des sockets** :
   - Utilisé lors de l'allocation/libération
   - Durée de verrouillage minimale
   - Protège la structure de la table

2. **Verrou par socket** :
   - Protège toutes les opérations sur un socket donné
   - Utilisé par TOUS les threads accédant au socket
   - Ordre d'acquisition strict (éviter deadlock)

3. **Variables de condition** :
   - Une par socket pour signaler les changements d'état
   - `data_available` : Données reçues ou état changé
   - Toujours associées au verrou du socket

**Exigences anti-deadlock** :
- Ne JAMAIS verrouiller deux sockets simultanément
- Si nécessaire : ordre fixe (par numéro de descripteur croissant)
- Durée de verrouillage minimale
- Pas d'appel bloquant pendant qu'on tient un verrou

---

## Gestion des timeouts

### Timeouts au niveau API

**`SO_RCVTIMEO` : Timeout de réception**

**Comportement** :
- S'applique à `recv()`, `recvfrom()`, `accept()`
- Valeur par défaut : Infini (blocage complet)
- Valeur configurée : Temps maximum d'attente

**Exigences d'implémentation** :
- Utiliser `pthread_cond_timedwait()` avec timestamp absolu
- Si timeout : retourner -1 avec errno = EAGAIN ou EWOULDBLOCK
- Ne DOIT PAS affecter les timeouts TCP internes

**Exemple de comportement** :
```
recv() appelé avec SO_RCVTIMEO = 5 secondes

Si données disponibles immédiatement → Retour immédiat
Si données arrivent après 3 secondes → Retour après 3 secondes
Si pas de données après 5 secondes → Retour -1 (EAGAIN)
```

**`SO_SNDTIMEO` : Timeout d'émission**

**Comportement** :
- S'applique à `send()`, `sendto()`
- Concerne le blocage si buffer plein (TCP surtout)

**Implémentation TCP** :
- Si buffer d'émission plein, attendre avec timeout
- Si fenêtre du pair = 0, attendre avec timeout
- Si timeout : retourner le nombre d'octets effectivement envoyés (ou -1)

### Timeouts TCP internes

**RTO (Retransmission Timeout)**

**Calcul dynamique requis** :
- Basé sur le RTT (Round-Trip Time) mesuré
- Algorithme de Jacobson/Karels :
  ```
  SRTT = (1-α) × SRTT + α × RTT_mesuré
  RTTVAR = (1-β) × RTTVAR + β × |SRTT - RTT_mesuré|
  RTO = SRTT + 4 × RTTVAR
  ```
- Valeurs typiques : α=1/8, β=1/4
- RTO minimum : 1 seconde
- RTO maximum : 60 secondes

**Backoff exponentiel** :
- À chaque retransmission : RTO = RTO × 2
- Après acquittement : Recalculer RTO normalement
- Évite la congestion en cas de perte réseau

**MSL (Maximum Segment Lifetime)**

**Définition** :
- Temps maximum qu'un segment peut rester en transit
- Valeur typique : 30 secondes, 60 secondes, ou 2 minutes

**Utilisation** :
- **TIME_WAIT** : Doit durer 2×MSL
- Raison : Garantir que tous les segments de la connexion sont expirés
- Permet la réutilisation sûre du même couple (IP:port, IP:port)

**Timeout de connexion (connect)**

**Valeur** :
- Généralement 75-120 secondes
- Plusieurs retransmissions du SYN
- Intervalles croissants : 3s, 6s, 12s, 24s, 48s

**Comportement** :
- Si timeout : retourner ETIMEDOUT
- Libérer les ressources
- État TCP → CLOSED

**Keepalive (optionnel)**

**Paramètres** :
- `TCP_KEEPIDLE` : Délai avant première sonde (ex: 2 heures)
- `TCP_KEEPINTVL` : Intervalle entre sondes (ex: 75 secondes)
- `TCP_KEEPCNT` : Nombre de sondes avant abandon (ex: 9)

**Comportement** :
- Envoyer des segments vides avec seq = seq_actuel - 1
- Si pas de réponse après N sondes : fermer connexion
- Utilisé pour détecter pairs morts

### Timeouts de fermeture

**FIN_WAIT_2 timeout**

**Problème** :
- Le pair peut ne jamais envoyer son FIN
- La connexion resterait bloquée indéfiniment

**Solution** :
- Implémenter un timeout (ex: 60 secondes)
- Si expiré : forcer la transition vers CLOSED
- Libérer les ressources

**TIME_WAIT obligatoire**

**Durée** : 2×MSL (non configurable, requis par RFC)

**Exigences** :
- DOIT être respecté pour toute fermeture active normale
- Empêche la réutilisation prématurée du couple (IP:port)
- Gère les ACK retardés ou les segments en transit

**Exemption possible** :
- Option SO_LINGER avec linger=0 : Fermeture brutale (RST)
- Attention : Peut causer des problèmes si utilisé incorrectement

---

## Gestion des erreurs

### Erreurs au niveau API Socket

**Catégories d'erreurs** :

1. **Erreurs de paramètres** :
   - Descripteur de fichier invalide → `EBADF`
   - Adresse invalide → `EFAULT`
   - Famille d'adresse non supportée → `EAFNOSUPPORT`
   - Type de socket incorrect → `EINVAL`

2. **Erreurs d'état** :
   - Socket pas lié avant `listen()` → `EDESTADDRREQ`
   - Socket déjà connecté → `EISCONN`
   - Socket non connecté lors de `send()` → `ENOTCONN`
   - Opération invalide pour le type → `EOPNOTSUPP`

3. **Erreurs de ressources** :
   - Table des sockets pleine → `EMFILE`
   - Mémoire insuffisante → `ENOMEM`
   - Port déjà utilisé → `EADDRINUSE`
   - Backlog plein → Ignorer connexion silencieusement

4. **Erreurs de timeout** :
   - Timeout de connexion → `ETIMEDOUT`
   - Timeout de réception → `EAGAIN` ou `EWOULDBLOCK`

5. **Erreurs réseau** :
   - Réseau inatteignable → `ENETUNREACH`
   - Host inatteignable → `EHOSTUNREACH`
   - Connexion refusée → `ECONNREFUSED` (RST reçu)
   - Connexion réinitialisée → `ECONNRESET`
   - Connexion fermée par pair → `EPIPE` ou retour 0

**Exigences de reporting** :
- Toutes les fonctions DOIVENT retourner -1 en cas d'erreur
- La variable `errno` DOIT être positionnée avec le code approprié
- Les messages d'erreur DOIVENT être clairs et précis
- Ne JAMAIS masquer les erreurs silencieusement (sauf cas spécifiques UDP/TCP)

### Erreurs au niveau TCP

**Réception de RST (Reset)**

**Signification** :
- Connexion refusée (aucun socket en écoute)
- État TCP invalide pour le segment reçu
- Fermeture abrupte demandée

**Comportement requis** :
- Transition immédiate vers CLOSED
- Libération des ressources
- Si thread bloqué en `recv()` : Retourner erreur ECONNRESET
- Si thread bloqué en `send()` : Retourner erreur EPIPE

**Génération de RST** :

**Cas où RST doit être envoyé** :
1. Segment reçu pour un port sans socket en écoute
2. Segment reçu avec état TCP incohérent
3. Fermeture brutale (SO_LINGER avec linger=0)
4. Trop de retransmissions

**Format** :
- Flag RST activé
- Numéro de séquence approprié
- Pas de données

**Segments invalides**

**Checksum incorrect** :
- DOIT être ignoré silencieusement
- Pas de RST, pas de réponse
- Incrémenter un compteur statistique

**Numéro de séquence hors fenêtre** :
- Envoyer un ACK avec le numéro attendu
- Ne pas accepter les données
- Aide le pair à se resynchroniser

**Segment pour connexion inexistante** :
- Envoyer RST si le segment n'a pas le flag RST
- Ignorer si le segment est déjà un RST

### Erreurs au niveau UDP

**Principe de silence** :
- UDP ignore la plupart des erreurs silencieusement
- Pas de RST, pas de notification automatique

**Checksum invalide** :
- Ignorer le datagramme
- Ne PAS le placer dans la file de réception
- Incrémenter compteur d'erreur

**Port destination inexistant** :
- **Option 1** (recommandée) : Envoyer ICMP Port Unreachable
- **Option 2** : Ignorer silencieusement
- Ne JAMAIS générer de RST (TCP uniquement)

**Datagramme trop grand** :
- `sendto()` retourne erreur EMSGSIZE
- MTU dépassé au niveau IP
- L'application doit fragmenter manuellement

**File de réception pleine** :
- Ignorer les nouveaux datagrammes
- Ou supprimer les plus anciens (stratégie tail-drop)
- Pas de notification à l'émetteur

### Gestion de la congestion

**Détection de congestion** :

**Indicateurs** :
1. Timeout de retransmission
2. Réception d'ACK dupliqués (3 identiques)
3. ECN (Explicit Congestion Notification) si supporté

**Réactions requises** :

**Slow Start** :
- Démarrer avec une fenêtre de congestion petite (cwnd = 1 MSS)
- Doubler à chaque RTT tant qu'aucune perte
- Arrêter à un seuil (ssthresh)

**Congestion Avoidance** :
- Croissance linéaire de cwnd
- Incrémenter de 1 MSS par RTT
- Continuer jusqu'à détection de perte

**Après perte** :
- ssthresh = cwnd / 2
- cwnd = 1 MSS (retour en slow start)
- Ou cwnd = ssthresh (fast recovery)

**Exigences** :
- Respecter min(cwnd, fenêtre_annoncée)
- Ne JAMAIS envoyer plus que ces deux limites
- Adapter dynamiquement selon conditions réseau

### Cas d'erreur fatals

**Abandon de connexion TCP** :

**Conditions** :
- Trop de retransmissions sans succès (typiquement 5-15)
- Timeout total dépassé (plusieurs minutes)

**Comportement** :
- Transition vers CLOSED
- Libération de toutes les ressources
- Si thread applicatif en attente :
  - `send()` → Retourne EPIPE
  - `recv()` → Retourne ECONNRESET
  - `connect()` → Retourne ETIMEDOUT

**Logging et debugging** :

**Exigences** :
- Journaliser les événements critiques :
  - Connexions refusées
  - RST envoyés/reçus
  - Abandons de connexion
  - Erreurs de checksum
- Maintenir des compteurs statistiques :
  - Segments envoyés/reçus
  - Retransmissions
  - Erreurs diverses
- Mode debug optionnel avec traces détaillées

---

## Annexes

### Diagramme d'états TCP complet

```
                        ┌─────────┐
                   ┌────│ CLOSED  │◄───────────────┐
                   │    └─────────┘                │
                   │         │                     │
         close() ou│         │ socket() / listen() │
          timeout  │         ▼                     │
                   │    ┌─────────┐                │
                   ├────│ LISTEN  │                │
                   │    └────┬────┘                │
                   │         │ SYN reçu            │
                   │         ▼                     │
                   │ ┌────────────────┐            │
                   ├─│  SYN_RECEIVED  │            │
                   │ └───────┬────────┘            │
                   │         │ ACK reçu            │
    connect() ─────┤         │                     │
       │           │         ▼                     │
       ▼           │  ┌──────────────┐            │
  ┌─────────┐      └─►│ ESTABLISHED  │◄───────────┤
  │SYN_SENT │         └───┬──────┬───┘            │
  └────┬────┘             │      │                │
       │ SYN-ACK reçu     │      │                │
       └──────────────────┘      │                │
                                 │ close()        │
            ┌────────────────────┼────────┐       │
            │                    │        │       │
            │ FIN reçu           │        │ FIN   │
            ▼                    ▼        │ envoyé│
      ┌──────────┐         ┌──────────┐  │       │
      │CLOSE_WAIT│         │FIN_WAIT_1│◄─┘       │
      └────┬─────┘         └────┬─────┘          │
           │                    │ ACK reçu       │
           │ close()            ▼                │
           │              ┌──────────┐           │
           │              │FIN_WAIT_2│           │
           │              └────┬─────┘           │
           │                   │ FIN reçu        │
           │                   ▼                 │
           │              ┌──────────┐           │
           └─────────────►│TIME_WAIT │───────────┘
             FIN envoyé   └──────────┘  2*MSL
                  │       │
                  │       │
                  ▼       ▼
            ┌──────────┬──────────┐
            │ LAST_ACK │ CLOSING  │
            └────┬─────┴────┬─────┘
                 │          │
                 │ ACK reçu │
                 ▼          ▼
            ┌─────────────────┐
            │     CLOSED      │
            └─────────────────┘
```

### Tableau récapitulatif des fonctions

| Fonction | TCP | UDP | État requis | Bloquant | Effet principal |
|----------|-----|-----|-------------|----------|----------------|
| `socket()` | ✓ | ✓ | - | Non | Alloue entrée dans table |
| `bind()` | ✓ | ✓ | ALLOUÉ | Non | Associe adresse locale |
| `listen()` | ✓ | ✗ | LIÉ | Non | Passe en mode passif |
| `connect()` | ✓ | ✓* | ALLOUÉ/LIÉ | Oui (TCP) | Établit connexion / mémorise dest |
| `accept()` | ✓ | ✗ | EN ÉCOUTE | Oui | Extrait connexion établie |
| `send()` | ✓ | ✓ | CONNECTÉ | Oui (si buffer plein) | Envoie données |
| `sendto()` | ✓* | ✓ | LIÉ | Oui (TCP buffer plein) | Envoie à adresse spécifiée |
| `recv()` | ✓ | ✓ | CONNECTÉ | Oui (si buffer vide) | Reçoit données |
| `recvfrom()` | ✓* | ✓ | LIÉ | Oui (si file vide) | Reçoit avec adresse source |
| `close()` | ✓ | ✓ | Tout | Peut bloquer (TCP) | Ferme et libère ressources |

**Légende** :
- ✓ : Supporté et utilisé normalement
- ✓* : Supporté mais usage atypique
- ✗ : Non supporté (erreur)

### Glossaire

**MSS (Maximum Segment Size)** : Taille maximale des données dans un segment TCP (typiquement 1460 octets pour Ethernet).

**MTU (Maximum Transmission Unit)** : Taille maximale d'un paquet IP (typiquement 1500 octets pour Ethernet).

**RTT (Round-Trip Time)** : Temps aller-retour d'un paquet.

**RTO (Retransmission Timeout)** : Délai avant retransmission d'un segment non acquitté.

**MSL (Maximum Segment Lifetime)** : Durée de vie maximale d'un segment dans le réseau.

**ISN (Initial Sequence Number)** : Numéro de séquence initial d'une connexion TCP.

**Backlog** : Taille de la file d'attente des connexions en attente d'`accept()`.

**Fenêtre glissante** : Mécanisme de contrôle de flux permettant l'envoi de plusieurs segments sans attendre d'acquittement.

**ACK cumulatif** : Acquittement confirmant la réception de tous les octets jusqu'à un certain numéro de séquence.

**Fast retransmit** : Retransmission déclenchée par 3 ACK dupliqués au lieu d'attendre le timeout.

**Checksum** : Somme de contrôle pour détecter les erreurs de transmission.

---

## Conclusion

Cette spécification définit l'ensemble des comportements attendus pour une implémentation complète d'une stack TCP/IP avec API socket. Les points clés à retenir :

### Différences fondamentales TCP vs UDP

- **TCP** : Connexion, fiabilité, ordre, contrôle de flux → Complexité élevée
- **UDP** : Sans connexion, pas de garanties → Simplicité maximale

### Exigences critiques

1. **Respect de la machine à états TCP** : Essentiel pour l'interopérabilité
2. **Synchronisation multi-thread** : Protection contre les race conditions
3. **Gestion rigoureuse des timeouts** : Éviter les blocages et deadlocks
4. **Traitement correct des erreurs** : Robustesse et debuggabilité

### Architecture multi-thread

- **Thread de réception** : Cœur du système, démultiplexage et distribution
- **Thread de retransmission** : Fiabilité TCP
- **Threads applicatifs** : Isolés des détails internes

Cette documentation doit servir de référence lors de l'implémentation et de la validation du système.

---

## Cas d'usage et scénarios

### Scénario 1 : Serveur TCP simple (echo server)

**Description** : Un serveur qui accepte des connexions et renvoie les données reçues.

**Séquence d'opérations** :

```
1. socket() → Obtenir fd_serveur
   - Allocation dans table des sockets
   - État: ALLOUÉ
   - Protocole: TCP

2. bind(fd_serveur, 0.0.0.0:8080)
   - Vérifier port 8080 non utilisé
   - Associer adresse locale
   - État: LIÉ

3. listen(fd_serveur, backlog=10)
   - Allouer file de 10 connexions
   - État socket: EN ÉCOUTE
   - État TCP: LISTEN

4. Boucle infinie :
   
   a) accept(fd_serveur) → fd_client (BLOQUANT)
      - Attendre connexion dans la file
      - Thread bloqué sur condition variable
      
      [Client distant envoie SYN]
      - Thread de réception intercepte SYN
      - Crée nouvelle connexion semi-établie
      - Envoie SYN-ACK
      - État: SYN_RECEIVED
      
      [Client renvoie ACK]
      - Thread de réception complète handshake
      - Place connexion dans file d'accept()
      - État: ESTABLISHED
      - Signale thread bloqué
      
      - accept() se débloque
      - Retourne nouveau fd_client
   
   b) recv(fd_client, buffer, taille) (BLOQUANT)
      - Si buffer vide: attendre
      - Thread de réception copie données reçues
      - Signale thread applicatif
      - Retourne nombre d'octets
   
   c) send(fd_client, buffer, taille)
      - Copier dans buffer d'émission TCP
      - Découper en segments
      - Envoyer avec contrôle de fenêtre
      - Retour immédiat (ou blocage si buffer plein)
   
   d) close(fd_client)
      - Envoyer FIN
      - État: FIN_WAIT_1
      - Attendre ACK → FIN_WAIT_2
      - Attendre FIN du pair → TIME_WAIT
      - Attendre 2*MSL
      - État: CLOSED
      - Libérer ressources

5. close(fd_serveur) [jamais atteint dans cet exemple]
   - Fermer toutes connexions en attente
   - Libérer la table
```

**Points clés** :
- Le serveur ne termine jamais (boucle infinie)
- Chaque client obtient un nouveau socket (accept crée un nouveau fd)
- Le socket serveur reste en LISTEN
- La fermeture est bidirectionnelle (four-way handshake)

### Scénario 2 : Client TCP

**Description** : Un client qui se connecte à un serveur et échange des données.

**Séquence d'opérations** :

```
1. socket() → fd
   - État: ALLOUÉ

2. [Optionnel] bind(fd, adresse_locale)
   - Si omis: système choisit port automatiquement
   - État: LIÉ

3. connect(fd, serveur:port) (BLOQUANT)
   - Générer ISN aléatoire
   - Envoyer SYN
   - État: SYN_SENT
   
   [Attente avec timeout ~60-120s]
   - Thread applicatif bloqué
   
   [SYN-ACK reçu]
   - Thread de réception traite SYN-ACK
   - Envoie ACK final
   - État: ESTABLISHED
   - Signale thread applicatif
   
   - connect() retourne 0 (succès)

4. send(fd, "GET / HTTP/1.1\r\n", ...)
   - Données copiées dans buffer TCP
   - Segmentation automatique
   - Numéros de séquence assignés
   - Envoi avec fenêtre glissante

5. recv(fd, buffer, taille)
   - Lire réponse du serveur
   - Peut nécessiter plusieurs recv() pour tout lire

6. close(fd)
   - Initier fermeture active
   - Four-way handshake complet
   - TIME_WAIT obligatoire
```

**Cas d'erreur** :
- **Serveur injoignable** : connect() timeout après retransmissions SYN
- **Serveur refuse** : RST reçu → ECONNREFUSED
- **Réseau coupé pendant transfert** : Retransmissions puis ETIMEDOUT

### Scénario 3 : Serveur UDP

**Description** : Serveur DNS simplifié recevant des requêtes et renvoyant des réponses.

**Séquence d'opérations** :

```
1. socket(AF_INET, SOCK_DGRAM, 0) → fd
   - Protocole: UDP
   - État: ALLOUÉ

2. bind(fd, 0.0.0.0:53)
   - Port 53 (DNS)
   - État: LIÉ

3. [PAS de listen() - UDP n'en a pas besoin]

4. Boucle infinie :
   
   a) recvfrom(fd, buffer, taille, &client_addr) (BLOQUANT)
      - Si file de datagrammes vide: attendre
      - Thread de réception ajoute datagrammes à la file
      - Signale thread applicatif
      - Extraire premier datagramme
      - Retourne données ET adresse source
   
   b) Traiter requête DNS (parsing, lookup)
   
   c) sendto(fd, réponse, taille, &client_addr)
      - Créer datagramme UDP immédiatement
      - Calculer checksum
      - Envoyer sans attendre acquittement
      - Pas de garantie de livraison

5. [Jamais de close() dans un serveur permanent]
```

**Caractéristiques UDP importantes** :
- Un seul socket pour TOUS les clients
- Pas de notion de connexion
- Chaque datagramme est indépendant
- Pas de `accept()`, pas de nouveau socket par client
- Réponse directe avec l'adresse extraite de `recvfrom()`

### Scénario 4 : Client UDP (connect + send)

**Description** : Client utilisant connect() pour simplifier l'envoi répété.

**Séquence d'opérations** :

```
1. socket(AF_INET, SOCK_DGRAM, 0) → fd
   - Protocole: UDP

2. connect(fd, serveur_dns:53)
   - Mémoriser adresse destination
   - AUCUN paquet envoyé sur réseau
   - Retour immédiat (pas de handshake)

3. Boucle de requêtes :
   
   a) send(fd, requête_dns, taille)
      - Utilise adresse mémorisée automatiquement
      - Équivalent à sendto() sans spécifier adresse
      - Envoi immédiat
   
   b) recv(fd, buffer, taille) avec timeout
      - Configurer SO_RCVTIMEO (ex: 5 secondes)
      - Si réponse arrive: retour normal
      - Si timeout: retourner EAGAIN
      - Application décide de retenter ou non

4. close(fd)
   - Libération immédiate
   - Pas de handshake de fermeture
```

**Avantage du connect() sur UDP** :
- Syntaxe simplifiée (send au lieu de sendto)
- Filtrage automatique (seuls datagrammes de l'adresse connectée sont reçus)
- Erreurs ICMP remontées à l'application

### Scénario 5 : Gestion de multiple connexions (serveur concurrent)

**Architecture avec threads** :

```
Thread principal :
1. socket() + bind() + listen()
2. Boucle infinie :
   - accept() → nouveau_fd
   - Créer thread worker(nouveau_fd)
   - Continuer à accepter d'autres connexions

Thread worker (un par client) :
1. recv() en boucle
2. Traitement des données
3. send() réponse
4. close() quand terminé
```

**Synchronisation requise** :
- Chaque thread accède à SON socket (nouveau_fd unique)
- Pas de partage de socket entre threads
- Le thread principal ne touche pas aux sockets clients
- Verrous individuels par socket protègent contre accès concurrent (thread réception vs thread worker)

**Alternative avec pool de threads** :

```
File de travail globale (protégée par mutex)

Thread principal :
- accept() → nouveau_fd
- Ajouter nouveau_fd dans file de travail
- Signaler un thread du pool

Threads du pool (N threads fixes) :
- Attendre un fd dans la file
- Extraire fd
- Traiter requête complète (recv/process/send/close)
- Retourner attendre un autre fd
```

### Scénario 6 : Fermetures anormales

#### Fermeture brutale (SO_LINGER)

**Configuration** :
```
Option SO_LINGER avec linger = 0
```

**Comportement** :
- close() envoie RST immédiatement
- Pas de four-way handshake
- Données en buffer sont perdues
- Transition directe vers CLOSED
- Pas de TIME_WAIT

**Usage** :
- Erreur irrécupérable détectée
- Besoin de libérer ressources rapidement
- Attention : peut causer problèmes côté pair

#### Fermeture après perte réseau

**Scénario** :
```
État: ESTABLISHED
  ↓
[Réseau coupé physiquement]
  ↓
Application appelle send()
  ↓
Segments envoyés mais pas d'ACK
  ↓
Timeouts RTO successifs (1s, 2s, 4s, 8s, ...)
  ↓
Après 5-15 retransmissions (plusieurs minutes)
  ↓
Abandon de connexion
État: CLOSED
send() retourne ETIMEDOUT ou EHOSTUNREACH
```

**Détection plus rapide avec Keepalive** :
- Active keepalive sur le socket
- Sondes envoyées après inactivité (ex: 2 heures)
- Si 9 sondes échouent (intervalle 75s) → ~11 minutes
- Connexion fermée avec erreur

#### Fermeture par timeout en FIN_WAIT_2

**Scénario** :
```
Application appelle close()
  ↓
Envoi FIN
État: FIN_WAIT_1
  ↓
ACK reçu
État: FIN_WAIT_2
  ↓
[Pair ne répond plus, pas de FIN]
  ↓
Timeout FIN_WAIT_2 (ex: 60 secondes)
  ↓
État: CLOSED (forcé)
Libération ressources
```

**Raison du timeout** :
- Éviter blocage indéfini si pair a crashé
- Libérer ressources système
- Compromis entre robustesse et conformité RFC

### Scénario 7 : Contrôle de flux TCP en action

**Situation** : Émetteur rapide, récepteur lent

```
Récepteur :
- Buffer de réception: 64 KB
- Application lit lentement (1 KB/s)

Émetteur :
- Application envoie 100 KB rapidement

Déroulement :

T=0s :
  Émetteur envoie segments jusqu'à remplir fenêtre (64 KB)
  Récepteur: buffer plein, annonce fenêtre = 0

T=1s :
  Récepteur: application lit 1 KB
  Récepteur annonce fenêtre = 1 KB
  Émetteur peut envoyer 1 KB

T=2s :
  Récepteur: application lit 1 KB
  Fenêtre = 1 KB
  Cycle continue...

Émetteur bloqué :
  send() de l'application bloque quand buffer local plein
  Attend que fenêtre distante s'ouvre
  Thread applicatif en pthread_cond_wait()
```

**Mécanisme de window probe** :
```
Si fenêtre = 0 persiste :
  Émetteur envoie des sondes périodiques (1 octet)
  Vérifie si fenêtre s'est ouverte
  Évite le deadlock si update de fenêtre est perdu
```

### Scénario 8 : Retransmission et perte de paquets

**Situation** : Perte d'un segment en transit

```
Émetteur envoie segments seq: 1000, 2000, 3000, 4000

Récepteur reçoit: 1000, [2000 perdu], 3000, 4000

Comportement récepteur :
  - Reçoit 1000 → ACK=2000 (attend 2000)
  - 2000 perdu
  - Reçoit 3000 → ACK=2000 (toujours, ACK dupliqué)
  - Reçoit 4000 → ACK=2000 (ACK dupliqué)
  
  Optionnel: bufferiser 3000 et 4000 hors séquence

Émetteur détecte :
  - Reçoit ACK=2000 (normal)
  - Reçoit ACK=2000 (duplicata #1)
  - Reçoit ACK=2000 (duplicata #2)
  - Reçoit ACK=2000 (duplicata #3)
  
  → Fast Retransmit déclenché !
  
  - Retransmet segment 2000 immédiatement
  - Pas d'attente du timeout RTO

Récepteur après retransmission :
  - Reçoit 2000 (enfin!)
  - Si 3000 et 4000 bufferisés:
    → ACK=5000 (acquitte tout d'un coup)
  - Sinon:
    → ACK=3000 (attend les suivants)
```

**Bénéfice du fast retransmit** :
- Récupération rapide (quelques RTT)
- Pas d'attente du RTO (plusieurs secondes)
- Améliore considérablement les performances

---

## Tests et validation

### Tests unitaires requis par composant

#### Table des sockets

**Tests d'allocation** :
- Allocation d'un socket libre
- Allocation quand table pleine
- Libération et réallocation
- Allocation concurrente (thread-safety)

**Tests d'états** :
- Transitions valides (ALLOUÉ → LIÉ → EN ÉCOUTE)
- Transitions invalides (erreur attendue)
- État cohérent après erreur

#### Protocole TCP

**Tests du handshake** :
- Three-way handshake normal
- SYN retransmis si perdu
- Timeout de connexion
- Réception de RST pendant handshake
- Backlog plein (SYN ignoré)

**Tests de transfert** :
- Envoi/réception de données simples
- Données volumineuses (fragmentation)
- Contrôle de fenêtre (fenêtre nulle)
- Segments hors séquence
- ACK retardés

**Tests de fermeture** :
- Fermeture normale (four-way)
- Fermeture simultanée
- Fermeture avec données en transit
- TIME_WAIT correct (2*MSL)
- Fermeture brutale (RST)

**Tests de fiabilité** :
- Perte de segments (retransmission timeout)
- Perte d'ACK (retransmission)
- Fast retransmit (3 ACK dupliqués)
- Calcul dynamique du RTO

#### Protocole UDP

**Tests de base** :
- Envoi/réception datagramme simple
- Datagrammes multiples (ordre préservé?)
- Checksum invalide (ignoré)
- Port inexistant (ICMP Port Unreachable)

**Tests de file** :
- File vide (recvfrom bloque)
- File pleine (stratégie de drop)
- Datagrammes de tailles variables

#### Tests multi-thread

**Synchronisation** :
- Accès concurrent au même socket
- Race condition sur table des sockets
- Deadlock (vérifier absence)
- Signalisation correcte (cond variables)

**Performance** :
- Latence du thread de réception
- Throughput avec connexions multiples
- Overhead de synchronisation

### Scénarios de tests d'intégration

#### Test 1 : Transfert fichier volumineux (TCP)

**Objectif** : Valider fiabilité et performances TCP

**Procédure** :
1. Serveur en écoute sur port
2. Client se connecte
3. Client envoie fichier de 100 MB
4. Serveur reçoit et vérifie checksum
5. Fermeture connexion

**Validations** :
- Aucune perte de données
- Checksum identique
- Temps de transfert acceptable
- Mémoire stable (pas de fuite)

#### Test 2 : Résilience aux pertes (TCP)

**Objectif** : Valider retransmissions

**Procédure** :
1. Injecter pertes aléatoires (5% des segments)
2. Transférer données
3. Vérifier intégrité

**Validations** :
- Données reçues complètes et correctes
- Retransmissions observées
- RTO s'adapte correctement

#### Test 3 : Multiples clients simultanés

**Objectif** : Valider scalabilité

**Procédure** :
1. Lancer serveur
2. Connecter 100 clients simultanément
3. Chaque client fait plusieurs requêtes
4. Fermer toutes connexions

**Validations** :
- Toutes connexions acceptées
- Aucune erreur côté serveur
- Performance acceptable
- Pas de deadlock

#### Test 4 : Déconnexions brutales

**Objectif** : Valider robustesse

**Procédure** :
1. Établir connexion
2. Simuler crash client (fermeture sans FIN)
3. Serveur doit détecter via timeout

**Validations** :
- Serveur libère ressources après timeout
- Aucune fuite mémoire
- Autres connexions non affectées

#### Test 5 : UDP haute fréquence

**Objectif** : Valider UDP sous charge

**Procédure** :
1. Client envoie 10000 datagrammes rapidement
2. Serveur compte les datagrammes reçus

**Validations** :
- Pertes acceptables (UDP normal)
- Pas de crash
- File de réception gérée correctement
- Datagrammes dans l'ordre d'arrivée

### Outils de validation

**Analyseur de paquets** :
- Capturer trafic avec tcpdump/Wireshark
- Vérifier conformité des segments (flags, seq, ack)
- Valider checksums
- Observer retransmissions

**Simulateur de réseau** :
- Injecter latence variable
- Simuler pertes de paquets
- Simuler réordonnancement
- Tester limites de bande passante

**Tests de régression** :
- Suite automatisée de tests
- Exécution à chaque modification
- Détection de régressions

**Fuzzing** :
- Envoyer segments malformés
- Valider robustesse face à entrées invalides
- Pas de crash, comportement défini

---

## Métriques et observabilité

### Statistiques à collecter

#### Par socket

**TCP** :
- Octets envoyés / reçus
- Segments envoyés / reçus
- Retransmissions
- ACK dupliqués reçus
- Timeouts RTO
- Fenêtre actuelle (send/receive)
- RTT moyen et variance
- État actuel

**UDP** :
- Datagrammes envoyés / reçus
- Datagrammes perdus (checksum invalide)
- Taille de la file de réception

#### Globales système

- Nombre de sockets actifs par type
- Connexions TCP par état
- Taux d'allocation/libération
- Erreurs par type (ECONNREFUSED, ETIMEDOUT, etc.)
- Utilisation mémoire (buffers)

### Interfaces de monitoring

**Commandes de debug** :
```
show_sockets()           # Liste tous sockets actifs
show_tcp_stats(fd)       # Stats détaillées d'une connexion
show_global_stats()      # Stats système globales
dump_tcp_state(fd)       # État complet d'une connexion
dump_buffers(fd)         # Contenu des buffers
```

**Logs structurés** :
```
[TCP] [fd=5] State transition: ESTABLISHED → FIN_WAIT_1
[TCP] [fd=5] Retransmission timeout, RTO=2000ms
[UDP] [fd=8] Received datagram from 192.168.1.10:5353
[ERROR] [fd=12] Connection reset by peer
```

### Indicateurs de performance (KPIs)

**Latence** :
- Temps de connexion (connect duration)
- RTT moyen
- Latence applicative (send → recv)

**Débit** :
- Throughput en MB/s
- Utilisation de la fenêtre TCP
- Taux de retransmission

**Fiabilité** :
- Taux de perte UDP
- Taux d'erreur connexion
- Uptime moyen des connexions

**Ressources** :
- Mémoire utilisée (buffers)
- CPU du thread de réception
- Nombre de threads actifs

---

## Optimisations possibles

### Optimisations mémoire

**Zero-copy** :
- Éviter copies multiples des données
- Utiliser pointeurs et offsets
- Partager buffers entre couches

**Pool de buffers** :
- Préallouer buffers réutilisables
- Éviter malloc/free fréquents
- Tailles standards (MTU aligned)

**Compression** :
- Buffers circulaires compacts
- Libération anticipée des données ACKées

### Optimisations CPU

**Batch processing** :
- Traiter plusieurs paquets à la fois
- Réduire surcoût de synchronisation
- Améliorer cache locality

**Calcul de checksum optimisé** :
- Instructions SIMD si disponibles
- Calcul incrémental
- Vérification lazy

**Fast path** :
- Optimiser le cas commun (segments in-order)
- Branchements prédictibles
- Minimiser les verrous

### Optimisations réseau

**Nagle's algorithm** :
- Grouper petits segments
- Réduire overhead TCP
- Configurable (TCP_NODELAY)

**Delayed ACK** :
- Grouper ACKs
- Réduire trafic
- Maximum 200ms de délai

**Large receive offload (LRO)** :
- Coalescer segments au niveau matériel
- Réduire interruptions
- Nécessite support NIC

### Scalabilité

**Table de hachage pour sockets** :
- O(1) au lieu de O(n) pour recherche
- Clé: (IP src, port src, IP dst, port dst)
- Gestion des collisions

**Multiple threads de réception** :
- Un thread par core CPU
- Partitionnement par flux (RSS)
- Nécessite synchronisation fine

**Lock-free queues** :
- Files sans verrous pour datagrammes UDP
- Atomics et CAS
- Complexité accrue

---

## Conformité aux standards

### RFCs implémentées (obligatoires)

**RFC 793** : Transmission Control Protocol
- Machine à états TCP
- Numéros de séquence et acquittements
- Fenêtre glissante
- Retransmission

**RFC 768** : User Datagram Protocol
- Format datagramme
- Checksum
- Multiplexage par port

**RFC 1122** : Requirements for Internet Hosts
- Comportements obligatoires et recommandés
- Gestion d'erreurs
- Timeouts

**RFC 2001** : TCP Slow Start, Congestion Avoidance
- Algorithmes de contrôle de congestion
- Slow start
- Congestion avoidance
- Fast retransmit / Fast recovery

### RFCs optionnelles (améliorations)

**RFC 2018** : TCP Selective Acknowledgment (SACK)
- ACKs non-cumulatifs
- Récupération plus efficace
- Performances améliorées

**RFC 6298** : Computing TCP's Retransmission Timer
- Algorithme moderne de calcul RTO
- Meilleure adaptation réseau

**RFC 7323** : TCP Extensions for High Performance
- Window scaling (fenêtres > 64KB)
- Timestamps
- PAWS (Protection Against Wrapped Sequences)

### Points de non-conformité acceptables

**Simplifications** :
- Pas d'options TCP avancées (SACK, timestamps)
- Fenêtre fixe (pas de scaling)
- Algorithme de congestion simplifié

**Justifications** :
- Implémentation pédagogique/prototype
- Réduction de complexité
- Fonctionnalités core présentes

**Documentation des limitations** :
- Lister explicitement les fonctionnalités manquantes
- Expliquer l'impact sur performances/compatibilité
- Fournir roadmap pour implémentation future

---

## Conclusion et perspectives

### Récapitulatif des exigences critiques

Cette documentation a spécifié l'ensemble des comportements essentiels pour une implémentation fonctionnelle d'une stack TCP/IP avec API socket :

1. **Architecture solide** :
   - Tables de gestion (sockets, TCP, UDP)
   - Machine à états TCP respectée
   - Démultiplexage efficace

2. **Fiabilité TCP** :
   - Three-way handshake correct
   - Retransmissions automatiques
   - Contrôle de flux et congestion
   - Fermeture bidirectionnelle propre

3. **Simplicité UDP** :
   - Sans connexion, sans état
   - Préservation des frontières
   - Traitement minimal

4. **Concurrence maîtrisée** :
   - Thread de réception performant
   - Synchronisation thread-safe
   - Pas de deadlock ni race condition

5. **Robustesse** :
   - Gestion complète des erreurs
   - Timeouts appropriés
   - Récupération après pannes

### Évolutions futures possibles

**Fonctionnalités avancées** :
- Support IPv6
- Options TCP (SACK, timestamps, window scaling)
- Algorithmes de congestion modernes (CUBIC, BBR)
- TLS/SSL intégré

**Performance** :
- Optimisations zero-copy
- Support multi-queue NIC
- Offloading matériel (TSO, LRO, GSO)

**Observabilité** :
- Interface web de monitoring
- Intégration Prometheus/Grafana
- Tracing distribué

**Conformité** :
- Validation suite de tests RFC
- Certification interopérabilité
- Benchmarks standardisés

### Utilisation de cette documentation

**Pour l'implémenteur** :
- Guide complet des comportements attendus
- Diagrammes et schémas de référence
- Checklist de validation

**Pour le testeur** :
- Scénarios de test exhaustifs
- Cas limites à vérifier
- Métriques à surveiller

**Pour le mainteneur** :
- Compréhension globale du système
- Exigences de chaque composant
- Points d'attention critiques

Cette spécification constitue le contrat entre la théorie réseau et l'implémentation pratique. Le respect rigoureux de ces exigences garantit une stack réseau fonctionnelle, fiable et interopérable.