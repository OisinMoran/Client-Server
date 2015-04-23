#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <math.h>
#include <conio.h>

#define LOADING_BAR_SIZE 50
#define RECTANGLE_CHAR 219

void printError(void);  // function to display error messages

void TootOwnHorn(void); // function to toot own horn

void print_progress(int filesize, int totalbytes);

int main()  {

    WSADATA wsaData;  // create structure to hold winsock data
    int retVal, read, nRx, j, written, finished = 0, acc_filesize, filesize, totalbytes = 0, nullpos = -1, firstblock = 1;
    int stop = 0;  // flag to control loops
    char serverIP[20];  // IP address of server
    int serverPort = 32980;     // port used by server
    char request[100];      // array to hold user input
    char reply[101];        // array to hold received bytes
    char command;         // character to hold transfer command
    char filename[100];     // array to hold filename of upload file
    FILE *fp;            // file pointer for upload file
    int no_digs_filesize;     // integer to hold number of digits in the filesize
    char indicator;         // character to hold server indicator response
    char data[3000];

    // 1. Winsock set up
    // Initialise winsock, version 2.2, giving pointer to data structure
    retVal = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (retVal != 0)    {  // check for error
        printf("*** WSAStartup failed: %d\n", retVal);
        printError();
        return 1;
    }
    else printf("WSAStartup succeeded\n" );

    // 2. Client socket set up
    // Create a handle for a socket, to be used by the client
    SOCKET clientSocket = INVALID_SOCKET;  // handle called clientSocket

while(1){
    // Create the socket, and assign it to the handle
    // AF_INET means IP version 4,
    // SOCK_STREAM means socket works with streams of bytes,
    // IPPROTO_TCP means TCP transport protocol.
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {  // check for error
        printf("*** Failed to create socket\n");
        printError();
    }
    else printf("Socket created\n" );

    // 3. Server details
    //printf("Enter IP address of server: ");   // ask user for IP address of server
    //scanf("%20s", serverIP);  // get IP address as string

    //printf("Enter port number: ");
   // scanf("%d", &serverPort);     // get port number as integer

   // gets(request);  // flush the endline from the input buffer

    // Build a structure to identify the service required
    // This has to contain the IP address and port of the server
    struct sockaddr_in service;  // IP address and port structure
    service.sin_family = AF_INET;  // specify IP version 4 family
    service.sin_addr.s_addr = inet_addr(serverIP);  // set IP address
    // function inet_addr() converts IP address string to 32-bit number
    service.sin_port = htons(serverPort);  // set port number
    // function htons() converts 16-bit integer to network format

    // 4. Server connection
    // Try to connect to the server
    printf("Trying to connect to %s on port %d\n", serverIP, serverPort);
    retVal = connect(clientSocket, (SOCKADDR *) &service, sizeof(service));
    if( retVal != 0)    {  // check for error
        printf("*** Error connecting\n");
        printError();
        getch(); // give user a chance to read error message
        return 0;
    }
    else printf("Connected!\n");

    // 5. Command choice
    // Ask user for command - upload (u), force upload (f), download (d), list server files (l) or remove file (r)
    printf("\nEnter command - upload (u), force upload (f), download (d), list server files(l) or remove file (r): ");
    command = getchar();

    // Rquest list of files from server
    if (command == 'l'){
        // First element of request array is command, second is null
        request[0] = command;
        request[1] = '\0';

        // Request list
        // Send request array
        retVal = send(clientSocket, request, strlen(request) + 1, 0);
        // send() arguments: socket handle, array of bytes to send,
        // number of bytes to send, and last argument of 0.

        if( retVal == SOCKET_ERROR) // check for error
            {
                printf("*** Error sending\n");
                printError();
            }
        finished = 0;
        do  // loop to receive list of files
        {
            // Try to receive some bytes
            // recv() arguments: socket handle, array to hold rx bytes,
            // maximum number of bytes to receive, last argument 0.
            nRx = recv(clientSocket, reply, 100, 0);
            // nRx will be number of bytes received, or error indicator
            if( nRx == SOCKET_ERROR)  // check for error
            {
                printf("Problem receiving\n");
                stop = 1;  // exit the loop if problem
            }
            else if (nRx == 0)  // connection closed
            {
                printf("Connection closed by server\n");
                stop = 1;
            }
            else if (nRx > 0)  // we got some data
            {
                if(reply[nRx-1] == '\0'){
                        printf("%s", reply);
                        finished = 1;
                }
                else{
                    reply[nRx] = '\0';
                    printf("%s", reply);
                }
            }
        }
        while (!finished && !stop);
    }
    // Remove file
    else if(command == 'r')    {
/************* Format request *****************/
    // Ask user for name of file to delete
    printf("\nEnter filename of file to delete (example.ext):");
    scanf("%s", filename);

    // First element of request array is transfer command
    request[0] = command;

    // Assign filename to the next available element of request array
    sprintf(request + 1, "%s", filename);
 /************* end request formatting *************/

    // Request deletion
    // Send request array
    retVal = send(clientSocket, request, strlen(request) + 1, 0);
    // send() arguments: socket handle, array of bytes to send,
    // number of bytes to send, and last argument of 0.

    if( retVal == SOCKET_ERROR) // check for error
        {
            printf("*** Error sending\n");
            printError();
        }
    }
    // Upload
    else if(command == 'u' || command == 'f')    {
        // 6. File transfer details
        // Ask user for name of file to upload
        printf("\nEnter filename of file to upload (example.ext):");
        scanf("%s", filename);

        // Open file for binary read & check for error
        if((fp = fopen(filename, "rb")) == NULL)  {
            printf("\nError opening upload file: %s.\n", filename);
            exit(EXIT_FAILURE);
        }

        // Calculate file size
        fseek(fp, 0, SEEK_END);  // set position to end of file
        filesize = ftell(fp);  // current position is filesize in bytes
        fseek(fp, 0, SEEK_SET);  // reset position to start of file

        // First element of request array is transfer command
        request[0] = command;

        // Convert filesize from integer to character array and assign
        // string to request array (terminating in null character)
        itoa(filesize, request + 1, 10);

        // Calculate number of digits in filesize integer
        no_digs_filesize = ceil(log10(filesize));

        // Assign filename to the next avaiable element of request array
        sprintf(request + (2 + no_digs_filesize), "%s", filename);

        // 7. Request upload
        // Send request array
        retVal = send(clientSocket, request, (3 + no_digs_filesize + strlen(filename)), 0);
        // send() arguments: socket handle, array of bytes to send,
        // number of bytes to send, and last argument of 0.
        if( retVal == SOCKET_ERROR) { // check for error
                printf("*** Error sending\n");
                printError();
        }
        else if (retVal != (3 + no_digs_filesize + strlen(filename)))
            printf("Err: retVal %d", (3 + no_digs_filesize + strlen(filename)));
        else printf("Sent %d bytes of upload request, waiting for reply from server...\n", retVal);

        // 8. Server response
        // Try to receive a byte
        // recv() arguments: socket handle, array to hold rx bytes,
        // maximum number of bytes to receive, last argument 0.
        nRx = recv(clientSocket, &indicator, 1, 0);
        // nRx will be number of bytes received, or error indicator

        if( nRx == SOCKET_ERROR)    {  // check for error
                printf("Problem receiving\n");
                //printError();
        }

        else if (nRx == 0)  {   // connection closed
                printf("Connection closed by server\n");
        }

        else if (nRx > 0)   {  // we got the server indicator response
                if(indicator == 'k') printf("\nServer has approved upload\n");
                else if(indicator == ' ') printf("ERROR: not enough space to upload %s to server.\n", filename);
                else if(indicator == 'f')printf("ERROR: %s already exists on the server.\n", filename);
        }

        // 9. File upload
        printf("Indicator: %c\n", indicator);
        if(indicator == 'k')    {
            // Send data in blocks of 3000 (refer to Kevin)
            acc_filesize = filesize;
            while(feof(fp) == 0 && ferror(fp) == 0) {
                read = (int)fread(data, 1, 3000, fp);
                filesize -= read;
                //printf("\nSending %d bytes of %s, %d bytes remaining\n", read, filename, filesize);
                if (ferror(fp)) {
                    perror("Send: Error reading input file");
                    fclose(fp);   // close input file
                    return 3;  // we are giving up on this
                }
                retVal = send(clientSocket, data, read, 0);
                if( retVal == SOCKET_ERROR)  // check for error
                {
                    printf("*** Error sending response\n");
                    printError();
                    return -1;
                }
                //printf("\nSending %d bytes of %s, %d bytes remaining\n", retVal, filename, filesize);
                print_progress(acc_filesize, acc_filesize - filesize);
             }
            printf("\rLOADED \n\n");
            TootOwnHorn();
        }
        else if(indicator == 'f')   {
            printf("Error: File already exists!\n");
        }
        else if(indicator == 'E')   {
            printf("Error: Unknown server error\n");
        }
    }

     // Download loop
    else if(command == 'd' || command == 'D')    {
/************* Format request *****************/
    // 6. File transfer details
    // Ask user for name of file to download
    printf("\nEnter filename of file to download (example.ext):");
    scanf("%s", filename);

    // First element of request array is transfer command
    request[0] = command;

    // Assign filename to the next available element of request array
    sprintf(request + 1, "%s", filename);
 /************* end request formatting *************/

    // 7. Request download
    // Send request array
    retVal = send(clientSocket, request, strlen(request) + 1, 0);
    // send() arguments: socket handle, array of bytes to send,
    // number of bytes to send, and last argument of 0.

    if( retVal == SOCKET_ERROR) // check for error
        {
            printf("*** Error sending\n");
            printError();
        }
    else printf("Sent %d bytes of download request, waiting for reply...\n", retVal);

 /************ start downloading section *************/
            do  // loop to receive entire reply
            {
                // Try to receive some bytes
                // recv() arguments: socket handle, array to hold rx bytes,
                // maximum number of bytes to receive, last argument 0.
                nRx = recv(clientSocket, reply, 100, 0);
                // nRx will be number of bytes received, or error indicator
                if(reply[0] == 'x' && firstblock == 1) {
                    printf("File does not exist\n");
                    break;
                }
                if( nRx == SOCKET_ERROR)  // check for error
                {
                    printf("Problem receiving\n");
                    stop = 1;  // exit the loop if problem
                }
                else if (nRx == 0)  // connection closed
                {
                    printf("Connection closed by server\n");
                    stop = 1;
                }
                else if (nRx > 0)  // we got some data
                {
                    for (j = 0; nullpos == -1; j++) // find end of file size marked by a null
                        if(reply[j] == '\0') nullpos = j;

                    if (firstblock == 1){
                        if((fp = fopen(filename, "wb")) == NULL) // open file and error check
                            printf("Error opening file for binary write.\n");
                            j = nullpos + 1; // data starts after null ended file size
                    }

                    written = fwrite(reply + j, 1, nRx - j, fp); // write to file

                    if (written != nRx - j)
                        fprintf(stderr, "Writing error: count: %d written: %d\n", nRx - j, written);

                    if (firstblock != 1) // not first block
                        totalbytes += nRx;
                    else{                // first block
                        filesize = atoi(reply);
                        firstblock = 0; // disable flag
                        totalbytes += nRx - (nullpos + 1); // subtract bytes for filesize
                    }
                    print_progress(filesize, totalbytes);
                }
                //printf("\nReceived %d bytes\n", totalbytes);
            }
            while ((totalbytes < filesize) && (stop == 0));
            if(totalbytes >= filesize)   {
                printf("\r\tLOADED \n\n");
                fclose(fp);
            }
            // continue until all file received error or connection closed
    }
/************ end downloading section *************/

    // Shut down the sending side of the TCP connection first
    retVal = shutdown(clientSocket, SD_SEND);
    if( retVal != 0)  // check for error
    {
        printf("*** Error shutting down sending\n");
        printError();
    }

    // Then close the socket
    retVal = closesocket(clientSocket);
    if( retVal != 0)  // check for error
    {
        printf("*** Error closing socket\n");
        printError();
    }
    else printf("Socket closed\n\n\n");
    fflush(stdin);
    //getch();
}
    // Finally clean up the winsock system
    retVal = WSACleanup();
    printf("WSACleanup returned %d\n",retVal);

    // Prompt for user input, so window stays open when run outside CodeBlocks
    printf("\nPress return to exit:");
    gets(request);
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

void TootOwnHorn(void){
    Beep(400,400);
    Beep(800,800);
    Beep(1600,1600);
}

void print_progress(int filesize, int totalbytes){
    // typecasting could be avoided by reordering division and multiplication but fails at lower file sizes than this method
    printf("\rLOADING: %3d%% ", (int)(((float)totalbytes/filesize)* 100));
}
