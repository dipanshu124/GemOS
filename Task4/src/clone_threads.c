#include<clone_threads.h>
#include<entry.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<mmap.h>

/*
  system call handler for clone, create thread like 
  execution contexts. Returns pid of the new context to the caller. 
  The new context starts execution from the 'th_func' and 
  use 'user_stack' for its stack
*/
long do_clone(void *th_func, void *user_stack, void *user_arg) 
{
  


  struct exec_context *new_ctx = get_new_ctx();
  struct exec_context *ctx = get_current_ctx();

  u32 pid = new_ctx->pid;
  
  if(!ctx->ctx_threads){  // This is the first thread
          ctx->ctx_threads = os_alloc(sizeof(struct ctx_thread_info));
          bzero((char *)ctx->ctx_threads, sizeof(struct ctx_thread_info));
          ctx->ctx_threads->pid = ctx->pid;
  }
     
 /* XXX Do not change anything above. Your implementation goes here*/
  
  struct thread *th = find_unused_thread(ctx);
  if(!th) return -1;
  *new_ctx = *ctx;
  new_ctx->pid = pid;
  new_ctx->ppid = ctx->pid;
  new_ctx->regs.entry_rsp = (u64)user_stack;
  new_ctx->regs.entry_rip = (u64)th_func;
  new_ctx->regs.rdi = (u64)user_arg;
  new_ctx->ctx_threads = NULL;
  th->pid = pid;
  th->status = TH_USED;
  th->parent_ctx = ctx; 
  // allocate page for os stack in kernel part of process's VAS
  // The following two lines should be there. The order can be 
  // decided depending on your logic.
   setup_child_context(new_ctx);
   new_ctx->type = EXEC_CTX_USER_TH;    // Make sure the context type is thread
  
   new_ctx->state = WAITING;            // For the time being. Remove it as per your need.

	
  return pid;

}

/*This is the page fault handler for thread private memory area (allocated using 
 * gmalloc from user space). This should fix the fault as per the rules. If the the 
 * access is legal, the fault handler should fix it and return 1. Otherwise it should
 * invoke segfault_exit and return -1*/

int handle_thread_private_fault(struct exec_context *current, u64 addr, int error_code)
{
  
   /* your implementation goes here*/
    struct exec_context *parent = current;
    if(!current->ctx_threads) parent = get_ctx_by_pid(current->ppid);
    struct thread_private_map *priv = NULL;
    for(int i = 0; i < MAX_THREADS; i++) {
        if(parent->ctx_threads->threads[i].status == TH_UNUSED) continue;
        for(int j = 0; j < MAX_PRIVATE_AREAS; j++) {
            if(parent->ctx_threads->threads[i].private_mappings[j].owner == NULL) continue;
            if(parent->ctx_threads->threads[i].private_mappings[j].start_addr <= addr && 
               (parent->ctx_threads->threads[i].private_mappings[j].start_addr +
               parent->ctx_threads->threads[i].private_mappings[j].length) > addr)
                priv = &parent->ctx_threads->threads[i].private_mappings[j];
        }
    }
    if(current->pid == priv->owner->pid || current->pid == parent->pid ||
      (priv->flags & TP_SIBLINGS_RDWR) || ((priv->flags & TP_SIBLINGS_RDONLY) && error_code == 0x4)) {
        asm volatile ("invlpg (%0);" 
                    :: "r"(addr) 
                    : "memory"); 

        u64 phy_addr = current->pgd;
	for(int lev = 0; lev < 3; lev++) {
            u64 *virt = (u64 *)osmap(phy_addr);
            u64 offset = addr >> (39 - lev * 9);
            offset &= 511;
            if((virt[offset] & 1) == 0) {
                u64 new_pfn = os_pfn_alloc(OS_PT_REG);
                bzero((char*)osmap(new_pfn), PAGE_SIZE);
		new_pfn <<= 12;
                virt[offset] = new_pfn;
            }
            virt[offset] |= 7;
            phy_addr = virt[offset] >> 12;
        }
        u64 *virt = (u64 *)osmap(phy_addr);
        u64 offset = addr >> 12;
        offset &= 511;
        if((virt[offset] & 1) == 0) {
            u64 new_pfn = os_pfn_alloc(USER_REG);
            bzero((char*)osmap(new_pfn), PAGE_SIZE);
	    new_pfn <<= 12;
            virt[offset] = new_pfn;
        }
        virt[offset] |= 7;
        if((priv->flags|TP_SIBLINGS_RDONLY) && error_code == 0x4) virt[offset] ^= 2;
        return 1;
    }
    segfault_exit(current->pid, current->regs.entry_rip, addr);
    return -1;
}

/*This is a handler called from scheduler. The 'current' refers to the outgoing context and the 'next' 
 * is the incoming context. Both of them can be either the parent process or one of the threads, but only
 * one of them can be the process (as we are having a system with a single user process). This handler
 * should apply the mapping rules passed in the gmalloc calls. */

int handle_private_ctxswitch(struct exec_context *current, struct exec_context *next)
{
  
   /* your implementation goes here*/
    struct exec_context *parent = next;
    if(!parent->ctx_threads) parent = get_ctx_by_pid(next->ppid);
    for(int i = 0; i < MAX_THREADS; i++) {
        if(parent->ctx_threads->threads[i].status == TH_UNUSED) continue;
        for(int j = 0; j < MAX_PRIVATE_AREAS; j++) {
            if(parent->ctx_threads->threads[i].private_mappings[j].owner == NULL) continue;
            u64 addr = parent->ctx_threads->threads[i].private_mappings[j].start_addr;
            u64 permsn;
            if(next->pid == parent->pid || next->pid == parent->ctx_threads->threads[i].private_mappings[j].owner->pid ||
               (parent->ctx_threads->threads[i].private_mappings[j].flags & TP_SIBLINGS_RDWR)) permsn = 7;
            else if(parent->ctx_threads->threads[i].private_mappings[j].flags & TP_SIBLINGS_RDONLY) permsn = 5;
            else permsn = 1;
            while(addr < parent->ctx_threads->threads[i].private_mappings[j].length + parent->ctx_threads->threads[i].private_mappings[j].start_addr) {
                asm volatile ("invlpg (%0);" 
                    :: "r"(addr) 
                    : "memory"); 

                u64 phy_addr = current->pgd;
                int valid = 1;
        	for(int lev = 0; lev < 3; lev++) {
                    u64 *virt = (u64 *)osmap(phy_addr);
                    u64 offset = addr >> (39 - lev * 9);
                    offset &= 511;
                    if((virt[offset] & 1) == 0) {
                        valid = 0;
                        break;
                    }
                    virt[offset] |= 7;
                    phy_addr = virt[offset] >> 12;
                }
                if(!valid) {
                    addr += 4096;
                    continue;
                }
                u64 *virt = (u64 *)osmap(phy_addr);
                u64 offset = addr >> 12;
                offset &= 511;
                if((virt[offset] & 1) != 0) {
                    virt[offset] >>= 3;
                    virt[offset] <<= 3;
                    virt[offset] |= permsn;
                }
                addr += 4096;
            }
        }
    }
    return 0;	

}
