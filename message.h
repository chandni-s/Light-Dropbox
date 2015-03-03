#ifndef PORT
#define PORT 10000
#endif
#define LISTENQ 5


#define MAXNAME 64
#define MAXCLIENTS 10
#define MAXFILES 10

#define CHUNKSIZE 256

struct login_message {
    char userid[MAXNAME];
    char dir[MAXNAME];
};

struct sync_message {
	char filename[MAXNAME];
	long int mtime;
	int size;
};


