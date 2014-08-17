/**
 * This files is run at user side. 
 * It takes arguments:
 * 1. IP address of machine in which  program is running
 * 2. path of Configuration file
 * 3. total number of nodes.
 * 
 * It then takes input two types of command: store or get.
 * 
 * Store:
 * In store it takes input 
 * 1. file which needs to be stored 
 * 2. node number which needs to be contacted.
 * 
 * A UDP connection  is established and transfers
 * message: store;md5sum;TcpIP:TcpPort;
 * and waits for TCP connection. After it is 
 * established it transfers the file.
 * 
 * Get:
 * In store it takes input 
 * 1. file which needs to be receive
 * 2. node number which needs to be contacted.
 * 
 * In get part it transfers via UDP
 * message: get;md5sum;TcpIP:TcpPort;
 * and waits for a TCP connection. After it is 
 * established it recieves files and saves it in folder recievedFiles.
 * 
 * If the user wants to perform more actions,
 * y is entered or else n to quit.
 * 
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/time.h>		
#include <fstream>		
#include <algorithm>	
#include <stdlib.h>
#include <sstream>
#include <string.h>		
#include <string>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <stdlib.h>

//Maximum number of bytes of data to be transferred
#define LENGTH 512

using namespace std;

/**uses perror to return error
 * Input: const char *msg - Custom message to be printed before the error message itself.
 */
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

/**returns IP address from the string which is a particular line from config file
 * Input: string s - Particular line from config file
 */
string getIP(string s) 
{
	string ipaddr;

	char c = s[0];
	int i = 1;
	while (c != ':') {
		ipaddr = ipaddr + c;
		c = s[i];
		i++;
	}
	return ipaddr;
}

/**returns hashvalue from the string returned from executing terminal command
 * Input: string s - string returned after executeing terminal command md5sum file
 */
string getHash(string s)
{
	string hash;
	int i = 1;
	char c = s[0];
	while (c != ' ') {
		hash = hash + c;
		i++;
		c = s[i];
	}
	return hash;
}

/**returns port from the string which is a particular line from config file
 * Input: string s - Particular line from config file
 */
string getPort(string s)
{
	string port;
	char c = s[0];
	int i = 1;
	while (c != ':') {
		i++;
		c = s[i];
	}
	i++;
	c = s[i];
	while (c != ' ') {
		port = port + c;
		i++;
		c = s[i];
	}
	return port;
}

/**returns folder from the string which is a particular line from config file
 * Input: string s - Particular line from config file
 */
string getFolder(string s)
{
	int pos = s.find(" ");
	if(pos==-1)
		pos = s.find("\t");
	return s.substr(pos+1);
}

/**function to execute terminal commands
 * Input : string cmd - Command which needs to be executed at terminal
 */
std::string exec(string cmd)
{
	FILE *pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return "ERROR";
	char buffer[128];
	std::string result = "";
	while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe) != NULL)
			result += buffer;
	}
	pclose(pipe);
	return result;
}

/**Main function which handles store and get functions
 * Input(through command line):
 * 1. argv[1] - IP address of machine in which  program is running
 * 2. argv[2] - path of Configuration file
 * 3. argv[3] - total number of nodes.
 */
int main(int argc, char *argv[])
{

	char *MYIP ;					 	//contains IP of the system
	string opType; 					 	//type of operation user wants to perform [store/get]
	string initialPath; 			 	//path of file name user wants to store
	int contactNode;				 	//Node to contacted at a given time
	int maxNodes;					 	//Keeps a count of number of nodes at a given time
	string destip; 					 	//ip address of node to be contacted at a given time
	int destPort; 					 	//port number of node to be contacted at a given time
	int sd, rc, i;					 	//temporary variables
	struct sockaddr_in cliAddr;		 	//Holds socket address information of UDP node created
	struct sockaddr_in remoteServAddr;  //Holds socket address information for remote node to be contacted
	struct hostent *h;					//Used to represent host which needs to be contacted
	char *configFilePath; 				//To store the path of File where information of Node is stored

	string **nodeinfo; 					//To store ip,port and folder information of all nodes


	/* check command line args */
	if(argc<4) {
		printf("usage : %s IP ConfigFile TotalNodes\n", argv[0]);
		return 1;
	}

	/*Initilize IP and  configFilePath and maxNodes with commandline arguments*/
	MYIP=argv[1];
	configFilePath=argv[2];
	maxNodes = atoi(argv[3]);

	/* generate random number between 6000 and 7000 to serve as port for TCP Server */
	srand (time(NULL));
	int  TSERVER_PORT= rand() % 1000 + 6000;

	/** Variables for TCP Server */
	int tsd;						//TCP socket descriptor
	int tnewSd;						//stores 1/-1 depending on function [accept/bind etc.] executed properly
	socklen_t tcliLen; 				//store length of socket address 
	struct sockaddr_in tcliAddr; 	//Holds socket address information for client
	struct sockaddr_in tservAddr; 	//Holds socket address information for TCP server
	char tline[LENGTH];				
	char revbuf[LENGTH];			//Receiver buffer
	char charport[7];
	sprintf(charport,"%d",TSERVER_PORT);
	string localport(charport);

	/*************************************************************************************************************************/


	/**Read content of configuration file and assign it's contents to nodeinfo */

	ifstream myfile(configFilePath);						//to read from config file
	if (myfile.is_open()) {									//check if file is open
		nodeinfo = new string *[maxNodes]; 					//initialize nodeinfo 
		int i = 0;
		myfile.seekg(0, ios::beg);							//set position of next character to the beginning of file
		string content;
		while (getline(myfile, content)) {
			nodeinfo[i] = new string[3];
			nodeinfo[i][0] = getIP(content);
			nodeinfo[i][1] = getPort(content);
			nodeinfo[i][2] = getFolder(content);
			i++;
		}
		myfile.close();
	} else { 												//print error message if unable to open the file
		cout << "Unable to open file";
		return 1;
	}


	/****************************************************************************************************/

	/**Create TCP Server and listen on a port*/

	tsd = socket(PF_INET, SOCK_STREAM, 0); 					//creates an endpoint for communication and returns a file descriptor for the TCP socket
	if (tsd < 0) { 											//error in opening socket
		perror("cannot open socket ");
		return 1;
	}

	tservAddr.sin_family = AF_INET; 										//set family
	tservAddr.sin_addr.s_addr = inet_addr(MYIP); 							//set IP address of TCP Server
	tservAddr.sin_port = htons(TSERVER_PORT); 								//set port number of TCP server

	string localip(inet_ntoa(tservAddr.sin_addr));							//ip address of TCP server in string
	if (bind(tsd, (struct sockaddr *) &tservAddr, sizeof(tservAddr)) < 0) { //associate tsd socket with TCP server having address tservAddr
		perror("cannot bind port "); 										//print error
		return 1;
	}
	listen(tsd, 5);

	/*******************************************************************************************************************/

	/**Create a UDP Client to transfer message*/

	char ans = 'y';//decides whether to continue the loop of user requests
	while (ans == 'y') {
		cout << "Enter type of Operation" << endl;
		cin >> opType;
		if (opType == "store") {

			cout << "Enter path where file is stored" << endl;
			cin >> initialPath;

			cout << "Enter Node it should contact" << endl;
			cin >> contactNode;

			if (contactNode > maxNodes) {//checks if a given node exists
				cout << "Node to be contacted doesnot exists" << endl;
				continue;	
			}
			
			cout<<"\n*********************************\n";

			destip = nodeinfo[contactNode][0];					//read ip address of destination node from nodeinfo
			destPort = atoi((nodeinfo[contactNode][1]).c_str());//read port of destination node from nodeinfo converted to integer

			/**check if the ip address is present by performing a DNS lookup*/
			h = gethostbyname(destip.c_str());	
			if (h == NULL) {
				cout<<"Unknown host "<<destip<<endl;
				return 1;
			}

			cout<<"Sending data to IP "<<destip<<endl;

			//remoteServAddr filled with information required to connect to destination node
			remoteServAddr.sin_family = h->h_addrtype;										//set family of remote node
			memcpy((char *) &remoteServAddr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);//set ip address of remote node
			remoteServAddr.sin_port = htons(destPort);										//set port number of remote node


			/* socket creation */
			sd = socket(PF_INET, SOCK_DGRAM, 0);//creates an endpoint for communication and returns a file descriptor for the UDP socket
			if (sd < 0) {						//if unable to open socket
				printf("cannot open socket \n");
				return 1;
			}

			/* set IP address and port information of UDP client created */
			cliAddr.sin_family = AF_INET;
			cliAddr.sin_addr.s_addr = inet_addr(MYIP);
			cliAddr.sin_port = htons(0);

			/*associate sd socket with UDP server having address cliAddr */
			rc = bind(sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
			if (rc < 0) {
				printf("Cannot bind port \n");
				return 1;
			}

			/* send data */
			/** check if file exists*/
			FILE * pFile;
			pFile = fopen (initialPath.c_str(),"r");
			if (pFile==NULL)
			{
				cout<<"File Doesnot exists"<<endl;
				continue;
			}
			fclose (pFile);


			string hashval = getHash(exec("md5sum " + initialPath));//get hashvalue of file by terminal command [md5sum filename]
			string data = "store;" + hashval + ";" + localip + ":" + localport + ";";//message to be send to node
			cout<<"Message sent: "<<data<<endl;

			//send data to remoteServAddr
			rc = sendto(sd, data.c_str(), data.length() + 1, 0, (struct sockaddr *) &remoteServAddr, sizeof(remoteServAddr));	/////// here keep checking for how may bytes went

			if (rc < 0) {//in case of faliure to send data
				printf("%s: cannot send data %d n", argv[0], contactNode);
				close(sd);
				return 1;
			}


			/************************************************************************************************************/

			/**TCP wait for the connection to be initiated*/
			printf("Waiting for data on port TCP %u \n", TSERVER_PORT);

			tcliLen = sizeof(tcliAddr);
			tnewSd = accept(tsd, (struct sockaddr *) &tcliAddr, &tcliLen);//accept a TCP connection from remote node

			if (tnewSd < 0) {
				perror("cannot accept connection");
				return 1;
			}


			// Send File to Server 
			char *fname = new char[initialPath.length() + 1];	//initalize fname with filename.length()+1 space
			strcpy(fname,initialPath.c_str());					//store filename in fname
			char sdbuf[LENGTH];									//buffer which store file contents
			printf("Sending %s to the Server... \n", fname);
			FILE *f = fopen(fname, "r");
			if (f == NULL) {
				printf("ERROR: File %s not found.n", fname);
				exit(1);
			}

			bzero(sdbuf, LENGTH);//initialize sdbuf
			int blocksize;
			while ((blocksize = fread(sdbuf, sizeof(char), LENGTH, f)) > 0) {	//read a block from file
				if (send(tnewSd, sdbuf, blocksize, 0) < 0) { 					//send the block to remote node
					printf("ERROR: Failed to send file %s. \n", fname);
					break;
				}
				bzero(sdbuf, LENGTH);
			}
			printf("File %s from Client was Sent \n", fname);
			cout<<"********************************* \n";
		}

		/*************************************************************************************************************************************************/

		/**If the command given is get */
		else if (opType == "get") {

			cout << "Enter file to be recieved" << endl;
			cin >> initialPath;

			cout << "Enter Node it should contact" << endl;
			cin >> contactNode;
			
			cout<<"\n********************************* \n";

			if (contactNode > maxNodes) {
				cout << "Node to be contacted doesnot exists" << endl;
				continue;	
			}

			destip = nodeinfo[contactNode][0];					//ip address of remote node stored
			destPort = atoi((nodeinfo[contactNode][1]).c_str());//port number of remote node stored in destPort

			h = gethostbyname(destip.c_str());					//Perform DNS lookup whether ip address is present
			if (h == NULL) {
				printf("%s: unknown host '%s' n", argv[0], argv[1]);
				return 1;
			}

			printf("sending data to '%s' (IP : %s) \n", h->h_name, inet_ntoa(*(struct in_addr *) h->h_addr_list[0]));

			//set family, ip address and port of remote node
			remoteServAddr.sin_family = h->h_addrtype;
			memcpy((char *) &remoteServAddr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
			remoteServAddr.sin_port = htons(destPort);


			/* UDP socket creation */
			sd = socket(PF_INET, SOCK_DGRAM, 0);//creates an endpoint for communication and returns a file descriptor for the UDP socket
			if (sd < 0) {
				printf("%s: cannot open socket n", argv[0]);
				return 1;
			}

			/* set family, ip address and port of the UDP client */
			cliAddr.sin_family = AF_INET;
			cliAddr.sin_addr.s_addr = inet_addr(MYIP);
			cliAddr.sin_port = htons(0);

			/*associate sd socket with UDP server having address cliAddr */
			rc = bind(sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));//associate socket sd with cliAddr
			if (rc < 0) {												//in case of error in binding
				printf("Cannot bind port \n");
				return 1;
			}


			string hashval=initialPath;
			string data = "get;" + hashval + ";" + localip + ":" + localport + ";";//data to be sent
			cout<<"Message sent: "<<data.c_str() << endl;

			/**send data to remoteServAddr */
			rc = sendto(sd, data.c_str(), data.length() + 1, 0, (struct sockaddr *) &remoteServAddr, sizeof(remoteServAddr));	

			if (rc < 0) {	//in case of faliure to send data
				printf("Cannot send data \n");
				close(sd);
				return 1;
			}

			/********************************************************************************************************************************/
			/**TCP connection has to be started here */
			printf("Waiting for connection on port TCP %u \n", TSERVER_PORT);

			tcliLen = sizeof(tcliAddr);
			tnewSd = accept(tsd, (struct sockaddr *) &tcliAddr, &tcliLen);//accept a new connection and return a new socket file descriptor to tnewSd

			if (tnewSd < 0) {
				perror("cannot accept connection ");
				return 1;
			}

			/*Receive File from Node */
			string base="recievedFiles/";
			char *fr_name = new char[(base+hashval).length() + 1]; 
			strcpy(fr_name,(base+hashval).c_str());
			FILE *fr = fopen(fr_name, "w"); 									//open file with name base+hashval in write mode
			if (fr == NULL) 													//in case file cannot be opened print error
				printf("File %s Cannot be opened file on server \n", fr_name);
			else {
				bzero(revbuf, LENGTH); 											//Set revbuf with null values.
				int fr_block_sz = 0;
				while ((fr_block_sz = recv(tnewSd, revbuf, LENGTH, 0)) > 0) {	//recieve file of maximum length LENGTH
					int write_sz = fwrite(revbuf, sizeof(char), fr_block_sz, fr);//write recieved characters to a file
					if (write_sz < fr_block_sz) {								//in case written number of byter are less than recieved number of bytes
						error("File write failed on server.n");
					}
					bzero(revbuf, LENGTH);

					/**if no more byter are recieved or numer 
					 * of bytes recived is less than 512 which indicates last chunk of file,
					 * break 
					 */
					if (fr_block_sz == 0 || fr_block_sz != 512) { 
						break;
					}
				}
				if (fr_block_sz < 0) { 											//in case of error print the error
					if (errno == EAGAIN) {
						printf("recv() timed out.n");
					} else {
						fprintf(stderr, "recv() failed due to errno = %dn", errno);
						exit(1);
					}
				}
				printf("Ok received from client! \n");
				fclose(fr); //file recieved succesfully and closed
			}
			cout<<"********************************* \n \n";
		}
		cout << "Enter y/n to continue" << endl;
		cin >> ans;
	}
}
