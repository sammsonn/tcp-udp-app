## PCOM - Tema 2
## Samson Alexandru-Valentin
## 322CC

-------------------------------------------------------------------------------
# Protocol
Protocol utilizat foloseste TCP pentru transport astfel:

- Pentru interpretarea corecta a partilor din fluxul de octeti am folosit functiile send_all si recv_all descrise mai jos.
- Mai intai este transmisa dimensiunea mesajului ce urmeaza in forma unui uint32_t.
- Receptorul primeste acest uint32_t.
- Apoi emitatorul transmite mesajul in cantitatea exacta care a fost transmisa anterior.
- Receptorul primeste mesajul in cantitatea exacta primita anterior.

-------------------------------------------------------------------------------
# server.c

## Functia `run_server`
Aceasta functie este responsabila pentru rularea buclei principale a serverului. Monitorizeaza evenimente pe descriptorii de fisiere pentru TCP si UDP si gestioneaza conexiunile si mesajele de la clienti.

- Initializeaza descriptorii de fisiere si evenimentele pentru `poll`.
- Intra intr-o bucla infinita in care:
  - Asteapta evenimente pe descriptorii de fisiere folosind functia `poll`.
  - Verifica evenimentele pe fiecare descriptor:
    - Daca evenimentul este pe socketul TCP, accepta noi conexiuni, primeste mesaje de la clienti si trimite mesaje altor clienti.
    - Daca evenimentul este pe socketul UDP, primeste mesaje si le trimite clientilor abonati la topicul corespunzator.
    - Daca evenimentul este pe intrarea standard, primeste comenzi de la utilizator (`exit`) si trimite mesaje de terminare la toti clientii inainte de a inchide conexiunea.

## Functia `main`
Aceasta functie este responsabila pentru initializarea conexiunii la portul specificat si apelarea functiei `run_server`.

- Verifica numarul corect de argumente.
- Extrage portul dorit din argumentele liniei de comanda.
- Creeaza un socket TCP si unul UDP si le leaga la portul specificat.
- Apoi, ruleaza functia `run_server`.
- La final, inchide socketurile.

-------------------------------------------------------------------------------
# subscriber.c

## Functia `run_subscriber`
Aceasta functie este responsabila pentru rularea buclei principale a abonatului. Monitorizeaza intrarea standard si socketul la care este conectat abonatul pentru a primi mesaje de la server.

- Initializeaza structura pentru `pollfd`.
- Seteaza descriptorii de fisiere si evenimentele pentru `poll`.
- Intra intr-o bucla infinita in care:
  - Asteapta evenimente pe descriptorii de fisiere folosind functia `poll`.
  - Verifica evenimentele pe fiecare descriptor:
    - Daca evenimentul este pe intrarea standard, citim comenzile introduse de utilizator (`subscribe`, `unsubscribe`, `exit`).
    - Daca evenimentul este pe socketul tcp, primim mesajele de la server si le afisam in functie de tipul de date (`INT`, `SHORT_REAL`, `FLOAT`, `STRING`).

## Functia `main`
Aceasta functie este responsabila pentru initializarea conexiunii la server si apelarea functiei `run_subscriber`.

- Verifica numarul corect de argumente.
- Extrage portul serverului din argumentele liniei de comanda.
- Creeaza un socket tcp si se conecteaza la server.
- Trimite ID-ul clientului la server.
- Apoi, ruleaza functia `run_subscriber`.
- La final, inchide socketul.

-------------------------------------------------------------------------------
# func.c

## Functia `topic_matches_subscription`
Aceasta functie verifica daca un topic se potriveste cu un abonament.

- Initializeaza doua variabile pentru a salva pozitia curenta in sirurile de caractere.
- Extrage primul nivel din topic si abonament folosind functia `strtok_r`.
- Parcurge nivelurile din topic si abonament si compara fiecare nivel:
  - Daca nivelul din abonament nu este "+", "*", si nu este egal cu nivelul din topic, returneaza 0.
  - Daca nivelul din abonament este "*", se potriveste cu orice numar de niveluri din topic.
  - Continua sa parcurga nivelurile din topic si abonament pentru a gasi o potrivire.
- Daca topicul si abonamentul au un numar diferit de niveluri, returneaza 0.
- Daca ajunge la acest punct, topicul se potriveste cu abonamentul si returneaza 1.

## Functiile `recv_all` si `send_all`
Aceste functii sunt responsabile pentru primirea si trimiterea tuturor octetilor pe un socket.

- `recv_all`:
  - Primeste un descriptor de fisier, un buffer si un numar specificat de octeti pentru a primi.
  - Primeste date de la socket si le salveaza in buffer pana cand primeste toti octetii specificati.
  - Returneaza numarul total de octeti primiti.

- `send_all`:
  - Primeste un descriptor de fisier, un buffer si un numar specificat de octeti pentru a trimite.
  - Trimite date catre socket si le salveaza din buffer pana cand trimite toti octetii specificati.
  - Returneaza numarul total de octeti trimisi.

-------------------------------------------------------------------------------
# func.h

## Macrodefinitii

- `DIE(assertion, call_description)`: Macrodefinitie pentru gestionarea erorilor. Afiseaza mesajul de eroare si inchide programul in cazul in care o asertiune este adevarata.

## Structuri de Date

### Structura `tcp_message`
Reprezinta un mesaj TCP ce este trimis intre client si server.

- `sub_id`: ID-ul abonatului.
- `command`: Comanda (ex. "subscribe", "unsubscribe").
- `topic`: Numele topicului.

### Structura `udp_message`
Reprezinta un mesaj UDP trimis de la server la client.

- `ip`: Adresa IP a clientului UDP.
- `port`: Portul clientului UDP.
- `topic`: Numele topicului.
- `data_type`: Tipul de date al mesajului.
- `payload`: Continutul mesajului.

### Structura `subscriber`
Reprezinta un abonat.

- `sockfd`: Descriptorul de fisier al socketului abonatului.
- `id`: ID-ul abonatului.

### Structura `topic`
Reprezinta un topic la care se pot abona abonatii.

- `name`: Numele topicului.
- `subscribers`: Lista de abonati la topic.
- `subscribers_count`: Numarul de abonati la topic.

-------------------------------------------------------------------------------
# Punctaj

RESULTS
-------
compile...............................................passed
server_start..........................................passed
c1_start..............................................passed
data_unsubscribed.....................................passed
c1_subscribe_all......................................passed
data_subscribed.......................................passed
c1_stop...............................................passed
c1_restart............................................passed
data_no_clients.......................................passed
same_id...............................................passed
c2_start..............................................passed
c2_subscribe..........................................passed
c2_stop...............................................passed
c2_subscribe_plus_wildcard............................passed
c2_subscribe_star_wildcard............................passed
c2_subscribe_compound_wildcard........................passed
c2_subscribe_wildcard_set_inclusion...................passed
quick_flow............................................passed
server_stop...........................................passed
