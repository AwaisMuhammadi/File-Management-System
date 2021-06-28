#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
using namespace std;

int main(int argc, char ** argv)
{
	int port;
	int sock = -1;
	struct sockaddr_in address;
	struct hostent * host;
	int len;
	
	/* checking commandline parameter */
	if (argc != 4)
	{
		printf("Invalid Number of arguments\n");
		return -1;
	}//exe hostname/ip port userName

	/* obtain port number */
	if (sscanf(argv[2], "%d", &port) <= 0)
	{
		fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
		return -2;
	}

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock <= 0)
	{
		fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
		return -3;
	}

	/* connect to server */
	address.sin_family = AF_INET;
	address.sin_port = port;
	host = gethostbyname(argv[1]);
	if (!host)
	{
		fprintf(stderr, "%s: error: unknown host %s\n", argv[0], argv[1]);
		return -4;
	}
	memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
	if (connect(sock, (struct sockaddr *)&address, sizeof(address)))
	{
		fprintf(stderr, "%s: error: cannot connect to host %s\n", argv[0], argv[1]);
		return -5;
	}
	//will send user name to the host
	int name_len = strlen(argv[3]);
	write(sock, &name_len, sizeof(int));
	write(sock, argv[3], name_len);
	printf("Connection established successfully!\n");
	//will take commands and send it to server
	char che;
	int chek=0;
	do {
		string input;
		printf("Insert command: ");
		if(chek==0){
			getline(cin,input);//create,a.txt
			chek++;
		}
		else{
			getchar();
			getline(cin,input);//create,a.txt
		}
		
		char * mn = strcpy(new char[input.length()+1],input.c_str());
		len = strlen(mn);
		write(sock, &len, sizeof(int));
		write(sock, mn, len);
		int len1;
		read(sock, &len1, sizeof(int));
		char * buffer;
		if (len1 > 0)
		{
			buffer = (char*)malloc((len1+1)*sizeof(char));
			buffer[len1] = 0;
			/* read message */
			read(sock, buffer, len1);
			string b_str = buffer;
			if(b_str=="-1"){
				cout << "Some other user is writting in this file. Kindly wait for sometime"<<endl;
				read(sock, &len1, sizeof(int));
				if (len1 > 0)
				{
					char* buffer1 = (char*)malloc((len1+1)*sizeof(char));
					buffer1[len1] = 0;
					read(sock, buffer1, len1);
					b_str = buffer1;
				}
			}
			stringstream sss(b_str);
			string t_str;
			while(getline(sss,t_str,',')){
				cout << t_str << endl;
			}
		}
		printf("Do you want to run another command:[y/n]");
		scanf("  %c",&che);
	}while(che=='y');
	
	/* close socket */
	close(sock);
	return 0;
}