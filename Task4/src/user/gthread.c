#include <gthread.h>
#include <ulib.h>

static struct process_thread_info tinfo __attribute__((section(".user_data"))) = {};
/*XXX 
      Do not modifiy anything above this line. The global variable tinfo maintains user
      level accounting of threads. Refer gthread.h for definition of related structs.
 */


/* Returns 0 on success and -1 on failure */
/* Here you can use helper system call "make_thread_ready" for your implementation */
static int end_handler() {
    u64 ret;
    asm volatile(
    "mov %%rax, %0;"
    : "=r"(ret)
    :
    : "memory");
    gthread_exit((void*)ret);
    return -1;
}

int gthread_create(int *tid, void *(*fc)(void *), void *arg) {
        
	/* You need to fill in your implementation here*/
	if(tinfo.num_threads == MAX_THREADS) return -1;
	struct thread_info *th = NULL;
	for(int i = 0; i < MAX_THREADS; i++) {
		if(tinfo.threads[i].status == TH_STATUS_UNUSED) {
			th = &tinfo.threads[i];
                        *tid = i;
			break;
		}
	}
	if(!th) return -1;
	void *stackp = NULL;
	stackp = mmap(NULL, TH_STACK_SIZE, PROT_READ|PROT_WRITE, 0);
	if(!stackp || stackp == MAP_ERR) return -1;
        *(u64 *) ((u64)stackp + TH_STACK_SIZE - 8) = (u64)(&end_handler);
	int thpid = clone(fc, ((u64)stackp) + TH_STACK_SIZE - 8, arg);
	if(thpid <= 0) return -1;
	tinfo.num_threads++;
	th->pid = thpid;
	th->tid = *tid;
	th->status = TH_STATUS_ALIVE;
	th->stack_addr = stackp;
        th->ret_addr = NULL;
	for(int j = 0; j < MAX_GALLOC_AREAS; j++) {
		th->priv_areas[j].owner = NULL;
		th->priv_areas[j].start = 0;
		th->priv_areas[j].length = 0;
		th->priv_areas[j].flags = 0;
	}
	make_thread_ready(thpid);
	return 0;
}

int gthread_exit(void *retval) {

	/* You need to fill in your implementation here*/
	int pid = getpid();
	for(int i = 0; i < MAX_THREADS; i++) {
		if(tinfo.threads[i].status != TH_STATUS_ALIVE) continue;
		if(tinfo.threads[i].pid == pid) {
			tinfo.threads[i].ret_addr = retval;
			tinfo.threads[i].status = TH_STATUS_USED;
		}
	}
	//call exit
	exit(0);
}

void* gthread_join(int tid) {
        
     /* Here you can use helper system call "wait_for_thread" for your implementation */
       
     /* You need to fill in your implementation here*/
	for(int i = 0; i < MAX_THREADS; i++) {
		if(tinfo.threads[i].status == TH_STATUS_UNUSED) continue;
		if(tinfo.threads[i].tid == tid) {
			while(tinfo.threads[i].status == TH_STATUS_ALIVE) {
                            int ret = wait_for_thread(tinfo.threads[i].pid);
                            if(ret < 0) break;
                        }
			tinfo.threads[i].status = TH_STATUS_UNUSED;
			munmap(tinfo.threads[i].stack_addr, TH_STACK_SIZE);
			tinfo.num_threads--;
			return tinfo.threads[i].ret_addr;
		}
	}

	return NULL;
}


/*Only threads will invoke this. No need to check if its a process
 * The allocation size is always < GALLOC_MAX and flags can be one
 * of the alloc flags (GALLOC_*) defined in gthread.h. Need to 
 * invoke mmap using the proper protection flags (for prot param to mmap)
 * and MAP_TH_PRIVATE as the flag param of mmap. The mmap call will be 
 * handled by handle_thread_private_map in the OS.
 * */

void* gmalloc(u32 size, u8 alloc_flag)
{
   
	/* You need to fill in your implementation here*/
	int pid = getpid();
	struct galloc_area *ga = NULL;
	for(int i = 0; i < MAX_THREADS; i++) {
		if(tinfo.threads[i].status != TH_STATUS_ALIVE) continue;
		if(tinfo.threads[i].pid == pid) {
			for(int j = 0; j < MAX_GALLOC_AREAS; j++) {
				if(tinfo.threads[i].priv_areas[j].owner) continue;
				ga = &tinfo.threads[i].priv_areas[j];
				ga->owner = &tinfo.threads[i];
				break;
			}
			break;
		}
	}
	if(!ga) return NULL;
	void *ptr = NULL;
	if(alloc_flag == GALLOC_OWNONLY) ptr = mmap(NULL, size, PROT_READ|PROT_WRITE|TP_SIBLINGS_NOACCESS, MAP_TH_PRIVATE);
	else if(alloc_flag == GALLOC_OTRDONLY) ptr = mmap(NULL, size, PROT_READ|PROT_WRITE|TP_SIBLINGS_RDONLY, MAP_TH_PRIVATE);
	else if(alloc_flag == GALLOC_OTRDWR) ptr = mmap(NULL, size, PROT_READ|PROT_WRITE|TP_SIBLINGS_RDWR, MAP_TH_PRIVATE);
	if(!ptr) {
		ga->owner = NULL;
		return NULL;
	}
	ga->start = (u64)ptr;
	ga->length = size;
	ga->flags = alloc_flag;
	return ptr;
}
/*
   Only threads will invoke this. No need to check if the caller is a process.
*/
int gfree(void *ptr)
{
   
    /* You need to fill in your implementation here*/
	int pid = getpid();
	for(int i = 0; i < MAX_THREADS; i++) {
		if(tinfo.threads[i].status != TH_STATUS_ALIVE) continue;
		if(tinfo.threads[i].pid == pid) {
			for(int j = 0; j < MAX_GALLOC_AREAS; j++) {
				if(!tinfo.threads[i].priv_areas[j].owner) continue;
				if(tinfo.threads[i].priv_areas[j].start == (u64)ptr) {
					int ret = munmap(ptr, tinfo.threads[i].priv_areas[j].length);
					if(ret < 0) return -1;
					tinfo.threads[i].priv_areas[j].owner = NULL;
					return 0;
				}
			}
			break;
		}
	}
     	return -1;
}
