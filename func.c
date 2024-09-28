#include <stdio.h>

#include "func.h"

// functia verifica daca un topic se potriveste cu un abonament
int topic_matches_subscription(char *topic, char *subscription)
{
	// variabile pentru a salva pozitia curenta in string-uri
	char *saveptr1, *saveptr2;
	// extragem primul nivel din topic si abonament
	char *topic_level = strtok_r(topic, "/", &saveptr1);
	char *subscription_level = strtok_r(subscription, "/", &saveptr2);

	// parcurgem nivelurile din topic si abonament
	while (topic_level != NULL && subscription_level != NULL) {
		// daca nivelul din abonament nu este "+" sau "*" si nu este egal cu nivelul din topic, nu este o potrivire
		if (strcmp(subscription_level, "+") != 0 && strcmp(subscription_level, "*") != 0 &&
			strcmp(topic_level, subscription_level) != 0) {
			return 0;
		}

		// daca nivelul din abonament este "*", se potriveste cu orice numar de niveluri din topic
		if (strcmp(subscription_level, "*") == 0) {
			// trecem la urmatorul nivel care nu este "*"
			do {
				subscription_level = strtok_r(NULL, "/", &saveptr2);
			} while (subscription_level != NULL && strcmp(subscription_level, "*") == 0);

			// daca nu exista mai multe niveluri in abonament, este o potrivire
			if (subscription_level == NULL) {
				return 1;
			}

			// trecem la nivelul din topic care se potriveste cu urmatorul nivel din abonament
			while ((topic_level = strtok_r(NULL, "/", &saveptr1)) != NULL &&
				   strcmp(topic_level, subscription_level) != 0)
				;

			if (topic_level == NULL) {
				// daca nu exista mai multe niveluri in topic, nu este o potrivire
				return 0;
			}
		}

		// trecem la urmatorul nivel in topic si abonament
		topic_level = strtok_r(NULL, "/", &saveptr1);
		subscription_level = strtok_r(NULL, "/", &saveptr2);
	}

	// daca topicul si abonamentul au un numar diferit de niveluri, nu este o potrivire
	if (topic_level != NULL || subscription_level != NULL) {
		return 0;
	}

	// daca am ajuns pana aici, topicul se potriveste cu abonamentul
	return 1;
}

// functia primeste toti octetii de la un socket
int recv_all(int sockfd, void *buffer, size_t len)
{
	// numarul de octeti primiti
	size_t bytes_received = 0;
	// numarul de octeti ramasi de primit
	size_t bytes_remaining = len;
	// bufferul in care vom primi datele
	char *buff = buffer;

	// continuam sa primim date pana cand am primit toti octetii
	while (bytes_remaining) {
		// primim date de la socket
		int bytes = recv(sockfd, buff, bytes_remaining, 0);
		// verificam daca primirea datelor a reusit
		DIE(bytes < 0, "recv_all recv");

		// actualizam numarul de octeti primiti
		bytes_received += bytes;
		// actualizam numarul de octeti ramasi de primit
		bytes_remaining -= bytes;
		// mutam pointerul in buffer
		buff += bytes;
	}

	// returnam numarul total de octeti primiti
	return bytes_received;
}

// functia trimite toti octetii la un socket
int send_all(int sockfd, void *buffer, size_t len)
{
	// numarul de octeti trimisi
	size_t bytes_sent = 0;
	// numarul de octeti ramasi de trimis
	size_t bytes_remaining = len;
	// bufferul din care vom trimite datele
	char *buff = buffer;

	// continuam sa trimitem date pana cand am trimis toti octetii
	while (bytes_remaining) {
		// trimitem date la socket
		int bytes = send(sockfd, buff, bytes_remaining, 0);
		// verificam daca trimiterea datelor a reusit
		DIE(bytes < 0, "send_all send");

		// actualizam numarul de octeti trimisi
		bytes_sent += bytes;
		// actualizam numarul de octeti ramasi de trimis
		bytes_remaining -= bytes;
		// mutam pointerul in buffer
		buff += bytes;
	}

	// returnam numarul total de octeti trimisi
	return bytes_sent;
}
