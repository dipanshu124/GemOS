#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>


// Per process info for the pipe.
struct pipe_info_per_process {

    // TODO:: Add members as per your need...
    u32 pid;
    u8 open_read_end;
    u8 open_write_end;
};

// Global information for the pipe.
struct pipe_info_global {

    char *pipe_buff;    // Pipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need...
    int read_end;
    int write_end;
    int size;
    int process_count;
};

// Pipe information structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct pipe_info {

    struct pipe_info_per_process pipe_per_proc [MAX_PIPE_PROC];
    struct pipe_info_global pipe_global;

};


// Function to allocate space for the pipe and initialize its members.
struct pipe_info* alloc_pipe_info () {
	
    // Allocate space for pipe structure and pipe buffer.
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign pipe buffer.
    pipe->pipe_global.pipe_buff = buffer;

    /**
     *  TODO:: Initializing pipe fields
     *  
     *  Initialize per process fields for this pipe.
     *  Initialize global fields for this pipe.
     *
     */
    pipe->pipe_global.read_end = 0;
    pipe->pipe_global.write_end = 0;
    pipe->pipe_global.size = 0;
    pipe->pipe_global.process_count = 1;

    for(int index = 0; index < MAX_PIPE_PROC; index++) {
        pipe->pipe_per_proc[index].pid = -1;
        pipe->pipe_per_proc[index].open_read_end = 0;
        pipe->pipe_per_proc[index].open_write_end = 0;
    }
    pipe->pipe_per_proc[0].pid = get_current_ctx()->pid;
    pipe->pipe_per_proc[0].open_read_end = 1;
    pipe->pipe_per_proc[0].open_write_end = 1;

    // Return the pipe.
    return pipe;

}

// Function to free pipe buffer and pipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_pipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->pipe->pipe_global.pipe_buff);
    os_page_free(OS_DS_REG, filep->pipe);

}

// Fork handler for the pipe.
int do_pipe_fork (struct exec_context *child, struct file *filep) {

    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the pipe.
     *  This handler will be called twice since pipe has 2 file objects.
     *  Also consider the limit on no of processes a pipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */

    if(!filep) return -EOTHERS;
    u32 pid = child->pid;
    struct pipe_info *pipe = filep->pipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PIPE_PROC; index++) {
        if(pipe->pipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    u32 ppid = get_current_ctx()->pid;
    int parent_index = -1;
    for(int index = 0; index < MAX_PIPE_PROC; index++) {
        if(pipe->pipe_per_proc[index].pid == ppid) {
            parent_index = index;
            break;
        }
    }
    if(parent_index == -1) return -EOTHERS;
    if(curr_process_index == -1) {
        if(pipe->pipe_global.process_count == MAX_PIPE_PROC) return -EOTHERS;
        for(int index = 0; index < MAX_PIPE_PROC; index++) {
            if(pipe->pipe_per_proc[index].pid == -1) {
                pipe->pipe_per_proc[index].pid = pid;
                pipe->pipe_per_proc[index].open_read_end = pipe->pipe_per_proc[parent_index].open_read_end;
                pipe->pipe_per_proc[index].open_write_end = pipe->pipe_per_proc[parent_index].open_write_end;
                pipe->pipe_global.process_count++;
                break;
            }
        }
    }
    else {
        pipe->pipe_per_proc[curr_process_index].open_read_end = pipe->pipe_per_proc[parent_index].open_read_end;
        pipe->pipe_per_proc[curr_process_index].open_write_end = pipe->pipe_per_proc[parent_index].open_write_end;
    }
    // Return successfully.
    return 0;

}

// Function to close the pipe ends and free the pipe when necessary.
long pipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the pipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the pipe.
     *  Use free_pipe() function to free pipe buffer and pipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *
     */

    int ret_value;
    if(!filep) return -EOTHERS;
    u32 pid = get_current_ctx()->pid;
    struct pipe_info *pipe = filep->pipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PIPE_PROC; index++) {
        if(pipe->pipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    if(curr_process_index == -1) return -EOTHERS;
    if(filep->mode == O_READ)
        pipe->pipe_per_proc[curr_process_index].open_read_end = 0;
    else if(filep->mode == O_WRITE)
        pipe->pipe_per_proc[curr_process_index].open_write_end = 0;
    if(pipe->pipe_per_proc[curr_process_index].open_write_end == 0 &&
       pipe->pipe_per_proc[curr_process_index].open_read_end == 0) {
        pipe->pipe_per_proc[curr_process_index].pid = -1;
        pipe->pipe_global.process_count--;
    }
    if(pipe->pipe_global.process_count == 0) free_pipe(filep);
    // Close the file and return.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Check whether passed buffer is valid memory location for read or write.
int is_valid_mem_range (unsigned long buff, u32 count, int access_bit) {

    /**
     *  TODO:: Implementation for buffer memory range checking
     *
     *  Check whether passed memory range is suitable for read or write.
     *  If access_bit == 1, then it is asking to check read permission.
     *  If access_bit == 2, then it is asking to check write permission.
     *  If range is valid then return 1.
     *  Incase range is not valid or have some permission issue return -EBADMEM.
     *
     */
    struct exec_context *current = get_current_ctx();

    for(int index = 0; index < MAX_MM_SEGS; index++) {
        if(index == MAX_MM_SEGS - 1) {
            if((current->mms[index].start <= buff) &&
               (current->mms[index].end >= buff + count) &&
               (current->mms[index].access_flags & access_bit)) return 1;
            break;
        }
        if((current->mms[index].start <= buff) &&
           (current->mms[index].next_free > buff + count) &&
           (current->mms[index].access_flags & access_bit)) return 1;
    }

    struct vm_area *vm = current->vm_area;
    while(vm) {
        if((vm->vm_start <= buff) && (vm->vm_end >= buff + count) &&
           (vm->access_flags & access_bit)) return 1;
        vm = vm->vm_next;
    }
    int ret_value = -EBADMEM;

    // Return the finding.
    return ret_value;

}

// Function to read given no of bytes from the pipe.
int pipe_read (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of Pipe Read
     *
     *  Read the data from pipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the pipe then just read
     *       that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If read end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */
    int bytes_read = 0;
    if(!filep) return -EOTHERS;
    if(filep->mode != O_READ) return -EACCES;
    if(is_valid_mem_range((unsigned long)buff, count, 2) < 0) return -EOTHERS;
    u32 pid = get_current_ctx()->pid;
    struct pipe_info *pipe = filep->pipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PIPE_PROC; index++) {
        if(pipe->pipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    if(curr_process_index == -1) return -EOTHERS;
    if(pipe->pipe_per_proc[curr_process_index].open_read_end == 0) return -EINVAL;
    while(pipe->pipe_global.size != 0) {
        if(bytes_read == count) break;
        buff[bytes_read] = pipe->pipe_global.pipe_buff[pipe->pipe_global.read_end];
        bytes_read++;
        pipe->pipe_global.size--;
        pipe->pipe_global.read_end++;
        if(pipe->pipe_global.read_end == MAX_PIPE_SIZE) pipe->pipe_global.read_end = 0;
    }

    // Return no of bytes read.
    return bytes_read;

}

// Function to write given no of bytes to the pipe.
int pipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of Pipe Write
     *
     *  Write the data from the provided buffer to the pipe buffer.
     *  If count is greater than available space in the pipe then just write data
     *       that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *       -EACCES: In case access is not valid.
     *       -EINVAL: If write end is already closed.
     *       -EOTHERS: For any other errors.
     *
     */
    int bytes_written = 0;
    if(!filep) return -EOTHERS;
    if(filep->mode != O_WRITE) return -EACCES;
    if(is_valid_mem_range((unsigned long)buff, count, 1) < 0) return -EOTHERS;
    u32 pid = get_current_ctx()->pid;
    struct pipe_info *pipe = filep->pipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PIPE_PROC; index++) {
        if(pipe->pipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    if(curr_process_index == -1) return -EOTHERS;
    if(pipe->pipe_per_proc[curr_process_index].open_write_end == 0) return -EINVAL;
    while(pipe->pipe_global.size != MAX_PIPE_SIZE) {
        if(bytes_written == count) break;
        pipe->pipe_global.pipe_buff[pipe->pipe_global.write_end] = buff[bytes_written];
        bytes_written++;
        pipe->pipe_global.size++;
        pipe->pipe_global.write_end++;
        if(pipe->pipe_global.write_end == MAX_PIPE_SIZE) pipe->pipe_global.write_end = 0;
    }
    
    // Return no of bytes written.
    return bytes_written;

}

// Function to create pipe.
int create_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of Pipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function. 
     *  Create pipe_info object by invoking the alloc_pipe_info() function and
     *       fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *       -ENOMEM: If memory is not enough.
     *       -EOTHERS: Some other errors.
     *
     */
    int fd1 = -1, fd2 = -1;
    for(int curr_fd = 0; curr_fd < MAX_OPEN_FILES; curr_fd++) {
        if(current->files[curr_fd]) continue;
        if(fd1 == -1) fd1 = curr_fd;
        else fd2 = curr_fd;
        if(fd1 != -1 && fd2 != -1) break;
    }
    if(fd1 == -1 || fd2 == -1) return -EOTHERS;
    struct file *file1 = alloc_file(), *file2 = alloc_file();
    current->files[fd1] = file1;
    current->files[fd2] = file2;
    struct pipe_info *pipe = alloc_pipe_info();
    
    if(!file1 || !file2 || !pipe) return -EOTHERS;
    file1->type = PIPE;
    file1->mode = O_READ;
    file1->pipe = pipe;
    file2->type = PIPE;
    file2->mode = O_WRITE;
    file2->pipe = pipe;

    // fops
    file1->fops->read = pipe_read;
    file1->fops->write = pipe_write;
    file1->fops->close = pipe_close;
    file2->fops->read = pipe_read;
    file2->fops->write = pipe_write;
    file2->fops->close = pipe_close;
    fd[0] = fd1;
    fd[1] = fd2;
    // Simple return.
    return 0;
} 
