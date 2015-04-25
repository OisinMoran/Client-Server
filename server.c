#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>

int retVal;
void printError(void);  // function to display error messages

#define SERV_PORT 32980  // port to be used by server
#define BUFF_SIZE 50000  // Standard Buffer Size for receiving and sending


time_t timeint; //variable to measure time intervals
int bytesSinceLastClock; //variable to measure bytes sent or received during intervals of time

// Function to check for and print errors based on recv() return value
void recvCheck(int ret){
	if( ret < 0) {
		printf("Problem receiving, connection closed by client?\n");
		printError();
	}else if(ret == 0){
		printf("Connection closed\n");
	}
}

// Function to check for and print errors based on send() return value
void sendCheck(int ret){
	if( retVal == SOCKET_ERROR)  // check for error
    {
		printf("*** Error sending response\n");
		printError();
	}
}

// Function that runs a command "cmd" locally and then sends the commands output to the client
// Used for listing server directory
int RunCmd(char * cmd, SOCKET *cSock){
	FILE* pipe =  NULL;
	char out[500];
	int read = 0;
	int retVal;
	if((pipe = _popen(cmd, "rt")) == NULL){
	    printf("aww Man\n");
		return -1;
	}
	while(!feof(pipe)){
        read = fread(out, 1, 500-1, pipe);
        retVal = send(*cSock, out, read, 0);
        sendCheck(retVal);
        if(retVal == SOCKET_ERROR)
            return -1;
	}
	out[0]='\0';
	send(*cSock, out, 1, 0);
	return 1;
}

// Function sends a file of name "filename" to client at "cSocket"
// The array "response" is used to read the file in blocks
int SendFile(char * filename, SOCKET *cSocket, char * response){
    int filesize;
	FILE *file = fopen(filename,"rb");
	if(file == 0){
		printf("File error\n");
		return -1;
	}

	fseek(file,0, SEEK_END);
	filesize=ftell(file);
	sprintf(response,"%d", filesize);
	int nIn = strlen(response);
	retVal = send(*cSocket, response, nIn+1, 0);
	sendCheck(retVal);
	if(retVal == SOCKET_ERROR)
        return -1;
	fseek(file, 0, SEEK_SET);
	timeint= clock();
    bytesSinceLastClock = 0;
	while(!feof(file)){
		nIn= fread(response, 1, BUFF_SIZE,file);
		retVal = send(*cSocket, response, nIn, 0);
		sendCheck(retVal);
		if(retVal == SOCKET_ERROR)
            return -1;
        bytesSinceLastClock += retVal;
        if(bytesSinceLastClock >= 5000000){
            putchar('\r');
            printf("Bit Rate: %.2lf kB/s        ",(bytesSinceLastClock/((clock()-timeint)/(double)CLOCKS_PER_SEC))/1000);
            bytesSinceLastClock = 0;
            timeint = clock ();
        }
	}
	return 1;
}

// Function receives bytes in blocks from a client, saving them in an array "request".
// Receiving stops once a null character has been received or the total amount of bytes received
// is greater than or equal to "Max".
int getStr(SOCKET* cSocket,char *request, int Max){
    int nullfound = 0;
    int size;
    int i;
    int TotalSize = 0;
    while(!nullfound && TotalSize < Max){
        size = recv(*cSocket, request+TotalSize, Max-TotalSize, 0);
		recvCheck(size);
        if(size <= 0){
			return -1;//recvError
		}
        for(i = TotalSize; i!= TotalSize+size; i++){
            if(request[i] == '\0')
                nullfound = 1;
        }
        TotalSize += size;
    }
    if(!nullfound)
        return -2;//string not found
    return TotalSize;
}


// Function shutsdown and closes the "cSocket"
int socketClear(SOCKET * cSocket){
    printf("Connection closing...\n");

    // Shut down the sending side of the TCP connection first
    retVal = shutdown(*cSocket, SD_SEND);
    if( retVal != 0)  // check for error
    {
        printf("*** Error shutting down sending\n");
        printError();
    }

    // Then close the client socket
    retVal = closesocket(*cSocket);
    if( retVal != 0)  // check for error
    {
        printf("*** Error closing client socket\n");
        printError();
    }
    else printf("Client socket closed\n\n");
	*cSocket = INVALID_SOCKET;
	return retVal;
}

int main(){
    WSADATA wsaData;  // create structure to hold winsock data
    int nRx;// variable to save the number of bytes received
	int i; // looping variable
    int nullfound = 0; // int set to 1 if null character is found, 0 otherwise
    int filesize;
    int overwrite = 0; // flag set to 1 if client wants to force upload
	int count = 0; // counts amount of requests handled by server since program started
    int headerlen;
    char request[BUFF_SIZE];  // array to hold received bytes
    char filename[100];
    char response[BUFF_SIZE]; // array to hold our response
    FILE *file;

    // Initialise winsock, version 2.2, giving pointer to data structure
    retVal = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (retVal != 0)  // check for error
    {
        printf("*** WSAStartup failed: %d\n", retVal);
        printError();
        return 1;
    }
    printf("WSAStartup succeeded\n" );

    // Create a handle for a socket, to be used by the server for listening
    SOCKET serverSocket = INVALID_SOCKET;  // handle called serverSocket

    // Create the socket, and assign it to the handle
    // AF_INET means IP version 4,
    // SOCK_STREAM means socket works with streams of bytes,
    // IPPROTO_TCP means TCP transport protocol.
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)  // check for error
    {
        printf("*** Failed to create socket\n");
        printError();
        return 2;
    }
    else printf("Socket created\n" );

    // Build a structure to identify the service offered
    struct sockaddr_in service;  // IP address and port structure
    service.sin_family = AF_INET;  // specify IP version 4 family
    service.sin_addr.s_addr = htonl(INADDR_ANY);  // set IP address
    // function htonl() converts 32-bit integer to network format
    // INADDR_ANY means we accept connection on any IP address
    service.sin_port = htons(SERV_PORT);  // set port number
    // function htons() converts 16-bit integer to netwsend(cSocket, response, nIn+1, 0);ork format

    // Bind the socket to the IP address and port just defined
    retVal = bind(serverSocket, (SOCKADDR *) &service, sizeof(service));
    if( retVal == SOCKET_ERROR)  // check for error
    {
        printf("*** Error binding to socket\n");
        printError();
        return 3;
    }
    else printf("Socket bound\n");

    // Listen for connection requests on this socket,
    // second argument is maximum number of requests to allow in queue
    retVal = listen(serverSocket, 2);
    if(retVal == SOCKET_ERROR)  // check for error
    {
        printf("*** Error trying to listen\n");
        printError();
        return 4;
    }
    else printf("Listening on port %d\n", SERV_PORT);

    // Create a new socket for the connection we expect
    // The serverSocket stays listening for more connection requests,
    // so we need another socket to connect with the client...
    SOCKET cSocket = INVALID_SOCKET;

    // Create a structure to identify the client
    struct sockaddr_in client;  // IP address and port structure
    int len = sizeof(client);  // initial length of structure

    // Wait until a connection is requested, then accept the connection.

	// Main server loop
	// This allows the program to handle many requests consecutively.
	while(1){
		count++; // Increment loop count by 1
		if(count > 1)
			socketClear(&cSocket);// Shutdown and close socket from last request
        printf("Waiting for new connection\n");
		cSocket = accept(serverSocket, (SOCKADDR *) &client, &len );// accept a new connection
		if( cSocket == INVALID_SOCKET)  // check for error
		{
			printf("*** Failed to accept connection\n");
			printError();
			break;
		}
		
		int clientPort = client.sin_port;  // get port number
		struct in_addr clientIP = client.sin_addr;  // get IP address
		// in_addr is a structure to hold an IP address
		printf("Accepted connection from %s using port %d\n",
			   inet_ntoa(clientIP), ntohs(clientPort));
		// function inet_ntoa() converts IP address structure to string
		// function ntohs() converts 16-bit integer from network form to normal

		// getStr() used to get the header from the client
		nRx = getStr(&cSocket, request, BUFF_SIZE);
		if(nRx < 0){// If getStr() fails return to the start of the loop. (getStr() will print any errors)
			continue;
		}
        overwrite = 0; //set ovewrite flag to false
		if(request[0]=='u'||request[0]=='f'){// Check first char in header to determine request type
		    if(request[0] == 'f'){
		        overwrite = 1; // Overwrite set to true if user wants to force upload
		        printf("Overwrite mode\n");
		    }
            filesize = atoi(request+1); // Extract filesize from header
			printf("\nFilesize: %d bytes\n", filesize);
			headerlen = strlen(request)+1; // Evaluate header length
			if(nRx > headerlen){ // Move any non header bytes to the beginning of the request array
				memcpy(request, headerlen + request,nRx-headerlen);
				headerlen = nRx-headerlen; // Store amount of non header bytes in array to "headerlen"
			}
			else headerlen = 0; // Set headerlen to 0 since there are no non header bytes left in the array

			for(i=0; i!= headerlen; i++){ // Check to see if the entire second header (the filename) is in the array
				if(request[i] == '\0'){
					nullfound = 1; // if null is found, the second header is complete 
					break;
				}
			}
			if(!nullfound)// if null not found in array, use getStr() to receive the rest of the filename
				getStr(&cSocket, request+headerlen, BUFF_SIZE);
			strcpy(filename, request);
			printf("Filename: %s\n", filename);
			
			if((file = fopen(filename, "rb")) != NULL && !overwrite){
				printf("File exists\n");
				response[0] = 'f';
				retVal = send(cSocket, response, 1, 0);
				sendCheck(retVal);
				fclose(file);
				continue;
			}else{
				file = fopen(filename, "wb");
				if (file == NULL){
					perror("Error opening file");
					continue;
				}
				response[0] = 'k';
				retVal = send(cSocket, response, 1, 0);
				sendCheck(retVal);
				if(retVal == SOCKET_ERROR){
					continue;
				}
                timeint = clock();
                bytesSinceLastClock = 0;
				while(filesize > 0){
					nRx = recv(cSocket, request, BUFF_SIZE, 0);
					recvCheck(nRx);
					if (nRx<0){
					    fclose(file);
                        break;
					}
					filesize -= nRx;
					if(fwrite(request, 1, nRx, file) != nRx){
						printf("write Error");
						fclose(file);
						break;
					}
					bytesSinceLastClock += nRx;
					if(bytesSinceLastClock >= 5000000){
					    putchar('\r');
					    printf("Bit Rate: %.2lf kB/s        ",(bytesSinceLastClock/((clock()-timeint)/(double)CLOCKS_PER_SEC))/1000);
                        bytesSinceLastClock = 0;
                        timeint = clock ();
					}
				}
			}
			if(fclose(file) == 0)
				printf("\nFILE RECEIVED!!\n");
			else printf("File close failed\n");
		}
		else if(request[0] == 'd'){//download
		    printf("File Being Downloaded\n");
			if(SendFile(request+1, &cSocket, response)==-1){
                response[0]='x';
                retVal = send(cSocket, response, 1, 0);
                printf("File requested no exist!\n");
			}
			else
                printf("\nFile sent\n");
		}else if(request[0] == 'l'){

            if(RunCmd("dir", &cSocket) == 1)
                printf("Server Directory Contents sent\n");
            else
                printf("Command FAILED!!\n");
		}else if(request[0]=='r'){
            if(remove(request+1) != 0){
                perror( "Error deleting file\n" );
                response[0]='e';
                retVal = send(cSocket, response, 1, 0);

            }
            else{
                puts( "File successfully deleted\n" );
                response[0]='k';
                retVal = send(cSocket, response, 1, 0);
            }
		}
	}





    // Then close the server socket
    retVal = closesocket(serverSocket);
    if( retVal != 0)  // check for error
    {
        printf("*** Error closing server socket\n");
        printError();
    }
    else printf("Server socket closed\n");

    // Finally clean up the winsock system
    retVal = WSACleanup();
    printf("WSACleanup returned %d\n",retVal);

    // Prompt for user input, so window stays open when run outside CodeBlocks
    printf("\nPress return to exit:");
    gets(response);
    return 0;
}

/* Function to print informative error messages
   when something goes wrong...  */
void printError(void)
{
	char lastError[1024];
	int errCode;

	errCode = WSAGetLastError();  // get the error code for the last error
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		lastError,
		1024,
		NULL);  // convert error code to error message
	printf("WSA Error Code %d = %s\n", errCode, lastError);
}
