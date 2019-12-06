int fileSetAttr( unsigned int fd, char *name, char *value, unsigned int name_size, 
		 unsigned int value_size, unsigned int flags )
{
	/* IMPLEMENT THIS */
	
	int i = 0;
	fstat_t *fstat = fs->proc->fstat_table[fd];
	file_t *file;
	
	if ( fstat == NULL ) {
		errorMessage("fileSetAttr: No file corresponds to fd");
		return -1;
	}

	file = fstat->file;

	if ( file == NULL ) {
		errorMessage("fileSetAttr: No file corresponds to fstat");
		return -1;
	}
    	/*
    	free(file->value);
    	file->value = (char*) malloc(value_size*sizeof(char));
    	memcpy(file->value, value, name_size);
    	*/
  	
    	// Create an xattr control block for the current file
    	int xcb_index = diskGetAttrBlock(file, flags); // needs to be changed
	//int dblk_index = diskGetAttrBlock(file, flags); // Probably needs to be changed
    	if (xcb_index == BLK_INVALID)
    	{
		// call diskGetAttrBlock(*file, BLOCK_CREATE);
      		errorMessage("Could not create attribute block");
      		return -1;
    	}
	/*
	if (flags == XATTR_CREATE) {
		
	}
	*/
    	file->attr_block = xcb_index;
    	dblock_t *dblk;
	xcb_t *xcb;
	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( xcb_index )));
	xcb = (xcb_t *)&dblk->data;   // convert from blank chars to a structure containing xcb and a bunch of dxattrs - union
	xcb->xattrs[xcb->no_xattrs].name = (char*) malloc(name_size*sizeof(char));
    	memcpy(&(xcb->xattrs[xcb->no_xattrs].name), name, name_size);
  	
	unsigned int total = 0;
    	unsigned int xattr_dblock_bytes = 0;
  	
  	int foundXattr = 0;
    	int xattrIndex = 0;
  	if (flags == XATTR_REPLACE)
    	{
      		for (i = 0; i < xcb->no_xattrs; i++)
      		{
        		if (strcmp(xcb->xattrs[i], name) == 0)
        		{
          			foundXattr = 1;
          			xattrIndex = i;
          			break;
        		}
      		}
    	}
    	if (!foundXattr)
    	{
      		/* Error case: print on failed XATTR_REPLACE */
      		errorMessage("fileSetAttr fail: no existing entry for name - incompatible with flag XATTR_REPLACE");
      		return -1;
    	}
    	// TODO make while loop actually begin to write at this offset
    	int offsetToStartAt = xcb->xattrs[xattrIndex].value_offset;
  	
  	
    	/* write to the file */
    	// QUESTION : What should we do if the memory of the xattr we want to overwrite has a size that is less than the value_size?
    	//            Wouldn't it overwrite the next xattr in value_blocks?
    	int newEndSize = xcb->xattrs[xcb->no_xattrs].value_offset + value_size;
    	// Check if newEndSize is less than xcb->xattrs[xcb->no_xattrs+1].value_offset
    	/////////////////////////////////////////////////////////////////////////
	// using a log based file system for the xattrs
	// write the value at the end of the log
	while ( total < value_size) {   /* more to write */
		int index = fstat->offset / ( FS_BLOCKSIZE - sizeof(dblock_t) );
		unsigned int xattr_dblock = xcb->value_blocks[index];
		unsigned int block_bytes;

		/* if block has not been brought into memory, copy it */
		if ( xattr_dblock == BLK_INVALID ) {
			xattr_dblock = diskGetBlock( file, index );
			xcb->value_blocks[index] = xattr_dblock;
      			
			if ( xattr_dblock == BLK_INVALID ) {
				errorMessage("fileSetAttr: Could get block from the disk");
				return -1;
			}
		}

		if ( index >= XATTR_BLOCKS ) {
			errorMessage("fileSetAttr: Max number of file attr data blocks for single file reached");
			return total;
		}

		/* write to this block */
        // TODO Is the first argument the correct offset for writing to xattr data blocks?
		xattr_dblock_bytes = diskWrite( &(file->diskfile->size), xattr_dblock, value, value_size, 
					 fstat->offset, total ); // do this in diskSetAttr. Call diskSetAttr in this function
		
		/* update the total written and the file offset as well */
		total += xattr_dblock_bytes;
		fstat->offset += xattr_dblock_bytes;
		value += xattr_dblock_bytes;
	}

	/* update the file's size (if necessary) */
	if ( fstat->offset > xcb->size ) {
		xcb->size = fstat->offset;
	}
    	xcb->no_xattrs++;
    	xcb->xattrs[xcb->no_xattrs].value_offset = fstat->offset;
	return total;
	// unsigned int bytesWritten = diskWrite( unsigned int *disk_offset, unsigned int block, 
	// 		char *buf, unsigned int bytes, 
	//		unsigned int offset, unsigned int sofar )
	
	// in this function, point to extended control block
	
	/* Error case: print on failed XATTR_CREATE */
	errorMessage("fileSetAttr fail: already an entry for name - incompatible with flag XATTR_CREATE");


	return 0;
}

/*
Huge memory chunk divided into blocks. A block is max 512 bytes. dblock_t is the first structure
in every block. The free tells you the data type of the block. If it is free, then free = 0.
If free is 1, then the type is ddirentry. If free is 2, then type is ddentry. If it is 4, then 
it is a file data block. By computing the data end, we know the size. Next points to the address 
of the next disk block.
data[0] in block 1 points to the hash table. 
Maximum of 32 files in this project.
block[0] in fcb_t is a pointer to the next file data block.
Need a linked list if the extended attributes are larger than the block size. 
*/

/**********************************************************************

    Function    : fileGetAttr
    Description : Read file extended attribute
    Inputs      : fd - file descriptor
                  name - name of attribute
                  value - value for attribute to retrieve
                  name_size - length of the name in bytes
                  size - of buffer for value
    Outputs     : number of bytes on success, <0 on error

***********************************************************************/

int fileGetAttr( unsigned int fd, char *name, char *value, unsigned int name_size, unsigned int size ) 
{
	/* IMPLEMENT THIS */
	
  
	/* IMPLEMENT THIS */
	
	int i = 0;
	fstat_t *fstat = fs->proc->fstat_table[fd];
	file_t *file;
	
	if ( fstat == NULL ) {
		errorMessage("fileSetAttr: No file corresponds to fd");
		return -1;
	}

	file = fstat->file;

	if ( file == NULL ) {
		errorMessage("fileSetAttr: No file corresponds to fstat");
		return -1;
	}
    	//file->name = (char*) malloc(name_size*sizeof(char));
    	//memcpy(file->name, name, name_size);
  	
    	
    	//free(file->value);
    	//file->value = (char*) malloc(value_size*sizeof(char));
    	//memcpy(file->value, value, name_size);
    	
  
    	// Create an xattr control block for the current file
    	/*int xcb_index = diskGetAttrBlock(file, flags);
    	if (dblk_index == BLK_INVALID)
    	{
      		errorMessage("Could not create attribute block");
      		return -1;
    	}
    	file->attr_block = xcb_index;
    	dblock_t *dblk;
	xcb_t *xcb;
	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( xcb_index )));
	xcb = (xcb_t *)&dblk->data;   // convert from blank chars to a structure containing xcb and a bunch of dxattrs - union
	
	unsigned int total = 0;
    	unsigned int xattr_dblock_bytes = 0;
    	// write to the file
	while ( total < value_size ) {   // more to write
		int index = fstat->offset / ( FS_BLOCKSIZE - sizeof(dblock_t) );
		unsigned int xattr_dblock = xcb->value_blocks[index];
		unsigned int block_bytes;

		// if block has not been brought into memory, copy it
		if ( xattr_dblock == BLK_INVALID ) {
			xattr_dblock = diskGetBlock( file, index );
			xcb->value_blocks[index] = xattr_dblock;
      
			if ( xattr_dblock == BLK_INVALID ) {
				errorMessage("fileSetAttr: Could get block from the disk");
				return -1;
			}
		}

		if ( index >= XATTR_BLOCKS ) {
			errorMessage("fileSetAttr: Max number of file attr data blocks for single file reached");
			return total;
		}

		// write to this block
        // TODO Is the first argument the correct offset for writing to xattr data blocks?
		xattr_dblock_bytes = diskWrite( &(file->diskfile->size), xattr_dblockblock, value, value_size, 
					 fstat->offset, total );

		// update the total written and the file offset as well
		total += xattr_dblock_bytes; 
		fstat->offset += xattr_dblock_bytes;
		value += xattr_dblock_bytes;
	}

	// update the file's size (if necessary)
	if ( fstat->offset > xcb->size ) {
		xcb->size = fstat->offset;
	}

	return total;
  
  
  
  
  //---------------------------------------------------------
	fstat_t *fstat = fs->proc->fstat_table[fd];
	file_t *file;

	if ( fstat == NULL ) {
		errorMessage("fileGetAttr: No file corresponds to fd");
		return -1;
	}

	file = fstat->file;

	if ( file == NULL ) {
		errorMessage("fileGetAttr: No file corresponds to fstat");
		return -1;
	}*/

	
	
	return 0;
}
