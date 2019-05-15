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
#include <map>

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

std::map<std::string, std::string> http_headers;
std::string http_method, http_path;

void header_parse(std::string head) {
    http_headers.clear();
    
    size_t pos1 = 0, pos2;
    pos2 = head.find(' ', pos1);
    http_method = head.substr(pos1, pos2-pos1);
    
    pos1 = pos2 + 1;
    pos2 = head.find(' ', pos1);
    http_path = head.substr(pos1, pos2-pos1);
    
    pos1 = pos2 + 1;
    pos1 = head.find("\r\n", pos1);
    
    while () {
        http_headers...
    }
    
    std::cout << "m: " << http_method << ", p: " << http_path << std::endl;
}

int main(int argc, char *argv[]) {
    const unsigned int MAX_BUFFER_LENGTH = 4096;
    
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[MAX_BUFFER_LENGTH], *webroot;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc < 3) {
        if (argc < 2) {
            portno = 8080;
        } else {
            portno = atoi(argv[1]);
        }
        
        webroot = argv[2];
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("Could not open a socket");
        exit(1);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Could not bind");
        exit(1);
    }
    
    listen(sockfd,5);
    printf("Server started on port %d\n", portno);
    
    while (1) {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        
        if (newsockfd < 0) {
            perror("Connection errored");       //     Let console know
            close(newsockfd);                        //     Close connection
            continue;                           //     Next connection
        }
        
        // Read header
        bzero(buffer,MAX_BUFFER_LENGTH);
        n = read(newsockfd,buffer,MAX_BUFFER_LENGTH - 1);
        if (n < 0) error("ERROR reading from socket");
        
        printf("Here is the message: %s\n",buffer);
        
        header_parse(std::string(buffer));
        
        // Send Data
        n = write(newsockfd,"HTTP/1.1 204 No Content\r\n\r\n",27);
        if (n < 0) error("ERROR writing to socket");
        close(newsockfd);
    }
    close(sockfd);
    return 0; 
}

