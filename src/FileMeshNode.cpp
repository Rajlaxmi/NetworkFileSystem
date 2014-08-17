/**
 * This file runs at each node.
 * It takes arguments:
 * 1. path of Configuration file
 * 2. Current Nodenum
 * 3. total number of nodes.
 * 
 * It then creates a UDP server and recieves message.
 * If the recieved message is of type store,
 * It creates a TCP Client and connects to Server and recieves file
 * 
 * In case the recieved message is of type get,
 * It creates a TCP Client and connects to Server and sends file
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
#include <string> 
#include <string.h> 
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h> 

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
string getIP(string s) {
	int pos = s.find(":");
	return s.substr(0,pos);
}

/**returns port from the string which is a particular line from config file
 * Input: string s - Particular line from config file
 */
string getPort(string s) {
	int pos = s.find(":");
	int pos1 = s.find(" ");
	return s.substr(pos+1,(pos1-1)-pos);
}

/**returns folder from the string which is a particular line from config file
 * Input: string s - Particular line from config file
 */
string getFolder(string s) {
	int pos = s.find(" ");
	if(pos==-1)
		pos = s.find("\t");
	return s.substr(pos+1);
}

/**Helper function used to parse message recieved at UDP port
 * Input : string s - message recieved
 * 		   int num - Denotes what to be returned
 * 			If num is 1 - return request Type
 * 					  2 - md5sum
 * 					  3 - IP:Port
 *  */
string nextword(string s, int num) {
	string res=s;
	while(num>1)
	{
		int pos=res.find(";");
		res=res.substr(pos+1);
		num--;
	}
	int pos=res.find(";");
	res=res.substr(0,pos);
	return res;
}

/**Returns md5sum modulo maxNodes
 * Input : string md5sum - md5sum of file
 * 			int n - max number of Nodes
 *  */
int md5mod(string md5sum,int n) {
	int md5int=0;
	char c;
	int i=0;
	while(i<md5sum.length())
	{

		c=md5sum.at(i);
		if(c>=97 && c<=122)
			md5int = (md5int*10+ c-87)%n;
		else
			md5int = (md5int*10+ c-48)%n; 
		i++;    
	} 
	return md5int;
}

/**
 * Main function which handles store and get functions
 * It takes arguments(through command line):
 * 1. argv[1] - path of Configuration file
 * 2. argv[2] - Current Nodenum
 * 3. argv[3] - total number of nodes. 
 */
int main(int argc, char *argv[]) {

	int intlocalport;							//to store UDP port of the UDP server running
	int sd, rc, n;								//temporary variables
	socklen_t cliLen; 							//store length of socket address 
	struct sockaddr_in cliAddr;					//Holds socket address information for client
	struct sockaddr_in servAddr;				//Holds socket address information for UDP Server running
	struct sockaddr_in remoteServAddr;			//Holds socket address information for Remote Server which is contacted
	struct hostent *h;							//Used to represent host which needs to be contacted
	char msg[LENGTH];							//Used to store messages recieved

	string localport; 							//store UDP port in string form
	string MYIP;								//store IP of the machine in which server is running
	string localfolder; 						//store folder in which files are saved

	string destip; 								//store IP address of destination machine which is contacted
	int destPort;								//store port of destination machine which is contacted

	int maxNodes=0; 							//store maximum number of nodes
	char *configFilePath; 						//To store the path of File where information of Node is stored
	int mynodenum=0;							//To store node id of current node

	/* check command line args */
	if(argc<4) {
		printf("usage : %s ConfigFile Nodenum MaxNodes\n", argv[0]);
		return 1;
	}

	/*Initialize configFilePath, own node number and maxNodes with commandline arguments*/
	configFilePath=argv[1];
	mynodenum = atoi(argv[2]);
	maxNodes = atoi(argv[3]);

	string content;
	string **nodeinfo; //To store ip,port and folder information of all nodes

	/**********************************************************************************************************************************************/

	/**Read content of configuration file and assign it's contents to nodeinfo */

	ifstream myfile (configFilePath);				//to read from config file
	if (myfile.is_open())							//check if file is open
	{
		nodeinfo = new string*[maxNodes];			//initialize nodeinfo 
		int i=0;
		myfile.seekg (0, ios::beg);					//set position of next character to the beginning of file
		while ( getline (myfile,content) )
		{
			nodeinfo[i] = new string[3];
			nodeinfo[i][0]=getIP(content);
			nodeinfo[i][1]=getPort(content);
			nodeinfo[i][2]=getFolder(content);
			if(i==mynodenum)
			{
				localport=nodeinfo[i][1];
				localfolder=nodeinfo[i][2];
				if(localfolder[localfolder.length()-1]!='/')
				{
					string back="/";
					localfolder=localfolder+back;
				}
			}
			intlocalport=atoi(localport.c_str());
			i++;
		}
		myfile.close();
	}
	else										//print error message if unable to open the file
	{
		cout << "Unable to open file"; 
		return 1;
	}

	/******************************************************************************************************/

	/**Creating UDP Server to recieve messages */

	/** UDP socket creation */
	sd=socket(AF_INET, SOCK_DGRAM, 0);
	if(sd<0) {
		printf("%s: cannot open socket \n",argv[0]);
		return 1;
	}

	/** bind local server port */
	servAddr.sin_family = AF_INET;						//set family
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);		//set IP address of UDP Server
	servAddr.sin_port = htons(intlocalport);			//set port number of UDP server


	rc = bind (sd, (struct sockaddr *) &servAddr,sizeof(servAddr));//associate tsd socket with TCP server having address tservAddr
	if(rc<0) {														//print error
		printf("%s: cannot bind port number %d \n", 
				argv[0], intlocalport);
		return 1;
	}

	printf("%s: waiting for data on port UDP %u\n", 
			argv[0],intlocalport);

	/** UDP server infinite loop to recieve messages*/
	while(1) {

		/* init buffer */
		memset(msg,0x0,LENGTH); 					//set msg buffer with 0x0 value

		/* receive message */
		cliLen = sizeof(cliAddr);
		n = recvfrom(sd, msg, LENGTH, 0, 
				(struct sockaddr *) &cliAddr, &cliLen);

		if(n<0) { 									//in case of error in receiveing, print error
			printf("%s: cannot receive data \n",argv[0]);
			continue;
		}

		/* print received message */
		printf("%s: from %s:UDP%u : %s \n", 
				argv[0],inet_ntoa(cliAddr.sin_addr),
				ntohs(cliAddr.sin_port),msg);

		/** Act on recieved message */
		string recieved(msg);
		string reqType=nextword(recieved,1);
		string md5sum=nextword(recieved,2);
		string ipport=nextword(recieved,3);

		int md5int = md5mod(md5sum,maxNodes);
		if(md5int==mynodenum)
		{
			cout<<"To be handled here " <<endl;
			if(reqType=="store")
			{

				/**********************************************************************************************************************************/
				/**Open the connection of TCP and accept the file*/

				int sockfd; 									//creates an endpoint for communication and returns a file descriptor for the TCP socket
				char revbuf[LENGTH]; 							//Receiver buffer
				struct sockaddr_in remote_addr; 				//Holds socket address information for remote server

				string userip=ipport.substr(0,ipport.find(":"));//store ip of user to be contacted
				int userport=atoi((ipport.substr(ipport.find(":")+1)).c_str());//store port of user to be contacted

				cout<<"IP of user "<<userip<<"  port "<<userport<<endl;
				/* Get the Socket file descriptor */
				if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				{
					fprintf(stderr, "ERROR: Failed to obtain Socket Descriptor! (errno = %d)\n",errno);
					exit(1);
				}



				/* set family, ip address and port of the remote TCP Server */
				remote_addr.sin_family = AF_INET; 				
				remote_addr.sin_port = htons(userport); 
				inet_pton(AF_INET, userip.c_str(), &remote_addr.sin_addr); 
				bzero(&(remote_addr.sin_zero), 8);

				
				/* Try to connect the remote */
				if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
				{
					fprintf(stderr, "ERROR: Failed to connect to the host! (errno = %d)\n",errno);
					exit(1);
				}
				else 
					printf("[Client] Connected to server in store part at port %d...ok!\n", userport);


				/*Receive File from Client */
				char *fr_name = new char[(localfolder+md5sum).length() + 1];
				strcpy(fr_name,(localfolder+md5sum).c_str());

				FILE *fr = fopen(fr_name, "w"); 			//open file with name localfolder+md5sum in write mode
				if(fr == NULL) 								//in case file cannot be opened print error message
					printf("File %s Cannot be opened file on server.\n", fr_name);
				else
				{
					bzero(revbuf, LENGTH); 					//Set revbuf with null values.
					int fr_block_sz = 0;
					while((fr_block_sz = recv(sockfd, revbuf, LENGTH, 0)) > 0) 		//recieve file chunk of maximum length LENGTH
					{
						int write_sz = fwrite(revbuf, sizeof(char), fr_block_sz, fr);//write recieved characters to a file
						if(write_sz < fr_block_sz)//in case written number of byter are less than recieved number of bytes print error
						{
							error("File write failed on server.\n");
						}
						bzero(revbuf, LENGTH);
						/**if no more byter are recieved or numer 
						 * of bytes recived is less than 512 which indicates last chunk of file,
						 * break 
						 */
						if (fr_block_sz == 0 || fr_block_sz != 512) 
						{
							break;
						}
						cout<<"Receiveing"<<endl;
					}
					if(fr_block_sz < 0)//in case of error print the error
					{
						if (errno == EAGAIN)
						{
							printf("recv() timed out.\n");
						}
						else
						{
							fprintf(stderr, "recv() failed due to errno = %d\n", errno);
							exit(1);
						}
					}
					printf("Ok received from client!\n");
					fclose(fr); 
				}
			}

			/************************************************************************************************************************************/
			else if(reqType=="get")
			{

				/**********************************************************************************************************************************/
				/**Open the connection of TCP and send the file*/

				int sockfd; 					//TCP socket descriptor
				char revbuf[LENGTH]; 			//stores file in chunks
				struct sockaddr_in remote_addr; //Holds socket address information for remote(user) end

				string userip=ipport.substr(0,ipport.find(":")); 				//store ip of user TCP server
				int userport=atoi((ipport.substr(ipport.find(":")+1)).c_str()); //store port of user TCP server

				//Send file to user
				FILE * pFile;
				pFile = fopen ((localfolder+md5sum).c_str(),"r");
				if (pFile==NULL)
				{
					cout<<"File Doesnot exists"<<endl;
					continue;
				}
				fclose (pFile);

				/* Get the Socket file descriptor */
				if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
				{
					fprintf(stderr, "ERROR: Failed to obtain Socket Descriptor! (errno = %d)\n",errno);
					exit(1);
				}

				//set family, ip address and port of remote node
				remote_addr.sin_family = AF_INET; 
				remote_addr.sin_port = htons(userport); 
				inet_pton(AF_INET, userip.c_str(), &remote_addr.sin_addr); 
				bzero(&(remote_addr.sin_zero), 8);


				/**send data to remoteServAddr */

				/* Try to connect the remote */
				if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
				{
					fprintf(stderr, "ERROR: Failed to connect to the host! (errno = %d)\n",errno);
					exit(1);
				}
				else 
					printf("[Client] Connected to server at port in get part %d...ok!\n", userport);


				char *fs_name = new char[(localfolder+md5sum).length() + 1];
				strcpy(fs_name,(localfolder+md5sum).c_str()); //open file with name localfolder+md5sum in write mode
				char sdbuf[LENGTH]; 
				printf("[Client] Sending %s to the Server... ", fs_name);
				FILE *fs = fopen(fs_name, "r");
				if(fs == NULL)									//in case file cannot be opened print error
				{
					printf("ERROR: File %s not found.\n", fs_name);
					exit(1);
				}

				bzero(sdbuf, LENGTH); //Set revbuf with null values.
				int fs_block_sz; 
				while((fs_block_sz = fread(sdbuf, sizeof(char), LENGTH, fs)) > 0)
				{
					if(send(sockfd, sdbuf, fs_block_sz, 0) < 0)
					{
						fprintf(stderr, "ERROR: Failed to send file %s. (errno = %d)\n", fs_name, errno);
						break;
					}
					bzero(sdbuf, LENGTH);
				}
				printf("Ok File %s from Client was Sent!\n", fs_name);
			}
		}

		else //This will block any other message from receiveing see if it can be done in background
		{
			cout<<"Not Mine"<<endl;
			/**Contacting the correct node id */

			destip=nodeinfo[md5int][0]; 					//ip address of remote node stored
			destPort=atoi((nodeinfo[md5int][1]).c_str());	//port number of remote node stored in destPort

			h = gethostbyname(destip.c_str()); 				//Perform DNS lookup whether ip address is present
			if(h==NULL) {
				printf("%s: unknown host '%s' \n", argv[0], argv[1]);
				return 1;
			}

			printf("%s: sending data to '%s' (IP : %s) Port %d\n", argv[0], h->h_name,
					inet_ntoa(*(struct in_addr *)h->h_addr_list[0]), destPort);

			//set family, ip address and port of remote(user) node
			remoteServAddr.sin_family = h->h_addrtype;
			memcpy((char *) &remoteServAddr.sin_addr.s_addr, 
					h->h_addr_list[0], h->h_length);
			remoteServAddr.sin_port = htons(destPort);

			//send message to correct node 
			rc = sendto(sd,recieved.c_str(), recieved.length()+1, 0, 
					(struct sockaddr *) &remoteServAddr, sizeof(remoteServAddr)); 

			if(rc<0) { //in case of any error print error
				printf("%s: cannot send data %d \n",argv[0],md5int);
				close(sd);
				return 1;
			}
			cout<<"Message sent!"<<endl;
		}

	}/* end of server infinite loop */

	return 0;

}
