/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
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
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler::Scheduler() {
    qsize = 0;
    Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    if( qsize == 0 ) {
        Console::puts("Queue is empty. No threads available. \n");
    }
    else {
        // Remove thread from queue for CPU time
        Thread * new_thread = ready_queue.dequeue();
        
        // Decrement queue size
        qsize = qsize - 1;
        
        // Re-enable interrupts
        if( !Machine::interrupts_enabled() ) {
            Machine::enable_interrupts();
        }
        
        // Context-switch and give CPU time to new thread
        Thread::dispatch_to(new_thread);
    }
}

void Scheduler::resume(Thread * _thread) {
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    // Add thread to ready queue
    ready_queue.enqueue(_thread);
    
    // Increment queue size
    qsize = qsize + 1;
    
    // Re-enable interrupts
    if( !Machine::interrupts_enabled() ) {
        Machine::enable_interrupts();
    }
}

void Scheduler::add(Thread * _thread) {
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    // Add thread to ready queue
    ready_queue.enqueue(_thread);
    
    // Increment queue size
    qsize = qsize + 1;
    
    // Re-enable interrupts
    if( !Machine::interrupts_enabled() ) {
        Machine::enable_interrupts();
    }
}

void Scheduler::terminate(Thread * _thread) {
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    int index = 0;
    
    // Iterate and dequeue each thread
    // Check if thread ID matches with thread to be terminated
    // If thread ID does not match, add thread back to ready queue
    for( index = 0; index < qsize; index++ ) {
        Thread *CurrentThread = ready_queue.dequeue();
        
        if( CurrentThread->ThreadId() != _thread->ThreadId() ) {
            ready_queue.enqueue(CurrentThread);
        }
        else {
            qsize = qsize - 1;
        }
    }
    
    // Re-enable interrupts
    if( !Machine::interrupts_enabled() ) {
        Machine::enable_interrupts();
    }
}

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   R R S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

RRScheduler::RRScheduler() {
    rr_qsize = 0;
    ticks = 0;
    hz = 5; // Frequency of update of ticks = 50 ms
    
    // Install an interrupt handler for interrupt code 0
    InterruptHandler::register_handler(0, this);  
    
    // Set interrupt frequency for timer
    set_frequency(hz);
}

void RRScheduler::set_frequency(int _hz) {
    hz = _hz;
    int divisor = 1193180 / _hz;            // The input clock runs at 1.19MHz
    Machine::outportb(0x43, 0x34);          // Set command byte to be 0x36
    Machine::outportb(0x40, divisor & 0xFF); // Set low byte of divisor
    Machine::outportb(0x40, divisor >> 8);   // Set high byte of divisor
}

void RRScheduler::yield() {
    // Send an EOI message to the master interrupt controller
    Machine::outportb(0x20, 0x20);
    
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    if( rr_qsize == 0 ) {
        // Console::puts("Queue is empty. No threads available. \n");
    }
    else {
        // Remove thread from RR queue for CPU time
        Thread * new_thread = rr_ready_queue.dequeue();
        
        // Reset tick count
        ticks = 0;
        
        // Decrement RR queue size
        rr_qsize = rr_qsize - 1;
        
        // Re-enable interrupts
        if( !Machine::interrupts_enabled() ) {
            Machine::enable_interrupts();
        }
        
        // Context-switch and give CPU time to new thread
        Thread::dispatch_to(new_thread);
    }
}

void RRScheduler::resume(Thread * _thread) {
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    // Add thread to ready queue
    rr_ready_queue.enqueue(_thread);
    
    // Increment RR queue size
    rr_qsize = rr_qsize + 1;
    
    // Re-enable interrupts
    if( !Machine::interrupts_enabled() ) {
        Machine::enable_interrupts();
    }
}

void RRScheduler::add(Thread * _thread) {
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    // Add thread to ready queue
    rr_ready_queue.enqueue(_thread);
    
    // Increment RR queue size
    rr_qsize = rr_qsize + 1;
    
    // Re-enable interrupts
    if( !Machine::interrupts_enabled() ) {
        Machine::enable_interrupts();
    }
}

void RRScheduler::terminate(Thread * _thread) {
    // Disable interrupts when performing any operations on ready queue
    if( Machine::interrupts_enabled() ) {
        Machine::disable_interrupts();
    }
    
    int index = 0;
    
    // Iterate and dequeue each thread
    // Check if thread ID matches with thread to be terminated
    // If thread ID does not match, add thread back to ready queue
    for( index = 0; index < rr_qsize; index++ ) {
        Thread * FirstThread = rr_ready_queue.dequeue();
        
        if( FirstThread->ThreadId() != _thread->ThreadId() ) {
            rr_ready_queue.enqueue(FirstThread);
        }
        else {
            rr_qsize = rr_qsize - 1;
        }
    }
    
    // Re-enable interrupts
    if( !Machine::interrupts_enabled() ) {
        Machine::enable_interrupts();
    }
}

void RRScheduler::handle_interrupt(REGS * _regs) {
    // Increment our ticks count
    ticks = ticks + 1;
    
    // Time quanta is completed
    // Preempt current thread and run next thread
    if (ticks >= hz ) {
        // Reset tick count
        ticks = 0;
        Console::puts("Time Quanta (50 ms) has passed \n");
        
        resume(Thread::CurrentThread()); 
        yield();
    }
}
