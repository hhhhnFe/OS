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

bool create (const char *file, unsigned initial_size)
{
  return filesys_create(file, initial_size);
}

bool remove (const char *file)
{
  return filesys_remove(file);
}

int open (const char *file, struct intr_frame *f)
{
  lock_acquire(&filesys_lock);
  struct file* fp = filesys_open(file);

  if (fp == NULL)
  {
    lock_release(&filesys_lock);
    return -1;
  }
  struct thread *cur_thread = thread_current();
  for (int i = 3; i < 128; i++)
  {
    if (cur_thread-> t_fd[i] == NULL)
    {
      thread_current() -> t_fd[i] = fp;
      lock_release(&filesys_lock);
      return i;
    }
  }

  lock_release(&filesys_lock);
  return -1;
}

int filesize (int fd, struct intr_frame *f)
{
  lock_acquire(&filesys_lock);  
  struct file* file = thread_current() -> t_fd[fd];

  if (file == NULL)
  {
    lock_release(&filesys_lock);
     exit(-1, f);
  }

  int ret = file_length(file);

  lock_release(&filesys_lock);
  return ret;
}
      

int read (int fd, void *buffer, unsigned len, struct intr_frame *f)
{
  int total_len = 0;
  unsigned i;

  lock_acquire(&filesys_lock);
  if (fd == 0)
  {
    for (i = 0; i < len; i++)
    {
      *(char *)(buffer + i) = input_getc(); 
      if (*(char*)(buffer + i) == '\0') break;
    }
    total_len = i;
  }
  else if (fd <= 1 || fd >= 128)
  {
    lock_release(&filesys_lock);
    return -1;
  }
  else
  {
    struct file* file = thread_current() -> t_fd[fd];
    if (file == NULL)
    {
      lock_release(&filesys_lock);
      exit(-1, f);
    }
    total_len = file_read(file, buffer, len);
  }

  lock_release(&filesys_lock);

  return total_len;
}

int write (int fd, const void* buffer, unsigned len, struct intr_frame *f)
{
  int total_len = 0;

  lock_acquire(&filesys_lock);
  if (fd == 1)
  {
    putbuf(buffer, len);
    total_len = len;
  }
  else if (fd <= 0 || fd >= 128)
  {
    lock_release(&filesys_lock);
    return 0;
  }
  else
  {
    struct file* file = thread_current() -> t_fd[fd];
    if (file == NULL)
    {
      lock_release(&filesys_lock);
      exit(-1, f);
    }
    if (file -> deny_write == false) total_len = file_write(file, buffer, len);
  }

  lock_release(&filesys_lock);
  return total_len;
}

void seek (int fd, unsigned pos, struct intr_frame *f)
{
  lock_acquire(&filesys_lock);
  struct file* file = thread_current() -> t_fd[fd];
  if (file == NULL)
  {
    lock_release(&filesys_lock);
    exit(-1, f);
  }

  file_seek(file, pos);
  lock_release(&filesys_lock);
}

unsigned tell (int fd, struct intr_frame *f)
{
  lock_acquire(&filesys_lock);

  struct file* file = thread_current() -> t_fd[fd];
  if (file == NULL)
  {
    lock_release(&filesys_lock);
    exit(-1, f);
  }
  unsigned ret = file_tell(file);

  lock_release(&filesys_lock);
  return ret;
}

void close (int fd, struct intr_frame *f)
{
  lock_acquire(&filesys_lock);

  if (fd < 3 || fd > 128)
  {
    lock_release(&filesys_lock);
    exit(-1, f);
  }
  struct file* file = thread_current() -> t_fd[fd];
  if (file == NULL)
  {
    lock_release(&filesys_lock);
    exit(-1, f);
  }
  thread_current() -> t_fd[fd] = NULL;
  file_close(file);

  lock_release(&filesys_lock);
}

int fibonacci (int num)
{
  int now , before, temp;
  if (num <= 1) return num;

  before = 0; now = 1;
  for (int i = 1; i < num; i++)
  {
    temp = now;
    now += before;
    before = temp;
  }

  return now;
}

int max_of_four_int (int i1, int i2, int i3, int i4)
{
  int ret = (i1 > i2) ? i1 : i2;
  ret = (ret > i3) ? ret : i3;
  ret = (ret > i4) ? ret : i4;

  return ret;
}

void check_valid (void *ptr, struct intr_frame *f, int size)
{
    if (!is_user_vaddr (ptr) || !is_user_vaddr (ptr + size - 1)
    || pagedir_get_page (thread_current () -> pagedir, ptr) == NULL
    || pagedir_get_page (thread_current () -> pagedir, ptr + size - 1) == NULL)
    {
      exit (-1, f);
    }
  return;
}

void check_buffer (void *ptr, struct intr_frame *f, int size)
{
  for (int i = 0; i < size; i++) check_valid(ptr, f, 1);
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
    case SYS_CREATE: // 1 : filename, 2 : initial_size
      check_valid(&ptr[1], f, 2 * sizeof(unsigned int));
      check_valid((char*) ptr[1], f, 1);
      if (ptr[1] == NULL) exit(-1, f);
      f -> eax = create((char*) ptr[1], (unsigned) ptr[2]);
      break;
    case SYS_REMOVE: // 1 : filename
      check_valid(&ptr[1], f, sizeof(unsigned int));
      check_valid((char*) ptr[1], f, 1);
      if (ptr[1] == NULL) exit(-1, f);
      f -> eax = remove((char*) ptr[1]);
      break;
    case SYS_OPEN: // 1 : filename
      check_valid(&ptr[1], f, sizeof(unsigned int));
      check_valid((char*) ptr[1], f, 1);
      if (ptr[1] == NULL) exit(-1, f);
      f -> eax = open((char*) ptr[1], f);
      break;
    case SYS_FILESIZE: // 1 : fd
      check_valid(&ptr[1], f, sizeof(unsigned int));
      f -> eax = filesize(ptr[1], f);
      break;
    case SYS_READ: // 1 : int fd, 2 : void *buffer, 3 : unsigned size
      check_valid(&ptr[1], f, 3 * sizeof(unsigned int));
      if ((void*) ptr[2] == NULL) exit(-1, f);
      check_buffer((void*) ptr[2], f, ptr[3]);
      f -> eax = read((int) ptr[1], (void*) ptr[2], (size_t) ptr[3], f);
      break;
    case SYS_WRITE: // 1 : int fd, 2 : void *buffer, 3 : unsigned size
      check_valid(&ptr[1], f, 3 * sizeof(unsigned int));
      if ((void*) ptr[2] == NULL) exit(-1, f);
      check_buffer((void*) ptr[2], f, ptr[3]);
      f -> eax = write((int) ptr[1], (void*) ptr[2], (size_t) ptr[3], f);
      break;
    case SYS_SEEK: // 1 : fd, 2 : position
      check_valid(&ptr[1], f, 2 * sizeof(unsigned int));
      seek(ptr[1], ptr[2], f);
      break;
    case SYS_TELL: // 1 : fd
      check_valid(&ptr[1], f, sizeof(unsigned int));
      f -> eax = tell(ptr[1], f);
      break;
    case SYS_CLOSE: // 1 : fd
      check_valid(&ptr[1], f, sizeof(unsigned int));
      close(ptr[1], f);
      break;
    case FIBONACCI: // 1 arguments
      check_valid(&ptr[1], f, sizeof(unsigned int));
      if ((int) ptr[1] < 0) exit(-1, f);
      f -> eax = fibonacci((int) ptr[1]);
      break;
    case MAX_OF_FOUR_INT: // 4 arguments
      check_valid(&ptr[1], f, 4 * sizeof(unsigned int));
      f -> eax = max_of_four_int((int) ptr[1], (int) ptr[2], (int) ptr[3], (int) ptr[4]);
      break;
  }
}
