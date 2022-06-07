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
#include <queue>
#include <dirent.h>
#include <vector>
#include <sys/stat.h>
#include <fcntl.h>
#include <unordered_map>
#include <array>

using namespace std;

int sock;
int port;
int thread_pool_size;
int queue_size;
int block_size;
queue<pair<string,int>> file_names;
unordered_map<int,int> client_info;
unordered_map<int,pthread_mutex_t> client_mutex;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
//  -p <port>
//  -s <thread_pool_size>
//  -q <queue_size>
//  -b <block_size>
int argument_check( string , char** );
void perror_exit ( string );
void sigint_handler ( int );
void* worker_thread ( void* );
void* communication_thread ( void* );

int main(int argc, char* argv[]) {
    if(argc!=9) {
        cout << "Not enough arguments!" << endl;
        exit(1);
    }
    if( (port = argument_check("-p",argv))==-1 ) {
        cout << "No 'port' found!" << endl;
        exit(1);
    }
    if( (thread_pool_size = argument_check("-s",argv))==-1 ) {
        cout << "No 'thread pool size' found!" << endl;
        exit(1);
    }
    if( (queue_size = argument_check("-q",argv))==-1 ) {
        cout << "No 'queue size' found!" << endl;
        exit(1);
    }
    if( (block_size = argument_check("-b",argv))==-1 ) {
        cout << "No 'block size' found!" << endl;
        exit(1);
    }
    cout << "Server's paramaters are:" << endl;
    cout << "port: " << port << endl;
    cout << "thread_pool_size: " << thread_pool_size << endl;
    cout << "queue_size: " << queue_size << endl;
    cout << "block_size: " << block_size << endl;

    //pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t)*thread_pool_size);
    for(int i=0 ; i < thread_pool_size ; ++i) {
        pthread_t thread;
        if( pthread_create(&thread,NULL,worker_thread,NULL)!=0 )
            perror_exit("pthread_create");
    }
    
    struct sockaddr_in server, client;
    struct sockaddr* serverptr = (struct sockaddr*) &server;
    struct hostent* rem;

    signal(SIGINT,sigint_handler);

    if ( ( sock = socket (AF_INET,SOCK_STREAM,0) ) < 0 )
        perror_exit ("socket") ;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    if ( (bind(sock,serverptr, sizeof(server))) < 0 )
        perror_exit ("bind") ;
    if ( listen(sock,10) < 0 )
        perror_exit ("listen");
    cout << "Server was successfully initialized..." << endl;
    cout << "Listening for connections to port " << port << endl;
    while(1){
        struct sockaddr* clientptr = (struct sockaddr*) &client;
        socklen_t clientlen = sizeof(client);
        int newsock;
        if ((newsock = accept(sock, clientptr, &clientlen)) < 0)
            perror_exit("accept");
        cout << "{Server}: Accepted connection from localhost" << endl;
        pthread_t com_thread;
        if (pthread_create(&com_thread,NULL,communication_thread,&newsock)!=0)
            perror_exit("pthread_create");
    }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void* worker_thread (void* argu) {
    while(1) {
        //int empty_client=0;
        pthread_mutex_lock(&queue_mutex);
        while(file_names.empty()) {
            pthread_cond_wait(&cond_nonempty,&queue_mutex);
        }
        pair<string,int> p;
        p = file_names.front();
        file_names.pop();
        string prnt = "[work_Thread: " + to_string(pthread_self()) + "]: Received task: <" + p.first + ", " + to_string(p.second) + ">";
        cout << prnt << endl;
        fflush(stdout);
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&cond_nonfull);
        int filesize;
        struct stat sb{};
        if(!stat(p.first.c_str(),&sb)) {
            filesize = sb.st_size;
        } else {
            perror("stat");
        }
        string temp_str = p.first + "/" + to_string(filesize) + "/" + to_string(block_size);
        char buffer[temp_str.length()+1];
        strcpy(buffer,temp_str.c_str());
        int32_t conv = htonl(temp_str.length());
        char *data = (char*)&conv;
        pthread_mutex_lock(&client_mutex.at(p.second));
        if(write(p.second,data,sizeof(conv))<0)
            perror_exit("write");
        if(write(p.second,buffer,sizeof(buffer))<0)
            perror_exit("write");
        int fd;
        if((fd = open(p.first.c_str(),O_RDONLY,0))<0)
            perror_exit("open");
        int ret;
        char to_write[block_size+1]={};
        while((ret = (read(fd,to_write,block_size)))>0){
            if(write(p.second,to_write,ret)<0)
                perror_exit("write");
            filesize-=ret;
            if(filesize==0)
                break;
            memset(to_write,0,sizeof(to_write));
        }
        // <file_desc,1>
        pthread_mutex_unlock(&client_mutex.at(p.second));
    }
    pthread_exit(NULL);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void* communication_thread (void* argu) {
    int client_socket = *((int*)argu);
    char buf[512]={};
    if (read(client_socket, buf, sizeof(buf)-1) < 0)
        perror_exit("read");
    if (strlen(buf)==0 || buf==NULL) {
        string prnt_error = "[com_Thread: " + to_string(pthread_self()) + "]: Received nothing from client! Aborting process!";
        cout << prnt_error << endl;
        fflush(stdout);
        pthread_exit(NULL);
    }
    string prnt1 = "[com_Thread: " + to_string(pthread_self()) + "]: About to scan directory: < " + buf + " >";
    cout << prnt1 << endl;
    fflush(stdout);
    queue<string> folders;
    string temp = buf;
    folders.push(temp);
    DIR *dir;
    struct dirent *ent;
    int file_counter=0;
    pair<int,int> info;
    vector<pair<string,int>> client_temp;
//    info.first = client_socket;
//    info.second = -1;
//    client_info.push_back(info);
//    info.second = 0;
//    worker_info.push_back(info);
    while(!folders.empty()) {
        string curr_folder = folders.front();
        folders.pop();
        if ((dir = opendir(curr_folder.c_str())) != NULL) {
            /* print all the files and directories within directory */
            while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] == '.')
                    continue;
                string name = ent->d_name;
                if ((name.find('.') != std::string::npos) || name=="makefile") {
                    pair<string,int> p;
                    p.first = curr_folder + "/" + name;
                    p.second = client_socket;
                    client_temp.push_back(p);
                    ++file_counter;
                } else {
                    folders.push(curr_folder + "/" + name);
                }
            }
            closedir(dir);
        } else {
            /* could not open directory */
            perror_exit("opendir");
        }
    }
    client_info[client_socket] = file_counter;
    int32_t conv = htonl(file_counter);
    char *data = (char*)&conv;
    if(write(client_socket,data,sizeof(conv))<0)
        perror_exit("write");
    pthread_mutex_t thr = PTHREAD_MUTEX_INITIALIZER;
    client_mutex[client_socket] = thr;
    for(int i=0 ; i<client_temp.size() ; i++) {
        pthread_mutex_lock(&queue_mutex);
        while(file_names.size()==queue_size) {
            pthread_cond_wait(&cond_nonfull,&queue_mutex);
        }
        string prnt2 = "[com_Thread: " + to_string(pthread_self()) + "]: Adding file " + client_temp[i].first + " to the queue";
        cout << prnt2 << endl;
        fflush(stdout);
        file_names.push(client_temp[i]);
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&cond_nonempty);
    }
    pthread_exit(NULL);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

void sigint_handler ( int sig ) {
    cout << endl << "Closing Server's socket..." << endl;
    close(sock);
    cout << "Server's socket closed!" << endl << "Server exiting..." << endl;
    exit(0);
}

void perror_exit ( string message ) {
    perror(message.c_str());
    exit(EXIT_FAILURE);
}

int argument_check (string argu,char* argv[]) {
    int arg_pos=1;
    while(arg_pos!=9){
        if(argv[arg_pos]==argu){
            return atoi(argv[arg_pos+1]);
            break;
        }
        arg_pos += 2;
    }
    return -1;
}