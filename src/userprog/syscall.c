#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_valid_addrs(struct intr_frame *f)
{

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("system call!\n");
  void *esp = f -> esp;
  int sys_num = f -> vec_no;
  
  switch (sys_num)
  {
    case SYS_HALT:
      break;
    case SYS_EXIT:
      break;
    case SYS_EXEC: 
      break;
    case SYS_WAIT:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      break;
  }

  thread_exit ();
}
