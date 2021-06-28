#include <iostream>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <sstream>
#include <fstream>
using namespace std;


pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
struct arg {
	string str;
};

struct fun_arg {
	vector<string> v;
};


int iteration = 0;

//structure of the file node
struct file {
	string name;
	string data;
	int metaData = 0;
	int stratLoc;
	int endLoc;
};
file* openedFile = NULL;


//structure of root node which is acting like root directory
struct rootNode {
	string name;
	vector<file*> fileVector; //vector inside root node to store file node which contains name, data, starting address, ending address and size of the file
	vector<int> freeLoc; //vector to store starting addresses of the chunks those become free on deleting or shifting file from 1 place to another
	int metaData;
	int lastUsedLoc;
	int memLimit;
};


rootNode* head;
struct openedFileInfo{
	string name;
	file* o_fileObj;
	string mode;
	vector<string> client_name;
};
vector<openedFileInfo*> openFiles_v;
//vector<openedFileInfo*> r_openFiles_v;
//vector<pair<string, file*>> 

//on start of debuging this function will be run to create root node and to set available memory limit
void start() {
	rootNode* n = new rootNode;
	n->name = "Root Directory";
	head = n;
	head->lastUsedLoc = 0;
	head->memLimit = 10200; //total memory we have
}

/*
	This function will create new file in directory if memory available. and allocate a single chunk of 100 bytes
	first it will check if anyspace available inside the memory already used it will allot that chunk to the file otherwise
	allocate one of the free chunks at the end
*/
int create(string fileName) {
	if (head->freeLoc.empty() == 1) {
		file* n = new file;
		n->name = fileName;
		n->data = "";
		n->stratLoc = head->lastUsedLoc + 1;
		if (n->stratLoc > head->memLimit) {
			cout << "Storage is Full. So file cannot be created." << endl;
			return 1;
		}
		n->endLoc = n->stratLoc + 99;
		n->metaData = n->name.size() + n->data.size();
		pthread_mutex_lock(&lock);
		head->lastUsedLoc = n->endLoc;
		head->metaData = head->metaData + n->metaData;
		head->fileVector.push_back(n);
		pthread_mutex_unlock(&lock);
		return 0;
	}
	else {
		file* n = new file;
		n->name = fileName;
		n->data = "";
		n->stratLoc = head->freeLoc[0];
		n->endLoc = n->stratLoc + 99;
		n->metaData = n->name.size() + n->data.size();
		pthread_mutex_lock(&lock);
		head->metaData = head->metaData + n->metaData;
		head->fileVector.push_back(n);
		head->freeLoc.erase(head->freeLoc.begin(), head->freeLoc.begin() + 1);
		pthread_mutex_unlock(&lock);
		return 0;
	}
}

file* searchFileResult;
//This function will search the filename file in the directory
int searchFile(string fileName) {
	int flag = 0;
	iteration = 0;
	for (auto i = head->fileVector.begin(); i != head->fileVector.end(); i++) {
		file* n = new file;
		n = *i;
		if (fileName == n->name) {
			searchFileResult = n;
			flag++;
			return 0;
		}
		iteration++;
	}
	if (flag == 0) {
		return 1;
	}
}


//this function will delete the file from the directory and will store the starting addresses of chunks used by that file
void deleteFile(string fileName) {
	int check, it_index;
	pthread_mutex_lock(&lock);
	iteration = 0;
	check = searchFile(fileName);
	it_index = iteration;
	pthread_mutex_unlock(&lock);
	if (check == 0) {
		if (searchFileResult->endLoc - searchFileResult->stratLoc < 100) {
			pthread_mutex_lock(&lock);
			head->metaData = head->metaData - searchFileResult->metaData;
			head->freeLoc.push_back(searchFileResult->stratLoc);
			head->fileVector.erase(head->fileVector.begin() + it_index, head->fileVector.begin() + it_index + 1);
			pthread_mutex_unlock(&lock);
		}
		else
		{
			while (searchFileResult->endLoc - searchFileResult->stratLoc >= 99)
			{
				pthread_mutex_lock(&lock);
				head->freeLoc.push_back(searchFileResult->endLoc - 99);
				pthread_mutex_unlock(&lock);
				searchFileResult->endLoc = searchFileResult->endLoc - 100;
			}
			pthread_mutex_lock(&lock);
			head->metaData = head->metaData - searchFileResult->metaData;
			head->fileVector.erase(head->fileVector.begin() + it_index, head->fileVector.begin() + it_index + 1);
			pthread_mutex_unlock(&lock);
		}
	}
}

/*
This function will append data at the end of the opened file
If total size of content in target file is greater than the allocated memory then file will be shifted at the end and will allocate new chunks to that file
*/
int write(file* fileObj, string text) {
	if (fileObj->metaData + text.size() <= fileObj->endLoc - fileObj->stratLoc) {
		pthread_mutex_lock(&write_lock);
		fileObj->data = fileObj->data + text;
		fileObj->metaData = fileObj->metaData + text.size();
		head->metaData = head->metaData + text.size();
		pthread_mutex_unlock(&write_lock);
		return 0;
	}
	else if (fileObj->endLoc == head->lastUsedLoc) {
		int b = fileObj->endLoc;
		int temp = (fileObj->metaData + text.size()) - (fileObj->endLoc - fileObj->stratLoc + 1);
		while (temp > 0) {
			fileObj->endLoc = fileObj->endLoc + 100;
			temp = temp - 100;
		}
		if (fileObj->endLoc > head->memLimit) {
			fileObj->endLoc = b;
			cout << "Storage is Full. So data cannot be added." << endl;
			return 1;
		}
		pthread_mutex_lock(&write_lock);
		head->lastUsedLoc = fileObj->endLoc;
		fileObj->data = fileObj->data + text;
		fileObj->metaData = fileObj->metaData + text.size();
		head->metaData = head->metaData + text.size();
		pthread_mutex_unlock(&write_lock);
		return 0;
	}
	else {
		int a = fileObj->stratLoc;
		int b = fileObj->endLoc;
		int tempMem = fileObj->endLoc - fileObj->stratLoc;
		fileObj->stratLoc = head->lastUsedLoc + 1;
		int tmem = (fileObj->metaData + text.size()) - (fileObj->endLoc - fileObj->stratLoc + 1);
		while (tmem > 0)
		{

			fileObj->endLoc = fileObj->endLoc + 100;
			tmem = tmem - 100;
		}
		if (fileObj->endLoc > head->memLimit) {
			fileObj->stratLoc = a;
			fileObj->endLoc = b;
			cout << "Storage is Full. So data cannot be added." << endl;
			return 1;
		}
		pthread_mutex_lock(&write_lock);
		head->lastUsedLoc = fileObj->endLoc;
		fileObj->data = fileObj->data + text;
		fileObj->metaData = fileObj->metaData + text.size();
		head->metaData = head->metaData + text.size();
		pthread_mutex_unlock(&write_lock);

		//insert the empty locations in vector
		if (b - a < 100) {
			pthread_mutex_lock(&lock);
			head->freeLoc.push_back(a);
			pthread_mutex_unlock(&lock);
		}
		else
		{
			while (b - a >= 99)
			{
				pthread_mutex_lock(&lock);
				head->freeLoc.push_back(b - 99);
				pthread_mutex_unlock(&lock);
				b = b - 100;
			}
		}
		return 0;
	}
}



/*
This function will write data at the given position of the opened file
If total size of content in target file is greater than the allocated memory then file will be shifted at the end and will allocate new chunks to that file
*/
int write(file* fileObj, int write_at, string text) {
	if (fileObj->metaData + text.size() <= fileObj->endLoc - fileObj->stratLoc) {
		int len = fileObj->data.length();
		string temp1 = fileObj->data.substr(0, write_at - 1);
		string temp2 = fileObj->data.substr(write_at - 1, len);
		pthread_mutex_lock(&write_lock);
		fileObj->data = temp1 + text + temp2;
		fileObj->metaData = temp1.size() + text.size() + temp2.size();
		head->metaData = head->metaData + text.size();
		pthread_mutex_unlock(&write_lock);
		return 0;
	}
	else if (fileObj->endLoc == head->lastUsedLoc) {
		int b = fileObj->endLoc;
		int temp = (fileObj->metaData + text.size()) - (fileObj->endLoc - fileObj->stratLoc + 1);
		while (temp > 0) {
			fileObj->endLoc = fileObj->endLoc + 100;
			temp = temp - 100;
		}
		if (fileObj->endLoc > head->memLimit) {
			fileObj->endLoc = b;
			cout << "Storage is Full. So data cannot be added." << endl;
			return 1;
		}
		int len = fileObj->data.length();
		string temp1 = fileObj->data.substr(0, write_at - 1);
		string temp2 = fileObj->data.substr(write_at - 1, len);
		pthread_mutex_lock(&write_lock);
		head->lastUsedLoc = fileObj->endLoc;
		fileObj->data = temp1 + text + temp2;
		fileObj->metaData = temp1.size() + text.size() + temp2.size();
		head->metaData = head->metaData + text.size();
		pthread_mutex_unlock(&write_lock);
		return 0;
	}
	else {
		int a = fileObj->stratLoc;
		int b = fileObj->endLoc;
		int tempMem = fileObj->endLoc - fileObj->stratLoc;
		fileObj->stratLoc = head->lastUsedLoc + 1;
		int tmem = (fileObj->metaData + text.size()) - (fileObj->endLoc - fileObj->stratLoc + 1);
		while (tmem > 0)
		{
			fileObj->endLoc = fileObj->endLoc + 100;
			tmem = tmem - 100;
		}
		if (fileObj->endLoc > head->memLimit) {
			fileObj->stratLoc = a;
			fileObj->endLoc = b;
			cout << "Storage is Full. So data cannot be added." << endl;
			return 1;
		}
		int len = fileObj->data.length();
		string temp1 = fileObj->data.substr(0, write_at - 1);
		string temp2 = fileObj->data.substr(write_at - 1, len);
		pthread_mutex_lock(&write_lock);
		head->lastUsedLoc = fileObj->endLoc;
		fileObj->data = temp1 + text + temp2;
		fileObj->metaData = temp1.size() + text.size() + temp2.size();
		head->metaData = head->metaData + text.size();
		pthread_mutex_unlock(&write_lock);

		//insert the empty locations in vector
		if (b - a < 100) {
			pthread_mutex_lock(&lock);
			head->freeLoc.push_back(a);
			pthread_mutex_unlock(&lock);
		}
		else
		{
			while (b - a >= 99)
			{
				pthread_mutex_lock(&lock);
				head->freeLoc.push_back(b - 99);
				pthread_mutex_unlock(&lock);
				b = b - 100;
			}
		}
		return 0;
	}

}

//this function will print all the files inside the directory
string printFile() {
	string s;
	int t = 0;
	for (auto i = head->fileVector.begin(); i != head->fileVector.end(); i++) {
		file* n = new file;
		n = *i;
		if(t==0){
			s = n->name;
			t++;
		}else{
			s = s + "," + n->name;
		}
	}
	return s;
}

// this will find the file in vector and return object of that file which will be used to read and write the file
file* open(string fileName) {
	for (auto i = head->fileVector.begin(); i != head->fileVector.end(); i++) {
		file* n = new file;
		n = *i;
		if (fileName == n->name) {
			return n;
		}
	}
	return NULL;
}
/*
This function will copy text from one file and will move in second file
If total size of content in target file is greater than the allocated memory then file will be shifted at the end and will allocate new chunks to that file
*/

int move(string sourceFile, string targetFile) {
	pthread_mutex_lock(&lock);
	iteration = 0;
	searchFile(sourceFile);
	int soureceLocation = iteration;
	file* sFile = searchFileResult;
	iteration = 0;
	searchFile(targetFile);
	int targetLocation = iteration;
	file* tFile = searchFileResult;
	pthread_mutex_unlock(&lock);
	if (tFile->metaData + sFile->metaData <= tFile->endLoc - tFile->stratLoc) {
		int temp = head->fileVector[targetLocation]->metaData;
		pthread_mutex_lock(&lock);
		head->fileVector[targetLocation]->data = head->fileVector[targetLocation]->data + head->fileVector[soureceLocation]->data;
		head->fileVector[targetLocation]->metaData = head->fileVector[targetLocation]->metaData + head->fileVector[soureceLocation]->metaData;
		head->metaData = head->metaData + head->fileVector[targetLocation]->metaData - temp;
		pthread_mutex_unlock(&lock);
		return 0;
	}
	else if (tFile->endLoc == head->lastUsedLoc) {
		int b = tFile->endLoc;
		int temp = (tFile->metaData + sFile->metaData) - (tFile->endLoc - tFile->stratLoc + 1);
		while (temp > 0) {
			tFile->endLoc = tFile->endLoc + 100;
			temp = temp - 100;
		}
		if (tFile->endLoc > head->memLimit) {
			tFile->endLoc = b;
			cout << "Storage is Full. So data cannot be added." << endl;
			return 1;
		}
		int temp1 = head->fileVector[targetLocation]->metaData;
		pthread_mutex_lock(&lock);
		head->lastUsedLoc = tFile->endLoc;
		head->fileVector[targetLocation]->data = head->fileVector[targetLocation]->data + head->fileVector[soureceLocation]->data;
		head->fileVector[targetLocation]->metaData = head->fileVector[targetLocation]->metaData + head->fileVector[soureceLocation]->metaData;
		head->metaData = head->metaData + head->fileVector[targetLocation]->metaData - temp1;
		pthread_mutex_unlock(&lock);
		return 0;
	}
	else
	{
		int a = tFile->stratLoc;
		int b = tFile->endLoc;
		int tempMem = tFile->endLoc - tFile->stratLoc;
		tFile->stratLoc = head->lastUsedLoc + 1;
		int tmem = (tFile->metaData + sFile->metaData) - (tFile->endLoc - tFile->stratLoc + 1);
		while (tmem > 0)
		{
			tFile->endLoc = tFile->endLoc + 100;
			tmem = tmem - 100;
		}
		if (tFile->endLoc > head->memLimit) {
			tFile->stratLoc = a;
			tFile->endLoc = b;
			cout << "Storage is Full. So data cannot be added." << endl;
			return 1;
		}
		int temp1 = head->fileVector[targetLocation]->metaData;
		pthread_mutex_lock(&lock);
		head->lastUsedLoc = tFile->endLoc;
		head->fileVector[targetLocation]->data = head->fileVector[targetLocation]->data + head->fileVector[soureceLocation]->data;
		head->fileVector[targetLocation]->metaData = head->fileVector[targetLocation]->metaData + head->fileVector[soureceLocation]->metaData;
		head->metaData = head->metaData + head->fileVector[targetLocation]->metaData - temp1;
		pthread_mutex_unlock(&lock);
		//insert the empty locations in vector
		if (b - a < 100) {
			pthread_mutex_lock(&lock);
			head->freeLoc.push_back(a);
			pthread_mutex_unlock(&lock);
		}
		else
		{
			while (b - a >= 99)
			{
				pthread_mutex_lock(&lock);
				head->freeLoc.push_back(b - 99);
				pthread_mutex_unlock(&lock);
				b = b - 100;
			}
		}
		return 0;
	}
}



//This function is used to print the info like starting, ending address of chunk, file name and total size of that file
//this will also make sample.dat file which will have same details
string showMem() {
	ofstream sampleFile("sample.dat");
	string s = "Starting Location of Chunk	Ending Location of Chunk	Total Size in bytes	File Name";
	sampleFile << "Starting Location of Chunk" << "	Ending Location of Chunk" << "	Total Size(B)" << "	File Name" << endl;
	for (auto i = head->fileVector.begin(); i != head->fileVector.end(); i++) {
		file* n = new file;
		n = *i;
		s = s + "," + to_string(n->stratLoc) + "				" + to_string(n->endLoc) + "				" + to_string(n->metaData) + "			"+ n->name;
		sampleFile << n->stratLoc << "				" << n->endLoc << "				" << n->metaData << "				" << n->name << endl;
	}
	sampleFile.close();
	return s;
}


//Simply print the name and content of the opened file
string read(file* fileObj) {
	pthread_mutex_lock(&read_lock);
	string s = "Content of the File is \"" + fileObj->data + "\"";
	pthread_mutex_unlock(&read_lock);
	return s;
}
//this will close the opened file
int close(string fileName, string client) {
	for (auto it = openFiles_v.begin(); it != openFiles_v.end(); it++) {
		openedFileInfo* n = new openedFileInfo;
		n = *it;
		if (n->name == fileName) {
			if(n->client_name.size() == 1){
				if(n->client_name[0]==client){
					int index = distance(openFiles_v.begin(), it);
					pthread_mutex_lock(&lock);
					openFiles_v.erase(openFiles_v.begin() + index, openFiles_v.begin() + index + 1);
					pthread_mutex_unlock(&lock);
					return 0;
				}
				else
				{
					return 2;
				}				
			}
			else if(n->client_name.size() > 1){
				for(auto i = n->client_name.begin(); i != n->client_name.end(); i++){
					string strr = *i;
					if(strr == client){
						int ind = distance(n->client_name.begin(), i);
						pthread_mutex_lock(&lock);
						n->client_name.erase(n->client_name.begin() + ind, n->client_name.begin() + ind + 1);
						pthread_mutex_unlock(&lock);
						return 0;
					}
				}
			}
			else{
				return 2;
			}
		}
	}
	return 1;
}
//this will read file from given position of the file till the size is given.
string read(file* fileObj, int start, int size) {
	pthread_mutex_lock(&read_lock);
	string str = fileObj->data.substr(start, size);
	pthread_mutex_unlock(&read_lock);
	string s;
	s = "Content of the File is \"" + str + "\"";
	return s;
}


typedef struct
{
	int sock;
	struct sockaddr address;
	socklen_t addr_len;
} connection_t;

void * process(void * ptr)
{
	
	char * buffer;
	int len;
	connection_t * conn;
	long addr = 0;
	char * name_buffer;
	string client_name;
	if (!ptr) pthread_exit(0); 
	conn = (connection_t *)ptr;
	//will read name of user
	int name_len;
	read(conn->sock, &name_len, sizeof(int));
	if (name_len > 0)
	{
		name_buffer = (char*)malloc((name_len+1)*sizeof(char));
		name_buffer[name_len] = 0;
		read(conn->sock, name_buffer, name_len);
		cout << "Connection of "<< name_buffer << " established successfully!"<<endl;
		client_name = name_buffer;
	}
	/* read length of message */
	while(read(conn->sock, &len, sizeof(int))){
		if (len > 0)
		{
			buffer = (char*)malloc((len+1)*sizeof(char));
			buffer[len] = 0;
			
			/* read message */
			read(conn->sock, buffer, len);
			string s = buffer;//create a.txt
			stringstream ss(s);
			string str;
			vector<string> argument;
			while(getline(ss,str,',')){
				argument.push_back(str);
			}


			//previous code section start


			int actionFlag = 0;
			if (argument[0] == "create") {
				actionFlag = 1;
			}
			else if (argument[0] == "delete") {
				actionFlag = 2;
			}
			else if (argument[0] == "move") {
				actionFlag = 3;
			}
			else if (argument[0] == "open") {
				actionFlag = 4;
			}
			else if (argument[0] == "close") {
				actionFlag = 5;
			}
			else if (argument[0] == "write") {
				actionFlag = 6;
			}
			else if (argument[0] == "write_at") {
				actionFlag = 7;
			}
			else if (argument[0] == "read") {
				actionFlag = 8;
			}
			else if (argument[0] == "read_at") {
				actionFlag = 9;
			}
			else if (argument[0] == "showMemoryMap") {
				actionFlag = 10;
			}
			else if (argument[0] == "ls") {
				actionFlag = 11;
			}
			//everything fine 

			switch (actionFlag){
				case 1: {
					char * outputString=NULL;
					string st;
					if(argument.size()==2){
						string inputFileName = argument[1];
						int check = searchFile(inputFileName);
						if (check == 1) {
							int m = create(inputFileName);
							if (m == 0) {
								st = "File of name " + inputFileName + " created successfully.";
							}
						}
						else {
							st = "File of this name already exist.";	
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 2: {
					char * outputString=NULL;
					string st;
					if(argument.size()==2){
						string inputFileName = argument[1];
						int check = searchFile(inputFileName);
						if (check == 0) {
							deleteFile(inputFileName);
							st = "File of name " + inputFileName + " deleted successfully.";
						}
						else {
							st = "File of this name does not exist.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}

					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 3: {
					char * outputString=NULL;
					string st;
					if(argument.size()==3){
						string sFileName = argument[1], tFileName = argument[2];//move,a.txt,b.txt
						int check = searchFile(sFileName);
						if (check == 0) {
							int chk = searchFile(tFileName);
							if (chk == 0) {
								int m = move(sFileName, tFileName);
								if (m == 0) {
									st = "Data Copied From " + sFileName + " to " + tFileName + " successfully.";
								}
							}
							else {
								st = tFileName + " File does not exist.";
							}
						}
						else {
							st = sFileName + " File does not exist.";
						}					
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 4: {
					char * outputString=NULL;
					string st;
					if(argument.size()==3){
						string inputFileName = argument[1];
						int check = searchFile(inputFileName);
						if (check == 0) {
							int ch = 0;
							for (auto i = openFiles_v.begin(); i != openFiles_v.end(); i++) {
								openedFileInfo* n = new openedFileInfo;
								n = *i;
								if (n->name == inputFileName) {
									if(n->mode==argument[2]){
										int d = 0;
										for(auto ite = n->client_name.begin();ite!=n->client_name.end();ite++){
											if(*ite == client_name){
												st = "File is already opened";
												d++;
											}
										}
										if(d==0){
											n->client_name.push_back(client_name);
											st = "File of name " + inputFileName + " opened successfully.";
										}
										
									}
									else{
										st = "File is opened in another mode. So! cannot open at this time.";
									}
									ch++;
								}
							}
							if (ch == 0) {
								file* openedFile = NULL;
								openedFile = open(inputFileName);
								if (openedFile != NULL)
								{
									openedFileInfo* n = new openedFileInfo;
									n->name = inputFileName;
									n->o_fileObj = openedFile;
									n->mode = argument[2];
									n->client_name.push_back(client_name);
									openFiles_v.push_back(n);
									st = "File of name " + inputFileName + " opened successfully.";
								}
								else {
									st = inputFileName + " File does not open.";
								}
							}
						}


						else {
							st = inputFileName + " File does not exist.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 5: {					
					char * outputString=NULL;
					string st;
					if(argument.size()==2){
						if (!openFiles_v.empty()) {
							string inputFileName = argument[1];
							int check = searchFile(inputFileName);
							if (check == 0) {
								int flag = close(inputFileName,client_name);
								if (flag == 0)
								{
									st = "File of name " + inputFileName + " closed successfully.";
								}
								else if (flag == 1)
								{
									st = inputFileName + " File does not open.";
								}
								else if(flag == 2)
								{
									st = "You have not opened this file. So you cannot close this file.";
								}
							}
							else {
								st = inputFileName + " File does not exist.";
							}
						}
						else {
							st = "No file is Opened.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 6: {
					char * outputString=NULL;
					string st;
					if(argument.size()==3){
						
						if (!openFiles_v.empty()) {
							string inputFileName = argument[1];
							string text = argument[2];
							int ch = 0;
							for (auto i = openFiles_v.begin(); i != openFiles_v.end(); i++) {
								openedFileInfo* n = new openedFileInfo;
								n = *i;
								if (n->name == inputFileName) {
									if(n->mode == "write"){
										int d = 0;
										for(auto ite = n->client_name.begin();ite!=n->client_name.end();ite++){
											if(*ite == client_name){
												d++;
											}
										}
										if(d == 0){
											st = inputFileName + "File is not Opened.";
										}
										else{
											int nn = 0;
											while (!(n->client_name[0] == client_name))
											{
												if(nn==0){
													//sending output string to client
													st = "-1";
													outputString = strcpy(new char[st.length()+1],st.c_str());
													int len = strlen(outputString);
													write(conn->sock, &len, sizeof(int));
													write(conn->sock, outputString, len);
													nn++;
												}
											}
											int m = write(n->o_fileObj, text);
											if (m == 0) {
												st = "Data is stored successfully.";
											}
										}
									}
									else
									{
										st = "File is opened in "+n->mode+" mode. So you cannot write at this time";
									}
									ch++;
								}
							}
							if (ch == 0) {
								st = inputFileName + "File is not Opened.";
							}

						}
						else {
							st = "No file is Opened yet. First open the file in which you want to write data.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 7: {
					char * outputString=NULL;
					string st;
					if(argument.size()==4){
						
						if (!openFiles_v.empty()) {
							string inputFileName = argument[1];
							string text = argument[2];
							int write_at = stoi(argument[3]);
							int ch = 0;
							for (auto i = openFiles_v.begin(); i != openFiles_v.end(); i++) {
								openedFileInfo* n = new openedFileInfo;
								n = *i;
								if (n->name == inputFileName) {
									if(n->mode == "write"){
										int d = 0;
										for(auto ite = n->client_name.begin();ite!=n->client_name.end();ite++){
											if(*ite == client_name){
												d++;
											}
										}
										if(d == 0){
											st = inputFileName + "File is not Opened.";
										}
										else{
											int nn = 0;
											while (!(n->client_name[0] == client_name))
											{
												if(nn==0){
													//sending output string to client
													st = "-1";
													outputString = strcpy(new char[st.length()+1],st.c_str());
													int len = strlen(outputString);
													write(conn->sock, &len, sizeof(int));
													write(conn->sock, outputString, len);
													nn++;
												}
											}
											int m;
											if (n->o_fileObj->data.size() > write_at) {
												m = write(n->o_fileObj, write_at, text);
											}
											else {
												m = write(n->o_fileObj, text);
											}
											if (m == 0) {
												st = "Data is stored successfully.";
											}
										}
									}
									else{
										st = "File is opened in "+n->mode+" mode. So you cannot write at this time";
									}
									ch++;
								}
							}
							if (ch == 0) {
								st = inputFileName + "File is not Opened.";
							}
						}
						else {
							st = "No file is Opened yet. First open the file in which you want to write data.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 8: {
					char * outputString=NULL;
					string st;
					if(argument.size()==2){				
						if (!openFiles_v.empty()) {
							string inputFileName = argument[1];
							int ch = 0;
							for (auto i = openFiles_v.begin(); i != openFiles_v.end(); i++) {
								openedFileInfo* n = new openedFileInfo;
								n = *i;
								
								if (n->name == inputFileName) {
									if(n->mode == "read"){
										int d = 0;
										for(auto ite = n->client_name.begin(); ite != n->client_name.end(); ite++){
											if(*ite == client_name){
												st = read(n->o_fileObj);
												d++;
											}
										}
										if(d == 0){
											st = inputFileName + "File is not Opened.";
										}
									}
									else{
										st = "File is opened in "+n->mode+" mode. So you cannot read at this time";
									}
									ch++;
								}
							}
							if (ch == 0) {
								st = inputFileName + "File is not Opened.";
							}

						}
						else {
							st = "No file is Opened yet. First open the file from which you want to read data.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 9: {
					char * outputString=NULL;
					string st;
					if(argument.size()==4){
						
						if (!openFiles_v.empty()) {

							string inputFileName = argument[1];
							int start = stoi(argument[2]), size = stoi(argument[3]);
							int ch = 0;
							for (auto i = openFiles_v.begin(); i != openFiles_v.end(); i++) {
								openedFileInfo* n = new openedFileInfo;
								n = *i;
								if (n->name == inputFileName) {
									if(n->mode == "read"){
										int d = 0;
										for(auto ite = n->client_name.begin(); ite != n->client_name.end(); ite++){
											if(*ite == client_name){
												if (n->o_fileObj->data.size() > start) {
													if (start + size <= n->o_fileObj->data.size()) {
														st = read(n->o_fileObj, start, size);
													}
													else
													{
														st = read(n->o_fileObj, start, n->o_fileObj->data.size());
													}
												}
												else {
													st = "You have entered invalid position";
												}
												d++;
											}
										}
										if(d == 0){
											st = inputFileName + "File is not Opened.";
										}
										
									}
									else{
										st = "File is opened in "+n->mode+" mode. So you cannot read at this time";
									}
									ch++;
								}
							}
							if (ch == 0) {
								st = inputFileName + "File is not Opened.";
							}

						}
						else {
							st = "No file is Opened yet. First open the file from which you want to read data.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 10: {
					char * outputString=NULL;
					string st;
					if(argument.size()==1){
						
						if (head->fileVector.empty() == 0) {							
							st = showMem();							
						}
						else
						{
							st = "Memory is Empty.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				case 11: {
					char * outputString=NULL;
					string st;
					if(argument.size()==1){
						if (head->fileVector.size() != 0) {
							st = printFile();
						}
						else
						{
							st = "No file exist in this directory.";
						}
					}
					else{
						st = "Invalid Number of arguments!";
					}
					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
				default:{
					char * outputString=NULL;
					string st;
					st = "You have entered a wrong command";

					//sending output string to client
					outputString = strcpy(new char[st.length()+1],st.c_str());
					int len = strlen(outputString);
					write(conn->sock, &len, sizeof(int));
					write(conn->sock, outputString, len);
					break;
				}
			}


			//previous code section end
			//printf("printing vector");
			//for (auto i = v.begin();i!=v.end();i++){
			//	cout<<*i<<endl;
			//}
			continue;

			free(buffer);
		}

	}


	/* close socket and clean up */
	close(conn->sock);
	free(conn);
	pthread_exit(0);
}

int main(int argc, char ** argv)
{
	start();
	int sock = -1;
	struct sockaddr_in address;
	int port = 95;
	connection_t * connection;
	pthread_t thread;

	/* check for command line arguments */
	if (argc != 1)//exefileofserver
	{
		fprintf(stderr, "Invalid number of arguments\n");
		return -1;
	}

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock <= 0)
	{
		fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
		return -3;
	}

	/* bind socket to port */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = port;
	if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
	{
		fprintf(stderr, "%s: error: cannot bind socket to port %d\n", argv[0], port);
		return -4;
	}

	/* listen on port */
	if (listen(sock, 10) < 0)
	{
		fprintf(stderr, "%s: error: cannot listen on port\n", argv[0]);
		return -5;
	}

	printf("%s: ready and listening at Port 95\n", argv[0]);
	
	while (1)
	{
		/* accept incoming connections */
		connection = (connection_t *)malloc(sizeof(connection_t));
		connection->sock = accept(sock, &connection->address, &connection->addr_len);
		if (connection->sock <= 0)
		{
			free(connection);
		}
		else
		{			
			/* start a new thread but do not wait for it */
			pthread_create(&thread, 0, process, (void *)connection);
			pthread_detach(thread);
		}
	}	
	return 0;
}