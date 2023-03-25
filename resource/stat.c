#include <sys/stat.h>
#include <stdio.h>


int main(int arg,char* avg[]){
	
	printf("%s\n",avg[1]);
	struct stat st;
	int ret = stat(avg[1],&st);
	printf("%d\n",ret);
	return 0;
}
