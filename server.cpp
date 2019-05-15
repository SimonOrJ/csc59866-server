// Code by JinWon Chung
// Usage: [port] [path]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <string>
#include <ctime>
#include <map>

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

std::map<std::string, std::string> http_headers;
std::string http_method, http_path;
const unsigned int MAX_BUFFER_LENGTH = 4096;
char buffer[MAX_BUFFER_LENGTH];

FILE *webfile;
int cliSock;
char commonTemplate[] = "\
Server: csc59866/group1\r\n\
Date: %s\r\n\
Connection: keep-alive\r\n\
";

void sendToSock(const char* msg, size_t size) {
    int n = write(cliSock, msg, size);
    
    std::cout << msg;
}

char *http_time(time_t rt) {
    char *time = new char[30];
    strftime(time, 30, "%a, %d %b %Y %T %Z", gmtime(&rt));
    return time;
}

char *http_common() {
    char* currtime;
    time_t rt;
    time(&rt);
    currtime = http_time(rt);
    
    sprintf(buffer, commonTemplate, currtime);
    sendToSock(buffer, sizeof(buffer));
}

void http200() {
    sendToSock("HTTP/1.1 200 OK\r\n", 17);
    http_common();
/*

Content-Type: image/png
Last-Modified: Thu, 15 Oct 2009 02:04:11 GMT
Accept-Ranges: bytes
Content-Length: 6394

*/
    
    sendToSock("\r\n", 2);
}

void http304() {
    sendToSock("HTTP/1.1 304 Not Modified\r\n", 27);
    http_common();
    sendToSock("\r\n", 2);
}

void http400() {
    sendToSock("HTTP/1.1 400 Bad Request\r\n", 26);
    http_common();
/*

Content-Type: image/png
Accept-Ranges: bytes
Content-Length: 6394

*/
    
    sendToSock("\r\n", 2);
}

void http404() {
    sendToSock("HTTP/1.1 404 Not Found\r\n", 24);
    http_common();
/*

Content-Type: image/png
Accept-Ranges: bytes
Content-Length: 6394

*/
    
    sendToSock("\r\n", 2);
}

void header_parse(std::string head) {
    http_headers.clear();
    
    size_t pos1 = 0, pos2;
    pos2 = head.find(' ', pos1);
    http_method = head.substr(pos1, pos2-pos1);
    if (http_method != "GET" && http_method != "HEAD") {
        http400();
        return;
    }
    
    pos1 = pos2 + 1;
    pos2 = head.find(' ', pos1);
    http_path = head.substr(pos1, pos2-pos1);
    
    pos1 = pos2 + 1;
    pos2 = head.find("\r\n", pos1);
    head.substr(pos1, pos2-pos1);
    if (head.find("HTTP/", pos1) != pos1) {
        http400();
        return;
    }
    
    pos1 = pos2 + 2;
    
    std::string key, val;
    while (1) {
        pos2 = head.find(": ", pos1);
        key = head.substr(pos1, pos2-pos1);
        pos1 = pos2 + 2;
        
        pos2 = head.find("\r\n", pos1);
        val = head.substr(pos1, pos2-pos1);
        pos1 = pos2 + 2;
        
        http_headers.insert( std::pair<std::string, std::string>(key, val) );
        if (head.substr(pos1, 2) == "\r\n")
            break;
    }
}

std::string webroot;

int sendhead() {
    std::string root("html");
    
    webfile = fopen((root + http_path).c_str(), "r");
    
    if (webfile == NULL) {
        http404();
        return -1;
    }
    
    // TODO: Date to Not Modified
    // TODO: OK
    
    // Send Data
    sendToSock("HTTP/1.1 204 No Content\r\n",25);
    http_common();
    sendToSock("\r\n", 2);
}

void senddata() {
    int n;
    
    
    
}

int main(int argc, char *argv[]) {
    std::string header;
    int serverSock, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    
    if (argc > 1) {
        portno = atoi(argv[1]);
    } else {
        portno = 8080;
    }
    
    if (argc > 2) {
        webroot = argv[2];
    } else {
        webroot = "html";
    }
    
    serverSock = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSock < 0) {
        perror("Could not open a socket");
        exit(1);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (bind(serverSock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Could not bind");
        exit(1);
    }
    
    listen(serverSock,5);
    printf("Server started on port %d\n", portno);
    
    while (1) {
        clilen = sizeof(cli_addr);
        cliSock = accept(serverSock, (struct sockaddr *) &cli_addr, &clilen);
        
        if (cliSock < 0) {
            perror("Connection errored");       //     Let console know
            close(cliSock);                        //     Close connection
            continue;                           //     Next connection
        }
        
        // Read header
        bzero(buffer,MAX_BUFFER_LENGTH);
        n = read(cliSock, buffer, MAX_BUFFER_LENGTH);
        if (n < 0) error("ERROR reading from socket");
        
        header = buffer;
        
        while (header.find("\r\n\r\n") == std::string::npos) {
            bzero(buffer,MAX_BUFFER_LENGTH);
            read(cliSock, buffer, MAX_BUFFER_LENGTH);
            header.append(buffer);
        }
        
        std::cout << " ### Client Request ###\n" << header << std::endl;
        
        // Parse incoming header
        header_parse(header);
        
        // Send data based on header
        printf(" ### Server Response ###\n");
        if (sendhead() == 0 && http_method != "HEAD")
            senddata();
        
        close(cliSock);
    }
    close(serverSock);
    return 0; 
}

// TODO: Input GET, HEAD, Conditional GET
// TODO: Output 200, 304, 400, 404
// $HTTP_SERVER_HOME
// $HTTP_ROOT = $HTTP_SERVER_HOME/html


