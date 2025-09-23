#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <asm-generic/socket.h>
#include <pthread.h>
#include <arpa/inet.h>


typedef struct{
    char name[16];
    int sockfd, connectfd; // file descriptors for socket and for accepted socket
    struct sockaddr_in server, client; // structs for server (to initialize socket) and client
    char client_ip[INET_ADDRSTRLEN];
    int clientPort;
    pthread_mutex_t lock;
    int buffer_size;
    char *buffer;
} newsock;

void ListActiveListeners(newsock *socklist, int sockCnt);
void CreateListener(newsock *socklist, int *sockCnt, int *capacity, int EditPort, int EditIndex);
void DeleteListener(newsock *socklist, int *sockCnt);
void* listenerThread(void* sock);
void EditListener(newsock *socklist, int *sockCnt, int *capacity);


int main(void){

    int capacity = 32; // initial max number of listeners
    int sockCnt = 0;

    newsock *socklist = calloc(capacity, sizeof(newsock));
    int userInp;
    int port;
    char *ip;
    
    while(1){
        input:
        printf("Welcome to listener:\n");
        printf("Enter one of the following options\n");
        printf("1 to list active listeners\n");
        printf("2 to edit existing listener (This will end listener activity and let you start another)\n");
        printf("3 to create new listener\n");
        printf("4 to read messages from a listener\n");
        printf("5 to delete a listener\n");
        printf("9 to terminate process\n");
        scanf("%d", &userInp);
        getchar(); // consume leftover newline from previous input


        if(userInp < 1 || (userInp > 5 && userInp != 9)){
            printf("Invalid option. Please try again.\n");
            goto input; // loop back to input prompt
        }

        switch(userInp){
            case 1:
                // list active listeners

                ListActiveListeners(socklist, sockCnt);
            
                break;
            case 2:
                // edit existing listener
                EditListener(socklist, &sockCnt, &capacity);
                // take inp to edit which one
                break;
            case 3:
                // create new listener
                CreateListener(socklist, &sockCnt, &capacity, 0, -1);
                break;
            case 4:
                // use a listener
                break;

            case 5:
                //list listeners and take inp to delete which one
                DeleteListener(socklist, &sockCnt);

                break;

            case 9:
                // close all listeners and exit process
                return 0;
            default:
        }
    }


}    


void CreateListener(newsock *socklist, int *sockCnt, int *capacity, int EditPort, int EditIndex) {

    newsock newSock;

    if(EditPort){
        close(socklist[EditIndex].sockfd);
        socklist[EditIndex].sockfd = -1; // Mark as inactive
        newSock = socklist[EditIndex];
        goto PORT_CHANGE;
    }
    if (*sockCnt >= *capacity) {
        *capacity *= 2;
        socklist = realloc(socklist, (*capacity) * sizeof(newsock));
        if (!socklist) {
            perror("Failed to allocate memory for new listener");
            exit(EXIT_FAILURE);
        }
    }
    newSock.buffer_size = 1024;
    newSock.buffer = malloc(newSock.buffer_size);
        if (!newSock.buffer) {
        perror("Failed to allocate memory for buffer");
        exit(EXIT_FAILURE);
    }

    printf("Enter name for new listener (max 15 characters): ");
    if(fgets(newSock.name, 15, stdin)){
        size_t len = strlen(newSock.name);
        if (len > 0 && newSock.name[len - 1] == '\n') {
            newSock.name[len - 1] = '\0'; // Remove newline character
        }
    } else {
        printf("Failed to read listener name.\n");
        return;
    }

    PORT_CHANGE:
    int port;
    printf("\033[34mEnter port number for new listener: \033[0m");
    if (scanf(" %d", &port) != 1 || port <= 0 || port > 65535) {
        getchar();
        printf("Invalid port number. Please enter a valid port (1-65535).\nEntered %d\n", port);
        return;
    }
    getchar();


    newSock.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (newSock.sockfd < 0) {
        perror("Socket creation failed\n");
        return;
    }

    newSock.server.sin_family = AF_INET;
    newSock.server.sin_port = htons(port);
    newSock.server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int flag = 1;
     if(setsockopt(newSock.sockfd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) == -1){
         printf("Setsockopt SO_REUSEPORT failed, errno is %d \n", errno);
         return;
     }

    if (bind(newSock.sockfd, (struct sockaddr*)&newSock.server, sizeof(newSock.server)) < 0) {
        perror("Bind failed");
        close(newSock.sockfd);
        return;
    }

    if (listen(newSock.sockfd, 1) < 0) {
        perror("Listen failed");
        close(newSock.sockfd);
        return;
        
    }

    if(EditPort !=1){
        socklist[*sockCnt] = newSock;
        (*sockCnt)++;
    }
    else{
        socklist[EditIndex] = newSock;
    }

    pthread_t thread_id;

    pthread_create(&thread_id, NULL, listenerThread, &newSock);
    printf("\033[32mListener %s created on port \033[0m%d\n", newSock.name, port);
}


void* listenerThread(void* sock) {

    newsock *listener = (newsock*)sock;

    ssize_t n = 0;
    int total_len = 0;
    while (1) {
        pthread_mutex_lock(&listener->lock);

        if(listener->connectfd = accept(listener->sockfd, (struct sockaddr*)&listener->client, NULL) >= 0){

            inet_ntop(AF_INET, &listener->client.sin_addr, listener->client_ip, sizeof(listener->client_ip));
            listener->clientPort = ntohs(listener->client.sin_port);

            printf("Accepted connection on listener %s\n", listener->name);
            printf("Client IP: %s, Client Port: %d\n", listener->client_ip, listener->clientPort);
        }
        if(listener->connectfd < 0){
            perror("Server accept failed");
            pthread_mutex_unlock(&listener->lock);
            continue;
        }
        

        n = read(listener->connectfd, listener->buffer + total_len, listener->buffer_size - total_len - 2);
        if(n > 0){
            total_len += n;


            if(total_len >= listener->buffer_size - 2) {
                listener->buffer = realloc(listener->buffer, listener->buffer_size * 2);
                listener->buffer_size *= 2;
                if (!listener->buffer) {
                    perror("Failed to allocate memory for buffer");
                    pthread_mutex_unlock(&listener->lock);
                    exit(EXIT_FAILURE);
                }
            }
            printf("Received data: %s\n", listener->buffer);
            listener->buffer[n] = '\n'; // new line
            listener->buffer[n + 1] = '\0'; // null terminator
        }
        pthread_mutex_unlock(&listener->lock);
        sleep(1); // to let parent grab access to listener if needed

    }
    return NULL;
}

void ListActiveListeners(newsock *socklist, int sockCnt) {
    printf("Active listeners:\n\n");
    for (int i = 0; i < sockCnt; i++) {
        if (socklist[i].sockfd != -1) {
            printf("%d: Listener \"%s\": Socket FD = %d, Port = %d\n\n", i+1, socklist[i].name, socklist[i].sockfd, ntohs(socklist[i].server.sin_port));
        } else {
            printf("Listener %d: Inactive\n", i + 1);
        }
    }
}

void DeleteListener(newsock *socklist, int *sockCnt) {
    int delIndex;

    ListActiveListeners(socklist, *sockCnt);

    printf("Enter the index of the listener to delete (1 to %d): ", *sockCnt);


    if (scanf("%d", &delIndex) != 1 || delIndex < 1 || delIndex > *sockCnt) {
        getchar();
        printf("Invalid index. Please try again.\n\n");
        return;
    }
    getchar(); // consume leftover newline

    delIndex--; // Convert to 0-based index

    close(socklist[delIndex].sockfd);
    socklist[delIndex].sockfd = -1; // Mark as inactive

    // Optional: Shift remaining listeners down
    for (int i = delIndex; i < *sockCnt - 1; i++) {
        socklist[i] = socklist[i + 1];
    }
    (*sockCnt)--;

    printf("\033[31mListener deleted successfully.\033[0m\n\n");
}

void EditListener(newsock *socklist, int *sockCnt, int *capacity){

    int inp;
    ListActiveListeners(socklist, *sockCnt);

    printf("Enter number of listener you want to edit\n");
    scanf("%d", &inp);
    getchar();
    if(inp < 1 || inp > *sockCnt){
        printf("Invalid index. Returning to main menu.\n\n");
        return;
    }
    else if( socklist[inp-1].sockfd == -1){
        printf("Listener is inactive. Returning to main menu.\n\n");
        return;
    }

    int choice;
    printf("Name: %s\nPort: %d\n\n", socklist[inp-1].name, ntohs(socklist[inp-1].server.sin_port));
    printf("What do you want to edit?\n1 for name\n2 for port (Editing port WILL close socket and open a new one)\n3 to exit\n");
    scanf("%d", &choice);
    getchar();
    inp -= 1;
    switch(choice){
        case 1:
            printf("Enter new name (max 15 characters): ");
            if(fgets(socklist[inp].name, 15, stdin)){
                size_t len = strlen(socklist[inp].name);
                if (len > 0 && socklist[inp].name[len - 1] == '\n') {
                    socklist[inp].name[len - 1] = '\0'; // Remove newline character
                }
            } else {
                printf("Failed to read listener name.\n");
                return;
            }
            printf("Name changed successfully to %s\n", socklist[inp].name);
            break;

        case 2:
            printf("Enter new port number: ");
            CreateListener(socklist, sockCnt, capacity, 1, inp); // Create new listener on new port
            
            break;

        case 3:
            return;

        default:
            printf("Invalid option. Returning to main menu.\n");
            return;
    }
}

void AccessListener(newsock *socklist, int *sockCnt){
    int inp;
    ListActiveListeners(socklist, *sockCnt);

    printf("\nEnter index of listener you want to access\n");
    scanf("%d", &inp);
    getchar();

    if(inp < 0 || inp >= *sockCnt){
        printf("Invalid index. Returning to main menu.\n");
        return;
    }
    else if( socklist[inp].sockfd == -1){
        printf("Listener is inactive. Returning to main menu.\n\n");
        return;
    }

    // Now you can use socklist[inp] to access the selected listener
    // For example, you might want to read messages from it or perform other operations
    printf("Accessing listener %s on port %d\n", socklist[inp].name, ntohs(socklist[inp].server.sin_port));
    pthread_mutex_lock(&socklist[inp].lock);
    printf("Locked listener %s for exclusive access.\n", socklist[inp].name);

    while(1) {
        printf("Type: \n0 to return to main menu\n1 to see client info\n2 to read messages\n3 to send message(still not developed)\n");
        scanf("%d", &inp);
        getchar();

        inp -= 1;

        if(inp == -1){
            pthread_mutex_unlock(&socklist[inp].lock);
            break;
        }
        else if(inp == 0){
            printf("Client IP: %s, Client connected to Port: %d\n\n", socklist[inp].client_ip, socklist[inp].clientPort);
        }
        else if(inp == 1){
            printf("%s", socklist[inp].buffer); // read messages
        }
        else if(inp == 2){
            // send message
        }
        else{
            printf("Invalid option. Returning to main menu.\n");
            pthread_mutex_unlock(&socklist[inp].lock);
        
        }
    }


    pthread_mutex_unlock(&socklist[inp].lock);
    return;
}

//  LEGACY IDEAS

// void ListenFork(newsock *listener) {
//     socklen_t len = sizeof(listener->client);

//     if (listener->connectfd < 0) {
//         perror("Server accept failed");
//         return;
//     }

//     if (fork() == 0) { // Child process
//         while(1){
//             listener->connectfd = accept(listener->sockfd, (struct sockaddr*)&listener->client, &len);
//         }
//     } else { // Parent process
//         close(listener->connectfd); // Close the connected socket in parent
//     }
// }


 //     LEGACY MAIN CODE

     // int sockfd, connectfd; 
    // struct sockaddr_in server, client; 

    // sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // if(sockfd == -1){
    //     printf("Socket creation failed.../n");
    //     return -1;
    // }





    // bzero(&server, sizeof(server)); // zero out server area to reset

    // server.sin_family = AF_INET;
    // server.sin_port = htons(PORT);
    // server.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // local ip of device, idk if this is correct
    // printf("inaddr_loopback: %d\n", INADDR_LOOPBACK);

    //     if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1){
    //     printf("Setsockopt SO_REUSEADDR failed, errno is %d \n", errno);
    //     return -1;
    // }

    // if((bind(sockfd, (struct sockaddr*)&server, sizeof(server))) != 0){
    //     perror("socket bind failed");
    //     return -1;
    // }
    // printf("bind succeeded\n");

    // while(1){
    //     if((listen(sockfd, 1)) != 0){
    //         perror("listening failed");
    //         return -1;
    //     }
    //     printf("listening on port %d\n", PORT);

    //     socklen_t len = sizeof(client);
    //     connectfd = accept(sockfd, (struct sockaddr*)&client, &len);

    //     if(connectfd < 0){
    //         printf("server accept failed\n");
    //         return -1;
    //     }

    //     printf("server accepted connection to client\n");
    //     write(connectfd, "you got in :)\n", 11);
    //     close(connectfd);
    // }
    