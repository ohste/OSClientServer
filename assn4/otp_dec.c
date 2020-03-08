#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(const char *msg) { perror(msg); exit(0); } // Error function used for reporting issues

int main(int argc, char *argv[])
{
	//Declare all the initial variables
	int socketFD, portNumber, charsWritten, charsRead;
	FILE* keyOpen, *plainOpen;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
	char buffer[100000];
	char* plaintext, *keytext, finaltext[100000];
	size_t plainlen = 0;
	size_t keylen = 0;

	if (argc < 4) { fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); exit(0); } // Check usage & args

	// Set up the server address struct
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[3]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverHostInfo = gethostbyname("localhost"); // Convert the machine name into a special form of address
	if (serverHostInfo == NULL) { fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(0); }
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

	// Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (socketFD < 0) error("CLIENT: ERROR opening socket");

	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){ // Connect socket to address
		fprintf(stderr, "Rejected connection to %d\n", portNumber);
		exit(2);
	}

	// Get input message from user
	memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer array
	strcpy(buffer, "dec");

	// Send message to server
	charsWritten = send(socketFD, buffer, strlen(buffer), 0); // Write to the server
	if (charsWritten < 0) error("CLIENT: ERROR writing to socket");
	if (charsWritten < strlen(buffer)) printf("CLIENT: WARNING: Not all data written to socket!\n");

	// Get return message from server
	memset(buffer, '\0', sizeof(buffer)); // Clear out the buffer again for reuse
	charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0); // Read data from the socket, leaving \0 at end
	if (charsRead < 0) error("CLIENT: ERROR reading from socket");

	//If we did not get "dec" back from server, close server and return error message
	if (strcmp(buffer,"dec")!=0){
		fprintf(stderr, "Rejected connection to %d\n", portNumber);
		close(socketFD);
		exit(2);
	}
	//Open each of the files
	keyOpen = fopen(argv[2],"r");
	plainOpen = fopen(argv[1],"r");
	//If we couldn't open the files, close the program and return an error
	if (keyOpen==NULL || plainOpen==NULL){
		fprintf(stderr, "Could not open file\n");
		close(socketFD);
		exit(1);
	}
	//Get the lines from each of the files
	getline(&plaintext, &plainlen, plainOpen);
	getline(&keytext, &keylen, keyOpen);
	//Close the files
	fclose(keyOpen);
	fclose(plainOpen);

	//Check if key is shorter than the plaintext
	if (strlen(keytext) < strlen(plaintext)){
		fprintf(stderr, "Key length is too short\n");
		close(socketFD);
		exit(1);
	}

	//Check that all the characters are within the specified characters
	int i;
	char within[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
	for(i = 0; i< sizeof(plaintext); i++){
		if (strchr(within, plaintext[i])==NULL){
			fprintf(stderr, "Bad character in %s\n", argv[1]);
			close(socketFD);
			exit(1);
		}
	}
	for(i = 0; i< sizeof(keytext); i++){
		if (strchr(within, keytext[i])==NULL){
			fprintf(stderr, "Bad character in %s\n", argv[1]);
			close(socketFD);
			exit(1);
		}
	}
	//Add the terminating message to buffer and send to server
	memset(buffer, '\0', sizeof(buffer));
	strcpy(buffer, plaintext);
	strcat(buffer, "@@");
	charsWritten = send(socketFD, buffer, strlen(buffer), 0);

	//Receive the confirmation message back from server
	memset(buffer, '\0', sizeof(buffer));
	charsRead = recv(socketFD, buffer, 1, 0);

	//Add the terminating message to buffer and send the key to the server
	memset(buffer, '\0', sizeof(buffer));
	strcpy(buffer, keytext);
	strcat(buffer, "@@");
	charsWritten = send(socketFD, buffer, strlen(buffer), 0);

	//Receive the final text from the server, continue looping if we don't have the full message
	memset(finaltext, '\0', sizeof(finaltext));
	while (strstr(finaltext, "@@")==NULL){
		memset(buffer, '\0', sizeof(buffer));
		charsRead = recv(socketFD, buffer, sizeof(buffer), 0);
		//printf("SERVER: Reading from buffer: %s\n", buffer);
		strcat(finaltext, buffer);
		if (charsRead <0){
			fprintf(stderr, "Socket was already closed Error 2\n");
			close(socketFD);
			exit(1);
		}
	}

	//Remove the terminating message and print the decrypted message
	int term = strstr(finaltext, "@@") - finaltext;
	finaltext[term] = '\0';
	printf("%s\n", finaltext);

	close(socketFD); // Close the socket
	return 0;
}
