#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>

#include "handler.h"
#include "utils.h"

// error message
char* const message_unlogin = "530 Please log in with USER and PASS first.\r\n";
char* const message_unknowncommand = "500 Syntax error, command unrecognized.\r\n";
char* const message_modeundefined = "503 Bad sequence of commands.\r\n";
char* const message_connectionerror = "426 TCP connection was established but then broken by the client or by network failure.\r\n";

int handle_command(struct Client* c, char* command, char* message){
    if(strcmp(command, "USER") == 0)
        handle_USER(c, message);        
    else if(strcmp(command, "PASS") == 0)
        handle_PASS(c, message);
    else if(strcmp(command, "SYST") == 0)
        handle_SYST(c);
    else if(strcmp(command, "TYPE") == 0)
        handle_TYPE(c, message);
    else if(strcmp(command, "QUIT") == 0)
        handle_QUIT(c);
    else if(strcmp(command, "PORT") == 0)
        handle_PORT(c, message);
    else if(strcmp(command, "RETR") == 0)
        handle_RETR(c, message);
    else if(strcmp(command, "PASV") == 0)
        handle_PASV(c);
    else if(strcmp(command, "STOR") == 0)
        handle_STOR(c, message);
    else if(strcmp(command, "CWD") == 0)
        handle_CWD(c, message);
    else if(strcmp(command, "PWD") == 0)
        handle_PWD(c);
    else if(strcmp(command, "MKD") == 0)
        handle_MKD(c, message);
    else if(strcmp(command, "LIST") == 0)
        handle_LIST(c);
    else if(strcmp(command, "RMD") == 0)
        handle_RMD(c, message);
    else if(strcmp(command, "RNFR") == 0)
        handle_RNFR(c, message);
    else if(strcmp(command, "RNTO") == 0)
        handle_RNTO(c, message);
    else{
        c->message = message_unknowncommand;
        send_message(c);
    }
    return 0;
}

int handle_USER(struct Client* c, char* username){
    if(strcmp(username, "anonymous") == 0){
        c->message = "331 Guest login ok, send your complete e-mail address as password.\r\n";
        send_message(c);
        strcpy(c->username, username);
    }
    return 0;
}

int handle_PASS(struct Client* c, char* password){
    if(strcmp(c->username, "anonymous") == 0){
        c->message = "230 login successfully.\r\n";
        send_message(c);
        strcpy(c->password, password);
        c->login = 1;
    }
    return 0;
}

int handle_SYST(struct Client* c){
    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    c->message = "215 UNIX Type: L8\r\n";
    send_message(c);
    return 0;
}

int handle_TYPE(struct Client* c, char* type){
    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    if(strcmp(type, "I") == 0){
        c->message = "200 Type set to I.\r\n";
        send_message(c);
        c->type = 1;
    }
    return 0;
}

int handle_QUIT(struct Client* c){

    c->message = "221 Goodbye!\r\n";
    send_message(c);
    close(c->sock);
    init_client(c);
    return 0;
}

int handle_PORT(struct Client* c, char* s){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    int h1, h2, h3, h4, p1, p2;
    sscanf(s, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4 ,&p1, &p2);
    memset(&(c->addr), 0, sizeof(c->addr));
    c->addr.sin_port = htons(p1 * 256 + p2);
    c->addr.sin_family = AF_INET;
    char ip[20];
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    if(inet_pton(AF_INET, ip, &(c->addr.sin_addr)) != 1){
        perror("inet_pton error");
        return 1;
    }

    if ((c->sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		perror("socket error");
		return 1;
	}
    c->mode = 1;
    c->message = "200 PORT command successful.\r\n";
    send_message(c);
    return 0;
}

int handle_RETR(struct Client* c, char* filename){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    if(c->mode == 1){

        if(connect(c->sockfd, (struct sockaddr*)&(c->addr), sizeof(c->addr)) == -1){
            perror("connect error");
            c->message = message_connectionerror;
            send_message(c);
            return 1;
        }
        struct stat st;
        stat(filename, &st);
        int size = st.st_size;

        char message[200];
        sprintf(message, "150 Opening BINARY mode data connection for %s (%d bytes).\r\n", filename, size);
        c->message = message;
        send_message(c);

        strcpy(c->filename, filename);
        pthread_t thread_id; 
        pthread_create(&thread_id, NULL, transfer_file, c);

    }else if(c->mode == 2){

        if((c->sockfd = accept(c->socklfd, NULL, NULL)) == -1){
            perror("accept error");
            return 1;
        }

        printf("PASV accepted.\n");

        struct stat st;
        stat(filename, &st);
        int size = st.st_size;

        char message[200];
        sprintf(message, "150 Opening BINARY mode data connection for %s (%d bytes).\r\n", filename, size);
        c->message = message;
        send_message(c);

        strcpy(c->filename, filename);
        pthread_t thread_id; 
        pthread_create(&thread_id, NULL, transfer_file, c);


    }else{
        c->message = message_modeundefined;
        send_message(c);
        return 1;
    }
    return 0;
}

int handle_PASV(struct Client* c){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    int rand_port = get_random_port();
    int p1 = rand_port / 256;
    int p2 = rand_port % 256;
    int h1, h2, h3, h4;
    get_local_ip(&h1, &h2, &h3, &h4);

    //sscanf(s, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4 ,&p1, &p2);
    memset(&(c->addr), 0, sizeof(c->addr));
    c->addr.sin_port = htons(rand_port);
    c->addr.sin_family = AF_INET;
    char ip[20];
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    if(inet_pton(AF_INET, ip, &(c->addr.sin_addr)) != 1){
        perror("inet_pton error");
        return 1;
    }

    if ((c->socklfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		perror("socket error");
		return 1;
	}

    if(bind(c->socklfd, (struct sockaddr*)&c->addr, sizeof(c->addr)) == -1){
        perror("bind error");
        return 1;
    }

    if(listen(c->socklfd, 1) == -1){
		perror("listen error");
        return 1;
    }

    c->mode = 2;

    char message[200];
    sprintf(message, "227 =%d,%d,%d,%d,%d,%d\r\n", h1, h2, h3, h4, p1, p2);
    c->message = message;
    send_message(c);
    return 0;
}

int handle_STOR(struct Client* c, char* dir){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    if(c->mode == 1){
        if(connect(c->sockfd, (struct sockaddr*)&(c->addr), sizeof(c->addr)) == -1){
            perror("connect error");
            c->message = message_connectionerror;
            send_message(c);
            return 1;
        }

        char message[200];
        sprintf(message, "150 Opening BINARY mode data connection for %s.\r\n", dir);
        c->message = message;
        send_message(c);

        strcpy(c->filename, dir);
        pthread_t thread_id; 
        pthread_create(&thread_id, NULL, store_file, c);

    }else if(c->mode == 2){

        if((c->sockfd = accept(c->socklfd, NULL, NULL)) == -1){
            perror("accept error");
            return 1;
        }

        printf("PASV accepted.\n");

        char message[200];
        sprintf(message, "150 Opening BINARY mode data connection for %s.\r\n", dir);
        c->message = message;
        send_message(c);

        strcpy(c->filename, dir);
        pthread_t thread_id; 
        pthread_create(&thread_id, NULL, store_file, c);

    }else{
        c->message = message_modeundefined;
        send_message(c);
        return 1;
    }
    
    return 0;
}

int handle_CWD(struct Client* c, char* dir){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    return 0;
}

int handle_PWD(struct Client* c){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    return 0;
}

int handle_MKD(struct Client* c, char* dir){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    return 0;
}

int handle_LIST(struct Client *c){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    return 0;
}

int handle_RMD(struct Client* c, char* dir){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    return 0;
}

int handle_RNFR(struct Client* c, char* dir){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    return 0;
}

int handle_RNTO(struct Client* c, char* dir){

    if(!c->login){
        c->message = message_unlogin;
        send_message(c);
        return 1;
    }

    return 0;
}
