// by JinWon Chung
// Code works but it doesn't work properly.
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string>

#define BUF_LENGTH 10000

void http_ok(int fp) { // 200
/*

HTTP/1.1 200 OK
Server: nginx/0.8.52
Date: Thu, 18 Nov 2010 16:04:38 GMT
Content-Type: image/png
Last-Modified: Thu, 15 Oct 2009 02:04:11 GMT
Accept-Ranges: bytes
Content-Length: 6394
Connection: keep-alive

*/
    
}

void http_not_modified() { // 304
/*

HTTP/1.1 304 Not Modified
Server: nginx/0.8.52
Date: Thu, 18 Nov 2010 16:10:35 GMT
Expires: Thu, 31 Dec 2011 16:10:35 GMT
Connection: keep-alive

*/


    
}

void http_bad_request() { // 400
/*

HTTP/1.1 400 Bad Request
Server: nginx/0.8.52
Date: Thu, 18 Nov 2010 16:04:38 GMT
Content-Type: image/png
Last-Modified: Thu, 15 Oct 2009 02:04:11 GMT
Accept-Ranges: bytes
Content-Length: 6394
Connection: keep-alive

*/
    
}

void http_not_found() { // 404
/*

HTTP/1.1 404 Not Found
Server: nginx/0.8.52
Date: Thu, 18 Nov 2010 16:04:38 GMT
Content-Type: image/png
Last-Modified: Thu, 15 Oct 2009 02:04:11 GMT
Accept-Ranges: bytes
Content-Length: 6394
Via: 1.1 proxyIR.my.corporate.proxy.name:8080 (IronPort-WSA/6.3.3-015)
Connection: keep-alive

*/
    
}


FILE *webfile(char* uri) {
    std::string root = "./html";
    FILE *fp;
    
    fp = fopen(root + uri, "r");
    return fp;
}

void parseheader() {
    switch () {
        case "":
            break;
    }
    
}

void webheader(char* buf) {
    int init_size = strlen(buf);
    char delim[] = "\n";

    char *ptr = strtok(buf, delim);

    while(ptr != NULL) {
        parseheader(ptr);
        ptr = strtok(NULL, delim);
    }

    /* This loop will show that there are zeroes in the str after tokenizing */
    for (int i = 0; i < init_size; i++)
    {
        printf("%d ", buf[i]); /* Convert the character to integer, in this case
                               the character's ASCII equivalent */
    }
    printf("\n");

    return 0;    

/*

GET / HTTP/1.1
Host: localhost:8080
Connection: keep-alive
Cache-Control: max-age=0

*/
}

void handleHttp(int conn) {
    char buffer[BUF_LENGTH];
    bzero(buffer, BUF_LENGTH);
    int receiving_flag = 1;
    int bufstrlen;
    
    // read the message from client and copy it in buffer
    do {
        read(conn, buffer, sizeof(buffer));
        printf("%s", buffer);
        // TODO: Buffer operation
        
        buffer
        
        // 
        bzero(buffer, BUF_LENGTH);
    } while (receiving_flag);
    
    // and send that buffer to client 
    FILE *f = webfile("/index.html");
    if (f == NULL) {
        perror("File could not be opened");
        http_not_found();
        return;
    }
    
    fseek(f, 0L, SEEK_END);
    long fsize = ftell(f);
    sprintf(buffer, "HTTP/1.1 200 OK\n\
Server: httpserver/group1\n\
Content-Type: text/html\n\
Connection: keep-alive\n\
Content-Length: %ld\n\n", fsize);
    printf("%s", buffer);
    write(conn, buffer, sizeof(buffer));
    bzero(buffer, BUF_LENGTH);
    

    size_t len, lenw;
    fseek(f, 0L, SEEK_SET);

    do {
        len = fread(buffer, sizeof(buffer), 1, f);
        printf("%d\n", len);
        if (len) {
            lenw = write(conn, buffer, sizeof(buffer));
        } else {
            lenw = 0;
        }
    } while (len > 0 && lenw != -1);
    if (lenw == -1) {
        perror("Could not complete file copy");
    }
    
    // Exit Condition
    if (strncmp("Connection: close", buffer, 4) == 0) { 
        printf("Connection closed\n"); 
    }
    printf("Connection closed v2\n"); 
}

int main() {
    int port = 8080; // TEMP: Port number
    char addr[] = "0.0.0.0"; // TEMP: Server Address
    
    struct sockaddr_in server, client;
    int sock;
    
    // IP:port definition
    server.sin_family = AF_INET;                // Define protocol as IPv4
    server.sin_port = htons(port);              // Define port
    inet_aton(addr, &server.sin_addr.s_addr);   // Define Address

    // Socket definition
    sock = socket(AF_INET, SOCK_STREAM, 0);     // Create a socket

    // Bind Socket to IP:port
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) != 0 )
                                                // Bind
    {                                           // If failed to bind
        printf("Could not bind to address %s port %d\n", addr, port);
        return 1;                               //   Print error and exit.
    }
    
    // Enable Socket Listening through IP:port
    listen(sock, 5);                            // Start listening
    printf("Server started on address %s port %d\n", addr, port);
                                                // Print confirmation message
    
    // Accept packets
    int conn, client_size;
    while(1) {                                  // Forever Loop
        client_size = sizeof(client);
        conn = accept(sock, (struct sockaddr *)&client, &client_size);
                                                //   Wait for connection...
        printf("Client connected\n");           //   Connected! Print to Console
        if (conn < 0) {                         //   If invalid connection
            perror("Connection errored");       //     Let console know
            close(conn);                        //     Close connection
            continue;                           //     Next connection
        }
        handleHttp(conn);                       //   Complete it in a function
    }
    
    return 0;                                   // Successful(?) exit
}



// TODO: Input GET, HEAD, Conditional GET
// TODO: Output 200, 304, 400, 404
// $HTTP_SERVER_HOME
// $HTTP_ROOT = $HTTP_SERVER_HOME/html













