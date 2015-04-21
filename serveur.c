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
 
#define IP "127.0.0.1"
#define PORT 8080
#define BACKLOG 10
#define CLIENTS 10
 
#define BUFFSIZE 1024
#define ALIASLEN 32
#define OPTLEN 16
 
	struct PACKET {
		// instruction
		char option[OPTLEN]; 
		// l'alias du client
		char alias[ALIASLEN]; 
		 // charge utile
		char buff[BUFFSIZE];
	};
	 
	struct THREADINFO {
		// pointeur du thread
		pthread_t thread_ID; 
		// fichier descripteur de socket
		int sockfd; 
		// l'alias du client
		char alias[ALIASLEN];
	};
	 
	struct LLNODE {
		struct THREADINFO threadinfo;
		struct LLNODE *next;
	};
	 
	struct LLIST {
		struct LLNODE *head, *tail;
		int size;
	};
	 
	int compare(struct THREADINFO *a, struct THREADINFO *b) {
		return a->sockfd - b->sockfd;
	}// fin compare
	 
	void list_init(struct LLIST *ll) {
		ll->head = ll->tail = NULL;
		ll->size = 0;
	}// fin void list_init
	 
	int list_insert(struct LLIST *ll, struct THREADINFO *thr_info) {
		if(ll->size == CLIENTS) return -1;
		if(ll->head == NULL) {
			ll->head = (struct LLNODE *)malloc(sizeof(struct LLNODE));
			ll->head->threadinfo = *thr_info;
			ll->head->next = NULL;
			ll->tail = ll->head;
		}
		else {
			ll->tail->next = (struct LLNODE *)malloc(sizeof(struct LLNODE));
			ll->tail->next->threadinfo = *thr_info;
			ll->tail->next->next = NULL;
			ll->tail = ll->tail->next;
		}
		ll->size++;
		return 0;
	}// fin list_insert
	 
	int list_delete(struct LLIST *ll, struct THREADINFO *thr_info) {
		struct LLNODE *curr, *temp;
		if(ll->head == NULL) return -1;
		if(compare(thr_info, &ll->head->threadinfo) == 0) {
			temp = ll->head;
			ll->head = ll->head->next;
			if(ll->head == NULL) ll->tail = ll->head;
			free(temp);
			ll->size--;
			return 0;
		}
		for(curr = ll->head; curr->next != NULL; curr = curr->next) {
			if(compare(thr_info, &curr->next->threadinfo) == 0) {
				temp = curr->next;
				if(temp == ll->tail) ll->tail = curr;
				curr->next = curr->next->next;
				free(temp);
				ll->size--;
				return 0;
			}
		}
		return -1;
	}// fin list_delete
	 
	void list_dump(struct LLIST *ll) {
		struct LLNODE *curr;
		struct THREADINFO *thr_info;
		printf("Connection count: %d\n", ll->size);
		for(curr = ll->head; curr != NULL; curr = curr->next) {
			thr_info = &curr->threadinfo;
			printf("[%d] %s\n", thr_info->sockfd, thr_info->alias);
		}
	}// fin list_dump
	 
	int sockfd, newfd;
	struct THREADINFO thread_info[CLIENTS];
	struct LLIST client_list;
	pthread_mutex_t clientlist_mutex;
	 
	void *io_handler(void *param);
	void *client_handler(void *fd);
	 
	int main(int argc, char **argv) {
		int err_ret, sin_size;
		struct sockaddr_in serv_addr, client_addr;
		pthread_t interrupt;
		 
		/* initialiser liste chaînée*/
		list_init(&client_list);
		 
		/* initier mutex */
		pthread_mutex_init(&clientlist_mutex, NULL);
		 
		/* ouvrir une socket */
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			err_ret = errno;
			fprintf(stderr, "socket() failed...\n");
			return err_ret;
		}
		 
		/* définir les valeurs initiales */
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(PORT);
		serv_addr.sin_addr.s_addr = inet_addr(IP);
		memset(&(serv_addr.sin_zero), 0, 8);
		 
		/* bind address avec socket */
		if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
			err_ret = errno;
			fprintf(stderr, "bind() failed...\n");
			return err_ret;
		}
		 
		/* commencer à écouter pour la connexion */
		if(listen(sockfd, BACKLOG) == -1) {
			err_ret = errno;
			fprintf(stderr, "listen() failed...\n");
			return err_ret;
		}
		 
		/*initier gestionnaire d'interruption pour contrôle IO */
		printf("Starting admin interface...\n");
		if(pthread_create(&interrupt, NULL, io_handler, NULL) != 0) {
			err_ret = errno;
			fprintf(stderr, "pthread_create() failed...\n");
			return err_ret;
		}
		 
		/* garder accepter des connexions */
		printf("Starting socket listener...\n");
		while(1) {
			sin_size = sizeof(struct sockaddr_in);
			if((newfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&sin_size)) == -1) {
				err_ret = errno;
				fprintf(stderr, "accept() failed...\n");
				return err_ret;
			}
			else {
				if(client_list.size == CLIENTS) {
					fprintf(stderr, "Connection full, request rejected...\n");
					continue;
				}
				printf("Connection requested received...\n");
				struct THREADINFO threadinfo;
				threadinfo.sockfd = newfd;
				strcpy(threadinfo.alias, "Anonymous");
				pthread_mutex_lock(&clientlist_mutex);
				list_insert(&client_list, &threadinfo);
				pthread_mutex_unlock(&clientlist_mutex);
				pthread_create(&threadinfo.thread_ID, NULL, client_handler, (void *)&threadinfo);
			}
		}// fin while
		 
		return 0;
	}// fin main
	 
	void *io_handler(void *param) {
		char option[OPTLEN];
		while(scanf("%s", option)==1) {
			if(!strcmp(option, "exit")) {
				/* vider */
				printf("Terminating server...\n");
				pthread_mutex_destroy(&clientlist_mutex);
				close(sockfd);
				exit(0);
			}
			else if(!strcmp(option, "list")) {
				pthread_mutex_lock(&clientlist_mutex);
				list_dump(&client_list);
				pthread_mutex_unlock(&clientlist_mutex);
			}
			else {
				fprintf(stderr, "Unknown command: %s...\n", option);
			}
		}// fin while
		return NULL;
	}// fin void *io_handler
	 
