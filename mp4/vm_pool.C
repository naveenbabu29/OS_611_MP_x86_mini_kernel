/*
 File: vm_pool.C
 
 Author:
 Date  : 2024/09/20
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "machine.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table)
{
	base_address = _base_address;
	size = _size;
	frame_pool = _frame_pool;
	page_table = _page_table;
	vm_pool_next = nullptr;
	n_vm_regions = 0;			// Number of virtual memory regions
	
	// Register the virtual memory pool
	page_table->register_pool(this);
	
	// First entry stores base address and page size
	allocated_vm_info * region = (allocated_vm_info*)base_address;
	region[0].base_address = base_address;
	region[0].length = PageTable::PAGE_SIZE;
	vm_regions = region;
	
	// Increment number of regions
	n_vm_regions = n_vm_regions + 1;
	
	// Calculate available virtual memory
	available_mem = available_mem - PageTable::PAGE_SIZE;
	
    Console::puts("Constructed VMPool object.\n");
}


unsigned long VMPool::allocate(unsigned long _size)
{
	unsigned long pages_count = 0;
	
	// If allocation request size is greater than available virtual memory
	if( _size > available_mem )
	{
		Console::puts("VMPool::allocate - Not enough virtual memory space available.\n");
		assert(false);
	}
	
	// Calculate number of pages to be allocated
	pages_count = ( _size / PageTable::PAGE_SIZE ) + ( (_size % PageTable::PAGE_SIZE) > 0 ? 1 : 0 );
	
	// Store details of new virtual memory region
	vm_regions[n_vm_regions].base_address = vm_regions[n_vm_regions-1].base_address +  vm_regions[n_vm_regions-1].length;
	vm_regions[n_vm_regions].length = pages_count * PageTable::PAGE_SIZE;
	
	// Calculate available memory
	available_mem = available_mem - pages_count * PageTable::PAGE_SIZE;
	
	// Increment number of virtual memory regions
	n_vm_regions = n_vm_regions + 1;
	
    Console::puts("Allocated region of memory.\n");
	
	// Return the allocated base_address
	return vm_regions[n_vm_regions-1].base_address; 
}


void VMPool::release(unsigned long _start_address)
{
	int index = 0;
	int region_no = -1;
	unsigned long page_count = 0;
	
	// Iterate and find region the start address belongs to
	for(index = 1; index < n_vm_regions; index++ )
	{
		if( vm_regions[index].base_address  == _start_address )
		{
			region_no = index;
		}
	}
	
	// Calculate number of pages to free
	page_count = vm_regions[region_no].length / PageTable::PAGE_SIZE;
	while( page_count > 0)
	{
		// Free the page
		page_table->free_page(_start_address);
		
		// Increment start address by page size
		_start_address = _start_address + PageTable::PAGE_SIZE;
		
		page_count = page_count - 1;
	}
	
	// Delete virtual memory region information
	for( index = region_no; index < n_vm_regions; index++ )
	{
		vm_regions[index] = vm_regions[index+1];
	}
	
	// Calculate available memory
	available_mem = available_mem + vm_regions[region_no].length;
	
	// Decrement number of regions
	n_vm_regions = n_vm_regions - 1;
    
	Console::puts("Released region of memory.\n");
}


bool VMPool::is_legitimate(unsigned long _address)
{
	Console::puts("Checked whether address is part of an allocated region.\n");
	
	if( (_address < base_address) || (_address > (base_address + size)) )
	{
		return false;
	}
	
	return true;
}