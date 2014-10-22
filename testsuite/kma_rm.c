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
 *    - added size as a parameter to kma_free
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

typedef struct
{
	/* data */
	int size;
	void *next;
	void *prev;
	void *whichpage;
} freelist_t;

typedef struct
{
	/* data */
	void *itself;
	int numpages;
	int numalloc;	//number of allocated blocks per page
	freelist_t *header;	//head of free list
} listheader_t;

/************Global Variables*********************************************/
kma_page_t *entryptr = 0;		//entry ptr
/************Function Prototypes******************************************/
void *findfirstfit(int size);		//return pointer to free space using first fit
void add (void* ptr, int size);	//adds pointer to free space 
void resolve(void);	//looks for pages being used with no allocated blocks and frees those pages
void remove(void *ptr);	//remove pointer from list
kma_page_t *initial(kma_page_t* page, int first);	//initialize page

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
	listheader_t *thispage;

	thispage = (*(freelist_t *) ret).whichpage;
	((*thispage).numalloc)++;		//increment number of allocated blocks on the page
	return ret;
}

void
kma_free(void* ptr, kma_size_t size)
{
	//add ptr to list
  add(ptr, size);
  //decrement number of allocated blocks on that page
  (((listheader_t*)(((freelist_t*) ptr)->whichpage))->numalloc)--;
	//free a page if it has no allocated blocks
	resolve();
}

/*initialize page */
kma_page_t* initial(kma_page_t *page, int first)
{
	listheader_t *listheader;
	*((kma_page_t**) page->ptr) = page;	

	listheader = (listheader_t*) (page->ptr); //set list header to base of page

	//set header to right after initial data structure
	listheader->header = (freelist_t*) ((long int) listheader + sizeof(listheader_t));

	if (first)
		entryptr = page;
	//add the page to the ll
	add(((void *) (listheader->header)), (PAGESIZE - sizeof(listheader_t)));
	(*listheader).numalloc = 0;
	(*listheader).numpages = 0;

	return page;
}

/* return pointer to free space using first fit */
void *findfirstfit(int size)
{
	if (size < sizeof(freelist_t))
		size = sizeof(freelist_t);	//min size allowed for rm

	listheader_t *mainpage;
	mainpage = (listheader_t*)(entryptr->ptr);
	freelist_t* temp = ((freelist_t *)(mainpage->header));
	int blocksize;

	while (temp != NULL)
	{
		
		blocksize = temp->size; //get a size

		if (blocksize >= size)	//found the first fit
		{
			if (blocksize == size || (blocksize - size) < sizeof(freelist_t))
			{
				//perfect fit or not enough space left, remove from list
				remove(temp);
				return ((void *) temp);
			}
			//else, free space and make new entry in ll
			add((void *) ((long int) temp + size), blocksize - size);
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

/* adds pointer to free space */
void add(void *ptr, int size)
{
	listheader_t *mainpage;
	mainpage = (listheader_t*)(entryptr->ptr);		//our ll

	void *temp = (void*)(mainpage->header);				//header

	listheader_t *temppage;
	freelist_t *thisptr;
	long int i = ((long int) ptr - (long int) mainpage) / PAGESIZE;		//how far our page is by num of pages

	temppage = (listheader_t*)((long int) mainpage + i * PAGESIZE);		//page we are adding free resource to
	thisptr = (freelist_t*) ptr;
	(*thisptr).whichpage = temppage;

	((freelist_t*)ptr)->size = size;
	((freelist_t*)ptr)->prev = NULL;

	//if first entry
	if (temp == ptr)
	{
		((freelist_t*)ptr)->next = NULL;
		return;
	}

	//add any others
	if (temp > ptr)		//if new memory comes before header
	{
		((freelist_t*) (mainpage->header))->prev = (freelist_t*)ptr;
		((freelist_t*)ptr)->next = ((freelist_t*)(mainpage->header));
		mainpage->header = (freelist_t*)ptr;	//set header to new ptr
		return;
	}
	//else find where to add by searching the list
	freelist_t *tempnext;
	while (((freelist_t *)temp)->next)
	{
		if (temp > ptr)
			break;
		temp = ((void *)(((freelist_t*)temp)->next));
	}
	tempnext = ((freelist_t*)temp)->next;
	//add to list
	((freelist_t*)temp)->next = ptr;
	((freelist_t*)ptr)->next = tempnext;
	((freelist_t*)ptr)->prev = temp;
	if (tempnext)
		tempnext->prev = ptr;

}

/* resolves list, looks for unallocated pages and frees them */
void resolve(void)
{
	listheader_t *mainpage;
	mainpage = (listheader_t*)(entryptr->ptr);

	listheader_t *temppage;
	int i;
	i = mainpage->numpages;	//find the last page, go there first
	int cont = 0;		//keep searching or not
	do
	{
		cont = 0;
		temppage = (((listheader_t*)((long int) mainpage + i * PAGESIZE)));		//get last page
		freelist_t *temp = mainpage->header;		//get header to list
		freelist_t *temp2;											//temp2 holds next entry in list
		if (((listheader_t*)temppage)->numalloc == 0)		//if page has no allocs, first remove all pointers in list to that page
		{
			while (temp != NULL)
			{
				temp2 = temp->next;
				if (temp->whichpage == temppage)
					remove(temp);
				temp = temp2;
			}
			cont = 1; //check for additional pages
			if (temppage == mainpage)		//if page is the main page, reset entryptr
			{
				entryptr = 0;
				cont = 0;
			}
			free_page((*temppage).itself); //free page, decrement numpages if applicable
			if (entryptr)
				(mainpage->numpages)--;
			i--;
		}
	} while(cont);

}

/* remove pointer from list */
void remove(void* ptr)
{
	freelist_t* temp = (freelist_t*)ptr;
	freelist_t* tempnext = temp->next;
	freelist_t* tempprev = temp->prev;

	//just the header
	if (tempnext == NULL && tempprev == NULL)
	{
		listheader_t* mainpage = (listheader_t*)(entryptr->ptr);
		mainpage->header = NULL;

		entryptr = 0;
		return;
	}
	//remove header from non empty list
	if (tempprev == NULL)
	{
		listheader_t* mainpage = (listheader_t*)(entryptr->ptr);
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