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

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <math.h>
#include <netdb.h>
#include <time.h>

#define MAX_BUF_SEND 9000
#define MAX_BUF_REC 9000
#define MAX_PEERS 8
#define MAX_IDLE 10     // seconds of idle before boot

struct score{
    int num;
    struct sockaddr_in info;
    int active;
    int numPieces;
    int *pieceMap;
    time_t lastActivity;
};

typedef struct score Score;

void printscoreboard(Score *scoreboard, int pieces, int peers);
int findScore(Score *scoreboard, struct sockaddr_in si_client, int peerIndex);
int findPiece(Score *scoreboard, int peerIndex, int pieceNum);
int sendall(int s, char *tosend, struct sockaddr *client, int len);

int main(int argc, char *argv[])
{
    struct sockaddr_in si_server, si_client;
    struct sockaddr *server, *client;
    Score scoreboard[MAX_PEERS];	//scoreboard to keep track of peers and their pieces
    int peers = 0;	// the current number of peers participating
    int N = 0; 		// the maximum number of participating peers that the server can handle.
    int P = 0;		// the maximum number of connections that a peer can have in the file sharing protocol.
    int M = 0;		// number of pieces in the file that is being shared.
    int C = 0;		// the chunk size for file pieces.
    int fileSize; 	// size of the file to be hosted on the server
    char * content;	// content read from the file
    int peerIndex = 0;
    FILE * file;
    int contentSize = 0;
    int s, i, len=sizeof(si_server);
    int port;					// Port number
    char buf[MAX_BUF_REC];
    char tosend[MAX_BUF_SEND];
    int readBytes;
    char method[MAX_BUF_REC];
    int pieceNum;
    char hostname[128];
    struct hostent *h;
    time_t curr_time;
    fd_set read_fds;         // create a file descriptor set
    struct timeval timeout;
    
    // Read port from input arguments, or generate a random port
    if ((argc != 6) && (argc != 7)) {
        fprintf(stderr, "usage: %s <N> <P> <C> <Port> <Filename> [debug]\n", argv[0]);
        printf("Angle brackets indicate mandatory arguments and square brackets indicate optional arguments.\n");
        return 1;
    }
    else{
        N = atoi(argv[1]);
        P = atoi(argv[2]);
        C = atoi(argv[3]);
        port = atoi(argv[4]);

        if(N < 0 || N > 8){
            printf("M must be greater than 0 and less than 9 \n");
            exit(1);
        }

        if(P < 1 || P > N){
            printf("P was not of expected format: 1<=P<=N \n");
            exit(1);
        }

        if(C <= 0){
            printf("C (chunk size) must be greater than 0 \n");
            exit(1);
        }

        // We assume argv[1] is a filename to open
        file = fopen( argv[5], "r" );

        /* fopen returns 0, the NULL pointer, on failure */
        if ( file == 0 )
        {
            printf( "Could not open file %s\n", argv[5]);
            exit(1);
        }
        
        fseek(file,0,SEEK_END);
        fileSize = ftell(file);
        fseek(file,0,SEEK_SET);

        contentSize = ((fileSize) * sizeof(char));
        content = malloc(contentSize + 1);
        if(content == NULL)
            exit(1);

        int temp = 0;
        int current = 0;
        /* read one character at a time from file, stopping at EOF. Store
        the character in the content array*/
        while((temp = fgetc(file)) != EOF){
            //printf("%c", temp);
            content[current] = temp;
            current ++;
        }
        content[contentSize] = '\0';
        fclose( file );
    }

    M = (contentSize + C - 1) / C;

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    {
        printf("Could not setup a socket!\n");
        return 1;
    }
    
    memset((char *) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(port);
    si_server.sin_addr.s_addr = htonl(INADDR_ANY);
    server = (struct sockaddr *) &si_server;
    client = (struct sockaddr *) &si_client;
    
    if (bind(s, server, sizeof(si_server))==-1){
        printf("Could not bind to port %d!\n", port);
        return 1;
    }


    if(gethostname(hostname, sizeof hostname) == 0){
        //printf("Hostname : %s\n", hostname);
        if((h = gethostbyname(hostname)) != NULL);
        //printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));
    }


    printf("Server initialized with the following parameters: N(%d), P(%d), M(%d), C(%d), Filename(%s), IP(%s), Port(%u)\n", N, P, M, C, argv[5], inet_ntoa(*((struct in_addr *)h->h_addr)), ntohs(si_server.sin_port));

    int quit = 0;
    while (!quit)
    {
        // create and set time-out period for select; may change after each call
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        // wait for a response from the server to come in on the socket
        FD_ZERO(&read_fds);      // zero out the set
        FD_SET(s, &read_fds); // put socket in set
        select(s+1, &read_fds, NULL, NULL, &timeout);

        //Update client last activity
        int index;
        index = findScore(scoreboard, si_client, peerIndex);
        scoreboard[index].lastActivity = time(NULL);
        curr_time = time(NULL);

        struct sockaddr *curClient;

        for(index = 0; index < peerIndex; index++){
            if(scoreboard[index].active == 1){
                if(scoreboard[index].lastActivity < curr_time - MAX_IDLE){
                    curClient = (struct sockaddr *) &scoreboard[index].info;
                    scoreboard[index].active = 0;
                    peers--;
                    sendto(s, "ACK bye", strlen(tosend), 0, curClient, len);
                    printf("Peer %d (%s:%u) is being removed for inactivity\n", index+1, inet_ntoa(scoreboard[index].info.sin_addr), ntohs(scoreboard[index].info.sin_port));
                    printscoreboard(scoreboard, M, peerIndex);
                }
            }
        }

        // check if a packet arrived (or if it was a timeout)
        if (!FD_ISSET(s, &read_fds)) continue;

        if ((readBytes=recvfrom(s, buf, MAX_BUF_SEND, 0, client, &len))==-1)
        {
            printf("Read error!\n");
            quit = 1;
        }
        buf[readBytes] = '\0'; // padding with end of string symbol

        printf("  Server received command \"%s\" from client %s on port %d\n",
               buf, inet_ntoa(si_client.sin_addr), ntohs(si_client.sin_port));

        if(strncmp(buf, "quit", 4) == 0)
            quit = 1;

        if( quit == 1 )
            sprintf(tosend, "OK, server shutting down");
        else if(strncmp(buf, "hi", 2) == 0)
        {
            int alreadyPeer = 0;
            int current = 0;

            current = findScore(scoreboard, si_client, peerIndex);
            if(current != -1){
                if(scoreboard[current].active == 0){
                    if(peers >= N){
                        sprintf(tosend, "Maximum peers");
                        printf("Unable to add client, maximum clients reached\n");
                    }
                    else{
                        scoreboard[current].active = 1;
                        alreadyPeer = 1;
                        peers++;
                        sprintf(tosend, "client now active");
                        printf("Client is already a peer, setting to active\n");
                    }
                }
                else{
                    sprintf(tosend, "Already active");
                    printf("Client is already a peer and is already active\n");
                    alreadyPeer = 1;
                }
            }
            else{
                if(peers < N){
                    //if(current == -1){
                    scoreboard[peerIndex].num = peerIndex+1;
                    scoreboard[peerIndex].info = si_client;
                    scoreboard[peerIndex].active = 1;
                    scoreboard[peerIndex].numPieces = 0;
                    scoreboard[peerIndex].pieceMap = malloc(M * sizeof(int));

                    if(scoreboard[peerIndex].pieceMap == NULL){
                        printf("error creating pieceMap\n");
                        exit(1);
                    }

                    int x;
                    for(x = 0; x < M; x++){
                        scoreboard[peerIndex].pieceMap[x] = 0;
                    }

                    printf("Adding new peer %d (%s:%u) to scoreboard\n", peerIndex+1, inet_ntoa(scoreboard[peerIndex].info.sin_addr), ntohs(scoreboard[peerIndex].info.sin_port));
                    sprintf(tosend, "%d %d %d %d %d %s %d", peerIndex+1, N, P, M, C, argv[5], si_client.sin_port);

                    peerIndex++;
                    peers++;
                }
                else{
                    sprintf(tosend, "Maximum peers");
                    printf("Unable to add client, maximum clients reached\n");
                }
            }

            printscoreboard(scoreboard, M, peerIndex);

        }
        else if(strncmp(buf, "get", 3) == 0)
        {
            char temp[MAX_BUF_REC];
            if(sscanf(buf, "%s %s", method, temp) != 2){
                printf("get command not of expected format \n");
                sprintf(tosend, "NACK");
            }
            else{
                int index = findScore(scoreboard, si_client, peerIndex);
                if(index == -1){
                    printf("Not a peer, doing nothing\n");
                    sprintf(tosend, "NACK");
                }
                else{
                    pieceNum = atoi(temp);

                    if(pieceNum <= 0 || pieceNum > M){
                        printf("Unable to get piece %d, outside range \n", pieceNum);
                        sprintf(tosend, "NACK");
                    }
                    else{
                        index = findPiece(scoreboard, peerIndex, pieceNum);
                        if(index == -1){
                            int current = (C* (pieceNum -1));
                            char string[MAX_BUF_SEND] = "";
                            int contentLength;

                            strncpy(string, content + current, C);
                            contentLength = strlen(string);
                            printf("Server sending piece %d to client %s on port %d\n", pieceNum, inet_ntoa(si_client.sin_addr), ntohs(si_client.sin_port));
                            sprintf(tosend, "piece %d %d %s", pieceNum, contentLength , string);
                        }
                        else{
                            printf("Server passing the buck to peer %d on port %d for piece %d \n", scoreboard[index].num, ntohs(scoreboard[index].info.sin_port), pieceNum);
                            sprintf(tosend, "peer %d %s %d", pieceNum, inet_ntoa(scoreboard[index].info.sin_addr), ntohs(scoreboard[index].info.sin_port));
                        }
                    }
                }
            }
        }
        else if(strncmp(buf, "got", 3) == 0){
            char temp[MAX_BUF_REC];
            if(sscanf(buf, "%s %s", method, temp) != 2){
                printf("got command not of expected format \n");
                sprintf(tosend, "got command not of expected format \"got x\"");
            }
            else{
                pieceNum = atoi(temp);

                if(pieceNum <= 0 || pieceNum > M){
                    printf("piece %d, outside range \n", pieceNum);
                    sprintf(tosend, "Not valid piece");
                }
                else{
                    int index = findScore(scoreboard, si_client, peerIndex);

                    if(index == -1){
                        printf("Not a peer, doing nothing\n");
                        sprintf(tosend, "not a peer");
                    }
                    else{
                        scoreboard[index].pieceMap[pieceNum-1] = 1;
                        scoreboard[index].numPieces++;
                        printscoreboard(scoreboard, M, peerIndex);
                        sprintf(tosend, "ACK got %d", pieceNum);
                    }

                }
            }
        }
        else if(strncmp(buf, "bye", 3) == 0)
        {
            int found = 0;
            int current = 0;

            current = findScore(scoreboard, si_client, peerIndex);
            if(current != -1){
                if(scoreboard[current].active == 0){
                    found = 1;
                    //sprintf(tosend, "Already not active");
                    printf("Client is not active, doing nothing\n");
                }
                else{
                    scoreboard[current].active = 0;
                    sprintf(tosend, "ACK bye");
                    printf("Peer %d (%s:%u) is being removed \n", current+1, inet_ntoa(scoreboard[current].info.sin_addr), ntohs(scoreboard[current].info.sin_port));
                    found = 1;
                    peers--;
                }
            }

            if(current == -1){
                printf("Client is not a peer, doing nothing\n");
                sprintf(tosend, "NACK");
            }

            printscoreboard(scoreboard, M, peerIndex);
        }
        else
        {
            sprintf(tosend, "NACK");
        }

        //sendto(s, tosend, strlen(tosend), 0, client, len);
        sendall(s, tosend, client, len);
    }
    close(s);
    free(content);
    return 0;
}

void printscoreboard(Score *scoreboard, int M, int peers){
    int i;

    printf("i   IP Address       Port    Active  Num  Piece Map \n");
    printf("------------------------------------------------------------------ \n");

    for(i = 0; i < peers; i++){
        printf("%-1d   %-15s  %-5d", scoreboard[i].num, inet_ntoa(scoreboard[i].info.sin_addr), ntohs(scoreboard[i].info.sin_port));

        if(scoreboard[i].active == 1){
            printf("   Y       ");
        }
        else{
            printf("   N       ");
        }

        printf("%-5d", scoreboard[i].numPieces);

        int x;
        for(x = 0; x < M; x++){
            printf("%d", scoreboard[i].pieceMap[x]);
        }
        printf("\n");
    }
}

int findScore(Score *scoreboard, struct sockaddr_in si_client, int peerIndex){
    int index;
    int current = 0;
    int found = 0;

    while((!found) && (current < peerIndex)){
        if(strcmp(inet_ntoa(scoreboard[current].info.sin_addr),inet_ntoa(si_client.sin_addr)) == 0){
            if(scoreboard[current].info.sin_port == si_client.sin_port){
                index = current;
                found = 1;
            }
        }
        current++;
    }

    if(!found){
        index = -1;
    }

    return index;
}

int findPiece(Score *scoreboard, int peerIndex, int pieceNum){
    int index;
    int current = 0;
    int found = 0;
    int numWith = 0;

    while((!found) && (current < peerIndex)){
        if(scoreboard[current].pieceMap[pieceNum-1] == 1){
            if(scoreboard[current].active == 1){
                index = current;
                numWith++;
            }
        }
        current++;
    }

    if(numWith != 0)
        found = 1;

    if(!found){
        index = -1;
    }

    printf("There are %d peers with piece %d \n", numWith, pieceNum);

    return index;
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

    return n==-1?-1:0; // return -1 on failure, 0 on success
}
