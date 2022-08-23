#include <debug.h>
#include <context.h>
#include <entry.h>
#include <lib.h>
#include <memory.h>


/*****************************HELPERS******************************************/

/*
 * allocate the struct which contains information about debugger
 *
 */
u32 childpid;
struct debug_info *alloc_debug_info()
{
	struct debug_info *info = (struct debug_info *) os_alloc(sizeof(struct debug_info));
	if(info)
		bzero((char *)info, sizeof(struct debug_info));
	return info;
}
/*
 * frees a debug_info struct
 */
void free_debug_info(struct debug_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct debug_info));
}



/*
 * allocates a page to store registers structure
 */
struct registers *alloc_regs()
{
	struct registers *info = (struct registers*) os_alloc(sizeof(struct registers));
	if(info)
		bzero((char *)info, sizeof(struct registers));
	return info;
}

/*
 * frees an allocated registers struct
 */
void free_regs(struct registers *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct registers));
}

/*
 * allocate a node for breakpoint list
 * which contains information about breakpoint
 */
struct breakpoint_info *alloc_breakpoint_info()
{
	struct breakpoint_info *info = (struct breakpoint_info *)os_alloc(
		sizeof(struct breakpoint_info));
	if(info)
		bzero((char *)info, sizeof(struct breakpoint_info));
	return info;
}

/*
 * frees a node of breakpoint list
 */
void free_breakpoint_info(struct breakpoint_info *ptr)
{
	if(ptr)
		os_free((void *)ptr, sizeof(struct breakpoint_info));
}

/*
 * Fork handler.
 * The child context doesnt need the debug info
 * Set it to NULL
 * The child must go to sleep( ie move to WAIT state)
 * It will be made ready when the debugger calls wait
 */
void debugger_on_fork(struct exec_context *child_ctx)
{
	// printk("DEBUGGER FORK HANDLER CALLED\n");
	child_ctx->dbg = NULL;
	child_ctx->state = WAITING;
	childpid = child_ctx->pid;
}


/******************************************************************************/


/* This is the int 0x3 handler
 * Hit from the childs context
 */

long int3_handler(struct exec_context *ctx)
{
	
	u64 rip = ctx->regs.entry_rip;
	rip--;
	u32 ppid = ctx->ppid;
	struct exec_context *parent_ctx = get_ctx_by_pid(ppid);
	int backtrace_count = 0;
	if(rip == (u64)parent_ctx->dbg->end_handler) {
		ctx->regs.entry_rsp -= 8;
		parent_ctx->dbg->top--;
		if(parent_ctx->dbg->top < 0) return -1;
		*(u64 *)ctx->regs.entry_rsp = parent_ctx->dbg->ret_addr[parent_ctx->dbg->top];
	}
	else {
		struct breakpoint_info *head = parent_ctx->dbg->head;
		int end_enable = -1;
		while(head != NULL) {
			if(head->addr == rip)  {
				end_enable = head->end_breakpoint_enable;
				break;
			}
			head = head->next;
		}
		if(end_enable == -1) return -1;
		parent_ctx->dbg->backtrace[backtrace_count] = rip;
		backtrace_count++;
		if(end_enable == 1) {
			//ctx->regs.entry_rsp -= 8;
			parent_ctx->dbg->ret_addr[parent_ctx->dbg->top] = *(u64 *)ctx->regs.entry_rsp;
			*(u64 *)ctx->regs.entry_rsp = (u64)parent_ctx->dbg->end_handler;
			parent_ctx->dbg->func_stack[parent_ctx->dbg->top++] = rip;
		}
	}
	parent_ctx->regs.rax = rip;
	ctx->regs.entry_rsp -= 8;
	*(u64 *)ctx->regs.entry_rsp = ctx->regs.rbp;
	u64 rsp = ctx->regs.entry_rsp;
	u64 rbp;
	int top = parent_ctx->dbg->top;
	while(1) {
		rbp = *(u64 *)rsp;
		rsp += 8;
		u64 ret_addr = *(u64 *)rsp;
		if(ret_addr == END_ADDR) break;
		if(ret_addr == (u64)parent_ctx->dbg->end_handler) {
			ret_addr = parent_ctx->dbg->ret_addr[--top];
		}
		parent_ctx->dbg->backtrace[backtrace_count] = ret_addr;
		backtrace_count++;
		rsp = rbp;
	}
	//printk("YOUR COUNT  =  %d\n", backtrace_count);
	parent_ctx->dbg->backtrace_count = backtrace_count;
	ctx->state = WAITING;
	parent_ctx->state = READY;
	schedule(parent_ctx);
	return 0;
}

/*
 * Exit handler.
 * Deallocate the debug_info struct if its a debugger.
 * Wake up the debugger if its a child
 */
void debugger_on_exit(struct exec_context *ctx)
{
	if(ctx->dbg == NULL) {
		u32 ppid = ctx->ppid;
		struct exec_context *parent_ctx = get_ctx_by_pid(ppid);
		parent_ctx->regs.rax = CHILD_EXIT;
		parent_ctx->state = READY;
		return;
	}
	struct breakpoint_info *head = ctx->dbg->head;
	while(head != NULL) {
		struct breakpoint_info *h = head;
		head = head->next;
		free_breakpoint_info(h);
	}
	free_debug_info(ctx->dbg);
	return;
}


/*
 * called from debuggers context
 * initializes debugger state
 */
int do_become_debugger(struct exec_context *ctx, void *addr)
{
        if(addr == NULL) return -1;
	*(char*)addr = INT3_OPCODE;
	ctx->dbg = alloc_debug_info();
        if(ctx->dbg == NULL) return -1;
        ctx->dbg->breakpoint_count = 0;
        ctx->dbg->head = NULL;
        ctx->dbg->end_handler = addr;
        ctx->dbg->number = 0;
	ctx->dbg->top = 0;
	return 0;
}

/*
 * called from debuggers context
 */
int do_set_breakpoint(struct exec_context *ctx, void *addr, int flag)
{
        if(addr == NULL) return -1;
	*(char*)addr = INT3_OPCODE;
	int on_stack = 0;
	for(int i = 0; i < ctx->dbg->top; i++) {
		if(ctx->dbg->func_stack[i] == (u64)addr) {
			on_stack = 1;
			break;	
		}
	}
        if(ctx->dbg->head == NULL) {
		struct breakpoint_info *head = alloc_breakpoint_info();
		if(head == NULL) return -1;
		head->num = ++ctx->dbg->number;
		head->addr = (u64)addr;
		head->next = NULL;
		head->end_breakpoint_enable = flag;
		ctx->dbg->breakpoint_count++;
		ctx->dbg->head = head;
		return 0;
        }
	struct breakpoint_info *head = ctx->dbg->head;
	struct breakpoint_info *tail = head;
	while(head != NULL) {
		tail = head;
		if(head->addr == (u64)addr) {
			if(head->end_breakpoint_enable == flag) return 0;
			if(on_stack && flag == 0) return -1;
			else {
				head->end_breakpoint_enable = flag;
				return 0;
			}
		}
		head = head->next;
	}
	if(ctx->dbg->breakpoint_count == MAX_BREAKPOINTS) return -1;
	head = alloc_breakpoint_info();
	if(head == NULL) return -1;
	head->num = ++ctx->dbg->number;
	head->addr = (u64)addr;
	head->next = NULL;
	head->end_breakpoint_enable = flag;
	tail->next = head;
	ctx->dbg->breakpoint_count++;
	return 0;
}

/*
 * called from debuggers context
 */
int do_remove_breakpoint(struct exec_context *ctx, void *addr)
{
	if(addr == NULL) return -1;
	int on_stack = 0;
	for(int i = 0; i < ctx->dbg->top; i++) {
		if(ctx->dbg->func_stack[i] == (u64)addr) {
			on_stack = 1;
			break;	
		}
	}
	struct breakpoint_info *head = ctx->dbg->head, *prev = NULL;
	while(head != NULL) {
		if(head->addr == (u64)addr) {
			if(on_stack && head->end_breakpoint_enable == 1) return -1;
			ctx->dbg->breakpoint_count--;
			if(prev == NULL) ctx->dbg->head = head->next;	
			else prev->next = head->next;
			free_breakpoint_info(head);
			*(char *)addr = PUSHRBP_OPCODE;
			return 0;
		}
		prev = head;
		head = head->next;
	}
	return -1;
}


/*
 * called from debuggers context
 */

int do_info_breakpoints(struct exec_context *ctx, struct breakpoint *ubp)
{
	struct breakpoint_info *head = ctx->dbg->head;
	for(int i = 0; i < ctx->dbg->breakpoint_count; i++) {
		if(head == NULL) return -1;
		ubp[i].addr = head->addr;
		ubp[i].num = head->num;
		ubp[i].end_breakpoint_enable = head->end_breakpoint_enable;
		head = head->next;	
	}
	if(head != NULL) return -1;
	return ctx->dbg->breakpoint_count;
}


/*
 * called from debuggers context
 */
int do_info_registers(struct exec_context *ctx, struct registers *regs)
{
	struct exec_context *child_ctx = get_ctx_by_pid(childpid);
	regs->entry_rip = child_ctx->regs.entry_rip - 1;
	regs->entry_rsp = child_ctx->regs.entry_rsp + 8;
	regs->rbp = child_ctx->regs.rbp;
	regs->rax = child_ctx->regs.rax;
	regs->rdi = child_ctx->regs.rdi;
	regs->rsi = child_ctx->regs.rsi;
	regs->rbp = child_ctx->regs.rbp;
	regs->rdx = child_ctx->regs.rdx;
	regs->rcx = child_ctx->regs.rcx;
	regs->rbp = child_ctx->regs.rbp;
	regs->r9 = child_ctx->regs.r9;
	regs->r8 = child_ctx->regs.r8;
	return 0;
}

/*
 * Called from debuggers context
 */
int do_backtrace(struct exec_context *ctx, u64 bt_buf)
{
	u64 *bt = (u64 *)bt_buf;
	for(int i = 0; i < ctx->dbg->backtrace_count; i++) bt[i] = ctx->dbg->backtrace[i];
	return ctx->dbg->backtrace_count;
}

/*
 * When the debugger calls wait
 * it must move to WAITING state
 * and its child must move to READY state
 */

s64 do_wait_and_continue(struct exec_context *ctx)
{
	struct exec_context *child_ctx = get_ctx_by_pid(childpid);
	ctx->state = WAITING;
	child_ctx->state = READY;
	schedule(child_ctx);
	return 0;
}






