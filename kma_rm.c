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
} freeblockL;

//header to ll
typedef struct
{
	/* data */
	void *self;		//copy of lheader pointer, used to free page
	int numpages;
	int numalloc;	//number of allocated blocks per page
	freeblockL *header;	//head of free list
} lheader;

/************Global Variables*********************************************/
kma_page_t *entryptr = 0;		//entry ptr to first page
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
	lheader *pageptr;

	pageptr = (*(freeblockL *) ret).pageid;
	((*pageptr).numalloc)++;		//increment number of allocated blocks on the page
	return ret;
}

void
kma_free(void* ptr, kma_size_t size)
{
	//addtofreelist ptr to list
  addtofreelist(ptr, size);
  //decrement number of allocated blocks on that page
  (((lheader*)(((freeblockL*) ptr)->pageid))->numalloc)--;
	//free a page if it has no allocated blocks
	freeunalloc();
}

/*initialize page */
void initial(kma_page_t *page, int first)
{
	lheader *listheader;
	*((kma_page_t**) page->ptr) = page;	

	listheader = (lheader*) (page->ptr); //set list header to base of page

	//set header to right after initial data structure
	listheader->header = (freeblockL*) ((long) listheader + sizeof(lheader));

	if (first)
		entryptr = page;
	//add the page to the ll
	addtofreelist(((void *) (listheader->header)), (PAGESIZE - sizeof(lheader)));
	(*listheader).numalloc = 0;
	(*listheader).numpages = 0;

	//return page;
}

/* return pointer to free space using first fit */
void *findfirstfit(int size)
{
	if (size < sizeof(freeblockL))
		size = sizeof(freeblockL);	//min size allowed for rm

	lheader *mainpage;
	mainpage = (lheader*)(entryptr->ptr);
	freeblockL* temp = ((freeblockL *)(mainpage->header));
	int blocksize;

	while (temp != NULL)
	{
		
		blocksize = temp->size; //get a size

		if (blocksize >= size)	//found the first fit
		{
			if (blocksize == size || (blocksize - size) < sizeof(freeblockL))
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
	initial(get_page(), 0); 	//didn't find fit, allocate new page, add to ll
	mainpage->numpages++;			//update number of pages
	//return new ptr
	return findfirstfit(size);
}

/* addtofreelists pointer to free space */
void addtofreelist(void *ptr, int size)
{
	lheader *mainpage;
	mainpage = (lheader*)(entryptr->ptr);		//llheader/first page

	void *temp = (void*)(mainpage->header);				//our ll

	lheader *temppage;
	long i = ((long) ptr - (long) mainpage) / PAGESIZE;		//how far our page is by num of pages

	temppage = (lheader*)((long) mainpage + i * PAGESIZE);		//page we are addtofreelisting free resource to
	(*(freeblockL*) ptr).pageid = temppage;


	((freeblockL*)ptr)->size = size;		//set size of new block
	((freeblockL*)ptr)->prev = NULL;

	//if first entry
	if (temp == ptr)
	{
		((freeblockL*)ptr)->next = NULL;
	}
	else if (temp > ptr)		//if new block comes before header
	{
		
		int newblocksize = ((freeblockL*) ptr)->size;			//size fo block to be added
		void* nextblock = (void *) ((long) ptr + newblocksize); //pointer to adjacent block of memory
		//check if adjacent block is in list, coalesce blocks if it is
		if ((nextblock == temp) && (((freeblockL*) nextblock)->pageid == ((freeblockL*)temp)->pageid))
		{
			if (((freeblockL*) (mainpage->header))->next == NULL)
			{
				((freeblockL* )ptr)->size += ((freeblockL*) (mainpage->header))->size;		
				((freeblockL* )ptr)->next = NULL;						
				mainpage->header = (freeblockL*)ptr;	//set header to new ptr							
			}
			else
			{
				freeblockL *newnext = ((freeblockL*) (mainpage->header))->next;
				newnext->prev = ptr;
				((freeblockL* )ptr)->next = newnext;
				((freeblockL* )ptr)->size += ((freeblockL*) (mainpage->header))->size;			
				mainpage->header = (freeblockL *)ptr;
			}
		}
		else		//add block to list, set as new header
		{
			((freeblockL*) (mainpage->header))->prev = (freeblockL*)ptr;
			((freeblockL*)ptr)->next = ((freeblockL*)(mainpage->header));
			mainpage->header = (freeblockL*)ptr;	//set header to new ptr
		}
	}
	else 
	{	//else find where to add by searching the list
		while (((freeblockL *)temp)->next)
		{
			if (temp > ptr)
				break;
			temp = ((void *)(((freeblockL*)temp)->next));
		}

		int blocksize = ((freeblockL*) temp)->size;	//size of block in list behind block to be added
		void *nextblock = (void *) ((long) temp + blocksize);	//pointer to adjacent block of memory

		//if new block is next to old block, coalesce adjacent free blocks
		if ((nextblock == ptr) && (((freeblockL*)nextblock)->pageid == ((freeblockL*)ptr)->pageid))
		{
			((freeblockL*) temp)->size = blocksize + ((freeblockL*)ptr)->size;


		}
		else
		{
			freeblockL *tempnext;
			tempnext = ((freeblockL*)temp)->next;
			//add to list
			((freeblockL*)temp)->next = ptr;
			((freeblockL*)ptr)->next = tempnext;
			((freeblockL*)ptr)->prev = temp;
			if (tempnext)
				tempnext->prev = ptr;
		}
	}
}

/* looks for unallocated pages and frees them */
void freeunalloc(void)
{
	lheader *mainpage;
	mainpage = (lheader*)(entryptr->ptr);

	lheader *temppage;
	int i;
	i = mainpage->numpages;	//find the last page, go there first
	int cont = 0;		//keep searching or not
	do
	{
		cont = 0;
		temppage = (((lheader*)((long) mainpage + i * PAGESIZE)));		//get last page
		freeblockL *temp = mainpage->header;		//get header to list
		freeblockL *temp2;											//temp2 holds next entry in list
		if (((lheader*)temppage)->numalloc == 0)		//if page has no allocs, first remove all pointers in list to that page
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
	freeblockL* temp = (freeblockL*)ptr;
	freeblockL* tempnext = temp->next;
	freeblockL* tempprev = temp->prev;

	//just the header
	if (tempnext == NULL && tempprev == NULL)
	{
		lheader* mainpage = (lheader*)(entryptr->ptr);
		mainpage->header = NULL;

		entryptr = 0;
		return;
	}
	//remove header from non empty list
	if (tempprev == NULL)
	{
		lheader* mainpage = (lheader*)(entryptr->ptr);
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
