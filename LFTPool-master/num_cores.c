/* 
 * File:   num_cores.c
 * Author: FU, QIANG
 *
 * Modified by: qfu638
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
 
 
int getcpunum() {
        char buf[16] = {0};
        int num;
        FILE* fp = popen("cat /proc/cpuinfo |grep processor|wc -l", "r");
        if(fp) {                                                                                                                                                                             
           fread(buf, 1, sizeof(buf) - 1, fp);
           pclose(fp);
        }   
        num = atoi(buf);
        if(num <= 0){ 
            num = 1;
        }   
        return num;
}
 
int main(int argc, char *argv[])
{
    printf("This machine has %d cores.\n", getcpunum());
    return 0;
}