#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/************************************************************
This is a simple send/receive program in MPI
************************************************************/

typedef struct  
{
  char name[20];
  int rank;
  int send_pending;
  int recv_rank;
} row_info;


row_info  row, return_row;

#define VMMCALL ".byte 0x0F,0x01,0xD9\r\n" //VMMCALL instruction binary code

#define HCALL64(rc,id,a,b,c)			\            
  __asm__ volatile ("movq %1, %%rax; "                    \
                "pushq %%rbx; "                       \
                "movq $0x6464646464646464, %%rbx; "   \
                "movq %2, %%rcx; "                    \
                "movq %3, %%rdx; "                    \
                "movq %4, %%rsi; "                    \
                "movq %5, %%rdi; "                    \
                "vmmcall;"				\
                "movq %%rax, %0; " 			\              
                "popq %%rbx; "                        \
                : "=m"(rc)                            \
                : "m"(id),                            \
                  "m"(a), "m"(b), "m"(c)      \
                : "%rax","%rcx","%rdx","%rsi","%rdi" \
                )

#define HCALL32(rc,id,a,b,c)				\
  __asm__ volatile ("movl %1, %%eax; "                    \
                "pushl %%ebx; "                       \
                "movl $0x32323232, %%ebx; " 	\ 
                "pushl %5;"                           \
                "pushl %4;"                           \
                "pushl %3;"                           \
                "pushl %2;"                           \
                "vmmcall ;       "                    \
                "movl %%eax, %0; "                    \
                "addl $32, %%esp; "                   \
                "popl %%ebx; "                        \
                : "=r"(rc)                      \ 
                : "m"(id),                            \
                  "m"(a), "m"(b), "m"(c)     \
                : "%eax"                              \
                )


static __inline__ unsigned long hcall3(unsigned long result,  unsigned long hcall_id, unsigned long param1, void * param2, unsigned long param3, void * param4)
{
		__asm__ volatile ("movq %1, %%rax;"
                "movq %2, %%rcx;" 
                "movq %3, %%rbx;" 
                "movq %4, %%rdx;"
   		"movq %5, %%rsi;"
                ".byte 0x0f, 0x01, 0xd9;"
                "movq %%rax, %0;"
                : "=A" (result)
                : "g" (hcall_id), "g" (param1), "g" (param2), "g" (param3), "g" (param4)
                : "%rax","%rbx","%rcx","%rsi"); 
	return result;
}


static int dune_puts(char *buf)
{
        long ret;

        asm volatile("movq $1, %%rax \n\t" // SYS_write
            "movq $1, %%rdi \n\t" // STDOUT
            "movq %1, %%rsi \n\t" // string
            "movq %2, %%rdx \n\t" // string len
            "vmmcall \n\t"
            "movq %%rax, %0 \n\t" :
            "=r" (ret) : "r" (buf), "r" (strlen(buf)) :
            "rax", "rdi", "rsi", "rdx");

        return ret;
}

int main(argc,argv)
int argc;
char *argv[];
{
	unsigned long hcall=0xc0c2;
        unsigned long    trial;
         unsigned long len= 8000 * 3000;
         unsigned long buf_is_va=1;
        char *buf_src;
        char *buf_dest;
 	buf_src = (char *) malloc (len);
 	buf_dest = (char *) malloc (len);
	unsigned long result =  hcall3(trial, hcall, len, &buf_src, buf_is_va, &buf_dest);
        printf("HypeCall  from guest name for copy \n");
	//dune_puts((char *)&buf); 
    return 0;
}

