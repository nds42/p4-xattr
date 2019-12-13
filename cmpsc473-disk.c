
/**********************************************************************

   File          : cmpsc473-disk.c

   Description   : File system function implementations
                   (see .h for applications)

***********************************************************************/
/**********************************************************************
Copyright (c) 2019 The Pennsylvania State University
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of The Pennsylvania State University nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

/* Include Files */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Project Include Files */
#include "cmpsc473-filesys.h"
#include "cmpsc473-disk.h"
#include "cmpsc473-list.h"
#include "cmpsc473-util.h"

/* Definitions */

/* program variables */

/* Functions */


/**********************************************************************

    Function    : diskDirInitialize
    Description : Initialize the root directory on disk
    Inputs      : directory reference
    Outputs     : 0 if success, -1 on error

***********************************************************************/

int diskDirInitialize( ddir_t *ddir )
{
	/* Local variables */
	dblock_t *first_dentry_block;
	int i;
	ddh_t *ddh; 

	/* clear disk directory object */
	memset( ddir, 0, FS_BLOCKSIZE );

	/* initialize disk directory fields */
	ddir->buckets = ( FS_BLOCKSIZE - sizeof(ddir_t) ) / (sizeof(ddh_t));
	ddir->freeblk = FS_METADATA_BLOCKS+1;    /* fs+super+directory */
	ddir->free = 0;                          /* dentry offset in that block */

	/* assign first dentry block - for directory itself */
	first_dentry_block = (dblock_t *)disk2addr( fs->base, 
						    (( FS_METADATA_BLOCKS+1 ) 
						     * FS_BLOCKSIZE ));
	memset( first_dentry_block, 0, FS_BLOCKSIZE );
	first_dentry_block->free = DENTRY_BLOCK;
	first_dentry_block->st.dentry_map = DENTRY_MAP;
	first_dentry_block->next = BLK_INVALID;

	/* initialize ddir hash table */
	ddh = (ddh_t *)ddir->data;     /* start of hash table data -- in ddh_t's */
	for ( i = 0; i < ddir->buckets; i++ ) {
		(ddh+i)->next_dentry = BLK_SHORT_INVALID;
		(ddh+i)->next_slot = BLK_SHORT_INVALID;
	}

	return 0;  
}


/**********************************************************************

    Function    : diskReadDir
    Description : Retrieve the on-disk directory -- only one in this case
    Inputs      : name - name of the file 
                  name_size - length of name
    Outputs     : on-disk directory or -1 if error

***********************************************************************/

ddir_t *diskReadDir( char *name, unsigned int name_size ) 
{
	return ((ddir_t *)block2addr( fs->base, dfs->root ));
}


/**********************************************************************

    Function    : diskFindDentry
    Description : Retrieve the on-disk dentry from the disk directory
    Inputs      : diskdir - on-disk directory
                  name - name of the file 
                  name_size - length of file name
    Outputs     : on-disk dentry or NULL if error

***********************************************************************/

ddentry_t *diskFindDentry( ddir_t *diskdir, char *name, unsigned int name_size ) 
{
	int key = fsMakeKey( name, diskdir->buckets, name_size );
	ddh_t *ddh = (ddh_t *)&diskdir->data[key];

	// find block in cache?  if not get from disk and put in cache

	while (( ddh->next_dentry != BLK_SHORT_INVALID ) || ( ddh->next_slot != BLK_SHORT_INVALID )) {
		dblock_t *dblk = (dblock_t *)disk2addr( fs->base, (block2offset( ddh->next_dentry )));
		ddentry_t *disk_dentry = (ddentry_t *)disk2addr( dblk, dentry2offset( ddh->next_slot ));
    
		if (( disk_dentry->name_size == name_size ) && 
		    ( strncmp( disk_dentry->name, name, disk_dentry->name_size ) == 0 )) {
			return disk_dentry;
		}

		ddh = &disk_dentry->next;
	}

	return (ddentry_t *)NULL;  
}


/**********************************************************************

    Function    : diskFindFile
    Description : Retrieve the on-disk file from the on-disk dentry
    Inputs      : disk_dentry - on-disk dentry
    Outputs     : on-disk file control block or NULL if error

***********************************************************************/

fcb_t *diskFindFile( ddentry_t *disk_dentry ) 
{
	if ( disk_dentry->block != BLK_INVALID ) {
		dblock_t *blk =  (dblock_t *)disk2addr( fs->base, (block2offset( disk_dentry->block )));
		return (fcb_t *)disk2addr( blk, sizeof(dblock_t) );
	}

	errorMessage("diskFindFile: no such file");
	printf("\nfile name = %s\n", disk_dentry->name);
	return (fcb_t *)NULL;  
}


/**********************************************************************

    Function    : diskCreateDentry
    Description : Create disk entry for the dentry on directory
    Inputs      : base - ptr to base of file system on disk
                  dir - in-memory directory
                  dentry - in-memory dentry
    Outputs     : none

***********************************************************************/

void diskCreateDentry( unsigned int base, dir_t *dir, dentry_t *dentry ) 
{
	ddir_t *diskdir = dir->diskdir;
	ddentry_t *disk_dentry;
	dblock_t *dblk, *nextblk;
	ddh_t *ddh;
	int empty = 0;
	int key;

	// create buffer cache for blocks retrieved from disk - not mmapped

	/* find location for new on-disk dentry */
	dblk = (dblock_t *)disk2addr( base, (block2offset( diskdir->freeblk )));
	disk_dentry = (ddentry_t *)disk2addr( dblk, dentry2offset( diskdir->free ));

	/* associate dentry with ddentry */
	dentry->diskdentry = disk_dentry;  
  
	/* update disk dentry with dentry's data */
	memcpy( disk_dentry->name, dentry->name, dentry->name_size );  // check bounds in dentry
	disk_dentry->name[dentry->name_size] = 0;   // null terminate
	disk_dentry->name_size = dentry->name_size;
 	disk_dentry->block = BLK_INVALID;

	/* push disk dentry into on-disk hashtable */
	key = fsMakeKey( disk_dentry->name, diskdir->buckets, disk_dentry->name_size );
	ddh = diskDirBucket( diskdir, key );
	/* at diskdir's hashtable bucket "key", make this disk_dentry the next head
	   and link to the previous head */
	disk_dentry->next.next_dentry = ddh->next_dentry;   
	disk_dentry->next.next_slot = ddh->next_slot;       
	ddh->next_dentry = diskdir->freeblk;
	ddh->next_slot = diskdir->free;

	/* set this disk_dentry as no longer free in the block */
	clearbit( dblk->st.dentry_map, diskdir->free, DENTRY_MAX );   

	/* update free reference for dir */
	/* first the block, if all dentry space has been consumed */
	if ( dblk->st.dentry_map == 0 ) { /* no more space for dentries here */
		/* need another directory block for disk dentries */
		/* try "next" block first until no more */
		unsigned int next_index = dblk->next;
		while ( next_index != BLK_INVALID ) {
			nextblk = (dblock_t *)disk2addr( base, block2offset( next_index ));
			if ( nextblk->st.dentry_map != 0 ) {
				diskdir->freeblk = next_index;
				dblk = nextblk;
				goto done;
			}
			next_index = nextblk->next;
		}
		
		/* get next file system free block for next dentry block */
		diskdir->freeblk = dfs->firstfree;
      
		/* update file system's free blocks */
		nextblk = (dblock_t *)disk2addr( base, block2offset( dfs->firstfree ));
		dfs->firstfree = nextblk->next;
		nextblk->free = DENTRY_BLOCK;   /* this is now a dentry block */
		nextblk->st.dentry_map = DENTRY_MAP;
		nextblk->next = BLK_INVALID;
		dblk = nextblk;
	}

done:
	/* now update the free entry slot in the block */
	/* find the empty dentry slot */
	empty = findbit( dblk->st.dentry_map, DENTRY_MAX );
	diskdir->free = empty;

	if (empty == BLK_INVALID ) {
		errorMessage("diskCreateDentry: bad bitmap");
		return;
	}      
}


/**********************************************************************

    Function    : diskCreateFile
    Description : Create file block for the new file
    Inputs      : base - ptr to base of file system on disk
                  dentry - in-memory dentry
                  file - in-memory file
    Outputs     : 0 on success, <0 on error 

***********************************************************************/

int diskCreateFile( unsigned int base, dentry_t *dentry, file_t *file )
{
	dblock_t *fblk;
	fcb_t *fcb;
	ddentry_t *disk_dentry;
	int i;
	unsigned int block;

	allocDblock( &block, FILE_BLOCK );

	if ( block == BLK_INVALID ) {
		return -1;
	}  
  
	/* find a file block in file system */
	fblk = (dblock_t *)disk2addr( base, (block2offset( block )));
	fcb = (fcb_t *)disk2addr( fblk, sizeof( dblock_t ));   /* file is offset from block info */

	/* associate file with the on-disk file */
	file->diskfile = fcb;

	// P3 - metadata 
	/* set file data into file block */
	fcb->flags = file->flags;
	/* XXX initialize attributes */
	fcb->attr_block = BLK_INVALID;    /* no block yet */

	/* initial on-disk block information for file */  
	for ( i = 0; i < FILE_BLOCKS; i++ ) {
		fcb->blocks[i] = BLK_INVALID;   /* initialize to empty */
	}

	/* get on-disk dentry */
	disk_dentry = dentry->diskdentry;

	/* set file block in on-disk dentry */
	disk_dentry->block = block;

	return 0;
}


/**********************************************************************

    Function    : diskWrite
    Description : Write the buffer to the disk
		  TODO Need a good explanation of the difference between disk_offset and offset
    Inputs      : disk_offset - pointer to place where offset is stored on disk
                  block - index to block to be written
                  buf - data to be written
                  bytes - the number of bytes to write
		  TODO What is the offset for? I thought we always wrote to the end of the file in a log-based filesystem
                  offset - offset from start of file
                  sofar - bytes written so far
    Outputs     : number of bytes written or -1 on error 

***********************************************************************/

unsigned int diskWrite( unsigned int *disk_offset, unsigned int block, 
			char *buf, unsigned int bytes, 
			unsigned int offset, unsigned int sofar )
{
	dblock_t *dblk;
	char *start, *end, *data;
	int block_bytes;
	unsigned int blk_offset = offset % FS_BLKDATA;

	/* compute the block addresses and range */
	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( block )));
	data = (char *)disk2addr( dblk, sizeof(dblock_t) );
	start = (char *)disk2addr( data, blk_offset );
	end = (char *)disk2addr( fs->base, (block2offset( (block+1) )));
	block_bytes = min(( end - start ), ( bytes - sofar ));

	/* do the write */
	memcpy( start, buf, block_bytes );
  
	/* compute new offset, and update in fcb if end is extended */
	offset += block_bytes;
  
	if ( offset > *disk_offset ) {
		*disk_offset = offset;
	}

	return block_bytes;  
}


/**********************************************************************

    Function    : diskRead
    Description : read the buffer from the disk
    Inputs      : block - index to file block to read
                  buf - buffer for data
                  bytes - the number of bytes to read
                  offset - offset from start of file
                  sofar - bytes read so far 
    Outputs     : number of bytes read or -1 on error 

***********************************************************************/

unsigned int diskRead( unsigned int block, char *buf, unsigned int bytes, 
		       unsigned int offset, unsigned int sofar )
{
	dblock_t *dblk;
	char *start, *end, *data;
	int block_bytes;
	unsigned int blk_offset = offset % FS_BLKDATA;

	/* compute the block addresses and range */
	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( block )));
	data = (char *)disk2addr( dblk, sizeof(dblock_t) );
	start = (char *)disk2addr( data, blk_offset );
	end = (char *)disk2addr( fs->base, (block2offset( (block+1) )));
	block_bytes = min(( end - start ), ( bytes - sofar ));

	/* do the read */
	memcpy( buf, start, block_bytes );

	return block_bytes;  
}


/**********************************************************************

    Function    : diskGetBlock
    Description : Get the block corresponding to this file location
    Inputs      : file - in-memory file pointer
                  index - block index in file
    Outputs     : block index or BLK_INVALID

***********************************************************************/

unsigned int diskGetBlock( file_t *file, unsigned int index )
{
	fcb_t *fcb = file->diskfile;
	unsigned int dblk_index;

	if ( fcb == NULL ) {
		errorMessage("diskGetBlock: No file control block for file");
		return BLK_INVALID;
	}

	/* if the index is already in the file control block, then return that */
	dblk_index = fcb->blocks[index]; 
 
	if ( dblk_index != BLK_INVALID ) {
		return dblk_index;
	}

	allocDblock( &dblk_index, FILE_DATA );

	if ( dblk_index == BLK_INVALID ) {
		return BLK_INVALID;
	}

	// P3: Meta-Data 
	/* update the fcb with the new block */
	fcb->blocks[index] = dblk_index;

	return dblk_index;
}


/**********************************************************************

    Function    : allocDblock
    Description : Get a free data block
    Inputs      : index - index for the block found or BLK_INVALID
                  blk_type - the type of use for the block
    Outputs     : 0 on success, <0 on error                  

***********************************************************************/

int allocDblock( unsigned int *index, unsigned int blk_type ) 
{
	dblock_t *dblk;

	/* if there is no free block, just return */
	if ( dfs->firstfree == BLK_INVALID ) {
		*index = BLK_INVALID;
		return BLK_INVALID;
	}

	/* get from file system's free list */
	*index = dfs->firstfree;

	/* update the filesystem's next free block */
	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( *index )));

	/* mark block as a file block */
	dblk->free = blk_type;

	/* update next freeblock in file system */
	// P3 - metadata below
	dfs->firstfree = dblk->next;

	return 0;
}
    


/* TASK **: Add the disk level implementation of set attributed and get attribute */

/**********************************************************************

    Function    : diskGetAttrBlock
    Description : Get the block for the file's attributes
    Inputs      : file - in-memory file pointer
                  flag - create block (BLOCK_CREATE) or not (BLOCK_PRESENT)
    Outputs     : block index or BLK_INVALID

***********************************************************************/

unsigned int diskGetAttrBlock( file_t *file, unsigned int flags )
{
	fcb_t *fcb = file->diskfile;
	unsigned int dblk_index;
	dblock_t *dblk;
	xcb_t *xcb;
	int i;

	if ( fcb == NULL ) {
		errorMessage("diskGetAttrBlock: No file control block for file");
		return BLK_INVALID;
	}

	/* if the attr_block is already in the file control block, then return that */
	dblk_index = fcb->attr_block; 
	if ( dblk_index != BLK_INVALID ) 
		return dblk_index;

	/* if not assigned, flags say block must already be present (BLOCK_PRESENT),
	   then return BLK_INVALID */
	if ( flags == BLOCK_PRESENT ) 
		return BLK_INVALID;  

	/* if not, create a new block if allowed (BLOCK_CREATE) */
	allocDblock( &dblk_index, ATTR_BLOCK );

	if ( dblk_index == BLK_INVALID ) {
		return BLK_INVALID;
	}

	/* update the fcb with the attr_block */
	fcb->attr_block = dblk_index;

	/* initialize the xattr control structure in the attr_block */
	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( dblk_index )));
	xcb = (xcb_t *)&dblk->data;   /* convert from blank chars to a structure containing xcb and a bunch of dxattrs - union */
	xcb->no_xattrs = 0;
	xcb->size = 0;   
	for ( i = 0; i < XATTR_BLOCKS; i++ ) {
		xcb->value_blocks[i] = BLK_INVALID;
	}

	return dblk_index;
}


/* Project 4: on-disk versions of the xattr functions */

/**********************************************************************

    Function    : diskSetAttr
    Description : Set the attribute for the file control block associated with file
    Inputs      : attr_block - index to attr_block
                  name - name of attribute
                  value - value for attribute
                  name_size - length of name string in bytes
                  value_size - length of value string in bytes
    Outputs     : 0 on success, <0 on error

***********************************************************************/
/*
Set the dxattr variables, get the value block, get the block index, and increment the no_xattrs 
every time we set a value. Get the value block by using the function allocDblock.

The diskSetAttr function should update the associated extended attribute blocks. This could mean that it will create a new xattr and write its value to the value blocks or find an existing xattr and replace the value associated to it in said value blocks. Your fileSetAttr function can then be purely responsible for only calling diskSetAttr when it is logically appropriate.
*/
int diskSetAttr( unsigned int attr_block, char *name, char *value, 
		 unsigned int name_size, unsigned int value_size )
{
	/* IMPLEMENT THIS */
	
	/* IMPLEMENT THIS */
	
	dblock_t *dblk;
	xcb_t *xcb;
	int i;
	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( attr_block )));
    xcb = (xcb_t *)&dblk->data;
	// set xcb->no_xattrs to 0
    char * xattr_ptr = (char *) (xcb->xattrs);
	for ( i = 0; i < xcb->no_xattrs; i++ )
	{
		if (((dxattr_t*)xattr_ptr)->name != NULL)
		{
			char * nameToCompare = (char*) malloc(name_size * sizeof(char));
			memcpy(nameToCompare, ((dxattr_t*)xattr_ptr)->name, name_size);
			if (strcmp(nameToCompare, name) == 0)
			{
				unsigned int total = 0;
				((dxattr_t*)xattr_ptr)->value_offset = xcb->size;
                unsigned int current_write_offset = ((dxattr_t*)xattr_ptr)->value_offset;
				while ( total < value_size ) {   // more to write
		        	// int index = ((dxattr_t*)xattr_ptr)->value_offset / (FS_BLOCKSIZE - sizeof(dblock_t));
                    int index = xcb->size / (FS_BLOCKSIZE - sizeof(dblock_t));
		        	unsigned int block = xcb->value_blocks[index];
		        	unsigned int block_bytes;

		        	// if block has not been brought into memory, copy it 
		        	if ( block == BLK_INVALID ) {		                
	            		if (allocDblock( &block, ATTR_BLOCK ) < 0)
                        {
                            errorMessage("diskSetAttr: Could not get block from the disk");
				            return -1;
                        }
		        	}

		        	if ( index >= XATTR_BLOCKS ) {
		            		errorMessage("diskSetAttr: Max size of value file attributes reached");
                            if (current_write_offset > xcb->size ) {
                				xcb->size = current_write_offset;
		                    }
                            ((dxattr_t*)xattr_ptr)->value_size = value_size;
		            		return total;
		        	}

		        	// write to this block 
		        	block_bytes = diskWrite( &(xcb->size), block, value, value_size, 
					         current_write_offset, total );

		        	// update the total written and the file offset as well 
		        	total += block_bytes; 
	        		// TODO Mirrored fileWrite by modifying offset by block_bytes, but it really doesn't make sense because
	        		//      we didn't just write to value_offset
	        		// TODO Is value_offset an indicator of where the value starts on the disk? or an indicator of where it 					
	                // ends?
		        	current_write_offset += block_bytes;
		        	value += block_bytes;
				}
                
		        ///* update the file's size (if necessary) */
		        if (current_write_offset > xcb->size ) {
        				xcb->size = current_write_offset;
		        }
                ((dxattr_t*)xattr_ptr)->value_size = value_size;
        		
        	}
		    // Successfully set already-existing attribute
		    return 0;
		}
        xattr_ptr += sizeof(dxattr_t) + ((dxattr_t*)xattr_ptr)->name_size;
            
	}
    		// Got through for loop without finding xattr to replace, so we will now
    		//create the xattr at index i within xcb->xattrs[] t 
    
		// TODO NEED TO INITIALIZE THE XATTRS VALUES WHEN WE CREATE A NEW XATTR, SEE END OF GIANT PIAZZA POST ON LINE 770
    		//xcb->xattrs[i].name = (char*) malloc(name_size * sizeof(char));
    		//xcb->xattrs[CORRECT POINTER ARITHMETIC FROM PIAZA POST].name = CURRENT POINTER INTO GIANT MEMORY BLOCK
        
    ((dxattr_t*)xattr_ptr)->name_size = name_size;
	memcpy(((dxattr_t*)xattr_ptr)->name, name, name_size);
	((dxattr_t*)xattr_ptr)->value_offset = xcb->size;
    unsigned int current_write_offset = ((dxattr_t*)xattr_ptr)->value_offset;
	unsigned int total = 0;
	while ( total < value_size ) {   // more to write
    	// int index = ((dxattr_t*)xattr_ptr)->value_offset / (FS_BLOCKSIZE - sizeof(dblock_t));
        int index = xcb->size / (FS_BLOCKSIZE - sizeof(dblock_t));
    	unsigned int block = xcb->value_blocks[index];
    	unsigned int block_bytes;

    	// if block has not been brought into memory, copy it 
    	if ( block == BLK_INVALID ) {		                
    		if (allocDblock( &block, ATTR_BLOCK ) < 0)
            {
                errorMessage("diskSetAttr: Could not get block from the disk");
	            return -1;
            }
    	}

    	if ( index >= XATTR_BLOCKS ) {
        		errorMessage("diskSetAttr: Max size of value file attributes reached");
                if (current_write_offset > xcb->size ) {
    				xcb->size = current_write_offset;
                }
                ((dxattr_t*)xattr_ptr)->value_size = value_size;
        		return total;
    	}

    	// write to this block 
    	block_bytes = diskWrite( &(xcb->size), block, value, value_size, 
		         current_write_offset, total );

    	// update the total written and the file offset as well 
    	total += block_bytes; 
		// TODO Mirrored fileWrite by modifying offset by block_bytes, but it really doesn't make sense because
		//      we didn't just write to value_offset
		// TODO Is value_offset an indicator of where the value starts on the disk? or an indicator of where it 					
        // ends?
    	current_write_offset += block_bytes;
    	value += block_bytes;
	}
    
    ///* update the file's size (if necessary) */
    if (current_write_offset > xcb->size ) {
			xcb->size = current_write_offset;
    }
    ((dxattr_t*)xattr_ptr)->value_size = value_size;
	
	// Update number of xattrs
	xcb->no_xattrs += 1;
    // Successfully set already-existing attribute
    return 0;
}

/*
Difference between disk and filesys is where we start to traverse.
*/

/**********************************************************************

    Function    : diskGetAttr
    Description : Get the value for the attribute for name of the file 
                  control block associated with file
    Inputs      : attr_block - index of attribute block
                  name - name of attribute
                  value - value buffer for attribute
                  name_size - length of name string in bytes
                  size - max amount that can be read
                  existsp - flag to check for existence of attr of name only
    Outputs     : number of bytes read on success, <0 on error                  

***********************************************************************/

int diskGetAttr( unsigned int attr_block, char *name, char *value, 
		 unsigned int name_size, unsigned int size, unsigned int existsp )
{ 
	/* IMPLEMENT THIS */

	//unsigned int = sizeof(dblock_t) + sizeof(xcb_t);
    	//char * buf = (char*) malloc(
    	//unsigned int bytes_read = diskRead(attr_block, char *buf, unsigned int bytes, 
	//	       unsigned int offset, unsigned int sofar )

    	//unsigned int = sizeof(dblock_t) + sizeof(xcb_t);
    	//char * buf = (char*) malloc(
    	//unsigned int bytes_read = diskRead(attr_block, char *buf, unsigned int bytes, 
	//	       unsigned int offset, unsigned int sofar )

	dblock_t *dblk;
	xcb_t *xcb;
	int i;
    	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( attr_block )));
	xcb = (xcb_t *)&dblk->data;
	// TODO In for loop, need to compare name with dxattr->name and name_size with dxattr->name_size
    char * xattr_ptr = (char *) (xcb->xattrs);
	for ( i = 0; i < xcb->no_xattrs; i++ )
	{
		if (((dxattr_t*)xattr_ptr)->name != NULL)
		{
			char * nameToCompare = (char*) malloc(name_size * sizeof(char));
			memcpy(nameToCompare, ((dxattr_t*)xattr_ptr)->name, name_size);
			unsigned int total = 0;
            if (strcmp(nameToCompare, name) == 0)
			{
                if (existsp == 1)
        		{       
            			return 1;
        		}
				unsigned int current_read_offset = ((dxattr_t*)xattr_ptr)->value_offset;
                int num_bytes_to_read = min(size, ((dxattr_t*)xattr_ptr)->value_size);
				while ( total < num_bytes_to_read ) {   // more to write
		        	// int index = ((dxattr_t*)xattr_ptr)->value_offset / (FS_BLOCKSIZE - sizeof(dblock_t));
                    int index = xcb->size / (FS_BLOCKSIZE - sizeof(dblock_t));
		        	unsigned int block = xcb->value_blocks[index];
		        	unsigned int bytes_read;

		        	// if block has not been brought into memory, copy it 
		        	if ( block == BLK_INVALID ) {		                
	            		if (allocDblock( &block, ATTR_BLOCK ) < 0)
                        {
                            errorMessage("diskSetAttr: Could not get block from the disk");
				            return -1;
                        }
		        	}

		        	if ( index >= XATTR_BLOCKS ) {
		            		errorMessage("diskSetAttr: Max size of value file attributes reached");
                            return total;
		        	}

		        	// Read this block 
                    bytes_read = diskRead(block, value, 
                                            num_bytes_to_read, 
                                            current_read_offset, 
                                            total);

		        	// update the total written and the file offset as well 
		        	total += bytes_read; 
	        		// TODO Mirrored fileWrite by modifying offset by block_bytes, but it really doesn't make sense because
	        		//      we didn't just write to value_offset
	        		// TODO Is value_offset an indicator of where the value starts on the disk? or an indicator of where it 					
	                // ends?
		        	current_read_offset += bytes_read;
		        	value += bytes_read;
				}
        	}
		    // Successfully set already-existing attribute
		    return total;
		}
        xattr_ptr += sizeof(dxattr_t) + ((dxattr_t*)xattr_ptr)->name_size;
            
	}
	return -1;
}
//typedef struct dxattr {
//	unsigned int name_size;        /* length of name string in bytes */
//	unsigned int value_offset;     /* offset of value in value blocks */
//	unsigned int value_size;       /* length of value string in bytes */
//	char name[0];                  /* reference to the name string */
//} dxattr_t;
	
//typedef struct xcb {
//	unsigned int value_blocks[XATTR_BLOCKS];  /* blocks for xattr values */
//	unsigned int no_xattrs;            /* the number of xattrs in the block */
//	unsigned int size;                 /* this is the end of the value list in bytes */
//	dxattr_t xattrs[0];                /* then a list of xattr structs (names and value refs) */
//} xcb_t;







