/* 
* Cory Ebner
* 10048381
* CPSC 441 Assignment 2
*
* a simple peer-assisted file sharing application using a UDP-based pull server.
*
* A simple peer-assisted file sharing application using a UDP-based pull server.
* To compile the server: gcc –o server server.c
* To run the server: ./server <N> <P> <C> <Port> <Filename> [Debug]
* To compile the peer client: gcc –o peer peer.c
* To run the peer client: ./peer <ServerIP:Port> [Debug]
* Angle brackets indicate mandatory arguments and square brackets indicate optional arguments.
* Note: Debug argument will be accepted but currently does not do anything.
*
* The application will make use of a central server, which provides a repository for storing
* files and keeping track of participating peers. The server will have a single file available.
* Each peer contacts the central server to find out what other peers are participating, and what
* pieces of the file each peer has.
*
* There are several important configuration parameters:
*   N is an integer parameter specifying the maximum number of participating peers that the server can handle. We will limit this to at most N = 8. Any additional peers beyond N that try to connect to the server are turned away (i.e., rejected).
*   P is an integer parameter specifying the maximum number of connections that a peer can have in the file sharing protocol. We will limit this to 1 <= P <= N.
*   M is an integer parameter specifying the number of pieces in the file that is being shared.
*   C is an integer parameter specifying the chunk size for file pieces.
* Note that the product of M and C is the total file size.
*
* Based on the code provide by Carey Williamson in wordserver.c and wordclient.c
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>

#define MAX_BUF_SEND 9000
#define MAX_BUF_REC 9000

// Assuming the server is running locally
#define SERVER_PORT 8385//8076
#define SERVER_IP "127.0.0.1"   /* loopback interface */
/*#define SERVER_IP "136.159.5.20"  /* csg.cpsc.ucalgary.ca */
/* #define SERVER_IP "136.159.5.22"  /* csl.cpsc.ucalgary.ca */

int sendall(int s, char *tosend, struct sockaddr *client, int len);

int main(int argc, char *argv[])
{
    struct sockaddr_in si_server, si_server2;
    struct sockaddr *server, *server2;
    int s, i, len = sizeof(si_server2);
    char buf[MAX_BUF_REC];
    char tosend[MAX_BUF_SEND];
    int readBytes;
    int quit;
    char *serverPortChar;
    char *serverIp;
    int serverPort;
    char charN[5]; 		// the maximum number of participating peers that the server can handle.
    char charP[5];		// the maximum number of connections that a peer can have in the file sharing protocol.
    char charM[5];		// number of pieces in the file that is being shared.
    char charC[5];		// the chunk size for file pieces.
    int N;
    int P;
    int M;
    int C;
    char filename[50];
    char myFilename[53] = "P";
    char *content;     // content read from the server/peers
    int *pieceMap;     // shows the pieces the this peer currently has
    int numPieces = 0;      //the number of pieces this peer currently has
    char charpeerNum[5];
    int peerNum;
    char charPortIn[6];
    int portIn;
    char hostname[128];
    struct hostent *h;
    int pieceToGet;
    fd_set read_fds;         // create a file descriptor set
    struct timeval timeout;

    // Read port from input arguments, or generate a random port
    if ((argc != 2) && (argc != 3)) {
        fprintf(stderr, "usage: %s <ServerIP:Port> [debug]\n", argv[0]);
        printf("Angle brackets indicate mandatory arguments and square brackets indicate optional arguments.\n");
        return 1;
    }

    //read serverIp and Port from arguments
    serverIp = strtok(argv[1], ":");
    serverPortChar = strtok(NULL, " ");
    serverPort = atoi(serverPortChar);

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    {
        printf("Could not setup a socket!\n");
        return 1;
    }

    memset((char *) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(serverPort);
    server = (struct sockaddr *) &si_server;
    server2 = (struct sockaddr *) &si_server2;
    if (inet_pton(AF_INET, serverIp, &si_server.sin_addr)==0)
    {
        printf("inet_pton() failed\n");
        return 1;
    }

    printf("Connecting to server (%s:%u)\n", serverIp, serverPort);

    while(1){

        // create and set time-out period for select; may change after each call
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        // wait for a response from the server to come in on the socket
        FD_ZERO(&read_fds);      // zero out the set
        FD_SET(s, &read_fds); // put socket in set
        select(s+1, &read_fds, NULL, NULL, &timeout);

        sprintf(tosend, "hi");


        if (sendto(s, tosend, strlen(tosend), 0, server, sizeof(si_server)) == -1)
        {
            printf("sendto failed\n");
            return 1;
        }

        // check if a packet arrived (or if it was a timeout)
        if (FD_ISSET(s, &read_fds)) break;
    }

    if ((readBytes=recvfrom(s, buf, MAX_BUF_SEND, 0, server2, &len))==-1)
    {
        printf("Read error!\n");
        quit = 1;
    }
    buf[readBytes] = '\0'; // padding with end of string symbol

    if(strncmp(buf, "Maximum peers", 13) == 0){
        printf("Unable to connect, maximum # of peers already reached\nPlease try again later\n");
        exit(1);
    }

    if(sscanf(buf, "%s %s %s %s %s %s %s", charpeerNum, charN, charP, charM, charC, filename, charPortIn) != 7){
        if (sendto(s, "bye", 3, 0, server, sizeof(si_server)) == -1)
        {
            printf("sendto failed\n");
        }
    }

    peerNum = atoi(charpeerNum);
    N = atoi(charN);
    P = atoi(charP);
    M = atoi(charM);
    C = atoi(charC);
    strcat(myFilename, charpeerNum);
    strcat(myFilename, "_");
    strcat(myFilename, filename);
    portIn = atoi(charPortIn);

    if(gethostname(hostname, sizeof hostname) == 0){
        //printf("Hostname : %s\n", hostname);
        if((h = gethostbyname(hostname)) != NULL);
        //printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));
    }

    content = malloc((M * C * sizeof(char))+1);
    if(content == NULL){
        printf("error creating content array\n");
        exit(1);
    }
    memset(content,' ', sizeof(content));
    content[M*C] = '\0';

    pieceMap = malloc(M * sizeof(int));
    if(pieceMap == NULL){
        printf("error creating pieceMap\n");
        exit(1);
    }

    int x;
    for(x = 0; x < M; x++){
        pieceMap[x] = 0;
    }

    printf("Peer %d initialized with the following parameters: N(%d), P(%d), M(%d), C(%d), Filename(%s), IP(%s), Port(%u)\n",
           peerNum, N, P, M, C, myFilename, inet_ntoa(*((struct in_addr *)h->h_addr)), ntohs(portIn));

    srand(time(NULL));

    quit = 0;
    while(!quit)
    {
        // create and set time-out period for select; may change after each call
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        // wait for a response from the server to come in on the socket
        FD_ZERO(&read_fds);      // zero out the set
        FD_SET(s, &read_fds); // put socket in set
        select(s+1, &read_fds, NULL, NULL, &timeout);

        // check if a packet arrived (or if it was a timeout)
        if (FD_ISSET(s, &read_fds)){
            if ((readBytes=recvfrom(s, buf, MAX_BUF_SEND, 0, server2, &len))==-1)
            {
                printf("Read error!\n");
                continue;
            }
            buf[readBytes] = '\0'; // padding with end of string symbol

            //printf("Client recieved: %s from server\n", buf);

            if(strncmp(buf, "get", 3) == 0){
                char temp[MAX_BUF_REC];
                char method[MAX_BUF_REC];
                if(sscanf(buf, "%s %s", method, temp) != 2){
                    printf("get command not of expected format \n");
                    sprintf(tosend, "NACK");
                }
                else{
                    pieceToGet = atoi(temp);

                    if(pieceToGet <= 0 || pieceToGet > M){
                        printf("Unable to get piece %d, outside range \n", pieceToGet);
                        sprintf(tosend, "NACK");

                    }
                    else{
                        int current = (C* (pieceToGet -1));
                        char string[MAX_BUF_SEND] = "";
                        int contentLength;

                        strncpy(string, content + current, C);
                        contentLength = strlen(string);
                        printf("Sending piece %d to peer %s on port %d\n", pieceToGet, inet_ntoa(si_server2.sin_addr), ntohs(si_server2.sin_port));
                        sprintf(tosend, "piece %d %d %s", pieceToGet, contentLength, string);
                    }
                }
                sendall(s, tosend, server2, sizeof(si_server2));
               /* if (sendto(s, tosend, strlen(tosend), 0, server2, sizeof(si_server2)) == -1)
                {
                    printf("sendto failed\n");
                    continue;
                }
                */
            }
            else if(strncmp(buf, "piece", 5) == 0){
                char temp[MAX_BUF_REC];
                char method[MAX_BUF_REC];
                char charPiece[MAX_BUF_REC];
                char charContentLength[MAX_BUF_REC];
                int contentLength;
                char temp2[MAX_BUF_REC];
                int offset;


                if(sscanf(buf, "%s %s %s %s", method, charPiece, charContentLength, temp2) != 4){
                    printf("piece command not of expected format \n");
                    sprintf(tosend, "NACK");
                }
                else{
                    offset = strlen(method) + strlen(charPiece) + strlen(charContentLength) + 3;
                    strcpy(temp, buf+offset);
                    pieceToGet = atoi(charPiece);
                    //contentLength = atoi(charContentLength);

                    if(strlen(temp) == C){
                        memcpy(&content[(pieceToGet -1)*C], temp, strlen(temp));
                        /*int i;
                        for(i = 0; i < strlen(temp); i++){
                            content[((pieceToGet -1)*C) + i] = temp[i];
                        }
*/
                        sprintf(tosend, "got %d", pieceToGet);

                        printf("Successfully got piece %d. Updating server.\n", pieceToGet);
                    }
                    else{
                        sprintf(tosend, "NACK");
                    }
                }

                if (sendto(s, tosend, strlen(tosend), 0, server, sizeof(si_server)) == -1)
                {
                    printf("sendto failed\n");
                    continue;
                }
            }
            else if(strncmp(buf, "ACK got", 7) == 0){
                char temp[MAX_BUF_REC];
                char method[MAX_BUF_REC];
                char charPiece[MAX_BUF_REC];
                if(sscanf(buf, "%s %s %s", method, temp, charPiece) != 3){
                    printf("ACK got command not of expected format \n");
                    sprintf(tosend, "NACK");
                }
                else{
                    pieceToGet = atoi(charPiece);

                    if(pieceToGet <= 0 || pieceToGet > M){
                        printf("piece %d, outside range \n", pieceToGet);
                        sprintf(tosend, "NACK");
                        if (sendto(s, tosend, strlen(tosend), 0, server, sizeof(si_server)) == -1)
                        {
                            printf("sendto failed\n");
                            continue;
                        }
                    }
                    else{
                        numPieces++;
                        pieceMap[pieceToGet-1] = 1;
                        //printf("Piece: %d successfully gotten\n", pieceToGet);
                        printf("Peer node has %d pieces to go\n", M - numPieces);
                    }

                }
            }
            else if(strncmp(buf, "peer", 4) == 0){
                char temp[MAX_BUF_REC];
                char method[MAX_BUF_REC];
                char peerIp[MAX_BUF_REC];
                int peerPort;
                char charPiece[MAX_BUF_REC];

                if(sscanf(buf, "%s %s %s %s", method, charPiece, peerIp, temp) != 4){
                    printf("peer command not of expected format \n");
                    sprintf(tosend, "NACK");
                    if (sendto(s, tosend, strlen(tosend), 0, server, sizeof(si_server)) == -1)
                    {
                        printf("sendto failed\n");
                        continue;
                    }
                }
                else{
                    struct sockaddr_in si_peer;
                    struct sockaddr *peer;
                    peerPort = atoi(temp);

                    memset((char *) &si_peer, 0, sizeof(si_peer));
                    si_peer.sin_family = AF_INET;
                    si_peer.sin_port = htons(peerPort);
                    peer = (struct sockaddr *) &si_peer;
                    if (inet_pton(AF_INET, peerIp, &si_peer.sin_addr)==0)
                    {
                        printf("inet_pton() failed\n");
                        continue;
                    }

                    printf("Requesting piece %d from peer\n", pieceToGet);
                    sprintf(tosend, "get %s", charPiece);
                    if (sendto(s, tosend, strlen(tosend), 0, peer, sizeof(si_peer)) == -1)
                    {
                        printf("sendto failed\n");
                        continue;
                    }

                }
            }
            else if(strncmp(buf, "ACK bye", 7) == 0){
                quit = 1;
            }
            else continue;
        }
        else{
            pieceToGet = rand()%M + 1;

            while(pieceMap[pieceToGet-1] == 1){
                pieceToGet = rand()%M + 1;
            }

            sprintf(tosend, "get %d", pieceToGet);
            printf("Requesting piece %d from server\n", pieceToGet);

            if (sendto(s, tosend, strlen(tosend), 0, server, sizeof(si_server)) == -1)
            {
                printf("sendto failed\n");
                continue;
            }
        }

        if(numPieces >= M){
            if (sendto(s, "bye", 3, 0, server, sizeof(si_server)) == -1)
            {
                printf("sendto failed\n");
                continue;
            }
        }

        //sleep(1);
    }
    FILE *f = fopen(myFilename, "w");
    fwrite(content, sizeof(char), strlen(content), f);
    fclose(f);
    //printf("%s\n", content);
    printf("File %s is complete. File located in directory of peer program. \nExiting.\n", myFilename);
    close(s);
    return 0;
}

int sendall(int s, char *tosend, struct sockaddr *client, int len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft; // how many we have left to send
    int n;
    int amountToSend;

    amountToSend = (int)strlen(tosend);
    bytesleft = amountToSend;

    while(total < amountToSend) {
        //n = send(s, buf+total, bytesleft, 0);
        n = sendto(s, tosend+total, bytesleft, 0, client, len);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    //len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}
