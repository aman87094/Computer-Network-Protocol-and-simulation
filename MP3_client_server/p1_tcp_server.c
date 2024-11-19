#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <dirent.h>

#define BUFFER_SIZE 1024

// Structure to hold client information
struct client_info {
    int socket;
    struct sockaddr_in address;
};

// Global variables
int max_streams;
char *root_directory;

// Function to handle client requests
void *handle_client(void *arg) {
    struct client_info *client = (struct client_info *)arg;
    char buffer[BUFFER_SIZE];
    char song_name[BUFFER_SIZE];
    FILE *song_file;

    // Receive song number from the client
    recv(client->socket, buffer, BUFFER_SIZE, 0);
    int song_number = atoi(buffer);
    sprintf(song_name, "%s/song%d.mp3", root_directory, song_number);

    // Display client's IP address and requested song name
    printf("Client IP: %s, Requested Song: %s\n", inet_ntoa(client->address.sin_addr), song_name);

    // Open the requested song file
    char str="NO such file\n";
    song_file = fopen(song_name, "rb");
    if (song_file == NULL) {
        perror("No such song\n");
        send(client->socket,str,strlen(str),0);
        pthread_exit(NULL);
    }

    // Send the song data to the client
    while (!feof(song_file)) {
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), song_file);
        send(client->socket, buffer, bytes_read, 0);
    }

    // Close the song file and client socket
    fclose(song_file);
    close(client->socket);
    free(client);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // Check for correct number of command line arguments
    printf("%d\n",argc);
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <root_directory> <max_streams>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    // Parse command line arguments
    
    int port = atoi(argv[1]);
    root_directory = argv[2];
    max_streams = atoi(argv[3]);

    // Create socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, max_streams) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    // Accept and handle incoming connections
    struct sockaddr_in client_addr;
    int client_socket;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t tid;

    while (1) {
        // Accept incoming connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        // Create client_info structure to pass to thread
        struct client_info *client = (struct client_info *)malloc(sizeof(struct client_info));
        if (client == NULL) {
            perror("Error allocating memory for client info");
            close(client_socket);
            continue;
        }
        client->socket = client_socket;
        client->address = client_addr;

        // Create new thread to handle client request
        if (pthread_create(&tid, NULL, handle_client, (void *)client) != 0) {
            perror("Error creating thread");
            close(client_socket);
            free(client);
            continue;
        }

        // Detach thread
        pthread_detach(tid);
    }

    // Close server socket
    close(server_socket);

    return 0;
}
