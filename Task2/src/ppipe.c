#include<ppipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>


// Per process information for the ppipe.
struct ppipe_info_per_process {

    // TODO:: Add members as per your need...

    u32 pid;
    u8 open_read_end;
    u8 open_write_end;
    int read_end;
    int size;
};

// Global information for the ppipe.
struct ppipe_info_global {

    char *ppipe_buff;       // Persistent ppipe buffer: DO NOT MODIFY THIS.

    // TODO:: Add members as per your need...

    int read_end;
    int write_end;
    int size;
    int process_count;
};

// Persistent ppipe structure.
// NOTE: DO NOT MODIFY THIS STRUCTURE.
struct ppipe_info {

    struct ppipe_info_per_process ppipe_per_proc [MAX_PPIPE_PROC];
    struct ppipe_info_global ppipe_global;

};


// Function to allocate space for the ppipe and initialize its members.
struct ppipe_info* alloc_ppipe_info() {

    // Allocate space for ppipe structure and ppipe buffer.
    struct ppipe_info *ppipe = (struct ppipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);

    // Assign ppipe buffer.
    ppipe->ppipe_global.ppipe_buff = buffer;

    /**
     *  TODO:: Initializing ppipe fields
     *
     *  Initialize per process fields for this ppipe.
     *  Initialize global fields for this ppipe.
     *
     */ 

    ppipe->ppipe_global.read_end = 0;
    ppipe->ppipe_global.write_end = 0;
    ppipe->ppipe_global.size = 0;
    ppipe->ppipe_global.process_count = 1;

    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        ppipe->ppipe_per_proc[index].pid = -1;
        ppipe->ppipe_per_proc[index].open_read_end = 0;
        ppipe->ppipe_per_proc[index].open_write_end = 0;
        ppipe->ppipe_per_proc[index].read_end = 0;
        ppipe->ppipe_per_proc[index].size = 0;
    }
    ppipe->ppipe_per_proc[0].pid = get_current_ctx()->pid;
    ppipe->ppipe_per_proc[0].open_read_end = 1;
    ppipe->ppipe_per_proc[0].open_write_end = 1;
    // Return the ppipe.
    return ppipe;

}

// Function to free ppipe buffer and ppipe info object.
// NOTE: DO NOT MODIFY THIS FUNCTION.
void free_ppipe (struct file *filep) {

    os_page_free(OS_DS_REG, filep->ppipe->ppipe_global.ppipe_buff);
    os_page_free(OS_DS_REG, filep->ppipe);

} 

// Fork handler for ppipe.
int do_ppipe_fork (struct exec_context *child, struct file *filep) {
    
    /**
     *  TODO:: Implementation for fork handler
     *
     *  You may need to update some per process or global info for the ppipe.
     *  This handler will be called twice since ppipe has 2 file objects.
     *  Also consider the limit on no of processes a ppipe can have.
     *  Return 0 on success.
     *  Incase of any error return -EOTHERS.
     *
     */

    if(!filep) return -EOTHERS;
    u32 pid = child->pid;
    struct ppipe_info *ppipe = filep->ppipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        if(ppipe->ppipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    u32 ppid = get_current_ctx()->pid;
    int parent_index = -1;
    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        if(ppipe->ppipe_per_proc[index].pid == ppid) {
            parent_index = index;
            break;
        }
    }
    if(parent_index == -1) return -EOTHERS;
    if(curr_process_index == -1) {
        if(ppipe->ppipe_global.process_count == MAX_PPIPE_PROC) return -EOTHERS;
        for(int index = 0; index < MAX_PPIPE_PROC; index++) {
            if(ppipe->ppipe_per_proc[index].pid == -1) {
                ppipe->ppipe_per_proc[index].pid = pid;
                ppipe->ppipe_per_proc[index].open_read_end = ppipe->ppipe_per_proc[parent_index].open_read_end;
                ppipe->ppipe_per_proc[index].open_write_end = ppipe->ppipe_per_proc[parent_index].open_write_end;
                ppipe->ppipe_per_proc[index].read_end = ppipe->ppipe_per_proc[parent_index].read_end;
                ppipe->ppipe_per_proc[index].size = ppipe->ppipe_per_proc[parent_index].size;
                ppipe->ppipe_global.process_count++;
                break;
            }
        }
    }
    else {
        ppipe->ppipe_per_proc[curr_process_index].open_read_end = ppipe->ppipe_per_proc[parent_index].open_read_end;
        ppipe->ppipe_per_proc[curr_process_index].open_write_end = ppipe->ppipe_per_proc[parent_index].open_write_end;
        ppipe->ppipe_per_proc[curr_process_index].read_end = ppipe->ppipe_per_proc[parent_index].read_end;
        ppipe->ppipe_per_proc[curr_process_index].size = ppipe->ppipe_per_proc[parent_index].size;
    }
    // Return successfully.
    return 0;

}


// Function to close the ppipe ends and free the ppipe when necessary.
long ppipe_close (struct file *filep) {

    /**
     *  TODO:: Implementation of Pipe Close
     *
     *  Close the read or write end of the ppipe depending upon the file
     *      object's mode.
     *  You may need to update some per process or global info for the ppipe.
     *  Use free_ppipe() function to free ppipe buffer and ppipe object,
     *      whenever applicable.
     *  After successful close, it return 0.
     *  Incase of any error return -EOTHERS.
     *                                                                          
     */


    int ret_value;
    if(!filep) return -EOTHERS;
    u32 pid = get_current_ctx()->pid;
    struct ppipe_info *ppipe = filep->ppipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        if(ppipe->ppipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    if(curr_process_index == -1) return -EOTHERS;
    if(filep->mode == O_READ)
        ppipe->ppipe_per_proc[curr_process_index].open_read_end = 0;
    else if(filep->mode == O_WRITE)
        ppipe->ppipe_per_proc[curr_process_index].open_write_end = 0;
    if(ppipe->ppipe_per_proc[curr_process_index].open_write_end == 0 &&
        ppipe->ppipe_per_proc[curr_process_index].open_read_end == 0) {
        ppipe->ppipe_per_proc[curr_process_index].pid = -1;
        ppipe->ppipe_global.process_count--;
    }
    if(ppipe->ppipe_global.process_count == 0) free_ppipe(filep);
    // Close the file.
    ret_value = file_close (filep);         // DO NOT MODIFY THIS LINE.

    // And return.
    return ret_value;

}

// Function to perform flush operation on ppipe.
int do_flush_ppipe (struct file *filep) {

    /**
     *  TODO:: Implementation of Flush system call
     *
     *  Reclaim the region of the persistent ppipe which has been read by 
     *      all the processes.
     *  Return no of reclaimed bytes.
     *  In case of any error return -EOTHERS.
     *
     */
    int reclaimed_bytes = 0;
    if(!filep) return -EOTHERS;
    struct ppipe_info *ppipe = filep->ppipe;
    int max_size = 0;
    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        if(ppipe->ppipe_per_proc[index].pid < 0) continue;
        if(ppipe->ppipe_per_proc[index].open_read_end == 0) continue;
        if(ppipe->ppipe_per_proc[index].size > max_size)
            max_size = ppipe->ppipe_per_proc[index].size;
    }
    reclaimed_bytes = ppipe->ppipe_global.size - max_size;
    ppipe->ppipe_global.size = max_size;
    ppipe->ppipe_global.read_end += reclaimed_bytes;
    if(ppipe->ppipe_global.read_end >= MAX_PPIPE_SIZE)
        ppipe->ppipe_global.read_end -= MAX_PPIPE_SIZE;

    // Return reclaimed bytes.
    return reclaimed_bytes;

}

// Read handler for the ppipe.
int ppipe_read (struct file *filep, char *buff, u32 count) {
    
    /**
     *  TODO:: Implementation of PPipe Read
     *
     *  Read the data from ppipe buffer and write to the provided buffer.
     *  If count is greater than the present data size in the ppipe then just read
     *      that much data.
     *  Validate file object's access right.
     *  On successful read, return no of bytes read.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If read end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */

    int bytes_read = 0;

    if(!filep) return -EOTHERS;
    if(filep->mode != O_READ) return -EACCES;
    u32 pid = get_current_ctx()->pid;
    struct ppipe_info *ppipe = filep->ppipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        if(ppipe->ppipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    if(curr_process_index == -1) return -EOTHERS;
    if(ppipe->ppipe_per_proc[curr_process_index].open_read_end == 0) return -EINVAL;
    while(ppipe->ppipe_per_proc[curr_process_index].size != 0) {
        if(bytes_read == count) break;
        buff[bytes_read] = ppipe->ppipe_global.ppipe_buff[ppipe->ppipe_per_proc[curr_process_index].read_end];
        bytes_read++;
        ppipe->ppipe_per_proc[curr_process_index].size--;
        ppipe->ppipe_per_proc[curr_process_index].read_end++;
        if(ppipe->ppipe_per_proc[curr_process_index].read_end == MAX_PPIPE_SIZE)
            ppipe->ppipe_per_proc[curr_process_index].read_end = 0;
    }
    // Return no of bytes read.
    return bytes_read;
	
}

// Write handler for ppipe.
int ppipe_write (struct file *filep, char *buff, u32 count) {

    /**
     *  TODO:: Implementation of PPipe Write
     *
     *  Write the data from the provided buffer to the ppipe buffer.
     *  If count is greater than available space in the ppipe then just write
     *      data that fits in that space.
     *  Validate file object's access right.
     *  On successful write, return no of written bytes.
     *  Incase of Error return valid error code.
     *      -EACCES: In case access is not valid.
     *      -EINVAL: If write end is already closed.
     *      -EOTHERS: For any other errors.
     *
     */

    int bytes_written = 0;

    if(!filep) return -EOTHERS;
    if(filep->mode != O_WRITE) return -EACCES;
    u32 pid = get_current_ctx()->pid;
    struct ppipe_info *ppipe = filep->ppipe;
    int curr_process_index = -1;
    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        if(ppipe->ppipe_per_proc[index].pid == pid) {
            curr_process_index = index;
            break;
        }
    }
    if(curr_process_index == -1) return -EOTHERS;
    if(ppipe->ppipe_per_proc[curr_process_index].open_write_end == 0) return -EINVAL;
    while(ppipe->ppipe_global.size != MAX_PPIPE_SIZE) {
        if(bytes_written == count) break;
        ppipe->ppipe_global.ppipe_buff[ppipe->ppipe_global.write_end] = buff[bytes_written];
        bytes_written++;
        ppipe->ppipe_global.size++;
        ppipe->ppipe_global.write_end++;
        if(ppipe->ppipe_global.write_end == MAX_PPIPE_SIZE) ppipe->ppipe_global.write_end = 0;
    }
    for(int index = 0; index < MAX_PPIPE_PROC; index++) {
        if(ppipe->ppipe_per_proc[index].pid > 0) ppipe->ppipe_per_proc[index].size += bytes_written;
    }
    // Return no of bytes written.
    return bytes_written;

}

// Function to create persistent ppipe.
int create_persistent_pipe (struct exec_context *current, int *fd) {

    /**
     *  TODO:: Implementation of PPipe Create
     *
     *  Find two free file descriptors.
     *  Create two file objects for both ends by invoking the alloc_file() function.
     *  Create ppipe_info object by invoking the alloc_ppipe_info() function and
     *      fill per process and global info fields.
     *  Fill the fields for those file objects like type, fops, etc.
     *  Fill the valid file descriptor in *fd param.
     *  On success, return 0.
     *  Incase of Error return valid Error code.
     *      -ENOMEM: If memory is not enough.
     *      -EOTHERS: Some other errors.
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
    struct ppipe_info *ppipe = alloc_ppipe_info();
    
    if(!file1 || !file2 || !ppipe) return -EOTHERS;
    file1->type = PPIPE;
    file1->mode = O_READ;
    file1->ppipe = ppipe;
    file2->type = PPIPE;
    file2->mode = O_WRITE;
    file2->ppipe = ppipe;

    // fops
    file1->fops->read = ppipe_read;
    file1->fops->write = ppipe_write;
    file1->fops->close = ppipe_close;
    file2->fops->read = ppipe_read;
    file2->fops->write = ppipe_write;
    file2->fops->close = ppipe_close;
    fd[0] = fd1;
    fd[1] = fd2;
    // Simple return.
    return 0;

}
