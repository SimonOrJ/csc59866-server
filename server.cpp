/*
 * CSC59866
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

/*
 * Global variables
 */
std::map<std::string, std::string> httpHeaders; // Header storage map
std::string httpMethod, filePath, webroot;      // Global strings
const unsigned int MAX_BUFFER_LENGTH = 4096;    // Max buffer length
char buffer[MAX_BUFFER_LENGTH]; // Global buffer with MBL length (4096)
int cliSock,                    // Socket
    connId = 0,                 // Connection ID (increments with every conn)
    keepAlive,                  // keep-alive flag
    resErr;                     // Header parsing error flag 

/*
 * Global file-based variables
 */
FILE *webFile;              // File pointer for reading                    
struct stat webFileStat;    // File statistics for dates


/* ######################################################################### *
 *          These functions are in charge of replying to the client          *
 * ######################################################################### */

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
 * This is used by sendHttpResponse() function.
 * This appends headers with information for body to the buffer.
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
    
    // e.g. filePath is "/index.html"
    slashPos = filePath.find_last_of('/'); // Find position of '/'
    
    if (slashPos == std::string::npos)  // If there is no /
        ext = filePath;                 // (400.html, 404.html files has none)
    else
        ext = filePath.substr(slashPos, filePath.length());   // Crop end in
    
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
 * Send a file
 * This uses buffer to gather and send data in chunks.
 * This is not called if HEAD method is used.
 * This is not called when content length is 0.
 */
void sendBody() {
    std::cout << "Sending content '" << filePath << "'...\n";  // Print info
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
    if (slen == -1) {                       // If there was a write error
        printf("[#%d][ERROR] ", connId);    //   Print client ID
        perror("Could not complete file transfer"); //   report it
    } else {
        std::cout << "... Done!\n";         // Print info
    }
}

/**
 * HTTP response sender
 * This is used by httpCode() function.
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
            && httpMethod != "HEAD"     // ... and is not HEAD method
            && webFile != NULL          // ... and file pointer is not NULL
    ) sendBody();                       //   then send the content into body.
}

/**
 * Error file preparer
 * Used only when request results in an error. (400, 404, 501)
 * @param file Error file path
 */
void openErrorFile(const char *file) {
    filePath = file;
    webFile = fopen(file, "r");
}

/**
 * HTTP response preparer
 * This function prepares HTTP respond with proper header.
 * @param code HTTP code (200, 304, 400, 404, 501)
 */
void httpCode(int code) {
    printf("[#%d] -> Sending server response\n", connId);   // Print intent
    
    switch (code) { // Switch based on code
        case 200:   // 200 OK
            strcpy(buffer, "HTTP/1.1 200 OK\r\n");              // Header
            strcat(buffer, "Last-Modified: ");  // Add Last-Modified header
            strcat(buffer, httpDateTime(webFileStat.st_mtime)); 
                                    // Add date of when file was last modified
            strcat(buffer, "\r\nCache-Control: public, max-age=86400\r\n");
                                    // Make browser cache file for 1 hour
            sendHttpResponse(1);    // Send response with body
            break;
            
        case 301:   // 301 Moved Permanently
                    //   This is similar to 302 (but is NOT the same)
        case 302:   // 302 Found
            strcpy(buffer, "HTTP/1.1 302 Found\r\n");    // Header
            strcat(buffer, "Location: ");       // Set location header
            strcat(buffer, filePath.c_str());   // Set file position
            strcat(buffer, "\r\n");             // Line break
            sendHttpResponse(0);    // Send response without body
            break;
            
        case 304:   // 304 Not Modified
            strcpy(buffer, "HTTP/1.1 304 Not Modified\r\n");    // Header
            sendHttpResponse(0);    // Send response without body
            break;
            
        case 400:   // 400 Bad Request
            openErrorFile("400.html");  // Prepare 400.html
            strcpy(buffer, "HTTP/1.1 400 Bad Request\r\n");     // Header
            sendHttpResponse(1);    // Send response with body
            break;
            
        case 404:   // 404 Not Found
            openErrorFile("404.html");  // Prepare 404.html
            strcpy(buffer, "HTTP/1.1 404 Not Found\r\n");       // Header
            sendHttpResponse(1);    // Send response with body
            break;
            
        case 501:   // 501 Not Implemented
        default:    // ... and all other cases
            openErrorFile("501.html");  // Prepare 501.html
            strcpy(buffer, "HTTP/1.1 501 Not Implemented\r\n"); // Header
            sendHttpResponse(1);    // Send response with body
    }
    
    printf("[#%d] >| Sending finished\n", connId);  // Print message on finish
}

/**
 * HTTP Responder
 * Responds to the HTTP request
 */
void sendHTTPResponse() {
    std::string path(webroot + filePath),              // Setup file path
        connection = httpHeaders["Connection"],        // Get Conn header
        modsince = httpHeaders["If-Modified-Since"];   // Get IMS header
    bzero(buffer,MAX_BUFFER_LENGTH);    // Zero out the buffer
    webFile = fopen(path.c_str(), "r"); // Open file from path
    double timecmp;                     // Time comparison
    
    if (connection.empty()) {   // If Connection header is not defined
        keepAlive = 0;          // Don't keep the connection alive
    } else {
        if (connection.find("keep-alive") != std::string::npos
            || connection.find("Keep-Alive") != std::string::npos
        )                   // if Connection = keep-alive
            keepAlive = 1;  // Keep alive
        else
            keepAlive = 0;  // don't keep alive (disconnect on complete)
    }
    
    if (webFile == NULL) {  // If file is not found
        httpCode(404);      // Send HTTP 404 then exit
        return;
    }
    
    stat(path.c_str(), &webFileStat);   // Get file information/stats
    if (webFileStat.st_mode & S_IFDIR) {// If "file" is directory
        filePath.push_back('/');    // Add '/' to path
        httpCode(302);              // Send 302 Found then exit
        return;
    }

    if (modsince.empty()) { // If IMS header is not defined
        timecmp = -1;       // *Force HTTP code 200*
    } else {
        time_t climod = parseHttpDateTime(modsince.c_str());// Parse IMS
        timecmp = difftime(climod, webFileStat.st_mtime);   // Check difference
    }
    
    if (timecmp >= 0) httpCode(304);// Send HTTP 304 if webFile is unchanged
    else httpCode(200);             // Send HTTP 200 if webFile is newer
    
    if (webFile != NULL) fclose(webFile);   // Make sure to close file
}


/* ######################################################################### *
 *          This function is in charge of parsing the client input           *
 * ######################################################################### */

/**
 * HTTP bad request preparer (used by parseHeaders())
 * If an HTTP request results in a 400 bad request, this should be run.
 */
void handleBadRequest() {
    keepAlive = 0;  // If not: break connection
    resErr = 1;     // Raise error flag
    httpCode(400);  // Respond with 400 Bad Request
}

/**
 * HTTP request header parser
 * This function reads headers and parses them into global variables.
 * Variables affected: [httpMethod, filePath, httpHeaders]
 */
void parseHeaders(std::string head) {
    size_t pos1 = 0, pos2, posn;    // Declare position trackers
    std::string key, val;           // Declare key-value pair
    
    printf("[#%d] <- Request received; parsing headers\n", connId);
                                    // Print intent
    std::cout << head << std::endl; // Print current headers
    
    httpHeaders.clear();            // Clear map to handle new headers
    
    // Find line break position first
    posn = head.find("\r\n", pos1);
    
    // Method
    pos2 = head.find(' ', pos1);    // Find next blank space
    if (pos2 > posn) {              // Make sure space is within line break
        handleBadRequest();         //   Trigger bad request and exit
        return;
    }
    httpMethod = head.substr(pos1, pos2-pos1);  // Store method
    // Note: Method is validated while gathering header buffer.
    
    // Path
    pos1 = pos2 + 1;                // Update pos1 tracker
    pos2 = head.find(' ', pos1);    // Find next blank space
    if (pos2 > posn) {              // Make sure space is within line break
        handleBadRequest();         //   Trigger bad request and exit
        return;
    }
    filePath = head.substr(pos1, pos2-pos1);    // Store path
    
    // HTTP Version
    pos1 = pos2 + 1;                // Update pos1 tracker
    pos2 = head.find("\r\n", pos1); // Set end to line break
    if (head.find("HTTP/", pos1) != pos1) {// Check if this position is "HTTP/"
        handleBadRequest();         //   Trigger bad request and exit
        return;
    }
    
    // Rest of the header
    pos1 = pos2 + 2;                // Update pos1 tracker
    while (1) {
        posn = head.find("\r\n", pos1);     // Find end of line
        
        pos2 = head.find(": ", pos1);       // Find key-value separator
        if (pos2 > posn) {              // Make sure colon is within line break
            handleBadRequest();             //   Trigger bad request and exit
            return;
        }
        key = head.substr(pos1, pos2-pos1); // This is the key
        pos1 = pos2 + 2;                    // Update pos1 tracker
        
        pos2 = head.find("\r\n", pos1);     // Find end of line
        val = head.substr(pos1, pos2-pos1); // This is the value
        pos1 = pos2 + 2;                    // Update pos1 tracker
        
        httpHeaders[key] = val;             // Put key-value pair in the map
        if (head.substr(pos1, 2) == "\r\n") // Check if there's another \n
            break;                          // Stop looping
    }
    
    printf("[#%d] |< Request parsing finished\n", connId);  // Print on finish
}


/* ######################################################################### *
 *            These functions are in charge of accepting clients             *
 * ######################################################################### */

/**
 * Client Process
 * Create a child process here so the server can accept more connections while
 * waiting on existing connections to complete and send data through read().
 */
void clientProcess() {
    connId++;           // Increment connection
    if (fork() != 0)    // Fork into a new child process
        return;         // If parent, exit out of the function

    std::string header; // Header mega-string
    int n;              // Read tracker
    
    printf("[#%d][N] ### New Client Connection ###\n", connId); // Print info
    
    keepAlive = 1;  // Set variables to start and...
    resErr = 0;     // ... get into the while loop
    while (keepAlive && resErr == 0) {      // Start loop
        bzero(buffer,MAX_BUFFER_LENGTH);    // Zero out the buffer
        
        // Read header
        n = read(cliSock, buffer, MAX_BUFFER_LENGTH);   // Read into buffer
        if (n < 0) {                            // If there was an error
            printf("[#%d][ERROR] ", connId);    //   Print client ID
            perror("Couldn't read socket!");    //   Append error and exit
            break;
        } else if (n == 0) {                    // If nothing was read
            break;                              //   Exit
        } else if (strcmp(buffer, "\r\n") == 0) {
            continue;           // Restart loop If only line break was sent
        }
        
        header = buffer;        // Turn initial buffer into string
        
        // Validate first line
        if (header.find(" /", 3) == std::string::npos
                || header.find(" HTTP/", 5) == std::string::npos
        ) {         // If path and/or "HTTP/1.1" is missing from the request
            keepAlive = 0;  //   Don't repeat the loop
            httpCode(400);  //   Send HTTP 400 Bad Request
            break;          //   and exit
        }
        
        // Validate method
        if (header.substr(0, 4) != "GET " && header.substr(0, 5) != "HEAD ") {
                            // If the method is not GET or HEAD 
            httpCode(501);  //   Send HTTP 501 Not Implemented
            continue;       //   and restart loop
        }
        
        // Receive rest of the header
        while (header.find("\r\n\r\n") == std::string::npos) {
                            // Keep reading until there is double line breaks
            bzero(buffer,MAX_BUFFER_LENGTH);                // Clear buffer
            n = read(cliSock, buffer, MAX_BUFFER_LENGTH);   // Read into buffer
            if (n < 0) {                            // If there was an error
                printf("[#%d][ERROR] ", connId);    //   Print client ID
                perror("Couldn't read socket");     //   Append error and exit
                break;
            } else if (n == 0) {                    // If nothing was read
                break;                              //   Exit
            }
            header.append(buffer);  // Append buffer to header string
        }
        
        parseHeaders(header);   // Parse incoming header
        if (resErr)             // If parseHeaders() resulted in an error
            break;              //   ... then exit out
        
        if (filePath.at(filePath.length()-1) == '/')// If path ends with '/'
            filePath.append("index.html");  // Look for index.html file
        
        sendHTTPResponse(); // Send response based on header
    }
    
    shutdown(cliSock, SHUT_RDWR);   // Shut down the socket before closing
    close(cliSock);                 // Close the socket
    printf("[#%d][X] ### Connection Closed ###\n", connId); // Print info
    
    exit(0);    // Exit the child process
}

/**
 * Main
 * The arguments taken are
 *   0. Program name
 *   1. Port number
 *   2. Web root path
 * @param argc Argument count (including the program name itself)
 * @param argv Arguments (with size of argc)
 */
int main(int argc, char *argv[]) {
    int serverSock, portno;                     // Socket and port
    socklen_t clilen;                           // Socket length
    struct sockaddr_in serverAddr, clientAddr;  // Socket address info
    
    setenv("TZ", "GMT", 1);     // Set timezone to GMT (for HTTP dates)

    if (argc > 1) {             // If there's some parameters
        portno = atoi(argv[1]); //   Interpret first parameter as port
    } else {
        portno = 8080;          //   Make port 8080 (80 is system reserved)
        printf("Accepted Params: [port] [root path]\n"); // Print arg helper
    }
    
    if (argc > 2) {         //   If there's 2 or more parameters
        webroot = argv[2];  //     Interpret as html root
    } else {
        webroot = "html";   //     Set "./html" as root
    }
    
    serverSock = socket(AF_INET, SOCK_STREAM, 0);   // Open up a socket
    if (serverSock < 0) {                   // If errored
        perror("Could not open a socket");  //   Print error message and exit
        exit(1);
    }
    
    bzero((char *) &serverAddr, sizeof(serverAddr)); // Initialize serverAddr
    
    serverAddr.sin_family = AF_INET;            // Set address as IPv4
    serverAddr.sin_addr.s_addr = INADDR_ANY;    // Accept from any address
    serverAddr.sin_port = htons(portno);        // Set port number
    
    if (bind(                                   // If binding...
            serverSock,                         // ... the open socket...
            (struct sockaddr *) &serverAddr,    // ... to the server addr...
            sizeof(serverAddr)                  // (size of server address)
    ) < 0 ) {                                   // ... fails, then
        perror("Could not bind to port");       //   Print error message...
        exit(1);                                //   ... and exit
    }
    
    listen(serverSock,5);   // Start listening to the open socket (success!)
    std::cout << "[#0][S] HTTP Server started on port " << portno
            << " and root dir '" << webroot << "'\n";
            // Print info with port and root directory
    
    while (1) {
        clilen = sizeof(clientAddr);
        cliSock = accept(serverSock, (struct sockaddr *) &clientAddr, &clilen);
        
        if (cliSock < 0) {                              // If socket errors
            perror("[#0][ERROR] Incoming connection");  //   Let console know
            close(cliSock);                             //   Close connection
            continue;                                   //   Next connection
        }
        
        clientProcess();    // If successful, create a new child process to
                            //   handle server operations.
    }
    
    close(serverSock);  // Close socket
    exit(0);            // and exit
}
