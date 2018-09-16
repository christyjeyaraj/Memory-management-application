#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer_mgr.h"
#include "dberror.h"
#include "storage_mgr.h"

#define MAX_PAGES 50000
#define MAX_FRAMES 300
#define MAX_K 25
/* A frame node consisting one page/frame of a buffer pool*/

typedef struct pageFrameNode{
    
    int pageNum;            /* the pageNum of page in the pageFile*/
    int frameNum;           /* the number of the frame in the frame list*/
    int dirty;              /* dirty = 1, not dirty = 0*/
    int fixCount;           /* fixCound of the page based on the pinning/un-pinning request*/
    int rf;                 /* reference bit per node to be used by clock*/
	int pageFrequency;     /* frequency of the page per client request for LFU*/
    char *data;             /* actual data pointed bu the framenode.*/
    struct pageFrameNode *next;
    struct pageFrameNode *previous;
    
}pageFrameNode;

/* A frame list with a pointer to head and tail node of tyoe pageFrameNode.*/
typedef struct frameList{
    
    pageFrameNode *head;    /* head node of the frame list. Should add new/updated/recently used node to head */
    pageFrameNode *tail;    /* tail node of the frame list. Should be the first/start to remove as per strategy*/
    
}frameList;

/* Required data per buffer pool. Will be attached to BM_BufferPool->mgmtData*/
typedef struct bufManInfo
{

    int numOfFrames;          /* filled number of frames in the frame list */
    int numberRead;            /* number of read done on the buffer pool */
    int numberWrite;           /* number of write done on the buffer pool */
    int countPin;       /* total number of pinning done for the bm */
    void *stratData;
    int pageToFrame[MAX_PAGES];         /* an array from pageNumber to frameNumber. The size should be same as the size of the pageFile.*/
    int frameToPage[MAX_FRAMES];        /* an array from frameNumber to pageNumber. The size should be same as the size of the frame list.*/
	  int pageToFrequency[MAX_PAGES];     /* an array mappping pageNumber to pageFrequency. The size should be same as the size of the pageFile.*/
    bool dirtyFlags[MAX_FRAMES];        /* dirtyflags of all the frames.*/
    int fixedCounts[MAX_FRAMES];        /* fixed count of all the frames.*/
    int khist[MAX_PAGES][MAX_K];        /* history of reference of each page in memory*/
    frameList *frames;      /* a pointer to the frame list of current buffer pool*/

}bufManInfo;

/*Frame list operations*/

/* To create a new node for frame list*/
pageFrameNode *newNode(){
    
    pageFrameNode *node = malloc(sizeof(pageFrameNode));
    node->pageNum = NO_PAGE;
    node->frameNum = 0;
    node->dirty = 0;
    node->fixCount = 0;
    node->rf = 0;
    node->data =  calloc(PAGE_SIZE, sizeof(SM_PageHandle));
    node->next = NULL;
    node->previous = NULL;
	node->pageFrequency = 0;
    
    return node;
}

/* To make the given node the head of the list.*/
void updateHead(frameList **list, pageFrameNode *updateNode)
{

    pageFrameNode *head = (*list)->head;
    if(updateNode == (*list)->tail)
    {
        pageFrameNode *temp = ((*list)->tail)->previous;
        temp->next = NULL;
        (*list)->tail = temp;
    }
    else if(updateNode == (*list)->head)
    {
      return;
    }
    else if(head == NULL)
    {
      return;
    }
    else if(updateNode == NULL)
    {
      return;
    }
    else
    {
        updateNode->previous->next = updateNode->next;
        updateNode->next->previous = updateNode->previous;
    }

    updateNode->next = head;
    head->previous = updateNode;
    updateNode->previous = NULL;

    (*list)->head = updateNode;
    (*list)->head->previous = NULL;
    return;
}

/* To find the node with given page number. It should be first checked by the lookup array,
 and if available in memory, then to find the exact node. */
pageFrameNode *findNodeByPageNum(frameList *list, const PageNumber pageNum)
{

    pageFrameNode *cur = list->head;

    while(cur != NULL)
    {
        if(cur->pageNum == pageNum)
        {
            return cur;
        }
        cur = cur->next;
    }

    return NULL;
}

pageFrameNode *pageInMemory(BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum)
{
    int flag=1;
    pageFrameNode *found;
    bufManInfo *info = (bufManInfo *)bm->mgmtData;

    while(flag)
    {
      if((info->pageToFrame)[pageNum] != NO_PAGE)
      {
        if((found = findNodeByPageNum(info->frames, pageNum)) == NULL)
        {
            return NULL;
        }

        /* provide the client with the data and details of page*/
        page->pageNum = pageNum;
        page->data = found->data;

        /* pinned, so increase the fix count and the read-count*/
        found->fixCount++;
        found->rf = 1;

        return found;
      }
      flag=0;
    }
    return NULL;
}

RC updateNewFrame(BM_BufferPool *const bm, pageFrameNode *found, BM_PageHandle *const page, const PageNumber pageNum){

    SM_FileHandle fh;
    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    RC status;

    if ((status = openPageFile ((char *)(bm->pageFile), &fh)) != RC_OK){
        return status;
    }

    /* If the frame to be replaced is dirty, write it back to the disk.*/
    if(found->dirty == 1)
    {
        if((status = ensureCapacity(pageNum, &fh)) != RC_OK){
            return status;
        }

        if((status = writeBlock(found->pageNum,&fh, found->data)) != RC_OK){
            return status;
        }
        (info->numberWrite)++;
    }

    /* Update the pageToFrame lookup, set the replaceable page's value to NO_PAGE.*/
    (info->pageToFrame)[found->pageNum] = NO_PAGE;

    /* Read the data into new frame.*/

    if((status = ensureCapacity(pageNum, &fh)) != RC_OK){
        return status;
    }

    if((status = readBlock(pageNum, &fh, found->data)) != RC_OK){
        return status;
    }

    /* provide the client with the data and details of page*/
    page->pageNum = pageNum;
    page->data = found->data;

    (info->numberRead)++;

    /* Set all the parameters of the new frame, and update the lookup arrays.*/
    found->dirty = 0;
    found->fixCount = 1;
    found->pageNum = pageNum;
    found->rf = 1;

    (info->pageToFrame)[found->pageNum] = found->frameNum;
    (info->frameToPage)[found->frameNum] = found->pageNum;

    closePageFile(&fh);

    return RC_OK;

}
/* The page replacement strategies.*/

RC pinPage_FIFO (BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum)
{
    pageFrameNode *found;
    bufManInfo *info = (bufManInfo *)bm->mgmtData;

    /* Check if page already in memory. The pageToFrame array provides the fast lookup.*/
    if((found = pageInMemory(bm, page, pageNum)) != NULL){
        return RC_OK;
    }

    /* If need to load the page*/

    /* If the frames in the memory are less than the total available frames, find out the first free frame from the head. */
    if((info->numOfFrames) >= bm->numPages)
    {
      /* For new page, if all the frames are filled, find the oldest frame with fix count 0 */
      found = info->frames->tail;

      while(found != NULL && found->fixCount != 0){
          found = found->previous;
      }

      if (found == NULL){
          return RC_NO_SPACE_IN_BUFFER;
      }

      updateHead(&(info->frames), found);
    }
    else
    {
      found = info->frames->head;
      int i = 0;

      while(i < info->numOfFrames){
          found = found->next;
          ++i;
      }

      /*This frame will be filled up now, so increase the frame count*/
      (info->numOfFrames)++;
      updateHead(&(info->frames), found);
    }

    RC status;

    if((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK){
        return status;
    }

    return RC_OK;
}

RC pinPage_LRU (BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum)
{
    pageFrameNode *found;
    bufManInfo *info = (bufManInfo *)bm->mgmtData;

    /* Check if page already in memory. The pageToFrame array provides the fast lookup.*/
    if((found = pageInMemory(bm, page, pageNum)) != NULL){
        /* Put this frame to the head of the frame list, because it is the latest used frame. */
        updateHead(&(info->frames), found);
        return RC_OK;
    }

    /* If need to load the page*/

    /* If the frames in the memory are less than the total available frames, find out the first free frame from the head. */
    if((info->numOfFrames) >= bm->numPages)
    {
      /* For new page, if all the frames are filled, then try to find a frame with fixed count 0
       * starting from the tail.*/
      found = info->frames->tail;

      while(found != NULL && found->fixCount != 0){
          found = found->previous;
      }

      /* If reached to head, it means no frames with fixed count 0 available.*/
      if (found == NULL){
          return RC_NO_SPACE_IN_BUFFER;
      }
    }
    else
    {
      found = info->frames->head;

      int i = 0;
      while(i < info->numOfFrames){
          found = found->next;
          ++i;
      }
      /*This frame will be filled up now, so increase the frame count*/
      (info->numOfFrames)++;
    }

    RC status;

    if((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK){
        return status;
    }

    /* Put this frame to the head of the frame list, because it is the latest used frame. */
    updateHead(&(info->frames), found);

    return RC_OK;
}

RC pinPage_LRU_K (BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum)
{
    pageFrameNode *found;
    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    int S = (int)(info->stratData);
    int i;
    (info->countPin)++;

    /* Check if page already in memory. The pageToFrame array provides the fast lookup.*/
    if((found = pageInMemory(bm, page, pageNum)) != NULL)
    {

        i=S-1;
        while(i>0)
        {
            info->khist[found->pageNum][i] = info->khist[found->pageNum][i-1];
            i--;
        }

        info->khist[found->pageNum][0] = info->countPin;

        return RC_OK;
    }
    /* If need to load the page*/

    /* If the frames in the memory are less than the total available frames, find out the first free frame from the head. */
    if((info->numOfFrames) >= bm->numPages)
    {
      /* For new page, if all the frames are filled, then try to find a frame with fixed count 0
       * starting from the tail.*/
      pageFrameNode *current;
      int dist, max_dist = -1;

      current = info->frames->head;

      while(current != NULL)
      {
          if(current->fixCount == 0 && info->khist[current->pageNum][S] != -1)
          {

              dist = info->countPin - info->khist[current->pageNum][S];

              if(dist > max_dist)
              {
                  max_dist = dist;
                  found = current;
              }
          }
          current = current->next;
      }

      /* If reached to head, it means no frames with fixed count 0 available.*/
      if(max_dist == -1)
      {
          current = info->frames->head;
          while(current->fixCount != 0 && current != NULL)
          {
              dist = info->countPin - info->khist[current->pageNum][0];
              if(dist > max_dist)
              {
                  max_dist = dist;
                  found = current;
              }
              current = current->next;
          }

          /* If reached to head, it means no frames with fixed count 0 available.*/
          if (max_dist == -1)
          {
              return RC_NO_SPACE_IN_BUFFER;
          }
      }
    }
    else
    {
      found = info->frames->head;
      int i = 0;
      while(i < info->numOfFrames)
      {
        found = found->next;
        ++i;
      }

      /*This frame will be filled up now, so increase the frame count*/
      (info->numOfFrames)++;
    }

    RC status;

    if((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK)
    {
        return status;
    }
    i=S-1;
    while(i>0)
    {
        info->khist[found->pageNum][i] = info->khist[found->pageNum][i-1];
        i--;
    }
    info->khist[found->pageNum][0] = info->countPin;

    return RC_OK;
}

RC pinPage_CLOCK (BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum)
{
    pageFrameNode *found;
    bufManInfo *info = (bufManInfo *)bm->mgmtData;

    /*Page is already in memory*/
    if((found = pageInMemory(bm, page, pageNum)) == NULL)
    {
      pageFrameNode *P = info->frames->head;

      /* retrieve first frame with rf = 0 and set all bits to zero along the way */
      do
      {
        P->rf = 0;
        P = P->next;
      } while(P != NULL && P->rf == 1);

      if (P == NULL)
      {
          return RC_NO_SPACE_IN_BUFFER;
      }

      found = P;
    }

    /* Page is not in memory */
    else
    {
        return RC_OK;
    }

    RC status;

    /*call updateNewFrame with the new value of found*/
    if((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK)
    {
      return status;
    }

    return RC_OK;
}


/*Buffer Pool Functions*/

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData)
{
    int i,flag=0;
    SM_FileHandle fh;

    if(numPages < 0 || numPages == 0)
    {
        return RC_INVALID_BUFFERMANAGER;
    }

    if (openPageFile ((char *)pageFileName, &fh) != RC_OK)
    {
        return RC_FILE_NOT_FOUND;
    }

    while(flag==0)
    {
    /* Initialize the data for mgmtInfo.*/
    bufManInfo *info = malloc(sizeof(bufManInfo));

    info->numOfFrames = 0;
    info->numberRead = 0;
    info->numberWrite = 0;
    info->stratData = stratData;
    info->countPin = 0;

    /* Initialize the lookup arrays with 0 values.*/
    memset(info->frameToPage,NO_PAGE,MAX_FRAMES*sizeof(int));
    memset(info->pageToFrame,NO_PAGE,MAX_PAGES*sizeof(int));
    memset(info->dirtyFlags,NO_PAGE,MAX_FRAMES*sizeof(bool));
    memset(info->fixedCounts,NO_PAGE,MAX_FRAMES*sizeof(int));
    memset(info->khist, -1, sizeof(&(info->khist)));
	  memset(info->pageToFrequency,0,MAX_PAGES*sizeof(int));

    /* Creating the linked list of empty frames. */
    info->frames = malloc(sizeof(frameList));

    info->frames->head = info->frames->tail = newNode();
    i=1;
    while(i<numPages)
    {
        info->frames->tail->next = newNode();
        info->frames->tail->next->previous = info->frames->tail;
        info->frames->tail = info->frames->tail->next;
        info->frames->tail->frameNum = i;
        ++i;
    }

    bm->numPages = numPages;
    bm->pageFile = (char*) pageFileName;
    bm->strategy = strategy;
    bm->mgmtData = info;

    closePageFile(&fh);
    flag=1;
  }
    return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm)
{
    int flag=0;
    if (bm->numPages < 0 || bm->numPages == 0 || !bm){
        return RC_INVALID_BUFFERMANAGER;
    }
    RC status;

    if((status = forceFlushPool(bm)) != RC_OK){
        return status;
    }

    do
    {
    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    pageFrameNode *cur = info->frames->head;

    while(cur != NULL){
        cur = cur->next;
        free(info->frames->head->data);
        free(info->frames->head);
        info->frames->head = cur;
    }

    info->frames->head = info->frames->tail = NULL;
    free(info->frames);
    free(info);

    bm->numPages = 0;
    flag=1;
  } while(flag==0);

    return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm)
{
    int flag=0;
    if (bm->numPages < 0 || bm->numPages == 0 || !bm){
        return RC_INVALID_BUFFERMANAGER;
    }

    do
    {
    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    pageFrameNode *cur = info->frames->head;

    SM_FileHandle fh;

    if (openPageFile ((char *)(bm->pageFile), &fh) != RC_OK){
        return RC_FILE_NOT_FOUND;
    }

    while(cur != NULL)
    {
        if(cur->dirty == 1)
        {
            if(writeBlock(cur->pageNum, &fh, cur->data) != RC_OK)
            {
                return RC_WRITE_FAILED;
            }
            cur->dirty = 0;
            (info->numberWrite)++;
        }
        cur = cur->next;
    }

    closePageFile(&fh);
    flag=1;
  } while(flag==0);

    return RC_OK;
}

/*Page Management Functions*/

RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
    int flag=1;
    if (bm->numPages < 0 || bm->numPages == 0 || !bm)
    {
        return RC_INVALID_BUFFERMANAGER;
    }

    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    pageFrameNode *found;

    /* Locate the page to be marked as dirty.*/
    while(flag)
    {
      if((found=findNodeByPageNum(info->frames, page->pageNum)) != NULL)
      {
        found->dirty = 1;
        flag=0;
      }
      else
      {
        return RC_NOSUCHPAGEINFRAME;
      }
    }
    return RC_OK;
}

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
    int flag=1;
    if (bm->numPages < 0 || bm->numPages == 0 || !bm)
    {
        return RC_INVALID_BUFFERMANAGER;
    }

    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    pageFrameNode *found;

    /* Locate the page to be unpinned */
    while(flag==1)
    {
      if((found = findNodeByPageNum(info->frames, page->pageNum)) == NULL)
      {
        return RC_NOSUCHPAGEINFRAME;
      }
      flag=0;
    }
    /* Unpinned so decrease the fixCount by 1 */
    while(flag==0)
    {
      if(found->fixCount < 0)
      {
          return RC_NOSUCHPAGEINFRAME;
      }
      else
      {
          found->fixCount--;
      }
      flag=1;
    }

    return RC_OK;
}

RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)

{
    int flag=0;
    if (bm->numPages < 0 || bm->numPages == 0 || !bm)
    {
        return RC_INVALID_BUFFERMANAGER;
    }

    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    pageFrameNode *found;
    SM_FileHandle fh;

    while(flag==0)
    {
      if (openPageFile ((char *)(bm->pageFile), &fh) == RC_OK)
      {
        /* Locate the page to be forced on the disk */
        if((found = findNodeByPageNum(info->frames, page->pageNum)) != NULL)
        {
          /* write the current content of the page back to the page file on disk */
          if(writeBlock(found->pageNum, &fh, found->data) == RC_OK)
          {
            (info->numberWrite)++;
            closePageFile(&fh);
            return  RC_OK;
          }
          else
          {
            closePageFile(&fh);
            return RC_WRITE_FAILED;
          }
        }
        else
        {
          closePageFile(&fh);
          return RC_NOSUCHPAGEINFRAME;
        }
      }
      else
      {
        return RC_FILE_NOT_FOUND;
      }
      flag=1;
    }
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page,
            const PageNumber pageNum)
{
    if (bm->numPages < 0 || bm->numPages == 0 || !bm)
    {
        return RC_INVALID_BUFFERMANAGER;
    }
    if(pageNum < 0)
    {
        return RC_READ_NON_EXISTING_PAGE;
    }
    if(bm->strategy==RS_FIFO)
      return pinPage_FIFO(bm,page,pageNum);
    else if(bm->strategy==RS_LRU)
      return pinPage_LRU(bm,page,pageNum);
    else if(bm->strategy==RS_CLOCK)
      return pinPage_CLOCK(bm,page,pageNum);
    else if(bm->strategy==RS_LRU_K)
      return pinPage_LRU_K(bm,page,pageNum);
    else
      return RC_UNKNOWN_STRATEGY;
    //return RC_OK;
}

/*Statistics Functions*/


PageNumber *getFrameContents (BM_BufferPool *const bm)
{
    /*return the value of frameToPage*/
    return ((bufManInfo *)bm->mgmtData)->frameToPage;
}

bool *getDirtyFlags (BM_BufferPool *const bm)
{
    /*goes through the list of frames and updates the value of dirtyFlags accordingly*/
    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    pageFrameNode *cur = info->frames->head;
    
    while (cur != NULL){
        (info->dirtyFlags)[cur->frameNum] = cur->dirty;
        cur = cur->next;
    }
    
    return info->dirtyFlags;
}

int *getFixCounts (BM_BufferPool *const bm)
{
    /*goes through the list of frames and updates the value of fixedCounts accordingly*/
    bufManInfo *info = (bufManInfo *)bm->mgmtData;
    pageFrameNode *cur = info->frames->head;
    
    while (cur != NULL){
        (info->fixedCounts)[cur->frameNum] = cur->fixCount;
        cur = cur->next;
    }
    
    return info->fixedCounts;
}

int getNumReadIO (BM_BufferPool *const bm)
{
    /*returns the value of numberRead*/
    return ((bufManInfo *)bm->mgmtData)->numberRead;
}

int getNumWriteIO (BM_BufferPool *const bm)
{
    /*returns the value of numberWrite*/
    return ((bufManInfo *)bm->mgmtData)->numberWrite;
}
