#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues

/*******************************************
*Name: cleanup
*Function: Checks if any child processes have exited and cleans then up and
					 prints the completed message.
Input: The child processes array of PIDs, the childExitMethod, and the number
			 of currently running processes to be reset if a child has ended.
**********************************************/
void cleanup(pid_t* childProcesses, int* childExitMethod, int* numCurrRunning){
	int i;
	for (i = 0; i<5; i++){
		if(childProcesses[i]!=-5){
			pid_t completed = waitpid(childProcesses[i], childExitMethod, WNOHANG); //check completion
			if (completed!=0){
				//printf("Background pid %d is completed ", completed); //Output the completed child
				//execStatus(*childExitMethod);	//Get the status message from the completed child
				childProcesses[i]=-5;
				*childExitMethod = -5;//Set the process to -5
			}
		}
	}
	*numCurrRunning--;
}

int main(int argc, char *argv[])
{
	//Declare and set all the initial variables
	pid_t childProcesses[5]={-5,-5,-5,-5,-5};
	int numCurrRunning=0;
	int childExitMethod=-5;
	int listenSocketFD, establishedConnectionFD, portNumber, charsRead, charsWritten;
	int connectionArray[5];
	int connectionLimit=0;
	socklen_t sizeOfClientInfo;
	char buffer[100000], plaintext[100000], keytext[100000], newtext[100000];
	struct sockaddr_in serverAddress, clientAddress;

	if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[1]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

	// Set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (listenSocketFD < 0) error("ERROR opening socket");

	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		error("ERROR on binding");
	listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections

	while(1){
		// Accept a connection, blocking if one is not available until one connects
		sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
		cleanup(childProcesses, &childExitMethod, &numCurrRunning);	//Cleanup old child processes
		connectionArray[connectionLimit] = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
		establishedConnectionFD = connectionArray[connectionLimit];	//Get the new connection
		connectionLimit=(connectionLimit + 1) % 5;	//Add value to number of connections we have going
		if (establishedConnectionFD < 0) error("ERROR on accept"); // Print error if not accepted

		//Fork and use the child process to handle the new connection
		childProcesses[numCurrRunning] = fork();
		switch(childProcesses[numCurrRunning]){
			case 0:{
				// Get the message from the client and display it
				memset(buffer, '\0', sizeof(buffer));
				charsRead = recv(establishedConnectionFD, buffer, 3, 0); // Read the client's message from the socket
				if (charsRead < 0) error("ERROR reading from socket"); //If not reading from socket, throw error

				//Check is we're reading the decrypting client, error if not
				if (strcmp(buffer,"dec")!=0){
					fprintf(stderr, "Not reading from otp_dec!\n");
					charsRead= send(establishedConnectionFD, "not", 3, 0);
					close(establishedConnectionFD);
					exit(1); // Close the existing socket which is connected to the client
					 // Close the listening socket
				}
				else{
					// Send a Success message back to the client
					charsRead = send(establishedConnectionFD, "dec", 3, 0); // Send success back
					if (charsRead <0){
						fprintf(stderr, "Socket was already closed Error 1\n");
						close(establishedConnectionFD);
						exit(1);
					}

					//Reset the buffer and grab plaintext from the client
					memset(plaintext, '\0', sizeof(plaintext));

					//Check for the terminating character, keep looping if we haven't found it
					while (strstr(plaintext, "@@")==NULL){
						memset(buffer, '\0', sizeof(buffer));
						charsRead = recv(establishedConnectionFD, buffer, sizeof(buffer), 0);
						strcat(plaintext, buffer); //Continually add to plaintext
						if (charsRead <0){
							fprintf(stderr, "Socket was already closed Error 2\n");
							close(establishedConnectionFD);
							exit(1);
						}
					}

					//Remove the termianting character
					int term = strstr(plaintext, "@@") - plaintext-1;
					plaintext[term] = '\0';
					charsWritten = send(establishedConnectionFD, "Y", 1, 0);


					//Reset the buffer and grab the keytext from the client
					memset(keytext, '\0', sizeof(keytext));
					while (strstr(keytext, "@@")==NULL){
						memset(buffer, '\0', sizeof(buffer));
						charsRead = recv(establishedConnectionFD, buffer, sizeof(buffer), 0);
						//printf("SERVER: Reading from buffer: %s\n", buffer);
						strcat(keytext, buffer);
						if (charsRead <0){
							fprintf(stderr, "Socket was already closed Error 2\n");
							close(establishedConnectionFD);
							exit(1);
						}
					}
					//Remove the terminating character
					term = strstr(keytext, "@@") - keytext-1;
					keytext[term] = '\0';

					memset(newtext, '\0', sizeof(newtext));
					//Do the cipher arithmetic to decrypt the message
					int i;
					int count = strlen(plaintext);
					//For each character,
					for (i=0; i<count; i++){
						if(plaintext[i]==32) plaintext[i]=64; //Set each space to 64
						if(keytext[i]==32) keytext[i]=64;
						plaintext[i]-=64;	//subtract 64 to manage values from 0-26
						keytext[i]-=64;
						newtext[i] = ((plaintext[i]-keytext[i]+27) % 27) + 64;
						if (newtext[i]==64) newtext[i]=32; //Reset all 64s to spacds
					}
					//Put the new message into the buffer and send to the client
					memset(buffer, '\0', sizeof(buffer));
					strcpy(buffer, newtext);
					strcat(buffer, "@@");
					charsWritten = send(establishedConnectionFD, buffer, strlen(buffer), 0);
					sleep(1);
					close(establishedConnectionFD); // Close the existing socket which is connected to the client
					 // Close the listening socket
				}
			}
			default:{
				numCurrRunning++;
			}
		}
	}

	close(listenSocketFD);
	return 0;
}
