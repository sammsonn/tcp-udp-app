#include <stdio.h>

#include "func.h"

// functia principala pentru rularea unui abonat
void run_subscriber(int sockfd, char *id)
{
	// initializam structura pentru poll
	struct pollfd fds[2];
	// alocam memorie pentru mesajul tcp
	tcp_message *message = malloc(sizeof(tcp_message));
	// setam numarul de descriptori de fisiere pe care ii vom monitoriza
	int nfds = 2;
	// initializam variabilele pentru numarul de octeti de trimis si de primit
	uint32_t bytes_to_send, bytes_to_receive;

	// setam descriptorii de fisiere si evenimentele pentru poll
	fds[0].fd = STDIN_FILENO;
	fds[0].events = POLLIN;
	fds[1].fd = sockfd;
	fds[1].events = POLLIN;

	// intram in bucla infinita a abonatului
	while (1) {
		// apelam functia poll pentru a monitoriza descriptorii de fisiere
		int rc = poll(fds, nfds, -1);
		// verificam daca apelul la poll a reusit
		DIE(rc < 0, "poll");

		// parcurgem toti descriptorii de fisiere
		for (int i = 0; i < nfds; i++) {
			// verificam daca pe descriptorul curent s-a produs evenimentul pollin
			if (fds[i].revents & POLLIN) {
				// daca descriptorul curent este intrarea standard
				if (fds[i].fd == STDIN_FILENO) {
					// initializam bufferul pentru citirea datelor
					char buffer[65];
					memset(buffer, 0, 65);

					// citim datele din intrarea standard
					rc = read(STDIN_FILENO, buffer, 65);
					// verificam daca citirea a reusit
					DIE(rc < 0, "read");

					// daca nu am citit niciun octet, inseamna ca intrarea standard a fost inchisa
					if (rc == 0) {
						break;
					}

					// daca comanda primita este "exit"
					if (strncmp(buffer, "exit", 4) == 0) {
						// anuntam serverul ca acest abonat se deconecteaza
						bytes_to_send = htonl(11);
						rc = send_all(sockfd, &bytes_to_send, sizeof(uint32_t));
						rc = send_all(sockfd, "DISCONNECT", 11);
						return;
					}

					// extragem prima parte a comenzii
					char *token = strtok(buffer, " ");
					if (token == NULL) {
						continue;
					}

					// daca comanda este "subscribe"
					if (strncmp(token, "subscribe", 9) == 0) {
						// extragem numele topicului
						token = strtok(NULL, "\n");
						if (token == NULL) {
							continue;
						}

						// verificam daca numele topicului este valid
						if (strlen(token) <= 50) {
							// pregatim mesajul pentru server
							strcpy(message->sub_id, id);
							strcpy(message->command, "subscribe");
							strcpy(message->topic, token);
							bytes_to_send = htonl(sizeof(tcp_message) - 51 + strlen(token) + 1);
							// trimitem mesajul la server
							rc = send_all(sockfd, &bytes_to_send, sizeof(uint32_t));
							rc = send_all(sockfd, message, ntohl(bytes_to_send));
							printf("Subscribed to topic %s\n", token);
						}
					} else if (strncmp(token, "unsubscribe", 11) == 0) {
						// extragem numele topicului
						token = strtok(NULL, "\n");
						if (token == NULL) {
							continue;
						}

						// verificam daca numele topicului este valid
						if (strlen(token) <= 50) {
							// pregatim mesajul pentru server
							strcpy(message->sub_id, id);
							strcpy(message->command, "unsubscribe");
							strcpy(message->topic, token);
							bytes_to_send = htonl(sizeof(tcp_message) - 51 + strlen(token) + 1);
							// trimitem mesajul la server
							rc = send_all(sockfd, &bytes_to_send, sizeof(uint32_t));
							rc = send_all(sockfd, message, ntohl(bytes_to_send));
							printf("Unsubscribed from topic %s\n", token);
						}
					}
					// daca descriptorul curent este socketul tcp
				} else if (fds[i].fd == sockfd) {
					// alocam memorie pentru mesajul udp
					udp_message *message = malloc(sizeof(udp_message));
					// primim numarul de octeti pe care ii vom primi
					rc = recv_all(sockfd, &bytes_to_receive, sizeof(uint32_t));
					bytes_to_receive = ntohl(bytes_to_receive);
					// primim mesajul
					rc = recv_all(sockfd, message, bytes_to_receive);

					// daca mesajul este "terminate", inchidem abonatul
					if (strncmp(message->ip, "TERMINATE", 9) == 0) {
						return;
					}

					// daca tipul de date este int
					if (message->data_type == 0) {
						// extragem semnul si valoarea
						u_int8_t sign = message->payload[0];
						u_int32_t value;
						memcpy(&value, message->payload + 1, sizeof(u_int32_t));
						value = ntohl(value);

						// daca numarul este negativ, il inmultim cu -1
						if (sign) {
							value = -value;
						}

						// afisam mesajul
						printf("%s:%hu - %s - INT - %d\n", message->ip, message->port, message->topic, value);
						// daca tipul de date este short_real
					} else if (message->data_type == 1) {
						// extragem valoarea
						uint16_t value;
						memcpy(&value, message->payload, sizeof(uint16_t));
						double final_value = ntohs(value) / 100.00;

						// afisam mesajul
						printf("%s:%hu - %s - SHORT_REAL - %.2f\n", message->ip, message->port, message->topic,
							   final_value);
						// daca tipul de date este float
					} else if (message->data_type == 2) {
						// extragem semnul, valoarea si puterea
						uint8_t sign = message->payload[0];
						uint32_t value;
						memcpy(&value, message->payload + 1, sizeof(uint32_t));
						uint8_t power = message->payload[5];
						double final_value = ntohl(value) / powf(10, power);

						// daca numarul este negativ, il inmultim cu -1
						if (sign) {
							final_value = -final_value;
						}

						// afisam mesajul
						printf("%s:%hu - %s - FLOAT - %.4f\n", message->ip, message->port, message->topic, final_value);
						// daca tipul de date este string
					} else if (message->data_type == 3) {
						// afisam mesajul
						printf("%s:%hu - %s - STRING - %s\n", message->ip, message->port, message->topic,
							   message->payload);
					}
				}
			}
		}
	}
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	// verificam daca numarul de argumente este corect
	DIE(argc != 4, "Usage: ./subscriber <ID_CLIENT> <IP_SERVER> <PORT_SERVER>");

	// extragem portul serverului
	uint16_t port;
	int rc = sscanf(argv[3], "%hu", &port);
	// verificam daca portul este valid
	DIE(rc != 1, "Given port is invalid");

	// cream socketul tcp
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	// verificam daca crearea socketului a reusit
	DIE(sockfd < 0, "socket");

	// activam optiunea tcp_nodelay pentru a dezactiva algoritmul nagle
	const int enable = 1;
	rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	// verificam daca setarea optiunii a reusit
	DIE(rc < 0, "setsockopt");

	// initializam structura pentru adresa serverului
	struct sockaddr_in serv_addr;
	socklen_t serv_len = sizeof(serv_addr);
	memset(&serv_addr, 0, serv_len);

	// setam familia de adrese si portul
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	// convertim adresa ip din format text in format binar
	rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
	// verificam daca conversia a reusit
	DIE(rc <= 0, "inet_pton");

	// ne conectam la server
	rc = connect(sockfd, (struct sockaddr *)&serv_addr, serv_len);
	// verificam daca conexiunea a reusit
	DIE(rc < 0, "connect");

	// trimitem id-ul clientului la server
	rc = send_all(sockfd, argv[1], 11);

	// rulam bucla principala a abonatului
	run_subscriber(sockfd, argv[1]);

	// inchidem socketul
	close(sockfd);

	return 0;
}
