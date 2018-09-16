=========================
#   Problem Statement   #
=========================
The goal of this assignment is to implement a buffer manager. The buffer manager manages a fixed number
of pages in memory that represent pages from a page file managed by the storage manager implemented in
assignment 1. The memory pages managed by the buffer manager are called page frames or frames for short.
We call the combination of a page file and the page frames storing pages from that file a Buffer Pool.
The Buffer manager should be able to handle more than one open buffer pool at the same time. However,
there can only be one buffer pool for each page file. Each buffer pool uses one page replacement
strategy that is determined when the buffer pool is initialized. At least implement two replacement
strategies FIFO and LRU.

===========================
#      Contribution       #
===========================
This is a group project. Group size is 4. And I worked on 
  a. Buffer Pool Functions.
  b. Page Replacement Strategies : FIFO.
  c. Related test cases and documentation


=================================
#      Logic and code flow     #
=================================

Data Structures and Design :
————————————————————————————

We used double-linked list to implement the frames. Each node of the frames is called pageFrameNode, and it contains
below attributes :
1. pageNum      - page # of the page in the pageFile.
2. frameNum     - no. of frames in the frame list.
3. dirty        - the dirty bit of the frame.( 1 = dirty, 0 = not dirty).
4. fixCount     - Based on the pinning/un-pinning request, fixCount of the page.
5. rf           - reference bit/node which can be used by any page replacement algorithm, if needed.
6. hist         - The history reference of each page in memory.
7. data         - The actual data which is pointed by the frameNode.

There are few attributes defined or required at BufferPool level. We have implemented a structure of such attributes
called bmInfo, and assigned it to the BM_BufferPool->mgmtData. The attributes of this structure are :
1. numOfFrames    - the # of filled number of frames in the frame list.
2. numberRead      - The total # of reads done on the buffer pool.
3. numberWrite     - The total # of writes done on the buffer pool.
4. countPin        - The total # of pinning done on the buffer pool.
5. stratData    - contains value of BM_BufferPool->stratData
6. pageToFrame  - It's an array from pageNumber to frameNumber. Value will be FrameNum, if page in memory , anyother case -1.
7. frameToPage  - It's an array from frameNumber to pageNumber. Value will be PageNum, if frame is full , anyother case -1.
8. dirtyFlags   - It's an array of dirty flags of all frames.
9. fixedCounts  - It's an array of fixed count of all frames.


Buffer Pool Functions :
——————————————————————

initBufferPool:
1. initBufferPool takes a BM_BufferPool instance as an argument and initializes its attributes based on various other arguments.
2. Validation is the first step of the function where in it validates the arguments provided. If any of them are invalid (i.e validation fails), it returns
   with an appropriate error message.
2. It then initializes the attributes of BM_BufferPool instance.
3. Lastly the frame list is initialized with empty frames.

shutdownBufferPool:
1. shutdownBufferPool takes a BM_BufferPool instance as an argument and then deallocates all memory.
2. Validation is the first step which returns an error in case of invalid arguments.
3. Next it empties all the frames of frame list and deallocates the memory of each node.
4. Lastly it deallocates the memory of BM_BufferPool->mgmtData, which finally points to the bmInfo structure.

forceFlushPool:
1. forceFlushPool takes a BM_BufferPool instance as an argument and writes all the dirty frames to the file on the disk.
2. Validation is the first step which returns an error in case of invalid arguments.
3. Iteration through the frame list starting from head is done next; If any frame has dirty bit set as 1, do as follows:
   a. Writes the data back to the file on disk.
   b. It sets the dirty bit as 0.
   c. It increases the value of numWrite by 1;
4. Once all the frames are iterated, it returns RC_OK if we get no error faced.


Page Management Functions :
———————————————————————————

pinPage:
1. This function calls pinpage functions defined accordingly with the given strategy: pinpage_FIFO, pinpage_LRU,
   pinpage_CLOCK etc.
2. These functions basically pins the page with the given pagenumber pageNum.
3. Buffer manager uses different strategies to locate the page requested and then provides the details of the page to client.

unpinPage:
1. This function will first iterate through the available pages in the frames to find the page to be unpinned and does the following based on the output:
2. If page is not found it returns an exception "RC_NOSUCHPAGEINFRAME"
3. If page is found, "fixCount" is decreased by 1 and the function returns RC_OK.

markDirty:
1. This function will immediately iterate through the available pages in the frames to find the page which is to be marked as dirty.
2. If the page is found then we set the dirty bit of the page node as 1 and return RC_OK.

forcePage:
1. This function will first iterate through the available pages in the frames to locate the page to be forced to disk.
2. If the page is found it opens the file and write current content of the page back to the page file on disk.
3. If the write operation is successful after page is found it returns RC_OK or else returns RC_NON_EXISTING_PAGE_IN_FRAME.


Statistics Functions:
————————————————————————
Statistics functions provide information regarding the buffer pool and its contents.
To provide information about the pool, the print debug functions use statistics functions.

getFrameContents:
1. frameToPage is included in BM_BufferPool->mgmtData. This is an array of PageNumbers where the ith element was
   the page stored in the ith page frame. This is updated whenever a new frame is added in the function updateNewFrame.
   getFrameContents returns this array.

getDirtyFlags:
1. This function returns an array of booleans where the ith element is true if the ith page frame is dirty.
2. This array is stored in BM_bufferPool->mgmtData->dirtyFlags. getDirtyFlags returns this array.
3. dirtyFlags is populated by traversing the list of frames and checking to see which frames are marked as dirty.

getFixCounts:
1. This function returns an array of ints where the ith element is the fix count of the page stored in the ith page frame.
2. 0 is returned for empty page frames.
3. This array is stored in BM_bufferPool->mgmtData->fixedCounts. getFixCounts returns this array.
4. fixedCounts is populated by traversing the list of frames and using the fixCount value of each frame.

getNumReadIO:
1. This function returns the number of pages that have been read from disk since a buffer pool was initialized.
2. This function returns the value of BM_bufferPool->mgmtData->numRead which is set in updateNewFrame and in initBufferPool.

getNumWriteIO:
1. This function returns the number of pages that have been written to the page file since the buffer pool was initialized.
2. This function returns the value of BM_bufferPool->mgmtData->numWrite which is set in updateNewFrame, initBufferPool,
   forcePage, and forceFlushPool.


The Page Replacement Strategies:
————————————————————————————————

pinPage_FIFO:

This function implements the FIFO page replacement strategy.

1. In the first place, it verifies whether the page is in memory. In the event that the page is discovered it calls the pageInMemory work portrayed in 
   "Helper  Functions" later, and returns RC_OK.
2. On the off chance that the page is required to be stacked in the memory then initially free frame is found beginning from head. 
   On the off chance that a empty frame is discovered then the page is stacked in the discovered frame and page details are set. 
   Likewise, the frame frame is refreshed to be the head of the linked list.
3. For new page, if every one of the frames are filled, at that point the function begins repeating from trail of the list to 
   find the most seasoned frame with fix count 0. The observed frame is refreshed to be the head of the linked list.
4. In the event that the frame is discovered after above strategy then updateNewFrame work depicted in "Helper Functions" later; 
   generally the function returns no more space in buffer error.

pinPage_LRU:

This function implements the LRU replacement policy as described in the lecture.

1. To begin with, it verifies whether the page is in memory. In the event that it will be, it calls the pageInMemory function portrayed in 
   Helper Functions later, and returns promptly with RC_OK.
2. Each time a frame is referenced, it is moved to the head of the framelist. In this way, at any moment the head will be the 
   most recent utilized frame, and the tail will be the minimum utilized frame.
3. In the event that the page isn't in memory, it begins iterating from the tail of the list to search for a frame with fixcount 0.
4. On the off chance that any such frame is discovered, it calls the updateNewFrame work portrayed in Helper Functions later; else it 
   returns no more space in buffer error.


Helper Functions:
—————————————————

There are mainly 4 helper function used throughout the application :

updateHead:
1. This function takes a framelist and a framenode as arguments, and makes the node the head of given list.
2. This function is utilized by various page substitution techniques with a specific end goal to keep the consistent request of the frames
   in the frame list.

findNodeByPageNum:
1. This function takes a framelist and a pageNumber as arguments, and looks for the page in the framelist.
2. In the event that the page is discovered, it restores the frameNode; otherwise returns NULL.
3. This function is utilized by various page replacement methodologies so as to locate the required frame from the frame list.

pageInMemory:
1. This funciton takes a BM_BufferPool, BM_PageHandle and a pageNumber as arguments, and looks for the page in the 
   framelist.
2. On the off chance that the page is discovered : 
   a. It sets the BM_PageHandle to refer to this page in memory. 
   b. It increases the fixcount of the page.
3. This function is utilized by various page replacement techniques on the off chance that when the page is as of now accessible in 
   the frame list.

updateNewFrame:
1. This function takes a BM_BufferPool, BM_PageHandle, the destination frame and a pageNumber as arguments.
2. On the off chance that the destination frame has a dirty page, it composes the page back to the disk and updates related attributes.
3. It at that point reads the destination page from the disk into memory, and store in the given frame.
4. It updates the pageNum, numRead, dirty, fixcount and rf attributes of the destination frame.
5. This function is utilized by various page replacement strategies on the off chance that when the page isn't in the memory. It calls the 
   function with the frame to be replaced, and the new page to be loaded.



==========================
#   Additional Features  #
==========================


Additional Page Replacement Strategies :
———————————————————————————————————————

pinPage_CLOCK:

This function implements the clock replacement policy as described in the lecture.

1. To start with, it verifies whether the page is in memory. In the event that it will be, it returns promptly with RC_OK.
2. In the event that the page isn't in memory, it searches for the first frame with a reference bit that is equivalent to zero. En route, 
   it sets all the reference bits to zero. The reference bit (rf) is set in pageFrameNode->rf.
3. The new value of found is used in updateNewFrame.

pinPage_LRU_K:

This function implements the LRU_K replacement policy as explained in the paper.

1. To begin with, it verifies whether the page is in memory. On the off chance that it will be, it calls the pageInMemory function portrayed in 
   Helper Functions later, and returns instantly with RC_OK.
2. Each time a frame is referenced, the reference number(current count of pinning) is updated in the history array
   (bminfo->khist).
3. On the off chance that the page isn't in memory, it begins iterating from the head of the list and calculate the distance as the 
   different of current check of pinning and kth reference of the page, for all pages in memory having fixcount 0.
4. The page with the maximum distance is replaced. On the off chance that no page is called k time (kth reference is - 1 for all pages), at that point 
   it works similarly as the LRU, and checks for the least recently used page.
5. Regardless, if any such frame is discovered, it calls the updateNewFrame function portrayed in Helper Functions later; 
   else it returns no more space in buffer error.

Additional Error checks :
————————————————————————
Below error cases are checked and tested :
1. Try to initialize an invalid buffer pool.(0 or negative number of frames, invalid strategy)
2. Try to pin a page into a full bufferpool.
3. Try to pin an invalid page.(uninitialized page instance, or a negative page number.)
4. Try to unpin a page which is not available in framelist.
5. Try to forceflush a page which is not available in framelist.
6. Try to markdirty a page which is not available in framelist.
7. Try to unpin a page which is available in the frame list, but not pinned by any one.
8. Try to shutdown a bufferpool which is not initialized.


Additional Test Cases :
——————————————————————

In addition to the default test cases, we have implemented test cases for all the additional page replacement strategies
and the additional error checks in the test_assign2_2.c. The instructions to run these test cases are provided above.


No memory leaks :
—————————————————

The essential and the additional test cases are implemented and tested with valgrind for no memory leaks.
