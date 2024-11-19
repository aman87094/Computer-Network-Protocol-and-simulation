#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ctype.h>

#define BUFFER_SIZE 1024
#define DEFAULT_PAGE "http_resources/index.html"
#define NOT_FOUND_PAGE "404.html"
char root_dir[1024] = "";

void *connection_handler(void *socket_desc);

void handle_get_request(int client_sock, char *path);

void handle_post_request(int client_sock, char *path, char *post_data);

void send_response(int client_sock, int status, char *content_type, char *content, int keep_alive);

const char *get_content_type(const char *path);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s <port> <root_directory>\n", argv[0]);
        return 1;
    }

    int server_sock, client_sock, port;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char *root_directory = argv[2];
    port = atoi(argv[1]);
    // printf("%s",root_directory);
    strcat(root_dir, root_directory);
    printf("\n%s\n", root_dir);
    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("Error creating socket");
        return 1;
    }

    // Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Error binding");
        return 1;
    }

    // Listen
    if (listen(server_sock, 10) < 0)
    {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);

    // Accept incoming connections
    client_len = sizeof(struct sockaddr_in);

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len)))
    {
        pthread_t thread_id;
        int *new_sock = malloc(1);
        *new_sock = client_sock;

        if (pthread_create(&thread_id, NULL, connection_handler, (void *)new_sock) < 0)
        {
            perror("Error creating thread");
            return 1;
        }

        pthread_detach(thread_id);
        
    }

    if (client_sock < 0)
    {
        perror("Error accepting connection");
        return 1;
    }

    return 0;
}

void *connection_handler(void *socket_desc)
{
    int client_sock = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    char request_type[5];
    char request_path[BUFFER_SIZE];
    char complete_file_path[BUFFER_SIZE];
    int keep_alive = 1;

    recv(client_sock, buffer, BUFFER_SIZE, 0);
    sscanf(buffer, "%s %s", request_type, request_path);
    // printf("%s",buffer);

    if (strstr(buffer, "Connection: keep-alive") != NULL)
    {
        keep_alive = 0;
    }

    if (strcmp(request_type, "GET") == 0)
    {

        if (strcmp(request_path, "/") == 0)
        {
            strcpy(complete_file_path, DEFAULT_PAGE);
            printf("\ncomp path : %s\n", complete_file_path);
        }
        else
        {
            strcpy(complete_file_path, root_dir);
            strcat(complete_file_path, request_path);
        }
        char *content_type = get_content_type(complete_file_path);

        char full_path[BUFFER_SIZE];
        FILE *file = fopen(complete_file_path, "rb");
        if (file)
        {
            //////////////////////////////////////////////////////////////////////////
            char header[BUFFER_SIZE];
            sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", content_type);
            if (keep_alive)
            {
                strcat(header, "Connection: keep-alive\r\n");
            }
            write(client_sock, header, strlen(header));

            char buf[BUFFER_SIZE];
            int bytes_read;
            while ((bytes_read = fread(buf, 1, BUFFER_SIZE, file)) > 0)
            {
                write(client_sock, buf, bytes_read);
            }

            fclose(file);
            
            // /////////////////////////////////////////////
        }
        else
        {
            printf("\nin else part\n");

            FILE *file1 = fopen("http_resources/404.html", "rb");
            if(file1){
                printf("\nfound file 1\n");
            }
            char header[BUFFER_SIZE];
            sprintf(header, "HTTP/1.1 404 ERROR\r\nContent-Type: %s\r\n\r\n", content_type);
            if (keep_alive)
            {
                strcat(header, "Connection: keep-alive\r\n");
            }
            write(client_sock, header, strlen(header));

            char buf[BUFFER_SIZE];
            int bytes_read;
            while ((bytes_read = fread(buf, 1, BUFFER_SIZE, file1)) > 0)
            {
                write(client_sock, buf, bytes_read);
            }
            printf("\n%s\n",header);

            fclose(file1);
        }
    }
    else if (strcmp(request_type, "POST") == 0)
    {
        handle_post_request(client_sock,root_dir, buffer);
    }

    if (!keep_alive)
    {
        free(socket_desc);
        close(client_sock);
    }
    return NULL;
}

void handle_post_request(int client_sock, char *path, char *post_data)
{
    // Extracting POST data
    char *data_start = strstr(post_data, "\r\n\r\n");
    if (data_start == NULL)
    {
        send_response(client_sock, 400, "text/html", "Bad Request", 0);
        return;
    }
    data_start += 4; // Move past "\r\n\r\n"
    printf("POst data : %s",data_start);
    int l = strlen(data_start);
    int start=-1,end=-1;
    for(int i=0; i<l-4; i++){
        if(data_start[i]=='%' && data_start[i+1]=='*' && data_start[i+2]=='*' && data_start[i+3]=='%'){
            if(start==-1) start=i+4;
            else end=i-1;
        }
    }
    // // Counting characters, words, and sentences
    int char_count = end-start+1;
    int word_count = 1;
    int sentence_count = 0;
    // int i;
    // printf("%s\n",data_start);
    for (int i = start; i<=end; i++)
    {
        if(data_start[i] == '.' || data_start[i] == '?' || data_start[i] == '!') sentence_count++;
        if(data_start[i]==' ') word_count++;
    }
    printf(" hello  %d  %d   %d ",char_count,word_count,sentence_count);
    // if (i > 0)
    // {
    //     if (!isspace(data_start[i - 1]))
    //     {
    //         word_count++;
    //         sentence_count++; // Count last sentence if not ending with punctuation
    //     }
    // }

    // Generating response
    char response[BUFFER_SIZE];
    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                      "<html><body>"
                      "<h1>POST Request Received!</h1>"
                      "<p>Number of characters: %d</p>"
                      "<p>Number of words: %d</p>"
                      "<p>Number of sentences: %d</p>"
                      "</body></html>",
            char_count, word_count, sentence_count);

    send(client_sock, response, strlen(response), 0);
}

void send_response(int client_sock, int status, char *content_type, char *content, int keep_alive)
{
    int cont_len = strlen(content);
    printf("content len : %d\n", cont_len);
    char response[cont_len + 1024];
    int res_len = cont_len + 1024;
    // sprintf(response, "HTTP/1.1 %d\r\nContent-Type: %s\r\n", status,content_type);
    sprintf(response, "HTTP/1.1 %d\r\n"
                      "Content-Type: %s\r\n"
                      // Include Content-Length header
                      "Content-Encoding: identity\r\n" // Include Content-Encoding header (identity for no encoding)
            ,
            status, content_type);
    if (keep_alive)
    {
        strcat(response, "Connection: keep-alive\r\n");
    }

    strcat(response, "\r\n");
    strcat(response, content);

    send(client_sock, response, strlen(response), 0);
}

// Function to determine content type based on file extension
const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.'); // Find the last occurrence of '.'
    if (!ext)
    {
        return "application/octet-stream"; // Default to binary if extension not found
    }
    ext++; // Move past the dot
    if (strcmp(ext, "html") == 0)
    {
        return "text/html";
    }
    else if (strcmp(ext, "css") == 0)
    {
        return "text/css";
    }
    else if (strcmp(ext, "jpeg") == 0 || strcmp(ext, "jpg") == 0)
    {
        return "image/jpeg";
    }
    else if (strcmp(ext, "png") == 0)
    {
        return "image/png";
    }
    else if (strcmp(ext, "gif") == 0)
    {
        return "image/gif";
    }
    else
    {
        return "application/octet-stream"; // Default to binary if extension not recognized
    }
}