#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main( int argc, char *argv[])
{
	openlog(NULL,0,LOG_USER);
	
	if( argc != 3 )
	{
		printf("Error Need 2 arguments\n");
		syslog(LOG_ERR,"Error Need 2 arguments, provided %d\n",(argc-1));
		return 1;
	}
	
		
	const char *writedirectory = argv[1] ; //"something.txt";
	const char *writestring = argv[2];
	
	FILE *file = fopen(writedirectory, "wb");
	if ( file == NULL )
	{
		syslog(LOG_ERR,"Error, File not found : %s\n",argv[1]);
		return 1;
	}
	else
	{
		if( fputs(writestring,file) <= 0 )
		{
			syslog(LOG_ERR,"Error, File not found : %s\n",argv[1]);
			return 1;
		}
		else
		{
			syslog(LOG_DEBUG,"Writing %s to file %s\n",argv[2], argv[1]);
		}
	}
	
	fclose(file);
	closelog();

return 0;
}
