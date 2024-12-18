#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 1024
#define USAGE_MESSAGE "Usage: client [-r n <pr1=value1 pr2=value2 ...>] <URL>\n"

void handle_error(const char *message);
void parse_url(const char *url, char *host, int *port, char *path);
void http_request(char *request, const char *host, const char *path, const char *params);
void check_parameters(int param_count, char **params);

int main(int argc, char *argv[]) {
    // parse command line arguments
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
            if (argc - i - 1 < param_count) { // Not enough parameters
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

    // validate that the correct number of parameters were provided in the command line
    if (!url || strncmp(url, "http://", 7) != 0 || argc - param_count - 3 > 1) {
        printf(USAGE_MESSAGE);
        exit(EXIT_FAILURE);
    }

    start_over:
    // parse URL
    char host[BUFFER_SIZE] = {0};
    char path[BUFFER_SIZE] = "/";
    int port;
    parse_url(url, host, &port, path);

    int sockfd;
    struct sockaddr_in server_addr;
    struct hostent *server;
    // create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        handle_error("socket");
    }
    if ((server = gethostbyname(host)) == NULL){
        herror("gethostbyname"), exit(EXIT_FAILURE);
    }

    // set up server address struct
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    // connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        handle_error("connect");

    // build and send HTTP request
    char request[BUFFER_SIZE];
    http_request(request, host, path, params);

    printf("HTTP request =\n%s\nLEN = %ld\n", request, strlen(request));

    // send request to server
    if (send(sockfd, request, strlen(request), 0) < 0) handle_error("send");

    // receive and display response
    char response[BUFFER_SIZE];
    ssize_t total_bytes = 0;
    ssize_t bytes_received;

    // receive response in chunks of data and print it to the console until the end of the response
    while ((bytes_received = recv(sockfd, response, BUFFER_SIZE - 1, 0)) > 0) {
        response[bytes_received] = '\0';
        printf("%s", response);
        total_bytes += bytes_received;
    }

    printf("\nTotal received response bytes: %zd\n", total_bytes);

    // check if there was an error receiving the response
    if (bytes_received < 0) handle_error("recv");

    // handle redirection (3XX responses)
    if (strstr(response, "HTTP/1.1 3") != NULL) {
        char *location_header = strstr(response, "Location: ");
        if (location_header != NULL) {
            location_header += strlen("Location: ");
            char *location_end = strstr(location_header, "\r\n");
            if (location_end != NULL) {
                *location_end = '\0';
                printf("\nRedirecting to: %s\n", location_header);

                // parse new url from location header
                url = location_header;
                strncpy(params, "", BUFFER_SIZE); // clear parameters for the new request

                close(sockfd); // close current socket
                goto start_over; // start a new request with the redirected URL
            }
        }
    }
    close(sockfd); // Close connection after receiving the response
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