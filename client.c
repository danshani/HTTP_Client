#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEBUG_MODE 0
#define BUFFER_SIZE 1024
#define INITIAL_BUFFER_SIZE 1024
#define USAGE_MESSAGE "Usage: client [-r n <pr1=value1 pr2=value2 ...>] <URL>\n"

void handle_error(const char *message);
void parse_url(const char *url, char *host, int *port, char *path);
void http_request(char *request, const char *host, const char *path, const char *params);
void check_parameters(int param_count, char **params);
void save_image_file(const unsigned char *data, size_t size, const char *prefix);

int main(int argc, char *argv[]) {
    char *url = NULL;
    int param_count = 0;
    char params[BUFFER_SIZE] = {0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            if (i++ >= argc){
                printf(USAGE_MESSAGE);
                exit(EXIT_FAILURE);
            }
            param_count = atoi(argv[i]);
            if (param_count <= 0){
                printf(USAGE_MESSAGE);
                exit(EXIT_FAILURE);
            }
            if (argc - i - 1 < param_count) {
                fprintf(stderr, "Too few parameters provided for -r\n");
                printf(USAGE_MESSAGE);
                exit(EXIT_FAILURE);
            }
            check_parameters(param_count, &argv[i + 1]);
            for (int j = 0; j < param_count; j++) {
                strcat(params, j == 0 ? "?" : "&");
                strcat(params, argv[++i]);
            }
        } else {
            url = argv[i];
        }
    }

    if (!url || strncmp(url, "http://", 7) != 0 || argc - param_count - 3 > 1) {
        printf(USAGE_MESSAGE);
        exit(EXIT_FAILURE);
    }

    start_over:
    char host[BUFFER_SIZE] = {0};
    char path[BUFFER_SIZE] = "/";
    int port;
    parse_url(url, host, &port, path);

    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *server;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("socket");
    }
    if ((server = gethostbyname(host)) == NULL){
        herror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        handle_error("connect");

    char request[BUFFER_SIZE];
    http_request(request, host, path, params);
    printf("HTTP request =\n%s\nLEN = %ld\n", request, strlen(request));

    if (send(sockfd, request, strlen(request), 0) < 0)
        handle_error("send");

    unsigned char response[BUFFER_SIZE];
    unsigned char* full_response = NULL;
    size_t total_response_size = 0;
    size_t allocated_size = INITIAL_BUFFER_SIZE;
    ssize_t bytes_received;

    full_response = malloc(allocated_size);
    if (!full_response) {
        handle_error("Initial memory allocation failed");
    }

    while ((bytes_received = recv(sockfd, response, BUFFER_SIZE - 1, 0)) > 0) {
        if (total_response_size + bytes_received >= allocated_size) {
            allocated_size *= 2;
            unsigned char* temp = realloc(full_response, allocated_size);
            if (!temp) {
                free(full_response);
                handle_error("Memory reallocation failed");
            }
            full_response = temp;
        }
        if (full_response) {
            memcpy(full_response + total_response_size, response, bytes_received);
        }
        total_response_size += bytes_received;
    }
    if(full_response) {
        full_response[total_response_size] = '\0';
    }
    printf("Response Content:\n");
    for (size_t i = 0; i < total_response_size; i++) {
        printf("%c", full_response[i]); // Print each character
    }
    printf("\n");

    if (bytes_received < 0) handle_error("recv");

    if(DEBUG_MODE == 1){
        save_image_file(full_response, total_response_size, "downloaded_image");
    }

    if (strstr((char*)full_response, "HTTP/1.1 3") != NULL) {
        char *location_header = strstr((char*)full_response, "Location: ");
        if (location_header != NULL) {
            location_header += strlen("Location: ");
            char *location_end = strstr(location_header, "\r\n");
            if (location_end != NULL) {
                *location_end = '\0';

                // Safely copy the new URL
                char new_url[BUFFER_SIZE];
                strncpy(new_url, location_header, BUFFER_SIZE - 1);
                new_url[BUFFER_SIZE - 1] = '\0';

                printf("\nRedirecting to: %s\n", new_url);

                // Update URL safely
                url = new_url;
                memset(params, 0, BUFFER_SIZE);

                free(full_response);
                close(sockfd);
                goto start_over;
            }
        }
    }
    printf("\nTotal received response bytes: %zd\n", total_response_size);

    free(full_response);
    close(sockfd);
    return EXIT_SUCCESS;
}

// handle error and exit
void handle_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}
// parse the URL into host, port, and path
void parse_url(const char *url, char *host, int *port, char *path) {
    const char *url_start = url + 7; // Skip "http://"
    const char *port_start = strchr(url_start, ':');
    const char *path_start = strchr(url_start, '/');

    // checks if port is specified before path and if it is a valid port number (1 - 2^16)
    if (port_start && (!path_start || port_start < path_start)) {
        // Port is specified
        strncpy(host, url_start, port_start - url_start);
        host[port_start - url_start] = '\0';
        sscanf(port_start + 1, "%d", port);
        if (*port <= 0 || *port >= 65536) {
            fprintf(stderr, "Invalid port number\n");
            printf(USAGE_MESSAGE);
            exit(EXIT_FAILURE);
        }
        if (path_start) {
            strcpy(path, path_start);
        } else {
            strcpy(path, "/");
        }
    } else {
        // otherwise, port is set to 80 as default.
        *port = 80;
        if (path_start) {
            strncpy(host, url_start, path_start - url_start);
            host[path_start - url_start] = '\0';
            strcpy(path, path_start);
        } else {
            strcpy(host, url_start);
            strcpy(path, "/");
        }
    }
}
// construct the HTTP GET request message to be sent to the server
void http_request(char *request, const char *host, const char *path, const char *params) {
    snprintf(request, BUFFER_SIZE,"GET %s%s HTTP/1.1\r\n""Host: %s\r\n""Connection: close\r\n\r\n",path, params ? params : "", host);
}
// check if the parameters are in the correct format (name=value)
void check_parameters(int param_count, char **params) {
    for (int i = 0; i < param_count; i++) {
        if (!strchr(params[i], '=')) {
            fprintf(stderr, "Parameter %d (%s) is not in name=value format\n", i + 1, params[i]);
            printf(USAGE_MESSAGE);
            exit(EXIT_FAILURE);
        }
    }
}
// if DEBUG_MODE is enabled, save the image file to the current directory
void save_image_file(const unsigned char* data, size_t size, const char* prefix) {
    // Find the start of the actual image data (after HTTP headers)
    const char* image_start = strstr((const char*)data, "\r\n\r\n");
    if (image_start == NULL) {
        fprintf(stderr, "Could not find image data start\n");
        return;
    }

    // Skip the header delimiter
    image_start += 4;
    size_t image_size = size - (image_start - (const char*)data);

    static int file_counter = 0;
    char filename[256];
    snprintf(filename, sizeof(filename), "%s_%d.jpg", prefix, file_counter++);

    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Failed to open image file");
        return;
    }

    size_t written = fwrite(image_start, 1, image_size, file);
    fclose(file);

    if (written != image_size) {
        fprintf(stderr, "Warning: Incomplete write to %s\n", filename);
    } else {
        printf("Saved image file: %s (Size: %zd bytes)\n", filename, image_size);
    }
}