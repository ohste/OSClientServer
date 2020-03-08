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
	pid_t childProcesses[5]={-5,-5,-5,-5,-5}; //Set the array to hold all child process PID
	int numCurrRunning=0;		//Keep track of number of processes currently running
	int childExitMethod=-5;	//Keep track of the exit status
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
		cleanup(childProcesses, &childExitMethod, &numCurrRunning); //Cleanup any ended child processes
		connectionArray[connectionLimit] = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
		establishedConnectionFD = connectionArray[connectionLimit];  //Set the current connection to the connection we just made
		connectionLimit=(connectionLimit + 1) % 5;	//Add to the connection limit
		if (establishedConnectionFD < 0) error("ERROR on accept");	//Catch accept error

		//Fork the child process to handle the connection
		childProcesses[numCurrRunning] = fork();
		switch(childProcesses[numCurrRunning]){
			case 0:{

				// Get the message from the client and display it
				memset(buffer, '\0', sizeof(buffer));
				charsRead = recv(establishedConnectionFD, buffer, 3, 0); // Read the client's message from the socket
				if (charsRead < 0) error("ERROR reading from socket");

				//If we aren't reading from enc, close the connection
				if (strcmp(buffer,"enc")!=0){
					fprintf(stderr, "Not reading from otp_enc!\n");
					charsRead= send(establishedConnectionFD, "not", 3, 0);
					close(establishedConnectionFD);
					exit(1); // Close the existing socket which is connected to the client
					 // Close the listening socket
				}
				//Otherwise, send a confirmation message back to client
				else{
					// Send a Success message back to the client
					charsRead = send(establishedConnectionFD, "enc", 3, 0); // Send success back
					if (charsRead <0){
						fprintf(stderr, "Socket was already closed Error 1\n");
						close(establishedConnectionFD);
						exit(1);
					}

					//Reset the buffer and grab plaintext from the client
					memset(plaintext, '\0', sizeof(plaintext));

					//If the string does not have the terminating message, keep pulling receiving.
					while (strstr(plaintext, "@@")==NULL){
						memset(buffer, '\0', sizeof(buffer));
						charsRead = recv(establishedConnectionFD, buffer, sizeof(buffer), 0);
						strcat(plaintext, buffer);	//Add the buffer to plain text
						//Check for some error
						if (charsRead <0){
							fprintf(stderr, "Socket was already closed Error 2\n");
							close(establishedConnectionFD);
							exit(1);
						}
					}

					//Remove the terminating message
					int term = strstr(plaintext, "@@") - plaintext-1;
					plaintext[term] = '\0';

					//Respond that we got the full message
					charsWritten = send(establishedConnectionFD, "Y", 1, 0);


					//Reset the buffer and grab the keytext from the client
					memset(keytext, '\0', sizeof(keytext));
					while (strstr(keytext, "@@")==NULL){
						memset(buffer, '\0', sizeof(buffer));
						charsRead = recv(establishedConnectionFD, buffer, sizeof(buffer), 0);
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

					//Do the cipher arithmetic
					memset(newtext, '\0', sizeof(newtext));
					int i;
					int count = strlen(plaintext);
					for (i=0; i<count; i++){
						if(plaintext[i]==32) plaintext[i]=64;
						if(keytext[i]==32) keytext[i]=64;
						plaintext[i]-=64;
						keytext[i]-=64;
						newtext[i] = (plaintext[i]+keytext[i]) % 27 + 64;
						if (newtext[i]==64) newtext[i]=32;
					}
					//Reset the buffer and send the final message
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
