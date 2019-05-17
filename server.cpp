/* CSC59866
 * Group1
 * - JinWon Chung
 * - Charlie Ding
 * - Yuxuan Zhao
 * Group 6
 * - Gong Qi Chen
 *
 * Program parameters: [port = 8080] [root = html]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <string>
#include <ctime>
#include <map>

const char *timeFormat = "%a, %d %b %Y %T %Z";
const char *commonTemplate = "\
Server: CSC59866/Group1 (C++)\r\n\
Date: %s\r\n\
Connection: %s\r\n\
";
const char *contentTemplate = "\
Content-Type: %s\r\n\
Accept-Ranges: bytes\r\n\
Content-Length: %ld\r\n\
";

std::map<std::string, std::string> http_headers;
std::string http_method, file_path, webroot;
const unsigned int MAX_BUFFER_LENGTH = 4096;
char buffer[MAX_BUFFER_LENGTH];

FILE *webfile;
struct stat webfile_stat;
int cliSock, connId = 0;
int keepAlive, resErr;

// Time creater for HTTP headers
char *http_time(time_t rt) {
    char *time = new char[30];
    strftime(time, 30, timeFormat, gmtime(&rt));
    return time;
}

// Time parser from HTTP header
time_t http_time_parse(const char* time) {
    struct tm tmv;
    memset(&tmv, 0, sizeof(struct tm));
    strptime(time, timeFormat, &tmv);
    return mktime(&tmv);
}

// Common Header appender
void http_header_common() {
    char* currtime;
    time_t rt;
    time(&rt);
    currtime = http_time(rt);
    
    sprintf(buffer + strlen(buffer), commonTemplate, currtime, keepAlive ? "keep-alive" : "close");
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
        printf("  Sent: %u\n", len);
    } while (lenw != -1 && len > 0);
    
    if (lenw == -1) {
        perror("Could not complete file copy");
    } else {
        std::cout << "... Done!\n";
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
    printf("[#%d] -> Sending server response\n", connId);
    
    switch (code) {
        case 200:
            strcpy(buffer, "HTTP/1.1 200 OK\r\n");
            // Add Last-Modified header
            strcat(buffer, "Last-Modified: ");
            strcat(buffer, http_time(webfile_stat.st_mtime));
            strcat(buffer, "\r\nCache-Control: public, max-age=86400\r\n");
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
    printf("[#%d] >| Sending finished\n", connId);
}

void http_receive(std::string head) {
    printf("[#%d] <- Received client request\n", connId);
    std::cout << head << std::endl;
    
    http_headers.clear();
    
    // Method
    size_t pos1 = 0, pos2;
    pos2 = head.find(' ', pos1);
    http_method = head.substr(pos1, pos2-pos1);
    
    // Path
    pos1 = pos2 + 1;
    pos2 = head.find(' ', pos1);
    file_path = head.substr(pos1, pos2-pos1);
    
    // HTTP Version
    pos1 = pos2 + 1;
    pos2 = head.find("\r\n", pos1);
    head.substr(pos1, pos2-pos1);
    if (head.find("HTTP/", pos1) != pos1) {
        keepAlive = 0;
        resErr = 1;
        send_http(400);
        return;
    }
    
    // Rest of the header
    pos1 = pos2 + 2;
    std::string key, val;
    while (1) {
        pos2 = head.find(": ", pos1);
        key = head.substr(pos1, pos2-pos1);
        pos1 = pos2 + 2;
        
        pos2 = head.find("\r\n", pos1);
        val = head.substr(pos1, pos2-pos1);
        pos1 = pos2 + 2;
        
        http_headers[key] = val;
        if (head.substr(pos1, 2) == "\r\n")
            break;
    }
    printf("[#%d] |< Request parsing finished\n", connId);
}

void http_respond() {
    bzero(buffer,MAX_BUFFER_LENGTH);
    std::string path(webroot + file_path);
    webfile = fopen(path.c_str(), "r");
    std::string connection = http_headers["Connection"];
    std::string modsince = http_headers["If-Modified-Since"];
    
    if (connection.empty()) {
        keepAlive = 0;
    } else {
        if (connection.find("keep-alive") != std::string::npos
            || connection.find("Keep-Alive") != std::string::npos)
            keepAlive = 1;
        else
            keepAlive = 0;
    }
    
    if (webfile == NULL) {
        send_http(404);
        return;
    }
    
    stat(path.c_str(), &webfile_stat);

    double timecmp;
    if (modsince.empty()) {
        timecmp = -1;
    } else {
        time_t climod = http_time_parse(modsince.c_str());
        timecmp = difftime(climod, webfile_stat.st_mtime);
    }
    
    if (timecmp >= 0) send_http(304);
    else send_http(200);
    
    if (webfile != NULL)
        fclose(webfile);
}

void httpClientAccept() {
    std::string header;
    int n;
    
    printf("[#%d][N] ### New Client Connection ###\n", connId);
    
    keepAlive = 1;
    resErr = 0;
    while (keepAlive && resErr == 0) {
        // Read header
        bzero(buffer,MAX_BUFFER_LENGTH);
        n = read(cliSock, buffer, MAX_BUFFER_LENGTH);
        if (n < 0) {
            perror("Couldn't read socket!");
            break;
        } else if (n == 0) {
            break;
        } else if (strcmp(buffer, "\r\n") == 0) {
            continue;
        }
        
        header = buffer;
        if (header.substr(0, 4) != "GET " && header.substr(0, 5) != "HEAD ") {
            keepAlive = 0;
            send_http(400);
            break;
        }
        
        while (header.find("\r\n\r\n") == std::string::npos) {
            bzero(buffer,MAX_BUFFER_LENGTH);
            n = read(cliSock, buffer, MAX_BUFFER_LENGTH);
            if (n < 0) {
                perror("Couldn't read socket!");
                break;
            } else if (n == 0) {
                break;
            }
            header.append(buffer);
        }
        
        // Parse incoming header
        http_receive(header);
        
        if (resErr)
            break;
        
        if (file_path == "/") file_path = "/index.html";
        
        // Send data based on header
        http_respond();
    }
    
    shutdown(cliSock, SHUT_RDWR);
    close(cliSock);
    printf("[#%d][X] ### Connection Closed ###\n", connId);
    
    exit(0);
}

// Main Stuff
int main(int argc, char *argv[]) {
    int serverSock, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    
    setenv("TZ", "GMT", 1); // Set timezone to GMT

    if (argc > 1) {
        portno = atoi(argv[1]);
    } else {
        portno = 8080;
        printf("Accepted Arguments: [port] [root path]\n");
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
        perror("Could not bind to port");
        exit(1);
    }
    
    listen(serverSock,5);
    printf("Server started on port %d\n", portno);
    
    while (1) {
        clilen = sizeof(cli_addr);
        cliSock = accept(serverSock, (struct sockaddr *) &cli_addr, &clilen);
        
        if (cliSock < 0) {
            perror("Connection errored");       //     Let console know
            close(cliSock);                     //     Close connection
            continue;                           //     Next connection
        }
        
        connId++;
        if (fork() == 0) {
            httpClientAccept();
        }
    }
    close(serverSock);
    return 0;
}
