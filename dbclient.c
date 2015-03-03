/* client process */
#include <unistd.h>
#include <errno.h>
#include <dirent.h> 
#include <stdio.h> 
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/stat.h>

#include "filedata.c"

#define MAXNAME 64
#define MAXSIZE 1023
#define MAXCLIENTS 10
#define MAXFILES 10
#define CHUNKSIZE 256

#ifndef PORT
#define PORT 53799
#endif

void safegets (char s[], int arraySize);
time_t get_modified_time (char *filename, char *path);
int get_size (char *filename, char *path);
void send_file (struct sync_message sm, int sock);
void get_file (struct sync_message sm, int sock);
int main(int argc, char *argv[])
{
	int sock;
	//struct hostnet *hostname;	//get host by name
	struct sockaddr_in sock_addr;
	struct login_message lm;
	struct sync_message sm, response;
	FILE *fp;
        char buf[CHUNKSIZE];
	
	sock_addr.sin_family = AF_INET;
   	sock_addr.sin_port = htons(PORT);
	

	/*set up socket*/
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
	}

	/* connect socket to servers's address - if failed-exit*/
	if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) == -1) 
	{
		perror("client:connect");
		//close(sock);
		exit(1);
	}
	printf("Writing to %d\n", PORT);
	/* send and receive info from server*/
	printf("Please enter userid: ");
	safegets(lm.userid, MAXSIZE);
	printf("Please enter director to sync: ");
	safegets(lm.dir, MAXSIZE);
	printf("\n");
	int j;
	//write userid and dir name on socket 
	j = send(sock, (struct sync_message*)&lm, sizeof(struct login_message),0);
	printf ("log in message sent with %d bytes\n", j);
    	strncpy(lm.dir,"/Users/chandni/Documents/CSC209/A42/test",sizeof(lm.dir));
	//process for synch
	DIR *d;
        struct dirent *dir;
        //struct stat f;
        d = opendir(lm.dir); //open directory stored in lm
	int bytes = 1;
   
        if (d)
        {
                //count # of files in dir
		printf("opening the dir\n");
                while ((dir = readdir(d)) != NULL)
                {
                        if (dir->d_name[0] =='.'){
                                continue;
                        }
			memset (sm.filename, '\0', sizeof(sm.filename));
			memset (response.filename, '\0', sizeof(response.filename));

                        strncpy(sm.filename,dir->d_name,sizeof(sm.filename)); //copy filename from dir                                                
                        sm.mtime = get_modified_time(dir->d_name, lm.dir);   //storing w/o format time into sm.mtime
                        sm.size = get_size(dir->d_name, lm.dir);
			
			if ((bytes = write(sock, sm.filename, sizeof(sm.filename))) <= 0){
				perror ("Write:");
                                exit(1);
			}
			printf ("Client sent file %s with %d bytes\n", sm.filename,bytes);
                    	
			memset (buf, '\0', sizeof(buf));//reset buf before writing
                        snprintf(buf, sizeof(buf), "%ld", sm.mtime); //convert sm.time to char[] for wri
                        bytes = write(sock, buf, sizeof(buf));
			printf ("Client sent time %s with %d bytes\n", buf,bytes);

                        memset (buf, '\0', sizeof(buf));
                        snprintf(buf, sizeof(buf), "%d", sm.size); //convert sm.size to char[] for writi
                        bytes = write(sock, buf, sizeof(buf));
			printf ("Client sent size %s with %d bytes\n", buf,bytes);

                        if((bytes = read(sock, response.filename, sizeof(response.filename))) < 0) {
                                perror("read");
                                exit(1);
                        }
			printf ("Client recieved file %s with %d bytes\n", response.filename,bytes);

                    	memset (buf, '\0', sizeof(buf)); //reset buf before reading
                        read(sock, buf, sizeof(buf)); //store response time string into buf
                        response.mtime = atol (buf);			
			//printf ("Client received time %s with %d bytes\n", buf,bytes);

			memset(buf, '\0', sizeof(buf));//reset buf before reading                   
                        read(sock, buf, sizeof(buf));
                        response.size = atoi (buf);
			printf ("Client received size %s with %d bytes\n", buf,bytes);

			//response message received
			if (difftime(sm.mtime, response.mtime) >= 0){ //if client more recent
				printf("File %s is more recent on client\n", sm.filename);
				send_file(sm,sock);
				printf("File %s transferred to server\n",sm.filename);
			}
			else{	
				printf("File %s is more recent on server\n", sm.filename);
				get_file(sm,sock);
				printf("File %s received from server\n",sm.filename);
			} 
			//send empty message
			printf("sending empty message\n");
			memset (sm.filename, '\0', sizeof(sm.filename));
                        sm.mtime = 0;   //storing w/o format time into sm.mtime
                        sm.size = 0;
			bytes = write(sock, sm.filename, sizeof(sm.filename));
                    	
			memset (buf, '\0', sizeof(buf));//reset buf before writing
                        snprintf(buf, sizeof(buf), "%ld", sm.mtime); //convert sm.time to char[] for wri
                        bytes = write(sock, buf, sizeof(buf));
                        memset (buf, '\0', sizeof(buf));
                        snprintf(buf, sizeof(buf), "%d", sm.size); //convert sm.size to char[] for writi
                        bytes = write(sock, buf, sizeof(buf));
			printf("reading response message\n");
			//read response msg
			memset (response.filename, '\0', sizeof(response.filename));//reset buf before writing                        
			bytes = read(sock, response.filename, sizeof(response.filename));

                    	memset (buf, '\0', sizeof(buf)); //reset buf before reading
                        read(sock, buf, sizeof(buf)); //store response time string into buf
                        response.mtime = atol (buf);			

                        response.size = 0;
			while (response.filename[0] != '\0' || response.mtime > 0){
				printf ("Receiving file %s \n",response.filename);
				get_file(response,sock);
				//read response msg
				memset (response.filename, '\0', sizeof(buf));//reset buf before writing                        
				bytes = read(sock, response.filename, sizeof(response.filename));

                    		memset (buf, '\0', sizeof(buf)); //reset buf before reading
                        	read(sock, buf, sizeof(buf)); //store response time string into buf
                       		response.mtime = atol (buf);			

				memset(buf, '\0', sizeof(buf));//reset buf before reading                   
                        	read(sock, buf, sizeof(buf));
                        	response.size = atoi (buf);
			}
						  
		}  
	}       
	return 0;
}
void send_file (struct sync_message sm, int sock){
	int bytes_left,bytes_read,bytes_written;
	FILE *fp;
	char buf[CHUNKSIZE];
	memset (buf, '\0', sizeof(buf));
	snprintf(buf, sizeof(buf), "%d", sm.size); //convert sm.size to char[] for writing
	bytes_written = write(sock, buf, sizeof(buf));
	printf("size %s written with %d bytse \n",buf,bytes_written);

	if (chdir("/Users/chandni/Documents/CSC209/A4/test") != 0) { //change directory to open file
	  perror("chdir");
	}
        fp = fopen (sm.filename,"r"); //open file for reading
	if (fp == NULL){
		perror("File open:");
		exit(1);
	}
	bytes_left = sm.size;
        while(bytes_left > 0){
		memset (buf, '\0', sizeof(buf)); //reset buf before reading
		if ((bytes_read = read(fileno(fp),buf,sizeof(buf))) < 0){
			perror("read");
		}
		if ((bytes_written = write(sock,buf,sizeof(buf))) < 0){
			perror ("write:");
			exit(1);
		}
		bytes_left = bytes_left - bytes_read;
	}
        fclose(fp); //close the file
}
void get_file (struct sync_message sm, int sock){
	int bytes_left,bytes_read,bytes_written;
	FILE *fp;
	char buf[CHUNKSIZE];
	if (chdir("/Users/chandni/Documents/CSC209/A4/test") != 0) { //change directory to open file
	  perror("chdir");
	}
	printf("File %s is more recent on server\n", sm.filename);
	fp = fopen(sm.filename, "w+");
	if (fp == NULL) {
		perror("open: ");
		exit(1);
	}
	memset(buf, '\0' ,sizeof(buf));
	if ((bytes_read = read(sock, buf, sizeof(buf))) < 0) {
		perror("read");
		exit(1);
	}
	bytes_left = atoi(buf);
	while (bytes_left > 0) {
		memset(buf, '\0', sizeof(buf));
		bytes_read = read(sock, buf, sizeof(buf));
		if ((bytes_written = write(fileno(fp), buf, bytes_read)) < 0) {
			perror("write:");
		}
		bytes_left = bytes_left - bytes_written;
	}
	fclose(fp);
}
// Function to get a line of input without overflowing target char array.
void safegets (char s[], int arraySize)
{
    int i = 0, maxIndex = arraySize-1;
    char c;
    while (i < maxIndex && (c = getchar()) != '\n')
    {
        s[i] = c;
        i = i + 1;
    }
    s[i] = '\0';
}

time_t get_modified_time (char *filename, char *path){
    if (chdir(path) != 0) {
        perror("chdir");
    }
   struct stat f;
   stat (filename, &f);
   //strftime(date, 20, "%d-%m-%y", localtime(&t));
   //printf("The file %s was last modified at %ld\n", filename, f.st_mtime);
   return f.st_mtime;
}

int get_size (char *filename, char *path){
   if (chdir(path) != 0) {
       perror("chdir");
   }
    struct stat f;
    if (stat (filename, &f) !=0){
        perror("Stat:");
    }
    //printf("File %s has size: %ld\n",filename, (intmax_t)f.st_size);
    //printf("File %s has size: %lld\n",filename, f.st_size);
    return (intmax_t)f.st_size;
}
   
