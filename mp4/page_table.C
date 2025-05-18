#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

#define VALID_BIT 0b01 //bit 0 -> 1=valid, 0=invalid
#define WRITE_BIT 0b10 //bit 1 -> 1=read/write, 0=read-only
#define USER_BIT 0b100 //bit 2 -> 1=user, 0=kernel
#define SET_PAGING_BIT 0x80000000

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;
VMPool * PageTable::vm_pool_head = nullptr;

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
    
    /* Initializing the page_directory and 
	 * Making the last entry to point back to the PDE (ie. Starting addr of PDE)*/
    page_directory = (unsigned long *)(kernel_mem_pool->get_frames(1) * PAGE_SIZE);
	page_directory[num_shared_frames - 1] = ((unsigned long) page_directory | WRITE_BIT | VALID_BIT);

    /* Initializing the page table */
    unsigned long *page_table = (unsigned long *)(process_mem_pool->get_frames(1) * PAGE_SIZE);
    
	/* Mapping page_table to the page_directory and setting the present bit */
    page_directory[0] = ((unsigned long)page_table | WRITE_BIT | VALID_BIT);
	
	/* Setting the other PDEs as invalid */
    for (idx = 1; idx < (num_shared_frames - 1); idx++) {
        page_directory[idx] = (page_directory[idx] | WRITE_BIT);
    }
	
    /* Mapping the first 4 MB of memory for page table - All pages marked as valid */
    for (idx = 0; idx < num_shared_frames; idx++) {
        page_table[idx] = (frame_addr | WRITE_BIT | VALID_BIT | USER_BIT);
        frame_addr = frame_addr + PAGE_SIZE;
    }
	
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
	unsigned long error_code = _r->err_code;
	
	// If page not present fault occurs
	if( (error_code & 1) == 0 )
	{
		// Get the page fault address from CR2 register
		unsigned long fault_address = read_cr2();

		// Get the page directory address from CR3 register
		unsigned long * page_dir = (unsigned long *)read_cr3();

		// Extract page directory index - first 10 bits
		unsigned long page_dir_index = (fault_address >> 22);

		// Extract page table index using mask - next 10 bits 
		// 0x3FF = 001111111111 - retain only last 10 bits
		unsigned long page_table_index = ( (fault_address & (0x3FF << 12) ) >> 12 );
		
		unsigned long *new_page_table = nullptr; 
		unsigned long *new_pde = nullptr;
		
		// Check if logical address is valid and legitimate
		unsigned int present_flag = 0;
		
		// Iterate through VM pool regions
		VMPool * temp = PageTable::vm_pool_head;
		
		for( ; temp != nullptr; temp = temp->vm_pool_next )
		{
			if( temp->is_legitimate(fault_address) == true )
			{
				present_flag = 1;
				break;
			}
		}
		
		if( (temp != nullptr) && (present_flag == 0) )
		{
		  Console::puts("Not a legitimate address.\n");
		  assert(false);	  	
		}
		
		// Check where page fault occured
		if ( (page_dir[page_dir_index] & 1 ) == 0 )
		{
			// Page fault occured in page directory - PDE is invalid
			
			int index = 0;
			
			new_page_table = (unsigned long *)(process_mem_pool->get_frames(1) * PAGE_SIZE);
			
			// PDE Address = 1023 | 1023 | Offset
			unsigned long * new_pde = (unsigned long *)( 0xFFFFF << 12 );               
			new_pde[page_dir_index] = ( (unsigned long)(new_page_table)| VALID_BIT | WRITE_BIT );
			
			// Set flags for each page - PTEs marked invalid
			for( index = 0; index < 1024; index++ )
			{
				// Set user level flag bit
				new_page_table[index] = USER_BIT;
			}
			
			// To avoid raising another page fault, handle invalid PTE case as well
			new_pde = (unsigned long *) (process_mem_pool->get_frames(1) * PAGE_SIZE);
			
			// PTE Address = 1023 | PTE | Offset
			unsigned long * page_entry = (unsigned long *)( (0x3FF << 22) | (page_dir_index << 12) );
			
			// Mark PTE valid
			page_entry[page_table_index] = ( (unsigned long)(new_pde) | VALID_BIT | WRITE_BIT );
		}

		else
		{
			// Page fault occured in page table page - PDE is present, but PTE is invalid
			new_pde = (unsigned long *) (process_mem_pool->get_frames(1) * PAGE_SIZE);
			
			// PTE Address = 1023 | PTE | Offset
			unsigned long * page_entry = (unsigned long *)( (0x3FF << 22)| (page_dir_index << 12) );
			
			page_entry[page_table_index] = ( (unsigned long)(new_pde) | VALID_BIT | WRITE_BIT );
		}
	}

	Console::puts("handled page fault\n");
}

void PageTable::register_pool(VMPool * _vm_pool)
{
	// Register the initial virtual memory pool
	if( PageTable::vm_pool_head == nullptr )
	{
		PageTable::vm_pool_head = _vm_pool;
	}
	
	// Register subsequent virtual memory pools
	else
	{
		VMPool * temp = PageTable::vm_pool_head;
		for( ; temp->vm_pool_next != nullptr; temp = temp->vm_pool_next );
		
		// Add pool to end of linked list
		temp->vm_pool_next = _vm_pool;
	}
	
    Console::puts("registered VM pool\n");
}

void PageTable::free_page(unsigned long _page_no) 
{
	// Extract page directory index - first 10 bits
	unsigned long page_dir_index = ( _page_no & 0xFFC00000) >> 22;
	
	// Extract page table index using mask - next 10 bits
	unsigned long page_table_index = (_page_no & 0x003FF000 ) >> 12;
	
	// PTE Address = 1023 | PTE | Offset
	unsigned long * page_table = (unsigned long *) ( (0x000003FF << 22) | (page_dir_index << 12) );
	
	// Obtain frame number to release
	unsigned long frame_no = ( (page_table[page_table_index] & 0xFFFFF000) / PAGE_SIZE );
	
	// Release frame from process pool
	process_mem_pool->release_frames(frame_no);
	
	// Mark PTE as invalid
	page_table[page_table_index] = page_table[page_table_index] | WRITE_BIT;
	
	// Flush TLB by reloading page table
	load();
	
	Console::puts("freed page\n");
}
	
