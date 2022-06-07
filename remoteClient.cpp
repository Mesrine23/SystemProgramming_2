#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h> /* sockets */
#include <sys/types.h> /* sockets */
#include <sys/socket.h> /* sockets */
#include <netinet/in.h> /* internet sockets */
#include <netdb.h> /* gethostbyaddr */
#include <unistd.h> /* fork */
#include <stdlib.h> /* exit */
#include <ctype.h> /* toupper */
#include <signal.h> /* signal */
#include <pthread.h>
#include <vector>
#include <bits/stdc++.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

using namespace std;

string server_check( string , char** );
void perror_exit ( string );

int main (int argc, char* argv[]) {
    if(argc!=7) {
        cout << "Not enough arguments!" << endl;
        exit(1);
    }
    string server_ip;
    if( (server_ip = server_check("-i",argv))=="" ) {
        cout << "No 'server ip' found!" << endl;
        exit(1);
    }
    string directory;
    if( (directory = server_check("-d",argv))=="" ){
        cout << "No 'directory' found!" << endl;
        exit(1);
    }
    int server_port;
    int arg_pos=1;
    while(arg_pos!=7){
        if(strcmp(argv[arg_pos],"-p")==0){
            server_port = atoi(argv[arg_pos+1]);
            break;
        }
        arg_pos += 2;
    }
    if(arg_pos==7) {
        cout << "No 'server port' found!" << endl;
    }
    cout << "Client's {" << getpid() << "} paramaters are:" << endl;
    cout << "serverIP: " << server_ip << endl;
    cout << "port: " << server_port << endl;
    cout << "directory: " << directory << endl;

    int sock;
    char buf[512];
    struct sockaddr_in server;
    struct sockaddr* serverptr = (struct sockaddr*) &server;
    struct hostent *rem;

    if( (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
        perror_exit("socket");
    if( (rem = gethostbyname(server_ip.c_str())) == NULL ){
        herror("gethostbyname");
        exit(1);
    }
    server.sin_family = AF_INET;
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(server_port);
    if( connect(sock,serverptr,sizeof(server)) < 0 )
        perror_exit("connect");
    cout << "Connecting to " << server_ip << " port " << server_port << endl;
    cout << "Passing dir < " << directory << " > to server." << endl;
    strcpy(buf,directory.c_str());
    if (write(sock,buf,sizeof(buf)) < 0)
        perror_exit("write");

    int32_t ret1;
    char *data1 = (char*)&ret1;
    if(read(sock,data1,sizeof(ret1))<0)
        perror_exit("read");
    int file_counter = ntohl(ret1);

    string dir = "client" + to_string(getpid());
    if(mkdir(dir.c_str(),0777)==-1){
        if(errno!=EEXIST)
            perror_exit("mkdir");
    }

    while(1) {
        //cout << endl;
        int32_t ret;
        char *data = (char*)&ret;
        if(read(sock,data,sizeof(ret))<0)
            perror_exit("read");
        int bytes = ntohl(ret);
        char filename[bytes+1];
        if(read(sock,filename,bytes+1)<0) {
            perror_exit("read");
        }
        string name = filename;
        vector<string> split;
        size_t pos = 0;
        int counter=0;
        while ((pos = name.find("/")) != std::string::npos) {
            string token = name.substr(0, pos);
            //cout << token << endl;
            split.push_back(token);
            name.erase(0, pos + 1);
        }
        split.push_back(name);
        int block_size = atoi(split[split.size()-1].c_str());
        //cout << "block size: " << block_size << endl;
        split.pop_back();
        int filesize = atoi(split[split.size()-1].c_str());
        //cout << "filesize: " << filesize << endl;
        split.pop_back();
        string receive_message;
        for(int i=0 ; i<split.size() ; ++i){
            receive_message += split[i];
            if(i!=split.size()-1)
                receive_message += "/";
        }
        cout << "Received: " << receive_message << endl;
        string file_name = split[split.size()-1];
        split.pop_back();
        string name_of_file = "client"+ to_string(getpid())+"/";
        for(int i=0; i<split.size() ; ++i){
            name_of_file += split[i];
            if(mkdir(name_of_file.c_str(),0777)==-1){
                if(errno!=EEXIST)
                    perror_exit("mkdir");
            }
            name_of_file += "/";
        }
        name_of_file += file_name;
        int fd;
        //cout << "I will create: " << name_of_file << endl;
        if((fd=open(name_of_file.c_str(),O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))<0)
            perror_exit("open");
        //cout << "opened: " << name_of_file << endl;
        int ret_read;
        char to_write[block_size+1]={};
        int bytes_to_read;
        if(filesize>=block_size)
            bytes_to_read = block_size;
        else
            bytes_to_read = filesize;
        while((ret_read = (read(sock,to_write,bytes_to_read)))>0) {
            if(write(fd,to_write,ret_read)<0)
                perror_exit("write");
            filesize-=ret_read;
            if(filesize==0)
                break;
            if(filesize<block_size)
                bytes_to_read = filesize;
            memset(to_write,0,sizeof(to_write));
        }
        --file_counter;
        if(file_counter==0)
            break;
    }
    close(sock);
    cout << "All done!" << endl << "Client exiting..." << endl;
    return 0;
}


void perror_exit ( string message ) {
    perror(message.c_str());
    exit(EXIT_FAILURE);
}

string server_check (string argu,char* argv[]) {
    int arg_pos=1;
    while(arg_pos!=7){
        if(argv[arg_pos]==argu){
            return argv[arg_pos+1];
            break;
        }
        arg_pos += 2;
    }
    return "";
}