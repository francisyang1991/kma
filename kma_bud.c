/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.2 $
 *    Last Modification: $Date: 2009/10/31 21:28:52 $
 *    File: $RCSfile: kma_bud.c,v $
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: kma_bud.c,v $
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
#ifdef KMA_BUD
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

#define PAGENUM 91

typedef struct
{
	void* nextbuffer;
} bufferNode_t;

typedef struct
{
	int 		size;
	bufferNode_t* 	buffer;//point to the available one or nothing
} headerList_t;

typedef struct
{
	kma_page_t*		ptr;// the origin res return by get page()
	void*			addr;//the ptr.ptr, the start addr of the page
	unsigned char	bitmap[64];// bit map, shows that the resource 
	int				numalloc;
} kpageheader_t;

typedef struct
{
	kma_page_t*		self;
	int				numpages;// the number of page 
	int				numalloc;// 0 means nothing//each page hold one 
	headerList_t	freelist[10];
	kpageheader_t	page[PAGENUM];
	void*	nextPage;
} pageList_t;

/************Global Variables*********************************************/

pageList_t* gEntry=0;

/************Function Prototypes******************************************/

pageList_t* initial_mainheader(kma_page_t* newpage);
void initial_pageheader(kpageheader_t* pageheader, kma_page_t* newpage);
kma_size_t roundUp(kma_size_t size);
int findFreeList(kma_size_t size);
kpageheader_t* chkfreepage();
headerList_t* splitBuffer(headerList_t* bud_list, kma_size_t bud_size);
headerList_t* combi_bud(headerList_t* bud_list, kpageheader_t* bud_page);
void insertbuffer(headerList_t* thefreelist, bufferNode_t* thebuffer);
bufferNode_t* deleteTheFirstBufferFromFreelist(headerList_t* thefreelist);
bufferNode_t* deleteBufferByNode(headerList_t* thefreelist, bufferNode_t* thebufaddr);
void fillbitmap(kpageheader_t* pageheader, void* bufferptr, kma_size_t roundsize);
void emptybitmap(kpageheader_t* pageheader, void* bufferptr, kma_size_t roundsize);
	
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
	if ((size + sizeof(void*)) > PAGESIZE){ // requested size too large
		return NULL;
	}
	if(!gEntry){// initialized the entry
		gEntry=initial_mainheader(get_page());
	}
	
	int roundsize=roundUp(size);
	int i;
	void* ret;

	// if there is not enough page, we create one, the the freelist will be available
	
	if((i=findFreeList(size))){
		i--;
		headerList_t* thelist;
		kpageheader_t* thepage=0;
		
		thelist = splitBuffer(&((*gEntry).freelist[i]), roundsize);
		ret = deleteTheFirstBufferFromFreelist(thelist);
		
		void* theaddr;
		theaddr=(void*)(((long int)(((long int)ret-(long int)gEntry)/PAGESIZE))*PAGESIZE+(long int)gEntry);
		pageList_t* temppage=gEntry;
		// find the page header
		while(!thepage){
			for(i = 0; i < PAGENUM; ++i)
			{
				if((*temppage).page[i].addr==theaddr){
					thepage=&((*temppage).page[i]);
					break;
				}
			}
			if((*temppage).nextPage==0)break;// it should find the page
			if(thepage)break;
			temppage=(*temppage).nextPage;
		}
		fillbitmap(thepage, ret, roundsize);

		(*gEntry).numalloc++;
		(*thepage).numalloc++;
		return (void*)ret;		
	}
	else{
		kpageheader_t* newpage=chkfreepage();// so we have the newpage. and it is available it freelist[9]
		headerList_t* thelist;
		
		thelist=splitBuffer(&((*gEntry).freelist[9]), roundsize);
		ret=deleteTheFirstBufferFromFreelist(thelist);
		fillbitmap(newpage, ret, roundsize);

		(*gEntry).numalloc++;
		(*newpage).numalloc++;
		return (void*)ret;
	}
  return NULL;
}

void 
kma_free(void* ptr, kma_size_t size)
{
	int roundsize=roundUp(size);
	
	// find its page and its header
	kpageheader_t* thepage=0;
	headerList_t* thelist=0;
	void* theaddr;
	int i;
	theaddr=(void*)(((long int)(((long int)ptr-(long int)gEntry)/PAGESIZE))*PAGESIZE+(long int)gEntry);
	pageList_t* temppage=gEntry;
	pageList_t* previouspage=0;
	// find the page header
	while(!thepage){
		for(i = 0; i < PAGENUM; ++i)
		{
			if((*temppage).page[i].addr==theaddr){
				thepage=&((*temppage).page[i]);
				break;
			}
		}
		if((*temppage).nextPage==0)break;// it should find the page
		if(thepage)break;
		previouspage=temppage;
		temppage=(*temppage).nextPage;
	}
	// find the header
	for(i = 0; i < 10; ++i)
	{
		if((*gEntry).freelist[i].size==roundsize){
			break;
		}
	}
	thelist=&((*gEntry).freelist[i]);
	insertbuffer(thelist,ptr);
	emptybitmap(thepage, ptr, roundsize);
	(*gEntry).numalloc--;
	(*thepage).numalloc--;
	
	
	headerList_t* otherlist=combi_bud(thelist, thepage);
//	if((*otherlist).size==8192){//the page is empty
	if((*thepage).numalloc==0){
		deleteTheFirstBufferFromFreelist(otherlist);
		free_page((*thepage).ptr);
		(*thepage).ptr=0;
		(*thepage).addr=0;
		(*gEntry).numpages--;
		(*temppage).numpages--;
	}
	if(((*temppage).numpages==0)&&(previouspage!=0))
	{
		(*previouspage).nextPage=(*temppage).nextPage;
		free_page((*temppage).self);
	}
	if((*gEntry).numpages==0)
	{
		free_page((*gEntry).self);
		gEntry=0;
	}
	
}

pageList_t* initial_mainheader(kma_page_t* newpage){
	pageList_t* ret;
	
	ret=(pageList_t*)(newpage->ptr);
	(*ret).self=newpage;
	(*ret).numpages=0;
	(*ret).numalloc=0;
	(*ret).nextPage=0;
	int i;
	for(i = 0; i < 10; ++i)
	{
		(*ret).freelist[i].size=16<<i;
		(*ret).freelist[i].buffer=0;
	}
	for(i = 0; i < PAGENUM; ++i)
	{
		(*ret).page[i].ptr=0;
		(*ret).page[i].addr=0;
	}
	return ret;
}

void initial_pageheader(kpageheader_t* pageheader, kma_page_t* newpage){
	int j;
	(*pageheader).ptr=newpage;
	(*pageheader).addr=(void*)(newpage->ptr);
	(*pageheader).numalloc=0;
	for(j = 0; j < 64; ++j)
	{
		(*pageheader).bitmap[j]=0;
	}
	// add the whole page to free list
	*((bufferNode_t**)((*pageheader).addr))=0;
	insertbuffer(&((*gEntry).freelist[9]), (bufferNode_t*)((*pageheader).addr));
}

kma_size_t roundUp(kma_size_t size){
	kma_size_t ret=16;
	while(size>ret){
		ret=ret<<1;
	}
	return ret;
}

int findFreeList(kma_size_t size){

	int i;
	int roundsize=roundUp(size);
	for(i = 0; i < 10; ++i)
	{
		if((*gEntry).freelist[i].size>=roundsize){
			if((*gEntry).freelist[i].buffer!=0)return i+1;
		}
	}
	return 0;
}

kpageheader_t* chkfreepage(){
	kpageheader_t* ret=0;
	pageList_t* temppage=gEntry;
	int i;
	
	// find the available page header
	while(!ret){
		for(i = 0; i < PAGENUM; ++i)
		{
			if((*temppage).page[i].ptr==0){
				ret=&((*temppage).page[i]);
				break;
			}
		}
		if((*temppage).nextPage==0)break;
		if(ret)break;
		temppage=(*temppage).nextPage;
	}
	// if finding a page, then init it
	if(ret!=0)
	{
		initial_pageheader(ret, get_page());
		(*gEntry).numpages++;
		(*temppage).numpages++;
	}
	// If there is no page yet, then create the header and page.
	else{
		(*temppage).nextPage=initial_mainheader(get_page());
		temppage=(*temppage).nextPage;
		ret=&((*temppage).page[0]);
		initial_pageheader(ret, get_page());
		(*gEntry).numpages++;
		(*temppage).numpages++;
	}
	
	return ret;
}

//create the bitmap for storing the free states of buddy lists.
void fillbitmap(kpageheader_t* pageheader, void* bufferptr, kma_size_t roundsize){
	unsigned char* bitmap=(*pageheader).bitmap;
	int i, offset, endbit;
	offset=(int)(bufferptr-(*pageheader).addr);
	endbit = offset + roundsize;
	offset /= 16;
	endbit /= 16;
	
	for( i = offset; i < endbit; ++i)
	{
		bitmap[i/8] |= (1<<(i%8));
	}
}

// clear the bitmap
void emptybitmap(kpageheader_t* pageheader, void* bufferptr, kma_size_t roundsize){
	unsigned char* bitmap=(*pageheader).bitmap;
	int i, offset, endbit;
	offset=(int)(bufferptr-(*pageheader).addr);
	endbit = offset + roundsize;
	offset /= 16;
	endbit /= 16;
	for( i = offset; i < endbit; ++i)
	{
		bitmap[i/8] &= (~(1<<(i%8)));
	}
}

// Split buffer into proper size for every request
headerList_t* splitBuffer(headerList_t* bud_list, kma_size_t bud_size){

	if(bud_size==((*bud_list).size))return bud_list;
	
	headerList_t* ret;
	ret=(headerList_t*)((long int)bud_list-sizeof(headerList_t));
	
	bufferNode_t* tempbuffer;
	bufferNode_t* tempbuffer0;
	bufferNode_t* tempbuffer1;	
	// dealing with the large block free list
	tempbuffer=deleteTheFirstBufferFromFreelist(bud_list);
	// dealing with the small block free list
	tempbuffer0=tempbuffer;
	tempbuffer1=(bufferNode_t*)((long int)tempbuffer0 + (*ret).size);
	insertbuffer(ret, tempbuffer1);
	insertbuffer(ret, tempbuffer0);

	if(bud_size < (*ret).size)
		ret=splitBuffer(ret, bud_size);
	return ret;
}

//merge the two buddy node if they are both free
headerList_t* combi_bud(headerList_t* bud_list, kpageheader_t* bud_page){
	if(8192==((*bud_list).size))return bud_list;
	
	headerList_t* ret;
	ret=(headerList_t*)((long int)bud_list + sizeof(headerList_t));
	unsigned char* bitmap=(*bud_page).bitmap;
	kma_size_t bud_size = (*bud_list).size;
	
	int i, offset, endbit;
	
	bufferNode_t* tempbuffer0;
	bufferNode_t* tempbuffer1;
	offset=(int)((void*)((*bud_list).buffer)-(void*)((*bud_page).addr));
	
	if(offset%(bud_size*2)==0){
	// the next one is the buddy
		tempbuffer0=(*bud_list).buffer;
		tempbuffer1=(bufferNode_t*)((void*)((*bud_list).buffer) + bud_size);
		
		offset += bud_size;
		endbit = offset + bud_size;
		offset /= 16;
		endbit /= 16;

		for( i = offset; i < endbit; ++i)
		{
			if(bitmap[i/8] & (1<<(i%8))){
				return bud_list;//it is not free
			}
		}
		tempbuffer1=deleteBufferByNode(bud_list,tempbuffer1);
		tempbuffer0=deleteTheFirstBufferFromFreelist(bud_list);
	}
	else{
	// the pevirous one it the buddy
	
		tempbuffer0=(bufferNode_t*)((void*)((*bud_list).buffer) - bud_size);
		tempbuffer1=(*bud_list).buffer;
		
		offset -= bud_size;
		endbit = offset + bud_size;
		offset /= 16;
		endbit /= 16;

		for( i = offset; i < endbit; ++i)
		{
			if(bitmap[i/8] & (1<<(i%8))){
				return bud_list;//it is not free
			}
		}
		tempbuffer0=deleteBufferByNode(bud_list,tempbuffer0);
		tempbuffer1=deleteTheFirstBufferFromFreelist(bud_list);
	}
	// now the buddy is free, we need to combine them
	//bufferNode_t* tempbuffer;
	
	insertbuffer(ret, tempbuffer0);

	if(bud_size < 8192)ret=combi_bud(ret, bud_page);
	return ret;
}

// insert the free buffer to the list it belonging to
void insertbuffer(headerList_t* thefreelist, bufferNode_t* currentBuffer){
	bufferNode_t* temp;
	temp=(*thefreelist).buffer;
	(*thefreelist).buffer=currentBuffer;
	(*currentBuffer).nextbuffer=temp;
}

// just delete the very first free buffer in the free list
bufferNode_t* deleteTheFirstBufferFromFreelist(headerList_t* thefreelist){
	bufferNode_t* ret;
	ret=(*thefreelist).buffer;
	(*thefreelist).buffer=(*ret).nextbuffer;
	return (bufferNode_t*)ret;
}

// delete the buffer for a certain address
bufferNode_t* deleteBufferByNode(headerList_t* thefreelist, bufferNode_t* node){
	bufferNode_t* ret = 0;
	void* tmp;
	tmp=(*thefreelist).buffer;
	while(tmp){
		if((*(bufferNode_t*)tmp).nextbuffer==node){// we find it!
			ret=node;
			(*(bufferNode_t*)tmp).nextbuffer=(*ret).nextbuffer;
			return ret;
		}
		tmp=(*(bufferNode_t*)tmp).nextbuffer;
	}
	return 0; // it should be free
}



#endif // KMA_BUD
