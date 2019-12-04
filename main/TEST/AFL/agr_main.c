#include <stdio.h>
#include <stdlib.h>
#include "agrlib_v2.h"
unsigned short int a = 6;
int main(int argc, char *argv[])
{
    char *path_;

    if(  argc != 2  ){
        return 0;
    }
    path_ = *(++argv);
    if(agregate(path_,&a) != NULL ){
	printf("************** END WITH OK ****************\n");
}else{
printf("************** END WITH ERROR ****************\n");

}

}
