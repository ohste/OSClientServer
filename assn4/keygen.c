#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void gen_str(int len, char* myString){
	char options[27] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ"; //Array of possible characters
	int i;

	//Loop for the string length
	for(i=0; i<len-2; i++){
		int pos = rand()%27; //Get a random character from 0-27
		myString[i] = options[pos];	//Set the value of the string position to that character
	}
	myString[i] = '\n';	//Add the null terminating character
}

int main(int argc, char* argv[]){
	//Check if we have proper arguments
	if (argc<2){
		fprintf(stderr, "Improper arguments\n");
		exit(1);
	}
	srand(time(NULL)); //Set the time
	size_t len = atoi(argv[1])+2; //Set the length of the string
	char myStr[len];	//Create string
	memset(myStr, '\0', len); //nullify strings
	gen_str(len, myStr);	//Generate the key
	printf("%s", myStr);	//Print the key to stdout
	return 0;
}
