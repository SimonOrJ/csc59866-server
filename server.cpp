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

/**
 * HTTP Header time format
 * To be used with strftime and strptime
 * used in httpDateTime() and parseHttpDateTime()
 */
const char *timeFormat = "%a, %d %b %Y %T %Z";

/**
 * HTTP Header common format
 * Used on every HTTP responses.
 * Formatters:
 *   1. Date from httpDateTime method
 *   2. "keep-alive" or "close"
 */
const char *commonTemplate = "\
Server: CSC59866/Group1 (C++)\r\n\
Date: %s\r\n\
Connection: %s\r\n\
";

/**
 * HTTP Header content format
 * Used on non-HEAD responses except 304.
 * Formatters:
 *   1. Content type based on file extension
 *   2. Following content length
 */
const char *contentTemplate = "\
Content-Type: %s\r\n\
Accept-Ranges: bytes\r\n\
Content-Length: %ld\r\n\
";

// Global variables
std::map<std::string, std::string> http_headers;    // Header storage map
std::string http_method, file_path, webroot;        // Global strings
const unsigned int MAX_BUFFER_LENGTH = 4096;        // Max buffer length
char buffer[MAX_BUFFER_LENGTH]; // Global buffer with MBL length (4096)
int cliSock,                    // Socket
    connId = 0,                 // Connection ID (increments with every conn)
    keepAlive,                  // keep-alive flag
    resErr;                     // Header parsing error flag 

// Global file-based variables
FILE *webFile;              // File pointer for reading                    
struct stat webFileStat;    // File statistics for dates

/**
 * Unix time to HTTP format builder
 * @param rt input Unix Epoch time
 * @return datetime in HTTP format
 */
char *httpDateTime(time_t rt) {
    char *time = new char[30];      // HTTP time is exactly 30 chars long
    strftime(time, 30, timeFormat, gmtime(&rt));    // Do the formatting
    return time;                    // Return datetime in HTTP format
}

/**
 * Time parser from HTTP header
 * @param time 30-character long time in HTTP format
 * @return time in Unix Epoch time format
 */
time_t parseHttpDateTime(const char* time) {
    struct tm tmv;                      // Declare tmv with tm struct...
    memset(&tmv, 0, sizeof(struct tm)); // ... and initialize it
    strptime(time, timeFormat, &tmv);   // Translate and put into tmv
    return mktime(&tmv);        // Convert then return in Unix Epoch format
}

/**
 * Header appender for response with body
 * This is not called if HEAD method is used.
 * This is not called when HTTP response is 304.
 */
void appendBodyInfo() {
    long fsize;                 // Filesize
    size_t slashPos, extPos;    // Position trackers
    std::string type, ext;      // Type and Extension strings
    
    if (webFile == NULL) {              // Check if file to send exists
        fsize = 0;                      // No file = no content
    } else {
        fseek(webFile, 0L, SEEK_END);   // Find end of file
        fsize = ftell(webFile);         // Get the file size
    }
    
    // e.g. file_path is "/index.html"
    slashPos = file_path.find_last_of('/'); // Find position of '/'
    
    if (slashPos == std::string::npos)  // If there is no /
        ext = file_path;                // (400.html, 404.html files has none)
    else
        ext = file_path.substr(slashPos, file_path.length());   // Crop end in
    
    // e.g. ext is "index.html"
    extPos = ext.find_last_of('.');     // Find position of '.'
    if (extPos == std::string::npos) {  // If '.' is not found
        type = "text/plain";            // assume text file...
    } else {
        ext = ext.substr(extPos + 1, ext.length()); // Crop end in
        // e.g. ext is "html"
        
        // Image Types
        if (ext == "jpg" || ext == "jpeg") type = "image/jpeg"; // jpeg files
        else if (ext == "png")  type = "image/png";             // png file
        else if (ext == "gif")  type = "image/gif";             // gif file
        else if (ext == "webp") type = "image/webp";            // webp file
        // Text Types
        else if (ext == "html") type = "text/html";         // html file
        else if (ext == "css")  type = "text/css";          // css file
        else if (ext == "js")   type = "text/javascript";   // js file
        
        else                    type = "text/plain";        // all other
    }
    
    
    sprintf(buffer + strlen(buffer), contentTemplate, // Append content info...
            type.c_str(),   // Type (into char array)
            fsize           // File size
    );                      // ... to the end of buffer
}

/**
 * Sending a file through body
 * This is not called if HEAD method is used.
 * This is not called when content length is 0.
 */
void sendBody() {
    std::cout << "Sending content '" << file_path << "'...\n";  // Print info
    size_t len;         // Bytes read
    ssize_t slen;       // Bytes sent
    
    rewind(webFile);    // Send file pointer to the beginning of file

    do {
        len = fread(buffer, 1, sizeof(buffer), webFile);    // Read file    
        if (len) {                                          // If exists
            slen = write(cliSock, buffer, sizeof(buffer));  // send it.
        } else {
            slen = 0;   // Default if there was nothing to write.
        }
        printf("  Sent: %u\n", len);        // Print bytes sent
    } while (slen != -1 && len == slen);    // Keep looping if no error or
                                            //   there is more data to send.
    if (slen == -1) {                           // If there was a write error
        perror("Could not complete file copy"); // report it
    } else {
        std::cout << "... Done!\n";         // Print info
    }
}

/**
 * HTTP Response Sender
 * This function is in charge of sending a response to the HTTP request.
 * @param hasBody A flag if this should also send the body of the response.
 */
void sendHttpResponse(int hasBody) {
    time_t rt;  // Unix time
    time(&rt);  // Get current time
    
    sprintf(buffer + strlen(buffer), commonTemplate,   // Append common info...
            httpDateTime(rt),                   // HTTP format datetime string
            keepAlive ? "keep-alive" : "close"  // keep-alive flag
    );                                          // ... to the end of buffer
    
    if (hasBody)            // If there will be body
        appendBodyInfo();   //   then append body info
    
    strcat(buffer, "\r\n"); // HTTP request complete with extra line break
    std::cout << buffer;    // Print header to send to console
    write(cliSock, buffer, strlen(buffer)); // Send the header!
    
    if (hasBody                         // If HTTP has body
            && http_method != "HEAD"    // ... and is not HEAD method
            && webFile != NULL          // ... and file pointer is not NULL
    ) sendBody();                       //   then send the content into body.
}

/**
 * HTTP Response Preparer
 * This function prepares HTTP respond with proper header.
 * @param code HTTP code (200, 304, 400, 404)
 */
void httpRespond(int code) {
    printf("[#%d] -> Sending server response\n", connId);
    
    switch (code) {
        case 200:
            strcpy(buffer, "HTTP/1.1 200 OK\r\n");
            // Add Last-Modified header
            strcat(buffer, "Last-Modified: ");
            strcat(buffer, httpDateTime(webFileStat.st_mtime));
            strcat(buffer, "\r\nCache-Control: public, max-age=86400\r\n");
            sendHttpResponse(1);
            break;
        case 304:
            strcpy(buffer, "HTTP/1.1 304 Not Modified\r\n");
            sendHttpResponse(0);
            break;
        case 400:
            file_path = "400.html";
            webFile = fopen("400.html", "r");
            strcpy(buffer, "HTTP/1.1 400 Bad Request\r\n");
            sendHttpResponse(1);
            break;
        case 404:
            file_path = "404.html";
            webFile = fopen("404.html", "r");
            strcpy(buffer, "HTTP/1.1 404 Not Found\r\n");
            sendHttpResponse(1);
            break;
        default:
            file_path = "501.html";
            webFile = fopen("501.html", "r");
            strcpy(buffer, "HTTP/1.1 501 Not Implemented\r\n");
            sendHttpResponse(1);
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
        httpRespond(400);
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
    webFile = fopen(path.c_str(), "r");
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
    
    if (webFile == NULL) {
        httpRespond(404);
        return;
    }
    
    stat(path.c_str(), &webFileStat);

    double timecmp;
    if (modsince.empty()) {
        timecmp = -1;
    } else {
        time_t climod = parseHttpDateTime(modsince.c_str());
        timecmp = difftime(climod, webFileStat.st_mtime);
    }
    
    if (timecmp >= 0) httpRespond(304);
    else httpRespond(200);
    
    if (webFile != NULL)
        fclose(webFile);
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
        if (
                header.find(" /", 3) == std::string::npos
                || header.find(" HTTP/", 5) == std::string::npos
        ) {
            keepAlive = 0;
            httpRespond(400);
            break;
        }
        
        if (header.substr(0, 4) != "GET " && header.substr(0, 5) != "HEAD ") {
            httpRespond(501);
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
