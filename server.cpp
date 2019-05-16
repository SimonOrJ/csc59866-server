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

char commonTemplate[] = "\
Server: csc59866/group1\r\n\
Date: %s\r\n\
Connection: keep-alive\r\n\
";
char contentTemplate[] = "\
Content-Type: %s\r\n\
Accept-Ranges: bytes\r\n\
Content-Length: %ld\r\n\
";

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

std::map<std::string, std::string> http_headers;
std::string http_method, file_path, webroot;
const unsigned int MAX_BUFFER_LENGTH = 4096;
char buffer[MAX_BUFFER_LENGTH];

FILE *webfile;
int cliSock;

// Time creater for HTTP headers
char *http_time(time_t rt) {
    char *time = new char[30];
    strftime(time, 30, "%a, %d %b %Y %T %Z", gmtime(&rt));
    return time;
}

// Common Header appender
void http_header_common() {
    char* currtime;
    time_t rt;
    time(&rt);
    currtime = http_time(rt);
    
    sprintf(buffer + strlen(buffer), commonTemplate, currtime);
}

// Header appender for response with body
void http_header_content() {
    long fsize;
    size_t slash, extpos;
    std::string type, ext;
    
    if (webfile == NULL) {
        fsize = 0;
    } else {
        fseek(webfile, 0L, SEEK_END);
        fsize = ftell(webfile);
    }
    
    slash = file_path.find_last_of('/');
    // As case with error handling files
    if (slash == std::string::npos)
        ext = file_path;
    else
        ext = file_path.substr(slash, file_path.length());
    
    extpos = ext.find_last_of('.');
    if (extpos == std::string::npos) {
        type = "text/plain";
    } else {
        ext = ext.substr(extpos + 1, ext.length());
        
        
        // Image Types
        if (ext == "jpg" || ext == "jpeg") type = "image/jpeg";
        else if (ext == "png")  type = "image/png";
        else if (ext == "gif")  type = "image/gif";
        else if (ext == "webp") type = "image/webp";
        // Text Types
        else if (ext == "html") type = "text/html";
        else if (ext == "css")  type = "text/css";
        else if (ext == "js")   type = "text/javascript";
        else                    type = "text/plain";
    }
    
    sprintf(buffer + strlen(buffer), contentTemplate, type.c_str(), fsize);
}

void http_body() {
    std::cout << "Sending content '" << file_path << "'...\n";
    size_t len, lenw;
    
    rewind(webfile);

    do {
        len = fread(buffer, 1, sizeof(buffer), webfile);
        if (len) {
            lenw = write(cliSock, buffer, sizeof(buffer));
        } else {
            lenw = 0;
        }
        printf("Sent: %ld\n", len);
    } while (lenw != -1 && len > 0);
    
    if (lenw == -1) {
        perror("Could not complete file copy");
    } else {
        std::cout << "... Sent\n";
    }
}

void http_send(int content) {
    http_header_common();
    if (content)
        http_header_content();
    strcat(buffer, "\r\n");
    std::cout << buffer;
    write(cliSock, buffer, strlen(buffer));
    
    if (content && http_method != "HEAD" && webfile != NULL)
        http_body();
}

void send_http(int code) {
    switch (code) {
        case 200:
            strcpy(buffer, "HTTP/1.1 200 OK\r\n");
            http_send(1);
            break;
        case 304:
            strcpy(buffer, "HTTP/1.1 304 Not Modified\r\n");
            http_send(0);
            break;
        case 400:
            file_path = "400.html";
            webfile = fopen("400.html", "r");
            strcpy(buffer, "HTTP/1.1 400 Bad Request\r\n");
            http_send(1);
            break;
        case 404:
            file_path = "404.html";
            webfile = fopen("404.html", "r");
            strcpy(buffer, "HTTP/1.1 404 Not Found\r\n");
            http_send(1);
            break;
        
    }
}

void header_parse(std::string head) {
    http_headers.clear();
    
    size_t pos1 = 0, pos2;
    pos2 = head.find(' ', pos1);
    http_method = head.substr(pos1, pos2-pos1);
    if (http_method != "GET" && http_method != "HEAD") {
        send_http(400);
        return;
    }
    
    pos1 = pos2 + 1;
    pos2 = head.find(' ', pos1);
    file_path = head.substr(pos1, pos2-pos1);
    
    pos1 = pos2 + 1;
    pos2 = head.find("\r\n", pos1);
    head.substr(pos1, pos2-pos1);
    if (head.find("HTTP/", pos1) != pos1) {
        send_http(400);
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

void http_respond() {
    bzero(buffer,MAX_BUFFER_LENGTH);
    
    webfile = fopen((webroot + file_path).c_str(), "r");
    if (webfile == NULL) {
        send_http(404);
        return;
    }
    
    // TODO: Date to Not Modified
    
    send_http(200);
    
    if (webfile != NULL)
        fclose(webfile);
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
        
        if (file_path == "/") file_path = "/index.html";
        
        // Send data based on header
        printf(" ### Server Response ###\n");
        http_respond();
        
        close(cliSock);
    }
    close(serverSock);
    return 0; 
}

// TODO: Input GET, HEAD, Conditional GET
// TODO: Output 200, 304, 400, 404
// $HTTP_SERVER_HOME
// $HTTP_ROOT = $HTTP_SERVER_HOME/html


