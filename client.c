#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
 
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
 
#define SERVERIP "127.0.0.1"
#define SERVERPORT 8080
 
#define BUFFSIZE 1024
#define ALIASLEN 32
#define OPTLEN 16
#define LINEBUFF 2048
 
	struct PACKET {
		// instruction
		char option[OPTLEN];
		// l'alias du client
		char alias[ALIASLEN]; 
		// charge utile
		char buff[BUFFSIZE]; 
	};
 
	struct USER {
		// socket descripteur de l'utilisateur
		int sockfd; 
		// le nom de l'utilisateur
		char alias[ALIASLEN]; 
	};
 
	struct THREADINFO {
		 // pointeur du thread
		pthread_t thread_ID;
		// fichier descripteur de socket
		int sockfd; 
	};
 
	int isconnected, sockfd;
	char option[LINEBUFF];
	struct USER me;
		 
	int connect_with_server();
	void setalias(struct USER *me);
	void logout(struct USER *me);
	void login(struct USER *me);
	void *receiver(void *param);
	void sendtoall(struct USER *me, char *msg);
	void sendtoalias(struct USER *me, char * target, char *msg);
 
	int main(int argc, char **argv) {
		int sockfd, aliaslen;
		memset(&me, 0, sizeof(struct USER));
		while(gets(option)) {
			if(!strncmp(option, "exit", 4)) {
				logout(&me);
				break;
			}
			if(!strncmp(option, "help", 4)) {
				FILE *fin = fopen("help.txt", "r");
				if(fin != NULL) {
					while(fgets(option, LINEBUFF-1, fin)) puts(option);
					fclose(fin);
				}
				else {
					fprintf(stderr, "Help file not found...\n");
				}
			}
			else if(!strncmp(option, "login", 5)) {
				char *ptr = strtok(option, " ");
				ptr = strtok(0, " ");
				memset(me.alias, 0, sizeof(char) * ALIASLEN);
				if(ptr != NULL) {
					aliaslen = strlen(ptr);
					if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
					strcpy(me.alias, ptr);
				}
				else {
					strcpy(me.alias, "Anonymous");
				}
				login(&me);
			}
			else if(!strncmp(option, "alias", 5)) {
				char *ptr = strtok(option, " ");
				ptr = strtok(0, " ");
				memset(me.alias, 0, sizeof(char) * ALIASLEN);
				if(ptr != NULL) {
					aliaslen = strlen(ptr);
					if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
					strcpy(me.alias, ptr);
					setalias(&me);
				}
			}
			else if(!strncmp(option, "whisp", 5)) {
				char *ptr = strtok(option, " ");
				char temp[ALIASLEN];
				ptr = strtok(0, " ");
				memset(temp, 0, sizeof(char) * ALIASLEN);
				if(ptr != NULL) {
					aliaslen = strlen(ptr);
					if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
					strcpy(temp, ptr);
					while(*ptr) ptr++; ptr++;
					while(*ptr <= ' ') ptr++;
					sendtoalias(&me, temp, ptr);
				}
			}
			else if(!strncmp(option, "send", 4)) {
				sendtoall(&me, &option[5]);
			}
			else if(!strncmp(option, "logout", 6)) {
				logout(&me);
			}
				else fprintf(stderr, "Unknown option...\n");
		}// fin while
		return 0;
	}// fin main
	 
	void login(struct USER *me) {
		int recvd;
		if(isconnected) {
			fprintf(stderr, "You are already connected to server at %s:%d\n", SERVERIP, SERVERPORT);
			return;
		}
		sockfd = connect_with_server();
		if(sockfd >= 0) {
			isconnected = 1;
			me->sockfd = sockfd;
			if(strcmp(me->alias, "Anonymous")) setalias(me);
			printf("Logged in as %s\n", me->alias);
			printf("Receiver started [%d]...\n", sockfd);
			struct THREADINFO threadinfo;
			pthread_create(&threadinfo.thread_ID, NULL, receiver, (void *)&threadinfo);
		}
		else {
			fprintf(stderr, "Connection rejected...\n");
		}
	} // fin void login
	 
	int connect_with_server() {
		int newfd, err_ret;
		struct sockaddr_in serv_addr;
		struct hostent *to;
		 
		/* generation d'address */
		if((to = gethostbyname(SERVERIP))==NULL) {
			err_ret = errno;
			fprintf(stderr, "gethostbyname() error...\n");
			return err_ret;
		}
		 
		/* ouvre un socket */
		if((newfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			err_ret = errno;
			fprintf(stderr, "socket() error...\n");
			return err_ret;
		}
		 
		/* dÃ©finir les valeurs initiales */
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(SERVERPORT);
		serv_addr.sin_addr = *((struct in_addr *)to->h_addr);
		memset(&(serv_addr.sin_zero), 0, 8);
		 
		/* essayez de connecter avec le serveur*/
		if(connect(newfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
			err_ret = errno;
			fprintf(stderr, "connect() error...\n");
			return err_ret;
		}
		else {
			printf("Connected to server at %s:%d\n", SERVERIP, SERVERPORT);
			return newfd;
		}
	}// fin connect_with_server
	 
	void logout(struct USER *me) {
		int sent;
		struct PACKET packet;
		if(!isconnected) {
			fprintf(stderr, "You are not connected...\n");
			return;
		}
		memset(&packet, 0, sizeof(struct PACKET));
		strcpy(packet.option, "exit");
		strcpy(packet.alias, me->alias);
		/* send request to close this connetion */
		sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
		isconnected = 0;
	}// fin void logout
	 
	void setalias(struct USER *me) {
		int sent;
		struct PACKET packet;
		if(!isconnected) {
			fprintf(stderr, "You are not connected...\n");
			return;
		}
		memset(&packet, 0, sizeof(struct PACKET));
		strcpy(packet.option, "alias");
		strcpy(packet.alias, me->alias);
		/* send request to close this connetion */
		sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
	}// fin void setalias
	 
	void *receiver(void *param) {
		int recvd;
		struct PACKET packet;
		printf("Waiting here [%d]...\n", sockfd);
		while(isconnected) {
			recvd = recv(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
			if(!recvd) {
				fprintf(stderr, "Connection lost from server...\n");
				isconnected = 0;
				close(sockfd);
				break;
			}
			if(recvd > 0) {
				printf("[%s]: %s\n", packet.alias, packet.buff);
			}
			memset(&packet, 0, sizeof(struct PACKET));
		}// fin while
		return NULL;
	}// fin void *receiver
	 
	void sendtoall(struct USER *me, char *msg) {
		int sent;
		struct PACKET packet;
		if(!isconnected) {
			fprintf(stderr, "You are not connected...\n");
			return;
		}
		msg[BUFFSIZE] = 0;
		memset(&packet, 0, sizeof(struct PACKET));
		strcpy(packet.option, "send");
		strcpy(packet.alias, me->alias);
		strcpy(packet.buff, msg);
		/* envoyez une demande Ã  fermer cette connexion */
		sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
	}// fin void sendtoall
	 
	void sendtoalias(struct USER *me, char *target, char *msg) {
		int sent, targetlen;
		struct PACKET packet;
		if(target == NULL) {
			return;
		}
		if(msg == NULL) {
			return;
		}
		if(!isconnected) {
			fprintf(stderr, "You are not connected...\n");
			return;
		}
		msg[BUFFSIZE] = 0;
		targetlen = strlen(target);
		memset(&packet, 0, sizeof(struct PACKET));
		strcpy(packet.option, "whisp");
		strcpy(packet.alias, me->alias);
		strcpy(packet.buff, target);
		strcpy(&packet.buff[targetlen], " ");
		strcpy(&packet.buff[targetlen+1], msg);
		/* envoyez une demande Ã  fermer cette connexion */
		sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
	}
