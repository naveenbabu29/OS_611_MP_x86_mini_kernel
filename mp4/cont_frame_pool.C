/*
 File: ContFramePool.C
 
 Author: Naveen Babu
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_nframes): Traverse the "bitmap" of states and look for a 
 sequence of at least _nframes entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _nframes-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _nframes): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_nframes): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::FrameState ContFramePool::get_state(unsigned long frame_no) {
    unsigned int bitmap_index = frame_no / 4;
    unsigned int position = 2 * (frame_no % 4);
    unsigned char mask_result = (bitmap[bitmap_index] >> (position)) & 0b11;
	FrameState state_output = FrameState::Used;

    if (mask_result == 0b11) {
        state_output = FrameState::HoS;
    } else if (mask_result == 0b00) {
        state_output = FrameState::Free;
    } else if (mask_result == 0b01) {
        state_output = FrameState::Used;
    }
    return state_output;
}

void ContFramePool::set_state(unsigned long frame_no, FrameState framestate) {
    unsigned int bitmap_index = frame_no / 4;
    unsigned int position = 2 * (frame_no % 4);
    
    switch (framestate) {
    case FrameState::Used:
        bitmap[bitmap_index] = bitmap[bitmap_index] ^ (1 << position); 
        break;
    case FrameState::Free:
        bitmap[bitmap_index] = bitmap[bitmap_index] & ~(3 << position);
        break;
    case FrameState::HoS:
        bitmap[bitmap_index] = bitmap[bitmap_index] ^ (3 << position);
        break;
    default:
        break;
    }
}

ContFramePool* ContFramePool::frame_pool_head;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _nframes,
                             unsigned long _info_frame_no)
{
    assert(_nframes <= FRAME_SIZE * 4);  

    base_frame_no = _base_frame_no;
    nframes = _nframes;
    nFreeFrames = _nframes;
    info_frame_no = _info_frame_no;
    
    /* If _info_frame_no is zero then we keep management info in the first
     * frame, else we use the provided frame to keep management info */
    if(info_frame_no == 0) {
        bitmap = (unsigned char *) (base_frame_no * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_no * FRAME_SIZE);
    }
	
	assert ((nframes % 8) == 0);
    
    /* Everything ok. Proceed to mark all frame as free and set bitmap to 0
	which indicates that all the frames are free */
    for(unsigned long fno = 0; fno < _nframes; fno++) {
        set_state(fno, FrameState::Free);
    }
    
    // Mark the first frame as being used if it is being used
    if(_info_frame_no == 0) {
        set_state(0, FrameState::Used);
        nFreeFrames--;
    }
	
	if (ContFramePool::frame_pool_head == nullptr) {
        ContFramePool::frame_pool_head = this;
        ContFramePool::frame_pool_head->next = nullptr;
    } else {
		// Adding new frame pool to existing linked list
		ContFramePool * temp = nullptr;
		for(temp = frame_pool_head; temp->next != nullptr; temp = temp->next);
        temp->next = this;
		temp = this;
		temp->next = nullptr;
    }
    Console::puts("ContframePool::Constructor initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
	// Any frames left to allocate?
    if ((_n_frames > nFreeFrames) || (_n_frames > nframes)) {
        Console::puts("These many free frames are not available");
        Console::puts("nFreeFrames = "); Console::puti(nFreeFrames);Console::puts("\n");
        Console::puts("_nframes = "); Console::puti(_n_frames);Console::puts("\n");
    }

	unsigned int i = 0, count = 0, cont_m_avail = 0;
	while ((i < ContFramePool::nframes) && (count <= _n_frames)) {
        if (get_state(i) == FrameState::Free) {
            count++;
        } else {
            count = 0;
        }

        if (count == _n_frames) {
            cont_m_avail = 1;
            break;
        }
        i++;
    }

    if(cont_m_avail == 0) {
        Console::puts("Continuous memory not available\n");
        return 0;
    }

    unsigned long first_free_frame = i - _n_frames + 1;  
    for (i = first_free_frame; i < (first_free_frame + _n_frames); i++) {
        if (i == first_free_frame) {
            set_state(i, FrameState::HoS);
        } else {
            set_state(i, FrameState::Used);
        }
        nFreeFrames --;
    }
	Console::puts("Frame alloted\n");
    return (first_free_frame + base_frame_no);
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _nframes)
{	
	// Mark all frames in the range as being used.
    for(unsigned long i = _base_frame_no; i < (_base_frame_no + _nframes); i++) {
        if(i == _base_frame_no) {
			set_state((i - base_frame_no), FrameState::HoS);
		} else {
		    set_state((i - base_frame_no), FrameState::Used);
		}
        nFreeFrames--;
    }
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    ContFramePool* curr_pool = ContFramePool::frame_pool_head;   
    while(curr_pool != nullptr) {
    	if((curr_pool->base_frame_no <= _first_frame_no) &&
            (_first_frame_no < (curr_pool->base_frame_no + curr_pool->nframes))) 
        {
    		break;
    	}
    	curr_pool = curr_pool->next;
    }

    if(curr_pool == nullptr) {
    	Console::puts("Pool not found\n");
    	return;
    }
    
    //Checking and releasing the HoS frame here
    if(curr_pool->get_state(_first_frame_no - curr_pool->base_frame_no) != FrameState::HoS) {
    	Console::puts("First frame is not a head frame\n");
    	return;
    } else {
        curr_pool->set_state(_first_frame_no, FrameState::Free);
        curr_pool->nFreeFrames++;
        _first_frame_no++;
    }

    while(curr_pool->get_state(_first_frame_no - curr_pool->base_frame_no) != FrameState::Free) {
        curr_pool->set_state(_first_frame_no, FrameState::Free);
        curr_pool->nFreeFrames++;
        _first_frame_no++;
    }
    return;
}

unsigned long ContFramePool::needed_info_frames(unsigned long _nframes)
{
    if (_nframes > FRAME_SIZE * 4) {
        Console::puts("Frame number out of range");
        return 0;
    }
    unsigned long max_bits_in_frame = 8 * ContFramePool::FRAME_SIZE;
    return (2 * _nframes) / max_bits_in_frame + ((2 * _nframes) % max_bits_in_frame > 0 ? 1 : 0);
}
