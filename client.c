#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <math.h>
#include <conio.h>

#define REQUEST_BUFFER_SIZE 112
#define BUFFER_SIZE 50000
#define IP_ADDRESS_SIZE 20
#define MAX_FILENAME_SIZE 100

void printError(void);  // function to display error messages
void TootOwnHorn(void); // function to toot own horn
void print_progress(int filesize, int totalbytes); // function to display progress of transfer
int no_digs(int); // function to calculate the number of digits in an integer

int main()  {

    WSADATA wsaData;                   // structure to hold winsock data
    FILE *fp;                          // file pointer for transfer file
    int retVal, read, nRx, j, written, remaining, filesize; // counters & flags for loops
    int totalbytes = 0, nullpos = -1, firstblock = 1, details = 0, stop = 0; // counters & flags for loops
    char request[REQUEST_BUFFER_SIZE]; // array to hold request bytes
    char reply[BUFFER_SIZE];           // array to hold received bytes
    char data[BUFFER_SIZE];            // array to hold data from data extraction
    char filename[MAX_FILENAME_SIZE];  // array to hold filename of transfer file
    char serverIP[IP_ADDRESS_SIZE];    // array to hold IP address of server
    char command = '\0';               // character to hold user command
    char debug = '\0';                 // character to hold debug choice
    char indicator;                    // character to hold server indicator response
    int serverPort;                    // integer to hold port used by server
    int no_digs_filesize;              // integer to hold number of digits in the filesize


    // 0. Welcome screen & debug mode option
    printf("*************************************************\n");
    printf("        INTERNET FILE TRANSFER - CLIENT\n");
    printf("*************************************************\n");
    printf("\nDebug mode (d) or normal mode (n):");

    debug = getchar();
    fflush(stdin);

    // Main loop to allow multiple interactions with server
    while(command != 'q'){
        // 1. Winsock set up
        // Initialise winsock, version 2.2, giving pointer to data structure
        retVal = WSAStartup(MAKEWORD(2,2), &wsaData);

        // Check for error
        if (retVal != 0)    {
            printf("*** WSAStartup failed: %d\n", retVal);
            printError();
            return 1;
        }
        else if (debug == 'd') printf("WSAStartup succeeded\n" );

        // 2. Client socket set up
        // Create a handle for a socket, to be used by the client
        SOCKET clientSocket = INVALID_SOCKET;  // handle called clientSocket

        // Create the socket, and assign it to the handle
        // AF_INET means IP version 4,
        // SOCK_STREAM means socket works with streams of bytes,
        // IPPROTO_TCP means TCP transport protocol.
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {  // Check for error
            printf("*** Failed to create socket\n");
            printError();
        }
        else if (debug == 'd') printf("Socket created\n" );

        // 3. Server details
        // If user has not yet been asked for server details or if user wants
        // to change the server details, ask user to input address & port no.
        if(details == 0) {
            // Ask user for IP address
            printf("\nEnter IP address of server: ");
            scanf("%20s", serverIP);

            // Ask user for port number
            printf("Enter port number: ");
            scanf("%d", &serverPort);

            // Reset details marker to avoid having to re-enter server details
            details = 1;
        }

        // Build a structure to identify the service required
        // This has to contain the IP address and port of the server
        struct sockaddr_in service;  // IP address and port structure
        service.sin_family = AF_INET;  // Specify IP version 4 family
        // Function inet_addr() converts IP address string to 32-bit number
        service.sin_addr.s_addr = inet_addr(serverIP);  // set IP address
        // Function htons() converts 16-bit integer to network format
        service.sin_port = htons(serverPort);  // Set port number

        // 4. Server connection
        // Try to connect to the server
        printf("\nTrying to connect to %s on port %d...", serverIP, serverPort);
        retVal = connect(clientSocket, (SOCKADDR *) &service, sizeof(service));

        // Check for error
        if(retVal != 0) {
            printf("Error connecting\n");
            printError();
            return 2;
        }

        else printf("\nConnected to %s on port %d          \n", serverIP, serverPort);

        // 5. Command choice
        // Ask user for command
        fflush(stdin);
        printf("\nEnter command (enter 'h' for help): ");
        command = getchar();
        fflush(stdin);

        // 5.1 Help
        if (command == 'h')  {
            printf("\nCHARACTER\tCOMMAND\n"
                   "   'd'\t  Download file from server\n"
                   "   'u'\t  Safe upload file to server (will not upload if file already exists)\n"
                   "   'f'\t  Force upload file to server (will replace file if already exists)\n"
                   "   'l'\t  List files on server\n"
                   "   'r'\t  Remove file from server\n"
                   "   'q'\t  Quit program\n"
                   "   'c'\t  Change server\n"
                   );

            printf("\nEnter command: ");
            command = getchar();
            fflush(stdin);
        }

        // 5.2 List files from server
        if (command == 'l') {
            // First element of request array is command, second is null
            request[0] = command;
            request[1] = '\0';

            // Send request array
            retVal = send(clientSocket, request, strlen(request) + 1, 0);
            // send() arguments: socket handle, array of bytes to send,
            // number of bytes to send, and last argument of 0.
            if(debug == 'd') printf("\rSent %d bytes", retVal);

            // Check for error
            if(retVal == SOCKET_ERROR)  {
                printf("*** Error sending\n");
                printError();
            }

            // Loop to receive list of files
            stop = 0;
            do {
                // Try to receive some bytes
                // recv() arguments: socket handle, array to hold rx bytes,
                // maximum number of bytes to receive, last argument 0.
                nRx = recv(clientSocket, reply, 100, 0);
                // nRx will be number of bytes received, or error indicator

                // Check for error
                if(nRx == SOCKET_ERROR) {
                    printf("Problem receiving\n");
                    stop = 1;
                }

                // Check if connection closed
                else if (nRx == 0)  {
                    printf("Connection closed by server\n");
                    stop = 1;
                }

                // Otherwise, we got some data
                else if (nRx > 0)   {
                    // Check if the received block is the final block
                    if(reply[nRx-1] == '\0') {
                        printf("%s", reply);
                        stop = 1; // If final block, exit receiving loop
                    }

                    // If not final block, print & keep receiving
                    else {
                        reply[nRx] = '\0';
                        printf("%s", reply);
                    }
                }
            }while (!stop);
            stop = 0;
        }


        // 5.3 Remove file from server
        else if(command == 'r') {
            // Ask user for name of file to delete
            printf("\nEnter filename of file to delete (example.ext): ");
            scanf("%s", filename);

            // First element of request array is transfer command
            request[0] = command;

            // Assign filename to the next available element of request array
            sprintf(request + 1, "%s", filename);

            // Send request array
            retVal = send(clientSocket, request, strlen(request) + 1, 0);
            // send() arguments: socket handle, array of bytes to send,
            // number of bytes to send, and last argument of 0.

            if(debug == 'd') printf("Sent %d bytes", retVal);

            // Check for error
            if(retVal == SOCKET_ERROR) {
                printf("*** Error sending\n");
                printError();
            }
        }

        // 5.4 Upload (safe or force)
        else if(command == 'u' || command == 'f') {
            // Ask user for name of file to upload
            printf("\nEnter filename of file to upload (example.ext): ");
            scanf("%s", filename);

            // Open file for binary read & check for error
            if((fp = fopen(filename, "rb")) == NULL)  {
                printf("\nError opening upload file: %s.\n", filename);
                break;
            }

            if(debug == 'd') printf("\nSuccessfully opened %s\n", filename);

            // Calculate file size
            fseek(fp, 0, SEEK_END);  // Set position to end of file
            filesize = ftell(fp);    // Current position is filesize in bytes
            fseek(fp, 0, SEEK_SET);  // Reset position to start of file
            if(debug == 'd') printf("Calculated size of %s: %d bytes\n", filename, filesize);

            // First element of request array is transfer command
            request[0] = command;

            // Convert filesize from integer to character array and assign
            // string to request array (terminating in null character)
            itoa(filesize, request + 1, 10);

            // Calculate number of digits in filesize integer
            no_digs_filesize = no_digs(filesize);

            // Assign filename to the next avaiable element of request array
            sprintf(request + (2 + no_digs_filesize), "%s", filename);

            // Send request array
            retVal = send(clientSocket, request, (3 + no_digs_filesize + strlen(filename)), 0);
            // send() arguments: socket handle, array of bytes to send,
            // number of bytes to send, and last argument of 0.

            // Check for error
            if(retVal == SOCKET_ERROR) {
                    printf("*** Error sending\n");
                    printError();
            }
            else if(debug == 'd') printf("Sent %d bytes of request, waiting for reply from server...\n", retVal);

            // Server response
            // Try to receive a byte
            // recv() arguments: socket handle, array to hold rx bytes,
            // maximum number of bytes to receive, last argument 0.
            nRx = recv(clientSocket, &indicator, 1, 0);
            // nRx will be number of bytes received, or error indicator
            if(debug == 'd' && nRx != SOCKET_ERROR) printf("Received %d bytes", nRx);

            // Check for error
            if(nRx == SOCKET_ERROR) {
                printf("Problem receiving\n");
            }

            // Check if connection closed
            else if (nRx == 0)  {
                printf("Connection closed by server\n");
            }

            // Otherwise we got the server indicator response
            else if (nRx > 0)   {
                if(indicator == 'k' && debug == 'd') printf("\nServer has approved upload\n");
                else if(indicator == 'f') printf("\n%s already exists on the server. Use force upload 'f' to overwrite\n\n", filename);
            }

            // File upload
            // 'k' indicates server is approving upload
            if(indicator == 'k')    {
                // Send data in blocks of size BUFFER_SIZE
                remaining = filesize;
                while(feof(fp) == 0 && ferror(fp) == 0) {
                    // Read data from file
                    read = (int)fread(data, 1, BUFFER_SIZE, fp);

                    // Check for error
                    if (ferror(fp)) {
                        perror("Send: Error reading input file");
                        fclose(fp);   // Close input file
                        return 3;  // We are giving up on this
                    }

                    // Debug
                    if(debug=='d') printf("Read %d bytes", read);

                    // Send extracted data & check for errors
                    retVal = send(clientSocket, data, read, 0);
                    if(retVal == SOCKET_ERROR) {
                        printf("*** Error sending response\n");
                        printError();
                        return 4;
                    }

                    // Update counters and print progress
                    remaining -= read;
                    print_progress(filesize, filesize - remaining);
                 }
                // Toot the horn and close the file
                TootOwnHorn();
                fclose(fp);
            }
        }

        // 5.5 Download
        else if(command == 'd') {
            // Ask user for name of file to download
            printf("\nEnter filename of file to download (example.ext): ");
            scanf("%s", filename);

            // First element of request array is transfer command
            request[0] = command;

            // Assign filename to the next available element of request array
            sprintf(request + 1, "%s", filename);

            // Send request array
            retVal = send(clientSocket, request, strlen(request) + 1, 0);
            // send() arguments: socket handle, array of bytes to send,
            // number of bytes to send, and last argument of 0.
            if(debug == 'd') printf("Sent %d bytes of request, waiting for reply from server...\n", retVal);

            // Check for error
            if(retVal == SOCKET_ERROR) {
                printf("*** Error sending\n");
                printError();
            }

            // Loop to receive entire reply
            totalbytes = 0;
            firstblock = 1;
            do  {
                // Try to receive some bytes
                // recv() arguments: socket handle, array to hold rx bytes,
                // maximum number of bytes to receive, last argument 0.
                nRx = recv(clientSocket, reply, 50000, 0);
                // nRx will be number of bytes received, or error indicator
                if(reply[0] == 'x' && firstblock == 1) {
                    printf("File does not exist\n\n");
                    break;
                }

                // Check for error
                if(nRx == SOCKET_ERROR) {
                    printf("Problem receiving\n");
                    stop = 1;  // exit the loop if problem
                }

                // Check if connection closed
                else if (nRx == 0) {
                    printf("Connection closed by server\n");
                    stop = 1;
                }

                // Otherwise we got some data
                else if (nRx > 0) {
                    // Find end of file size marked by a null
                    for (j = 0; nullpos == -1; j++) if(reply[j] == '\0') nullpos = j;

                    if (firstblock == 1) {
                        // Open file and error check
                        if((fp = fopen(filename, "wb")) == NULL) printf("Error opening file for binary write.\n");
                        j = nullpos + 1; // Data starts after null ended file size
                    }

                    written = fwrite(reply + j, 1, nRx - j, fp); // Write to file
                    if(debug == 'd') printf("\t\tWrote %d bytes to file", written);

                    // Check for error
                    if (written != nRx - j) fprintf(stderr, "Writing error - count: %d written: %d\n", nRx - j, written);

                    // Not first block
                    if (firstblock != 1) totalbytes += nRx;

                    // First block
                    else {
                        filesize = atoi(reply);
                        firstblock = 0; // Disable flag
                        totalbytes += nRx - (nullpos + 1); // Subtract bytes for filesize
                    }
                }
                // Print progress indicator
                print_progress(filesize, totalbytes);

                // Continue until all file received, error or connection closed
            } while ((totalbytes < filesize) && (stop == 0));

            if(totalbytes >= filesize)   {
                TootOwnHorn();
                fclose(fp);
            }
        }

        // 5.6 Change server
        else if(command == 'c') {
            // Set details marker to allow re-entry of IP address and port details
            details = 0;
        }

        // 6. Close connection
        // Shut down the sending side of the TCP connection first
        retVal = shutdown(clientSocket, SD_SEND);

        // Check for error
        if(retVal != 0) {
            printf("*** Error shutting down sending\n");
            printError();
        }

        // Then close the socket
        retVal = closesocket(clientSocket);

        // Check for error
        if(retVal != 0) {
            printf("*** Error closing socket\n");
            printError();
        }

        else if(debug == 'd') printf("\nSocket closed\n");
    }

    // Finally clean up the winsock system
    retVal = WSACleanup();
    if(debug == 'd') printf("WSACleanup returned %d\n",retVal);

    // Prompt for user input, so window stays open when run outside CodeBlocks
    printf("\nPress return to exit:");
    gets(request);
    return 0;
}

// Function to calculate the number of digits in an integer
int no_digs(int number) {
    return floor(1 + log10(number));
}

// Function to print informative error messages when something goes wrong
void printError(void) {
    char lastError[1024];
    int errCode;

    errCode = WSAGetLastError();  // Get the error code for the last error
    FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    errCode,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    lastError,
    1024,
    NULL);  // Convert error code to error message
    printf("WSA Error Code %d = %s\n", errCode, lastError);
}

// Function to play sound when file transfer is complete
void TootOwnHorn(void){
    Beep (553,300);Sleep(150);
    Beep (661,300);Sleep(150);
    Beep (883,300);Sleep(150);
}

void print_progress(int filesize, int totalbytes){
    // typecasting could be avoided by reordering division and multiplication
    // but fails at lower file sizes than this method
    printf("\rLOADING: %3d%% ", (int)(((float)totalbytes/filesize)* 100));
}
