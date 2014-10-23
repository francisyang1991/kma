/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the resource map algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - addtofreelisted size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_RM
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

//doubly linked list hold available free blocks
typedef struct
{
	/* data */
	int size;		//size of current freeblock
	void *next;	//next free block
	void *prev;	//prev free block
	void *pageid;	//id of page
} freeblockll_t;

//header to ll
typedef struct
{
	/* data */
	void *self;		//copy of llheader_t pointer, used to free page
	int numpages;
	int numalloc;	//number of allocated blocks per page
	freeblockll_t *header;	//head of free list
} llheader_t;

/************Global Variables*********************************************/
kma_page_t *entryptr = 0;		//entry ptr
/************Function Prototypes******************************************/
void *findfirstfit(int size);		//return pointer to free space using first fit
void addtofreelist (void* ptr, int size);	//addtofreelists pointer to free space 
void freeunalloc(void);	//looks for pages being used with no allocated blocks and frees those pages
void remove(void *ptr);	//remove pointer from list
void initial(kma_page_t* page, int first);	//initialize page

/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
	if ((size + sizeof(void *)) > PAGESIZE)		//ignore requests larger than page size
	{
		return NULL;
	}

	void *ret;

	if (!entryptr)		//initialize first page if entryptr is null
		initial(get_page(), 1);
	
	//call findfirstfit to find fit in list
	//if no fit can be found, allocate a new page
	ret = findfirstfit(size);
	llheader_t *thispage;

	thispage = (*(freeblockll_t *) ret).pageid;
	((*thispage).numalloc)++;		//increment number of allocated blocks on the page
	return ret;
}

void
kma_free(void* ptr, kma_size_t size)
{
	//addtofreelist ptr to list
  addtofreelist(ptr, size);
  //decrement number of allocated blocks on that page
  (((llheader_t*)(((freeblockll_t*) ptr)->pageid))->numalloc)--;
	//free a page if it has no allocated blocks
	freeunalloc();
}

/*initialize page */
void initial(kma_page_t *page, int first)
{
	llheader_t *listheader;
	*((kma_page_t**) page->ptr) = page;	

	listheader = (llheader_t*) (page->ptr); //set list header to base of page

	//set header to right after initial data structure
	listheader->header = (freeblockll_t*) ((long) listheader + sizeof(llheader_t));

	if (first)
		entryptr = page;
	//add the page to the ll
	addtofreelist(((void *) (listheader->header)), (PAGESIZE - sizeof(llheader_t)));
	(*listheader).numalloc = 0;
	(*listheader).numpages = 0;

	//return page;
}

/* return pointer to free space using first fit */
void *findfirstfit(int size)
{
	if (size < sizeof(freeblockll_t))
		size = sizeof(freeblockll_t);	//min size allowed for rm

	llheader_t *mainpage;
	mainpage = (llheader_t*)(entryptr->ptr);
	freeblockll_t* temp = ((freeblockll_t *)(mainpage->header));
	int blocksize;

	while (temp != NULL)
	{
		
		blocksize = temp->size; //get a size

		if (blocksize >= size)	//found the first fit
		{
			if (blocksize == size || (blocksize - size) < sizeof(freeblockll_t))
			{
				//perfect fit or not enough space left, remove from list and do not add new entry to reflect a free block
				remove(temp);
				return ((void *) temp);
			}
			//else, free space and make new entry in ll
			addtofreelist((void *) ((long) temp + size), blocksize - size);
			remove(temp);

			return ((void *) temp);			
		}
		temp = temp->next;
	}
	initial(get_page(), 0); 	//didn't find fit, allocate new page, addtofreelist to ll
	mainpage->numpages++;			//update number of pages
	//return new ptr
	return findfirstfit(size);
}

/* addtofreelists pointer to free space */
void addtofreelist(void *ptr, int size)
{
	llheader_t *mainpage;
	mainpage = (llheader_t*)(entryptr->ptr);		//llheader/first page

	void *temp = (void*)(mainpage->header);				//our ll

	llheader_t *temppage;
	freeblockll_t *thisptr;
	long i = ((long) ptr - (long) mainpage) / PAGESIZE;		//how far our page is by num of pages

	temppage = (llheader_t*)((long) mainpage + i * PAGESIZE);		//page we are addtofreelisting free resource to
	thisptr = (freeblockll_t*) ptr;
	(*thisptr).pageid = temppage;

	((freeblockll_t*)ptr)->size = size;		//set size of new block
	((freeblockll_t*)ptr)->prev = NULL;

	//if first entry
	if (temp == ptr)
	{
		((freeblockll_t*)ptr)->next = NULL;
	}

	else if (temp > ptr)		//if new block comes before header
	{
		
		int newblocksize = ((freeblockll_t*) ptr)->size;			//size fo block to be added
		void* nextblock = (void *) ((long) ptr + newblocksize); //pointer to adjacent block of memory
		//check if adjacent block is in list, coalesce blocks if it is
		if ((nextblock == temp) /*&& (((freeblockll_t*) nextblock)->pageid == ((freeblockll_t*)temp)->pageid)*/)
		{
			if (((freeblockll_t*) (mainpage->header))->next == NULL)
			{
				((freeblockll_t* )ptr)->size += ((freeblockll_t*) (mainpage->header))->size;				
				mainpage->header = (freeblockll_t*)ptr;	//set header to new ptr							
			}
			else
			{
				freeblockll_t *newnext = ((freeblockll_t*) (mainpage->header))->next;
				newnext->prev = ptr;
				((freeblockll_t* )ptr)->next = newnext;
				((freeblockll_t* )ptr)->size += ((freeblockll_t*) (mainpage->header))->size;			
				mainpage->header = (freeblockll_t *)ptr;
			}
		}
		else		//add block to list, set as new header
		{
			((freeblockll_t*) (mainpage->header))->prev = (freeblockll_t*)ptr;
			((freeblockll_t*)ptr)->next = ((freeblockll_t*)(mainpage->header));
			mainpage->header = (freeblockll_t*)ptr;	//set header to new ptr
		}
	}
	else 
	{	//else find where to add by searching the list
		while (((freeblockll_t *)temp)->next)
		{
			if (temp > ptr)
				break;
			temp = ((void *)(((freeblockll_t*)temp)->next));
		}

		int blocksize = ((freeblockll_t*) temp)->size;	//size of block in list behind block to be added
		void *nextblock = (void *) ((long) temp + blocksize);	//pointer to adjacent block of memory

		//if new block is next to old block, coalesce adjacent free blocks
		if ((nextblock == ptr) /*&& (((freeblockll_t*)nextblock)->pageid == ((freeblockll_t*)ptr)->pageid)*/)
		{
			((freeblockll_t*) temp)->size = blocksize + ((freeblockll_t*)ptr)->size;


		}
		else
		{
			freeblockll_t *tempnext;
			tempnext = ((freeblockll_t*)temp)->next;
			//add to list
			((freeblockll_t*)temp)->next = ptr;
			((freeblockll_t*)ptr)->next = tempnext;
			((freeblockll_t*)ptr)->prev = temp;
			if (tempnext)
				tempnext->prev = ptr;
		}
	}
}

/* looks for unallocated pages and frees them */
void freeunalloc(void)
{
	llheader_t *mainpage;
	mainpage = (llheader_t*)(entryptr->ptr);

	llheader_t *temppage;
	int i;
	i = mainpage->numpages;	//find the last page, go there first
	int cont = 0;		//keep searching or not
	do
	{
		cont = 0;
		temppage = (((llheader_t*)((long) mainpage + i * PAGESIZE)));		//get last page
		freeblockll_t *temp = mainpage->header;		//get header to list
		freeblockll_t *temp2;											//temp2 holds next entry in list
		if (((llheader_t*)temppage)->numalloc == 0)		//if page has no allocs, first remove all pointers in list to that page
		{
			while (temp != NULL)
			{
				temp2 = temp->next;
				if (temp->pageid == temppage)
					remove(temp);
				temp = temp2;
			}
			cont = 1; //check for additional pages
			if (temppage == mainpage)		//if page is the main page, reset entryptr
			{
				entryptr = 0;
				cont = 0;
			}
			free_page( (*temppage).self); //free page, decrement numpages if applicable
			if (entryptr)
				(mainpage->numpages)--;
			i--;
		}
	} while(cont);

}

/* remove pointer from list */
void remove(void* ptr)
{
	freeblockll_t* temp = (freeblockll_t*)ptr;
	freeblockll_t* tempnext = temp->next;
	freeblockll_t* tempprev = temp->prev;

	//just the header
	if (tempnext == NULL && tempprev == NULL)
	{
		llheader_t* mainpage = (llheader_t*)(entryptr->ptr);
		mainpage->header = NULL;

		entryptr = 0;
		return;
	}
	//remove header from non empty list
	if (tempprev == NULL)
	{
		llheader_t* mainpage = (llheader_t*)(entryptr->ptr);
		tempnext->prev = NULL;
		mainpage->header = tempnext;		//set as new header
		return;
	}

	//tail of list
	if (tempnext == NULL)
	{
		tempprev->next = NULL;
		return;
	}
	//any other part in list
	tempnext->prev = tempprev;
	tempprev->next = tempnext;
	return;
}

#endif // KMA_RM
