#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "utils.h"

int deal_with_parameters(int* port, char* root_dir, int argc, char** argv){
    if(argc == 1)
        return 0;
    
    if(argc != 3 && argc != 5)
        return -1;
    
    if(argc == 3){
        if(strcmp(argv[1], "-port") == 0)
            *port = atoi(argv[2]);
        else if(strcmp(argv[2], "-root") == 0)
            strcpy(root_dir, argv[2]);
    }

    if(argc == 5){
        for(int i = 0; i < 2; ++i)
            if(strcmp(argv[i * 2 + 1], "-port") == 0)
                *port = atoi(argv[i * 2 + 2]);
            else if(strcmp(argv[i * 2 + 1], "-root") == 0)
                strcpy(root_dir, argv[i * 2 + 2]);
    }

    return 0;
}

int send_message(struct Client* c){
    write(c->sock, c->message, strlen(c->message));
    return 0;
}

int over_connections(int fd){
    char *message = "421 sorry, but the server is busy, please try again later.\r\n";
    write(fd, message, strlen(message));
    return 0;
}

int processing_command(char* s, char* cm, char* ms){
    sscanf(s, "%s %s",cm ,ms);
    return 0;
}

void *transfer_file(void* cv){
    
    struct Client* c = cv;

    char buffer[8192];
    char file_path[200];
    strcpy(file_path, c->root_dir);
    strcat(file_path, c->dir);
    strcat(file_path, c->filename);
    c->filefd = open(file_path,  O_RDONLY);

    int filefd = c->filefd;
    int sockfd = c->sockfd;
    int skip_bytes = c->skip_bytes;
    int socklfd = c->socklfd;
    c->skip_bytes = 0;

    int times = skip_bytes / sizeof(buffer);
    while(times--)
        read(filefd, buffer, sizeof(buffer));
    read(filefd, buffer, skip_bytes % sizeof(buffer));

    while (1) {
        int bytes_read = read(filefd, buffer, sizeof(buffer));

        if (bytes_read == 0)
            break;

        if (bytes_read < 0) {
            return NULL;
        }

        void *p = buffer;
        while (bytes_read > 0) {
            int bytes_written = write(sockfd, p, bytes_read);
            if (bytes_written <= 0) {
                return NULL;
            }
            bytes_read -= bytes_written;
            p += bytes_written;
        }
    }
    c->message = "226 Transfer complete.\r\n";
    send_message(c);
    close(sockfd);
    close(socklfd);
    c->mode = 0;
    return NULL;
}

void *store_file(void *cv){
    struct Client* c = cv;

    char file_path[200];
    strcpy(file_path, c->root_dir);
    strcat(file_path, c->dir);
    strcat(file_path, c->filename);
    int skip_bytes = c->skip_bytes;
    int sockfd = c->sockfd;
    int socklfd = c->socklfd;
    c->skip_bytes = 0;

    int fp;
    if(skip_bytes == 0)
        fp = open(file_path, O_WRONLY | O_CREAT , S_IRWXG | S_IRWXO | S_IRWXU);
    else
        fp = open(file_path, O_WRONLY | O_APPEND, S_IRWXG | S_IRWXO | S_IRWXU);

    char buffer[8192];
    int p = 0;

    while (1) {
        int bytes_read = read(sockfd, buffer + p, 8191 - p);
        if (bytes_read < 0) {
            close(sockfd);
            break;
        } else if (bytes_read == 0) {
            break;
        }
        int file_state = write(fp, buffer, bytes_read);
        if(file_state == 0){
            break;
        }
    }
    close(fp);

    c->message = "226 Transfer complete.\r\n";
    send_message(c);
    close(sockfd);
    close(socklfd);
    c->mode = 0;
    return NULL;
}

void *transfer_list(void *cv){
    struct Client* c = cv;
    char com[400];
    sprintf(com, "ls -l %s%s", c->root_dir, c->dir);
    FILE *fs = popen(com, "r");
    char buffer[8192];
    int sockfd = c->sockfd;
    int socklfd = c->socklfd;

    while(fgets(buffer, 8191, fs)){
        int p = 0;
        int len = strlen(buffer);
        while (p < len) {
            int n = write(sockfd, buffer + p, len - p);
            if (n < 0) {
                close(sockfd);
                return NULL;
            } else {
                p += n;
            }
        }
    }

    c->message = "226 Transfer complete.\r\n";
    send_message(c);
    close(sockfd);
    close(socklfd);
    c->mode = 0;
    return NULL;
}

int get_random_port(){
    //srand((unsigned)(time(NULL)));
    return rand() % (65536 - 20000) + 20000;
}

int get_local_ip(int sock, int *h1, int* h2, int* h3, int* h4){
    socklen_t addr_size = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;
    getsockname(sock, (struct sockaddr *)&addr, &addr_size);
 
    char* host = inet_ntoa(addr.sin_addr);
    sscanf(host,"%d.%d.%d.%d", h1, h2, h3, h4);
    return 0;
}

int checkuserinfo(char* username, char* password){
    FILE* fp = fopen("userinfo.txt", "r");
    char user[200], pass[200];
    while(fscanf(fp, "%s\n", user) != EOF){
        fscanf(fp, "%s\n", pass);
        if(strcmp(user, username) == 0 && strcmp(pass, password) == 0)
            return 0;
    }
    return -1;
}

void resolvepath(char* rootpath, char* currentpath, char* result){
    int len = strlen(rootpath);
    if(len >= strlen(currentpath)){
        result[0] = '/';
        result[1] = '\0';
        return;
    }
    int legal = 1;
    for(int i = 0; i < len; ++i)
        if(rootpath[i] != currentpath[i]){
            legal = 0;
            break;
        }
    if(legal){
        for(int i = len; i < strlen(currentpath); ++i)
            result[i - len] = currentpath[i];
        result[strlen(currentpath) - len] = '/';
        result[strlen(currentpath) - len + 1] = '\0';
    }else{
        result[0] = '/';
        result[1] = '\0';
    }
}
