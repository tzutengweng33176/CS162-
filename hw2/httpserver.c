#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

#define BUFF_SIZE 1024

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;


/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */

int is_File(char *path){
	struct stat path_stat;
	stat(path,&path_stat);
	return S_ISREG(path_stat.st_mode);
}

int is_Directory(char *path){
	struct stat path_stat;
	stat(path,&path_stat);
	return S_ISDIR(path_stat.st_mode);
}

void write_from_file_to_fd(int fd,char *file_path){
	int file_des = open(file_path,O_RDONLY);
	if(file_des == -1){
			printf("fail to open file\n");
	}


        char buffer[1024];
	while(read(file_des,buffer,sizeof(buffer)) != 0){
		http_send_string(fd,buffer);
	}
        close(file_des);
}

int read_from_target_write_to_client(int target_fd,int client_fd){
	char buffer[BUFF_SIZE];
	int nbytes;
	nbytes = read(target_fd,buffer,BUFF_SIZE);
	printf("received %d bytes.\n",nbytes);
	printf("%s",buffer);
	if(nbytes < 0){
		perror("read");
		exit(EXIT_FAILURE);
	}
	else if (nbytes == 0){
		return -1;
	}
	else{
		http_send_string(client_fd,buffer);
		return 0;
	}
}


void handle_files_request(int fd) {

  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */

  struct http_request *request = http_request_parse(fd);
//When you press the button, you're requesting my_documents, which is a directory that does not contain index.html, so we should return a list of files 

  
 char file_path[1024];
 


 strcpy(file_path, server_files_directory);  
 strcat(file_path, request->path); //concatenate path to server_files_directory
  printf("File path is %s\n", file_path);
  
 if(is_Directory(file_path)){   
     http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
 // how do I know whether index.html file exists in the directory?
    //list the files of the directory
    DIR* dir=opendir(file_path);
     //check if there is index.html
    struct dirent *dp;

    int a=0;
    do {
        if ((dp = readdir(dir)) != NULL) {
	    if (strcmp(dp->d_name, "index.html") == 0){
                (void) printf("found %s\n", "index.html");
                (void) closedir(dir);
                a=1;
                strcat(file_path,"/index.html");
                write_from_file_to_fd(fd,file_path);
             }else{  //if index.html is not in the directory
              if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..")!=0 ){
                  http_send_string(fd,"<a href=\"");
                  http_send_string(fd,  dp->d_name);
                  http_send_string(fd, "\">") ;
                  http_send_string(fd,  dp->d_name);
                  http_send_string(fd,"</a>""</br>");
                }
             }
    }} while (dp != NULL);
    if(a==0){
     http_send_string(fd,"<a href=\"..");
    //.. means the parent of the current working directory 
     http_send_string(fd, "\">"); 
     http_send_string(fd,"Parent directory");
     http_send_string(fd, "</a>""</br>");
}

}   else if(is_File(file_path)){
  	
         http_start_response(fd, 200);
        http_send_header(fd, "Content-type", http_get_mime_type(request->path));
  	http_end_headers(fd);
          http_send_string(fd,
      "<center>"
      "<h1>Welcome to httpserver!</h1>"
      "<hr>");
       char *file_extension = strrchr(file_path, '.');
      if (strcmp(file_extension, ".jpg") == 0 || strcmp(file_extension, ".jpeg") == 0) {
        http_send_string(fd, "<style>""img {width: 100%;}" "</style>" );
       http_send_string(fd, "<body>");
       http_send_string(fd, "<img src=../");
       http_send_string(fd,file_path);
       http_send_string(fd,"alt=\"Test\"" "style=\"width:256px;height:256px;\"" ">");
     http_send_string(fd, "</body>");
     }else{
      
     http_send_string(fd,"<object data=\"");
     http_send_string(fd,file_path);
     http_send_string(fd, "\">" "type=\"text/plain\"");
     http_send_string(fd,">");
     http_send_string(fd, "</object>");

     
  }
      http_send_string(fd,"</center>");
}else{  
    http_start_response(fd, 404);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
     http_send_string(fd,
      "<center>"
      "<h1>404 Not Found</h1>"
      "<hr>"
      "<p>404 Not found</p>"
      "</center>");
     
}

}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */

// struct sockaddr_in {
//               sa_family_t    sin_family; /* address family: AF_INET */
//               in_port_t      sin_port;   /* port in network byte order */
//               struct in_addr sin_addr;   /* internet address */
 //          };

           /* Internet address. */
//           struct in_addr {
//               uint32_t       s_addr;     /* address in network byte order *///          };

  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);
//The htons() function converts the unsigned short integer hostshort from host byte order to network byte order.


  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client_socket_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));
// connect() system call connects the socket referred to by the file descriptor client_socket_fd to the address specified by target_address. 

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    return;

  }

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
}


void* thread_loop(void *f){
    void (*request_handler)(int) = f;
    int client_fd;
    printf("Thread created\n");

    while(1){
        client_fd = wq_pop(&work_queue); //thread created will wait on the condition variable!
        request_handler(client_fd);
        printf("I'm back %lu!\n", (unsigned long)pthread_self());
        close(client_fd);
        printf("closed fd %d\n", client_fd);
    }

    return NULL;
}



void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  /*
   * TODO: Part of your solution for Task 2 goes here!
   */
   int i;
   pthread_t thread;

  
     // Initialize num threads
   for(i=0; i<num_threads; i++){
      if(pthread_create(&thread, NULL, thread_loop, request_handler)!=0){
        fprintf(stderr, "Couldn't initialize thread %d, Error", i);
    }

   }  
}
/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;
 


//  pid_t pid;
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

//assigning a name to a socket
  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {//Wait a client to connect to the port
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }
    //Accept the client and obtain a new connection socket

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
     //Read in and parse the HTTP request
    // TODO: Change me?
    
    if(num_threads>0){
    wq_push(&work_queue, client_socket_number);
    //will signal thread waiting on the condition variable 
      

    }else{

    request_handler(client_socket_number);
    close(client_socket_number);

    }
 /*   pid = fork();
    if (pid > 0) {
      close(client_socket_number);
    } else if (pid == 0) {
      signal(SIGINT, SIG_DFL); // Un-register signal handler (only parent should have it)
      close(*socket_number);
      request_handler(client_socket_number);
      close(client_socket_number);
      exit(EXIT_SUCCESS);
    } else {
      fprintf(stderr, "Failed to fork child: error %d: %s\n", errno, strerror(errno));
      exit(errno);
    }
*/


    /*printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port); ORIGINAL CODE*/
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);
  wq_init(&work_queue);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {//file mode
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {//proxy mode
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }

    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
