*** Problem Statement:

The goal of this assignment is to implement a simple storage manager-a module that is capable of reading blocks from a file on disk into memory and writing blocks from memory to a file on disk. 
The storage manager deals with pages (blocks) of fixed size (PAGE_SIZE). 
In addition to reading and writing pages from a file, it provides methods for creating, opening, and closing files. 
The storage manager must maintain several types of information for an open file: the number of total pages in the file, the current page position (for reading and writing), the file name, and a POSIX file descriptor or FILE pointer. 

================================================================================================================================
*** This is a group project. The group size is 4. And I worked on the Manipulating page files sections

================================================================================================================================
*** Logic:

File Related Functions:
The main purpose of these methods is to manage page files which are then used by the read and write methods. One of the main points of our idea included that we needed to store  the total number of pages in the file so that this information could be accessed by the respective read and write methods.

createPageFile:
1. Creates a file based on the given fileName
2. Creates two pages - one for storing the total number of pages
                     - second is the first page where data will begin to be written to

openPageFile:
1. Retrieves the total number of pages
2. Assigns the contents of the SM_FileHandle struct
3. mgmtInfo is used to store the FILE pointer to be used by the read and write methods

closePageFile:
1. Closes the page file pointed to by the FILE pointer in fHandle->mgmtInfo
2. If the file could not be closed, returns RC_FILE_NOT_FOUND

destroyPageFile:
1. Deletes the page file associated with fileName
2. If this file could not be found, returns RC_FILE_NOT_FOUND


Read Related Functions:
The main purpose of these functions is to read pages from the file into memory. The function takes the file pointer from the file handler, and read the respective page given with the first argument - pageNum into the memory pointed by memPage. 

readBlock:
1. First check if the file exists or not. If it doesnt't exist, returns RC_FILE_NOT_FOUND
2. Check if the page number is valid or not. If it is not valid, returns RC_READ_NON_EXISTING_PAGE
3. If valid, it checks if the file pointer is available or not. If it is unavailable, returns RC_FILE_HANDLE_NOT_INIT
4. With valid file pointer, it reads the given page and current page position is increased.

getBlockPos:
This function gets the current page position from the attribute curPagePos of the file handler.

readFirstBlock:
This function reads the first block by providing the pageNum argument as 0 to the readBlock function.

readPreviousBlock:
Read page from the previous block.

readCurrentBlock:
Read page from the current block.

readNextBlock:
Read one page from the next block.

readLastBlock:
Read page from the last block.


Write Related Functions:
Purpose of implementing write methods is to write blocks from memory to a file present on a disk in an efficient manner considering given constraints and requirements.

writeBlock:
1. Check if the page number is valid or not. If it is not valid, returns RC_READ_NON_EXISTING_PAGE
2. Check if the file pointer is available or not. If it is unavailable, returns RC_FILE_HANDLE_NOT_INIT
3. Verifyerify the page number before writing the data into the page.
4. Seek the file write pointer to the page number given by the user.
5. With valid file pointer, it writesd the data from memory block and current page position is increased.

writeCurrentBlock:
Call writeBlock function and pass current page position as the page number where data is to be written.

appendEmptyBlock:
Write an empty page to the file (disk) by appending to the end.

ensureCapacity:
1. First, check If the file has less than numberOfPages then required.
2. If yes, then calculate number of pages insufficient to meet the required size of the file.
3. Call appendEmptyBlock function and append the insufficient number of pages to the file. 

================================================================================================================================================================
*** Additional ERROR CODES:

 RC_NOSUCHPAGEINBUFFER 18
 
================================================================================================================================================================
*** No additional data structures used
*** No memory leaks
 





