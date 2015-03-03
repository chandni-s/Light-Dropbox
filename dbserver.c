#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>        /* for getenv */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */
#include "filedata.c"

#include <sys/stat.h>
#include <sys/types.h>

#define MAXCLIENTS 10

//#include "filedata.h"
#ifndef PORT
#define PORT 53799
#endif
void get_files (struct client_info *client, struct sync_message sync);
void write_to_client (struct client_info *client, char filename[]);
int main()
{
    int i, fd, clientfd, maxfd, nready;
    //struct client_info clients[MAXCLIENTS]; already in filedata.c
	socklen_t len;
	struct sockaddr_in r, q;
	fd_set allset;
	fd_set rset;
	int k;
	int opt = 1;
    
    struct login_message msg;
   	struct sync_message syncmsg,respmsg;
   	struct file_info *find;
	struct file_info new_files [MAXFILES];
	char buf [CHUNKSIZE];
    
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return(1);
	}
	memset(&r, '\0', sizeof r);
	r.sin_family = AF_INET;
	r.sin_addr.s_addr = INADDR_ANY;
	r.sin_port = htons(PORT);
    	printf("Listening on %d\n", PORT);
	
	if (bind(fd, (struct sockaddr *)&r, sizeof r) < 0) {
		perror("bind");
		return(1);
	}
	if (listen(fd, 10)) {
		perror("listen");
		return(1);
	}
	// Make sure we can reuse the port immediately after the
    	// server terminates. Avoids the "address in use" error
	//set master socket to allow multiple connections , this is just a good habit, it will work without this
    	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    	{
        	perror("setsockopt");
       		exit(EXIT_FAILURE);
    	}
    	init(); //initialize client list to be empty
	FD_ZERO(&allset);
	FD_SET(fd, &allset);
	maxfd = fd;
    
	while(1) {
		rset = allset;
		nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
		if(nready == 0) {
			printf("timeout happened\n");
			continue;
		}
        
		if(nready == -1) {
			perror("select");
			break;
		}
        
		if (FD_ISSET(fd, &rset)){
			printf("Someone is connecting\n");
			len = sizeof q;
			if ((clientfd = accept(fd, (struct sockaddr *)&q, &len)) < 0) {
				perror("accept");
				return(1);
			}
			for(i = 0; i < MAXCLIENTS; i++) {
				if(clients[i].sock == -1) {
					
					clients[i].sock = clientfd;
					clients[i].STATE = 0; //log-in
                    
					FD_SET(clientfd, &allset);
					
					if (clientfd > maxfd) {
						maxfd = clientfd;
                    }
					break;
				}
			}
			//display_clients();					
		}
		for(i = 0; i < MAXCLIENTS; i++) {
			if(clients[i].sock != -1 && FD_ISSET(clients[i].sock, &rset)) {
				/* safe to read */
				//check state
				if (clients[i].STATE == 0){ //logging in time
					int j;
					if((k = recv(clients[i].sock,(struct login_message*)&msg, sizeof(struct login_message),0)) < 0) {
						perror ("read:");
						clients[i].sock = -1; //reset the socket
						continue;//if error, skip through everything until someone connects
					
					}
					else {
						printf("log in message was received with %d bytes\n",k);
					}
					if (add_client (msg) < 0){ //cannot add client
						//print from stderr
						continue; //if error, skip through everything until someone connects
					}
					strncpy(clients[i].userid, msg.userid, sizeof(clients[i].userid));
					printf ("%s have logged in\n", clients[i].userid);
					strncpy(clients[i].dirname, msg.dir, sizeof(clients[i].dirname));
					printf ("%s wants to access %s\n", clients[i].userid, clients[i].dirname);
					
					for (k=0; k < MAXCLIENTS; k++) { //go through clients
						if ((clients[k].sock != -1) && (strcmp(clients[k].dirname,msg.dir)) ==0 && (i!=k)) { //same directory as client k
							for (j = 0; j < MAXFILES; j++){
								clients[i].files[j] = clients[k].files[j]; //set them to the same files thus "sharing directory"
							}
						}
					}
					display_clients();
					clients[i].STATE = 1;
					for (k = 0; k < MAXFILES; k++){ //create a copy of cleint i's files for checking purposes
						strncpy(new_files[k].filename,clients[i].files[k].filename, sizeof(new_files[k].filename));
						printf ("File %s added to newfiles\n",new_files[k].filename);
						new_files[k].mtime = clients[i].files[k].mtime;
					}
				}	
				else if (clients[i].STATE == 1){ //syncing time
					
					memset (syncmsg.filename, '\0', sizeof(syncmsg.filename));
					if((k = read(clients[i].sock, syncmsg.filename, sizeof(syncmsg.filename))) > 0) { //read sync message from client
						printf ("Server received file %s with %d bytes\n",syncmsg.filename, k);
						memset (buf, '\0', sizeof(buf));
						k = read(clients[i].sock,buf, sizeof(buf)); //copy time
						printf ("Server received time %s with %d bytes\n",buf, k);
						syncmsg.mtime = atol(buf);
						
						memset (buf, '\0', sizeof(buf));
						k = read(clients[i].sock,buf, sizeof(buf)); //copy size
						printf ("Server received size %s with %d bytes\n",buf, k);
						syncmsg.size = atoi(buf);
						for (k = 0; k < MAXFILES; k++){ //remove file from new_file if client has it
							if (strcmp (new_files[k].filename,syncmsg.filename) == 0){
								memset(new_files[k].filename,'\0', sizeof(new_files[k].filename)); //set to null
								new_files[k].mtime = 0;
							}
						}	
						//sync message created				
					}
					else {					
						printf("Someone disconnected\n");
						clients[i].sock = -1;
						continue; //skip this client
						
					}
					if (syncmsg.filename[0] =='\0'){ //empty sync message

						printf("getting empty sync message\n");	

						for (k = 0; k < MAXFILES; k++){ //create a copy of cleint i's files for checking purposes
							printf ("Newfiles have %s\n",new_files[k].filename);
						}					
						for (k = 0; k < MAXFILES; k++){ //go through the new_files list and send them to client
							if (new_files[k].filename[0] !='\0'){ //filename is not empty
								//send response message								
								printf("sending response message");
								memset(respmsg.filename,'\0', sizeof(respmsg.filename));
								strncpy(respmsg.filename,new_files[k].filename,sizeof(respmsg.filename));
								write (clients[i].sock, respmsg.filename,sizeof(respmsg.filename));

								memset (buf, '\0', sizeof(buf));
								snprintf(buf, sizeof(buf), "%ld", new_files[k].mtime); //convert sync.time to char[] for write
								k = write (clients[i].sock, buf, sizeof(buf)); //write time back

								write_to_client (&clients[i], new_files[k].filename);
								printf("Need to transfer file %s to client\n", new_files[k].filename); 
							}
							else{
								printf ("running loop %d\n",k);
							}
						}
						printf("sending empty message");
						//send empty message
						memset(respmsg.filename,'\0', sizeof(respmsg.filename));
						write (clients[i].sock, respmsg.filename,sizeof(respmsg.filename));

						k = write (clients[i].sock, "0", sizeof(char)); //write 0 time back
				
						k = write (clients[i].sock, "0", sizeof(char)); //write 0 size back
					}
					else {
						printf("finding file from sync message\n");
						//find sync message filename in client's file list
						find = check_file(clients[i].files,syncmsg.filename);
											
						if (find != NULL){ //if filename was found or added
							printf("File found from sync message\n");
							for (k = 0; k < MAXFILES; k++){ //go through file list and take out if in client
								if (strcmp(new_files[k].filename,find->filename)==0){
									//if found, set filename to null in new_files so we know client has it
									new_files[k].filename[0] = '\0';
								}
							}
							//create response message with file's info	
							memset(respmsg.filename,'\0', sizeof(respmsg.filename));
							strncpy(respmsg.filename,find->filename,sizeof(respmsg.filename));
							respmsg.mtime = find->mtime;
							respmsg.size = 0; //just set size to 0 since it is redundant
							//response message created
							if((k = write (clients[i].sock, respmsg.filename,sizeof(respmsg.filename))) <0){
								perror ("Write:");
								continue;
							}
							printf ("Server sent file %s with %d bytes\n",respmsg.filename, k);

							memset (buf, '\0', sizeof(buf));
							snprintf(buf, sizeof(buf), "%ld", respmsg.mtime); //convert sync.time to char[] for write
							k = write (clients[i].sock, buf, sizeof(buf)); //write time back
							printf ("Server sent time %s with %d bytes\n",buf, k);	
				
							memset (buf, '\0', sizeof(buf));
		                			snprintf(buf, sizeof(buf), "%d", respmsg.size); //convert sm.size to char[] for writi
							k = write (clients[i].sock, buf, sizeof(buf)); //write size back
							printf ("Server sent size %s with %d bytes\n\n",buf, k);
				
							if (difftime(syncmsg.mtime, respmsg.mtime) < 0){ //if server more recent
								printf("Server file is more recent\n");
								write_to_client (&clients[i], respmsg.filename);
								clients[i].STATE = 1;
							}	
							else{
								printf("2.\n");
								clients[i].STATE = 2;
							}
						}
					}
					
				}
				else if (clients[i].STATE == 2){ //get that file
					printf("Getting the file %s from client\n",syncmsg.filename); //print continued
					get_files(&(clients[i]), syncmsg);
					clients[i].STATE = 1; //set to sync
				}				
			}
		}        
	}
	printf("Server died");
	close(clientfd);
	close(fd);
	return(0);
}
//write a given file name through syncmsg to client side
void write_to_client (struct client_info *client, char filename[]){
	if ((chdir("/Users/chandni/Documents/CSC209/A4/test")) != 0) { //change directory to open file
     		perror("chdir");
 	}
	FILE *fp_client;
	int bytes;
	int bytes_read, bytes_written;
	char ch [CHUNKSIZE];
	//get file size
	struct stat f;
    	if (stat (filename, &f) !=0){
       		 perror("Stat:");
    	}
    	bytes = (intmax_t)f.st_size;
	fp_client = fopen(filename, "r");
	if (fp_client == NULL) {
		perror("open: ");
	}
	//printf("File %d has size %d bytse \n", filename,file_size);
	while (bytes > 0) {
		printf("writing...\n");
		memset (ch, '\0', sizeof(ch)); //reset buf before reading
		if ((bytes_read = read(fileno(fp_client),ch,sizeof(ch))) < 0){
			perror("read");
		}
		printf("bytes read from %s is %d\n", filename, bytes_read);
		if ((bytes_written = write(client->sock,ch,bytes_read)) < 0){
			perror ("write:");
		}
		printf("bytes written to client from %s is %d\n", filename, bytes_written);
		bytes = bytes - bytes_written;
	}
        fclose(fp_client); //close the file
}

void get_files (struct client_info *client, struct sync_message sync){
	FILE *fp;
	int bytes_read,bytes_written;
	char buf [CHUNKSIZE];
	int file_size;        
	if ((chdir("/Users/chandni/Documents/CSC209/A4/test")) != 0) { //change directory to open file
     		perror("chdir");
 	}
	printf ("file name %s\n", sync.filename);	
	fp = fopen (sync.filename,"w+"); //open or create file for writing
	if (fp == NULL){
		perror("File open:");
	}       
	memset(buf,'\0',sizeof(buf));
        if ((bytes_read = read(client->sock, buf, sizeof(buf))) < 0){
		perror ("Read:");
	}
        file_size = atoi (buf);
	printf("size %d read with %d bytse \n",file_size, bytes_read);
	while (file_size > 0){
		memset(buf,'\0',sizeof(buf));
		bytes_read = read(client->sock,buf,sizeof(buf));
		if ((bytes_written = write(fileno(fp),buf,bytes_read)) < 0){
			perror("Write:");
		}
		//printf("Written: %s",buf);
		file_size = file_size - bytes_written;	
	}
	printf("reading done \n");
	fclose(fp);
}

