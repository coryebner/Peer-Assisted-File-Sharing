# Simple-Peer-to-Peer-File-Sharing
A simple peer-assisted file sharing application using a UDP-based pull server.

To compile the server: gcc –o server server.c

To run the server: ./server <N> <P> <C> <Port> <Filename>

To compile the peer client: gcc –o peer peer.c

To run the peer client: ./peer <ServerIP:Port> [Debug]

Angle brackets indicate mandatory arguments and square brackets indicate optional arguments. 
Note: Debug argument will be accepted but currently does not do anything.

There are several important configuration parameters:
•	N is an integer parameter specifying the maximum number of participating peers that the server can handle. 
  We will limit this to at most N = 8. 
  Any additional peers beyond N that try to connect to the server are turned away (i.e., rejected).
•	P is an integer parameter specifying the maximum number of connections that a peer can have in the file sharing protocol. 
  We will limit this to 1 <= P <= N. 
•	M is an integer parameter specifying the number of pieces in the file that is being shared.
•	C is an integer parameter specifying the chunk size for file pieces. 
•	Note that the product of M and C is the total file size.

The application uses a pull-based design where a participating peer A who needs piece i would consult the server to find a 
peer B who has piece i, and then contact peer B to download piece i. A pull based server was used because it made more sense 
to myself. The client only asks for a piece when it is ready for the piece in a pull based server. In a push based the server 
is constantly trying to send pieces even if the client doesn’t want them at this time. 
It made more sense for the client to waste cpu cycles trying to get info from the server instead of the server wasting 
cpu cycles pushing out data to a client

UDP was used. UDP allowed the server and peers to only utilize one socket a piece. 
This greatly simplified the process of creating the program as I did not need to keep track of all connections. 
My program also doesn’t rely on messages sent being received which works well with UDP. 
If an ACK is not received, the sender just resends its request at a later time. 
The downside to my approach with UDP is that I do not have extremely robust data transfer mechanisms. 
The receiver might get data and accepts it if a header matches what it was expecting but it doesn’t check if the rest of the 
data is what it wanted. E.g. the data could have been corrupted. Based on testing (more on this later) my solution 
successfully handles the N = 8, P = 8, C = 8192, and M = 32 for a single 256 KB ASCII test file case but larger files and 
parameters may break the program. Specifically the program can never handle more than 8 peers at once as this is hardcoded.
Extra Feature: The peer version of the file is saved to the directory of the program once completed.

Testing and known Issues:
Testing was performed on a VMware virtual machine running the latest Linux Mint 16 64bit and a laptop running 
Linux Mint 16 64bit running on a home network. 
The program was able to successfully handle the dolittle.txt, 32KB and 256KB tests on the same machine using a loopback 
interface and between the machines. The peer will occasionally send multiples of the same request to the server. 
The most notable instance of this is when the peer tries to connect with the “hi” command. 
The server appears to handle these requests fine. When running the server for a period of time the server will accept a 
peer assigning him garbage for a peerIndex. Not needed data is sent which was going to be used to solve transmission 
reliability, this was not implemented but the data is still sent. The code is not memory efficient and contains memory leaks.
