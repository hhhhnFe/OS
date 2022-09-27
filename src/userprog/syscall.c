#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/off_t.h"

struct file 
{
  struct inode *inode;        
  off_t pos;                  
  bool deny_write;            
};

struct lock filesys_lock;
int readcnt;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void exit (int status, struct intr_frame *f)
{
  thread_current ()-> exit_status = status;
  printf ("%s: exit(%d)\n", thread_name(), status);
  f->eax = status;
  thread_exit ();
}

int read (int fd, void *buffer, unsigned len)
{
  int total_len;
  unsigned i;

  lock_acquire(&filesys_lock);
  // only from STDIN
  if (fd == 0)
  {
    for (i = 0; i < len; i++)
    {
      if (input_getc() == '\0') break;
    }
    total_len = i;
  }

  lock_release(&filesys_lock);
  return total_len;
}

int write(int fd, const void* buffer, unsigned len)
{
  int total_len;

  lock_acquire(&filesys_lock);

  // only to STDOUT
  if (fd == 1)
  {
    putbuf(buffer, len);
    total_len = len;
  }

  lock_release(&filesys_lock);
  return total_len;
}

void check_valid(void *ptr, struct intr_frame *f, int size)
{
    if (!is_user_vaddr (ptr) || !is_user_vaddr (ptr + size - 1)
    || pagedir_get_page (thread_current () -> pagedir, ptr) == NULL
    || pagedir_get_page (thread_current () -> pagedir, ptr + size - 1) == NULL)
    {
      exit (-1, f);
    }
  return;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  unsigned int *ptr = ((unsigned int *) f -> esp);
  int sysc_num;

  check_valid(&ptr[0], f, sizeof(unsigned int));
  sysc_num = ptr[0];
  switch (sysc_num)
  {
    case SYS_HALT: // 0 
      shutdown_power_off();
      break;
    case SYS_EXIT: // 1 : int status
      check_valid(&ptr[1], f, sizeof(unsigned int));
      exit(ptr[1], f);
      break;
    case SYS_EXEC: // 1 : char* cmdline
      check_valid(&ptr[1], f, sizeof(unsigned int));
      check_valid((void *) ptr[1], f, 1);
      f -> eax = process_execute((void*) ptr[1]);
      break;
    case SYS_WAIT: // 1 : pid_t pid
      check_valid(&ptr[1], f, sizeof(unsigned int));
      f -> eax = process_wait(ptr[1]);
      break;
    case SYS_READ: // 1 : int fd, 2 : void *buffer, unsigned size
      check_valid(&ptr[1], f, 3 * sizeof(unsigned int));
      check_valid((void*) ptr[2], f, ptr[3]);
      if ((int) ptr[1] == 0 && (void*) ptr[2] == NULL) exit(-1, f);
      f -> eax = read((int) ptr[1], (void*) ptr[2], (size_t) ptr[3]);
      break;
    case SYS_WRITE: // 1 : int fd, 2 : void *buffer, unsigned size
      check_valid(&ptr[1], f, 3 * sizeof(unsigned int));
      check_valid((void*) ptr[2], f, ptr[3]);
      if ((int) ptr[1] == 1 && (void*) ptr[2] == NULL) exit(-1, f);
      f -> eax = write((int) ptr[1], (void*) ptr[2], (size_t) ptr[3]);
      break;
    /*
    case FIBONACCI: // 4 arguments
      break;
    case MAX_OF_FOR_INT // 4 arguments
      break
    */ 
  }
}
