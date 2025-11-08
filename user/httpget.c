#include "kernel/types.h"
#include "user/user.h"
#include "kernel/net/socket.h"

#define HTTP_PORT 80
#define BUFFER_SIZE 1024

int ip_addr[4] = {0, 0, 0, 0};

void ip_str_to_addr(const char *ip_str, int *ip_arr) {
    int i = 0;
    for (; *ip_str; ip_str++) {
        if (*ip_str >= '0' && *ip_str <= '9') {
            int val = 0;
            while (*ip_str >= '0' && *ip_str <= '9') {
                val = val * 10 + (*ip_str - '0');
                ip_str++;
            }
            ip_arr[i++] = val;
            if (*ip_str == '.') {
                continue;
            } else if (*ip_str == '\0') {
                break;
            } else {
                // Invalid character
                return;
            }
        }
    }
}

// Helper function to convert IP string to binary format
// For simplicity, we'll hardcode the IP: 5.9.243.187
static uint32_t
ip_to_addr(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
    return (a) | (b << 8) | (c << 16) | (d << 24);
}

int
main(int argc, char *argv[])
{
    int soc, ret;
    struct sockaddr_in server;
    static char response[BUFFER_SIZE]; // Make static to avoid stack overflow
    char *host;
    char *path;
    unsigned char *addr;
    int total_received = 0;
    int req_len;

    // Default values
    host = "192.168.1.59"; // Hardcoded server IP
    path = "/";

    // Parse command line arguments if provided
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        path = argv[2];
    }

    printf("HTTP GET Client\n");
    printf("Connecting to %s:%d%s\n", host, HTTP_PORT, path);

    ip_str_to_addr(host, ip_addr);

    // Create TCP socket
    soc = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (soc == -1) {
        printf("socket: failure\n");
        exit(1);
    }
    printf("socket: success, soc=%d\n", soc);

    // Setup server address
    server.sin_family = AF_INET;
    server.sin_port = htons(HTTP_PORT);
    server.sin_addr.s_addr = ip_to_addr(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    addr = (unsigned char *)&server.sin_addr;
    printf("Connecting to %d.%d.%d.%d:%d\n", 
           addr[0], addr[1], addr[2], addr[3], ntohs(server.sin_port));

    // Connect to server
    if (connect(soc, (struct sockaddr *)&server, sizeof(server)) == -1) {
        printf("connect: failure\n");
        close(soc);
        exit(1);
    }
    printf("connect: success\n");

    // Build and send HTTP GET request in parts to avoid buffer issues
    printf("Sending HTTP request...\n");
    
    // Send "GET "
    ret = send(soc, "GET ", 4);
    if (ret == -1) goto send_error;
    req_len = ret;
    
    // Send path
    ret = send(soc, path, strlen(path));
    if (ret == -1) goto send_error;
    req_len += ret;
    
    // Send " HTTP/1.0\r\nHost: "
    ret = send(soc, " HTTP/1.0\r\nHost: ", 17);
    if (ret == -1) goto send_error;
    req_len += ret;
    
    // Send hostname
    ret = send(soc, host, strlen(host));
    if (ret == -1) goto send_error;
    req_len += ret;
    
    // Send final headers
    ret = send(soc, "\r\nConnection: close\r\n\r\n", 23);
    if (ret == -1) goto send_error;
    req_len += ret;
    
    printf("send: %d bytes sent\n", req_len);

    // Receive and print HTTP response
    printf("\n--- HTTP Response ---\n");
    while (1) {
        ret = recv(soc, response, BUFFER_SIZE - 1);
        if (ret <= 0) {
            if (ret == 0) {
                printf("\n--- Connection closed by server ---\n");
            } else {
                printf("\nrecv: error\n");
            }
            break;
        }
        response[ret] = '\0';  // Null terminate for printing
        printf("%s", response);
        total_received += ret;
    }

    printf("\nTotal received: %d bytes\n", total_received);

    close(soc);
    exit(0);

send_error:
    printf("send: failure\n");
    close(soc);
    exit(1);
}
