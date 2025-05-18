#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;

#define VALID_BIT 1 //bit 0 -> 1=valid, 0=absent
#define WRITE_BIT 2 //bit 1 -> 1=read/write, 0=read-only
#define USER_BIT 4 //bit 2 -> 1=user, 0=kernel
#define SET_PAGING_BIT 0x80000000
#define PTE_INDX_MASK 0x3ff
#define PT_ADDR_MASK 0xfffff000

void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
    PageTable::kernel_mem_pool  = _kernel_mem_pool;
    PageTable::process_mem_pool = _process_mem_pool;
    PageTable::shared_size      = _shared_size;
    Console::puts("Paging System is Initialized\n");
}

PageTable::PageTable()
{
    int idx;
    unsigned long frame_addr = 0;
    /* Paging is disabled initially */
    paging_enabled = 0;
    
    /* Finding the number of frames required for shared space */
    unsigned long num_shared_frames = (PageTable::shared_size) / PAGE_SIZE ;
    
    /* Initializing the page_directory */
    page_directory = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);
    
    /* Initializing the page table */
    unsigned long *page_table = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);
    
    /* Mapping the page_table_pages (PTEs) with logical addr and setting the present bit */
    for (idx = 0; idx < num_shared_frames; idx++) {
        page_table[idx] = frame_addr | WRITE_BIT | VALID_BIT;
        frame_addr = frame_addr + PAGE_SIZE;
    }
	
	for (idx; idx < 1024; idx++) {
        page_table[idx] = frame_addr | WRITE_BIT;
        frame_addr = frame_addr + PAGE_SIZE;
    }
	
	/* Mapping page_table to the page_directory and setting the present bit */
    page_directory[0] = (unsigned long)page_table | WRITE_BIT | VALID_BIT;
    
    /* Setting the other PDEs as invalid */
    for (idx = 1; idx < num_shared_frames; idx++) {
        page_directory[idx] = 0 | WRITE_BIT;
    }
	
	/*Making the last entry to point back to the PDE (ie. Starting addr of PDE) */
	page_directory[num_shared_frames-1] = (unsigned long) page_directory | WRITE_BIT | VALID_BIT;

    Console::puts("Constructed Page Table object\n");
}

void PageTable::load()
{
    current_page_table = this;
    /* Store the Page directory address in the (PTBR)CR3 register */
    write_cr3((unsigned long)current_page_table->page_directory);
    Console::puts("Loaded page table\n");
}

void PageTable::enable_paging() 
 {
    /* Setting the paging bit in the cr0 register and setting the paging_enabled variable */
    write_cr0(read_cr0() | SET_PAGING_BIT);
    paging_enabled = 1;
    Console::puts("Enabled paging\n");
}  

void PageTable::handle_fault(REGS * _r)
{
    unsigned int  idx             = 0;
    unsigned int  err_code        = _r->err_code;
    unsigned long fault_address   = read_cr2();
    unsigned long *page_directory = (unsigned long *)read_cr3();
    unsigned long pde_idx         = (fault_address >> 22);
    unsigned long pte_idx         = ((fault_address >> 12) & PTE_INDX_MASK);
    unsigned long *curr_pte_addr  = nullptr;
    
	/* There is an invalid entry in PDE or PTE */
    if ((err_code & 1) == 0) {
        if ((page_directory[pde_idx] & 1) == 0) {
            page_directory[pde_idx] = (unsigned long)(kernel_mem_pool->get_frames(1) * PAGE_SIZE | WRITE_BIT | VALID_BIT);
            curr_pte_addr = (unsigned long *)(page_directory[pde_idx] & PT_ADDR_MASK);
            
            for (idx = 0; idx < 1024; idx++) {
                curr_pte_addr[idx] = 0 | USER_BIT;
            }
			curr_pte_addr[pte_idx] = (process_mem_pool->get_frames(1) * PAGE_SIZE) | WRITE_BIT | VALID_BIT | USER_BIT;
        } else {
            curr_pte_addr = (unsigned long *)(page_directory[pde_idx] & PT_ADDR_MASK);
            curr_pte_addr[pte_idx] = (process_mem_pool->get_frames(1) * PAGE_SIZE) | WRITE_BIT | VALID_BIT | USER_BIT;
        }
	} else {
		Console::puts("No invalid entry in PDE or PTE(Might be some error)\n");
	}
    Console::puts("handled page fault\n");
}
