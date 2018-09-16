#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "storage_mgr.h"
#include "dberror.h"

/* Function: initStorageManager
 * Function to initialize the Storage Manager
 */
void initStorageManager(void)
{
	printf("Storage Manager initialized and started\n");
}


/*
*******************  Page Functions  *************************
*/

/* Function: createPageFile
 * This function creates a new page file with one page  of size PAGE_SIZE and fills that single page with '\0' bytes
 */
RC createPageFile(char *fileName)
{
	// Opening a new page file in write mode
	FILE *file;	
	file = fopen(fileName, "wb+");
	char *fillChar[PAGE_SIZE];
	char *allpage_str, *actual_data;

	// Checks if the "file" pointer has a non-zero value for fopen() to be successful
	if (file != NULL)
	{
		// Allocate "first" page to store total number of pages information
		allpage_str = (char *)malloc(PAGE_SIZE);

		// Copying '\0' to a block of memory pointed to by "fillChar"
		memset(fillChar, '\0', PAGE_SIZE);

		/* Considered as actual first page for the data 
		 * Allocate memory ("PAGE_SIZE" bytes) using the malloc() for the memory block pointed to by "actual_data" 
		*/
		actual_data = (char *)malloc(PAGE_SIZE);
		strcat(allpage_str, "1\n");

		fwrite(allpage_str, sizeof(char), PAGE_SIZE, file);
		fwrite(actual_data, sizeof(char), PAGE_SIZE, file);

		// Deallocate the memory allocated to the memory block pointed to by "allpage_str"
		free(allpage_str);
		free(actual_data);

		fclose(file);
		return RC_OK;
	}
	else
	{
		return RC_FILE_NOT_FOUND;
	}
}

/* Function: openPageFile 
 * This function opens an already existing page file and finds the number of pages in the opened file and get other info 
 */
RC openPageFile(char *fileName, SM_FileHandle *fHandle)
{
	FILE *file;
	// Opening a file in read mode
	file = fopen(fileName, "r+");

	while (file != NULL)
	{
		char *str_ptr;

		// Allocate memory of size "PAGE_SIZE" bytes to the block pointed by "str_ptr"
		str_ptr = (char *)malloc(PAGE_SIZE);

		// fgets() retrieves the contents from "file" and stores into the string pointed to by "str_ptr"
		fgets(str_ptr, PAGE_SIZE, file);

		// remove trailing newline char
		str_ptr = strtok(str_ptr, "\n"); 

		// Storing the "file" pointer to "mgmtInfo" of fHandle
		fHandle->mgmtInfo = file;

		// Finding the total number of pages in the file using atoi()
		fHandle->totalNumPages = atoi(str_ptr);

		// Seeking/Moving the cursor to the beginning of the file
		fseek(file, 0, SEEK_SET);

		// Get the current position of the cursor using ftell()
		fHandle->curPagePos = ftell(file);

		// Assign values to SM_FileHandle struct components
		fHandle->fileName = fileName;

		// Deallocate the memory allocated by malloc() to the memory block pointed to by "str_ptr"
		free(str_ptr);

		return RC_OK;
	}
	if (file == NULL)
		return RC_FILE_NOT_FOUND;
}

/* Function: closePageFile
 * This function closes an opened page file 
 */
RC closePageFile(SM_FileHandle *fHandle)
{
	FILE *file;
	// Opening page file in "read" mode
	file = fopen(fHandle->fileName, "r");

	// Check if the file has closed
	if (fclose(file) == 0)
	{
		file = NULL;
		return RC_OK;
	}
	else
	{
		return RC_FILE_NOT_FOUND;
	}
}

/* Function: destroyPageFile
 * This function destroys or deletes a page file even if it is opened or closed.
 */
RC destroyPageFile(char *fileName)
{
	FILE *file = fopen(fileName, "wb+");

	// int remove() returns '0' if a file is successfully deleted
	int destroyPageStatus = remove(fileName);

	if (destroyPageStatus == 0)
	{
		// File is deleted
		return RC_OK;
	}
	else
	{
		return RC_FILE_NOT_FOUND;
	}
}


/*
*******************  Read Functions  *************************
*/

/* Function: readBlock
 * This function reads a page numbered with pageNum into "memPage".
 * It first checks if the page number is valid or not.
 * If valid, it checks if the file pointer is available or not.
 * With valid file pointer, it reads the given page and current page position is increased.
 */
RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	FILE *file;
	long int position;
	// Open file in read mode
	file = fopen(fHandle->fileName, "r");

	while (file)
	{
		// Check if the file is open or not
		if (file == NULL)
		{
			return RC_FILE_NOT_FOUND;
		}

		if (fHandle == NULL || fHandle->totalNumPages < pageNum)
		{
			return RC_FILE_HANDLE_NOT_INIT;
		}

		// Check if the file does not exist
		if (pageNum < 0 || pageNum > fHandle->totalNumPages)
		{
			return RC_READ_NON_EXISTING_PAGE;
		}

		if (memPage == NULL)
		{
			return RC_NOSUCHPAGEINBUFFER;
		}

		position = (pageNum + 1) * PAGE_SIZE * sizeof(char);

		// Seeks the cursor to the beginning of the file handle.
		fseek(fHandle->mgmtInfo, position, SEEK_SET);

		// Read the contents of the "fHandle->mgmtInfo" to the location pointed to by "memPage".
		fread(memPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);

		fHandle->curPagePos = pageNum;
		fclose(file);
		return RC_OK;
	}
}

/* Function: getBlockPos
 * This function gets the current page position in the file using the attribute "curPagePos".
 */
int getBlockPos(SM_FileHandle *fHandle)
{
	if (fHandle == NULL)
	{
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else
	{
		// Current page position in the file  is assigned to "'position"
		int position = fHandle->curPagePos;
		return position;
	}
}

/* Function: readFirstBlock
 * This function reads the first block by providing the pageNum argument as 0 to the readBlock function.
 */
RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	FILE *file;
	file = fHandle->mgmtInfo;

	do
	{
		if (fHandle != NULL)
		{
			// Call to the readBlock() specifying the first block as parameter
			return readBlock(0, fHandle, memPage);
		}
		else
		{
			return RC_FILE_HANDLE_NOT_INIT;
		}
		if (file == NULL)
		{
			return RC_FILE_NOT_FOUND;
		}
	} while (file);
}

/* Function: readPreviousBlock
 * This function reads the previous block by providing the "pageNum" argument as (current_position - 1) to the readBlock function.
 */
RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	FILE *file;
	file = fHandle->mgmtInfo;

	if (fHandle == NULL)
	{
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else if (file == NULL)
	{
		return RC_FILE_NOT_FOUND;
	}
	else
	{
		int curPage = fHandle->curPagePos;
		if (curPage == 0)
		{
			return RC_READ_NON_EXISTING_PAGE;
		}
		else
		{
			int prevPage = curPage - 1;
			return readBlock(prevPage, fHandle, memPage);
		}
	}
}

/* Function: readCurrentBlock
 * This function reads the current block by providing the pageNum argument as current_position to the readBlock function.
 */
RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	FILE *file;
	file = fHandle->mgmtInfo;
	int curPage = fHandle->curPagePos;
	int totalPages = fHandle->totalNumPages;

	if (fHandle == NULL)
	{
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else if (file == NULL)
	{
		return RC_FILE_NOT_FOUND;
	}
	else if (curPage < 0 || curPage > totalPages)
	{
		return RC_READ_NON_EXISTING_PAGE;
	}
	else
	{
		// Calling the readBlock() with the current block position as the parameter
		return readBlock(curPage, fHandle, memPage);
	}
}

/* Function: readNextBlock
 * This function reads the next block by providing the pageNum argument as (current_position + 1) to the readBlock function.
 */
RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	FILE *file;
	file = fHandle->mgmtInfo;
	int curPage = fHandle->curPagePos;
	int nxtPage = curPage + 1;
	int totalPages = fHandle->totalNumPages;

	if (fHandle == NULL)
	{
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else if (file == NULL)
	{
		return RC_FILE_NOT_FOUND;
	}
	else if (nxtPage > totalPages)
	{
		return RC_READ_NON_EXISTING_PAGE;
	}
	else
	{
		return readBlock(nxtPage, fHandle, memPage);
	}
}

/* Function: readLastBlock
 * This function reads the last block by providing the pageNum argument as (current_position - 1) to the readBlock function.
 */
RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	FILE *file;
	file = fHandle->mgmtInfo;

	do
	{
		int lstPage = fHandle->totalNumPages;

		if (fHandle == NULL)
		{
			return RC_FILE_HANDLE_NOT_INIT;
		}
		else if (file == NULL)
		{
			return RC_FILE_NOT_FOUND;
		}
		else if (memPage == NULL)
		{
			return RC_NOSUCHPAGEINBUFFER;
		}
		else
		{
			// Calling the readBlock() with the last block as the parameter.
			return readBlock(lstPage, fHandle, memPage);
		}
	} while (file);
}


/*
*******************  Write Functions  *************************
*/

/* Function: writeBlock
 * This function writes the contents of the page handler to the file 
 */
RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	long int position;

	if (fHandle == NULL)
	{
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else if (pageNum < 0 || pageNum > fHandle->totalNumPages)
	{
		return RC_READ_NON_EXISTING_PAGE;
	}
	else if (memPage == NULL)
	{
		return RC_NOSUCHPAGEINBUFFER;
	}
	else
	{
		position = (pageNum + 1) * PAGE_SIZE * sizeof(char);

		// Seeks file write pointer to the beginning of the page
		fseek(fHandle->mgmtInfo, position, SEEK_SET);

		// Write the contents of fHandle->mgmtInfo to the memory block pointed to by "memPage"
		fwrite(memPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);

		fHandle->curPagePos = pageNum;
		return RC_OK;
	}
}

/* Function: writeCurrentBlock
 * This function write a page to the disk using the current block position. 
 */
RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	FILE *file;
	file = fHandle->mgmtInfo;
	int curPage = fHandle->curPagePos;

	if (file == NULL)
	{
		return RC_FILE_NOT_FOUND;
	}
	else if (fHandle == NULL)
	{
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else
	{
		// Writing a page using the current block position "curPage" as argument to writeBlock()
		return writeBlock(curPage, fHandle, memPage);
	}
}

/* Function: appendEmptyBlock
 * This function appends the file with an empty block and fills that new last block with '\0'bytes 
 */
RC appendEmptyBlock(SM_FileHandle *fHandle)
{
	int seek_end;
	// Allocates memory and return a pointer to it
	SM_PageHandle blank = (char *)calloc(PAGE_SIZE, sizeof(char));
	int totalPages = fHandle->totalNumPages + 1;
	int position = (totalPages + 1) * PAGE_SIZE * sizeof(char);

	// Seeks file write pointer to the last page
	seek_end = fseek(fHandle->mgmtInfo, position, SEEK_END);

	if (seek_end == 0)
	{
		// Writes data from the memory block pointed by eb to the file i.e last page is filled with zero bytes.
		fwrite(blank, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);

		totalPages = totalPages + 1;
		fHandle->curPagePos = fHandle->totalNumPages;
		rewind(fHandle->mgmtInfo);
		int new_position = (totalPages + 1) * PAGE_SIZE * sizeof(char);

		// Update total number of pages information in the file using fprintf()
		fprintf(fHandle->mgmtInfo, "%d\n", fHandle->totalNumPages);

		fseek(fHandle->mgmtInfo, new_position, SEEK_SET);
		free(blank);
		return RC_OK;
	}
	else
	{
		free(blank);
		return RC_WRITE_FAILED;
	}
}

/* Function: ensureCapacity
 * This function checks if the file has less than "numberOfPages" pages 
 * If yes,then increases the size to numberOfPages by calling the appendEmptyBlock() 
 */
RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle)
{
	FILE *file;
	file = fHandle->mgmtInfo;

	if (file == NULL)
	{
		return RC_FILE_NOT_FOUND;
	}
	else if (fHandle == NULL)
	{
		return RC_FILE_HANDLE_NOT_INIT;
	}
	else
	{
		int totalPages = fHandle->totalNumPages;
		int i = numberOfPages - totalPages;
		int x = 0;
		if (totalPages < numberOfPages)
		{
			while (x < i)
			{
				appendEmptyBlock(fHandle);
				x++;
			}
		}
		return RC_OK;
	}
}
