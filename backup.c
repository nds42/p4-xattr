int diskSetAttr( unsigned int attr_block, char *name, char *value, 
		 unsigned int name_size, unsigned int value_size )
{
	/* IMPLEMENT THIS */
	
	dblock_t *dblk;
	xcb_t *xcb;
	int i;
    	dblk = (dblock_t *)disk2addr( fs->base, (block2offset( attr_block )));
    	// TODO We have no way of validating that our xcb is valid and really exists. How can we do this?
	xcb = (xcb_t *)&dblk->data;   /* convert from blank chars to a structure containing xcb and a bunch of dxattrs - union */
	// field attr_block will be invalid if block is invalid. 

    	// TODO TODO TODO REPLACE DOUBLE-FOR-LOOP LOGIC WITH TWO CALLS TO diskGetAttr
	// set xcb->no_xattrs to 0
	for ( i = 0; i < xcb->no_xattrs; i++ )
	{
		if (xcb->xattrs[i].name != NULL)
		{
			char * nameToCompare = (char*) malloc(name_size * sizeof(char));
			memcpy(nameToCompare, xcb->xattrs[i].name, name_size);
			if (strcmp(nameToCompare, name) == 0)
			{
				// fstat_t *fstat = fs->proc->fstat_table[fd];
				// file_t *file;
				unsigned int total = 0;				
				// write to the file
				while ( total < value_size ) {   // more to write
		        	int index = xcb->xattrs[i].value_offset / (FS_BLOCKSIZE - sizeof(dblock_t));
		        	unsigned int block = xcb->value_blocks[index];
		        	unsigned int block_bytes;

		        	// if block has not been brought into memory, copy it 
		        	if ( block == BLK_INVALID ) {
		            		errorMessage("diskSetAttr: INVALID LOGIC REACHED, NEED TO MODIFY");			                
		            		exit(1);			            
		            		//block = diskGetBlock( file, index );
			        	//file->blocks[index] = block;
		      
			        	//if ( block == BLK_INVALID ) {
				    //    errorMessage("fileWrite: Could get block from the disk");
				    //    return -1;
			        	//}
		        	}

		        	if ( index >= XATTR_BLOCKS ) {
		            		errorMessage("diskSetAttr: Max size of value file reached");
		            		return total;
		        	}

		        	// write to this block 
		        	// TODO The first argument of diskWrite is a pointer to the offset for the file we are going to write 						
		            	// some info to disk for. Considering we are on a log-based filesystem, this makes sense, since we want 					
		            	// to modify the offset to the newly written location, where the file's data is now stored. However, why 						
		            	// would we pass in a pointer to the file->diskfile->size if we are supposed to be passing in an offset 					
		            	// to modify? Is the offset the disk location of the start of the file?
		        	// TODO Why is an offset passed into diskWrite as well as a disk_offset?
		        	block_bytes = diskWrite( &(xcb->size), block, value, value_size, 
					         xcb->xattrs[i].value_offset, total );

		        	// update the total written and the file offset as well 
		        	total += block_bytes; 
		        		// TODO Mirrored fileWrite by modifying offset by block_bytes, but it really doesn't make sense because
		        		//      we didn't just write to value_offset
		        		// TODO Is value_offset an indicator of where the value starts on the disk? or an indicator of where it 					
		                // ends?
		        	xcb->xattrs[i].value_offset += block_bytes;
		        	value += block_bytes;
				}

					///* update the file's size (if necessary) */
					if ( xcb->xattrs[i].value_offset > xcb->size ) {
		    				xcb->size = xcb->xattrs[i].value_offset;
					}
		        		// Update number of xattrs
		        		xcb->no_xattrs += 1;
			    	}
		    		// Successfully set already-existing attribute
		    		return 0;
			}
		}
    		// Got through for loop without finding xattr to replace, so we will now
    		//create the xattr at index i within xcb->xattrs[] t 
    
		// TODO NEED TO INITIALIZE THE XATTRS VALUES WHEN WE CREATE A NEW XATTR, SEE END OF GIANT PIAZZA POST ON LINE 770
    		//xcb->xattrs[i].name = (char*) malloc(name_size * sizeof(char));
    		//xcb->xattrs[CORRECT POINTER ARITHMETIC FROM PIAZA POST].name = CURRENT POINTER INTO GIANT MEMORY BLOCK
        
    		memcpy(xcb->xattrs[i].name, name, name_size);
		// fstat_t *fstat = fs->proc->fstat_table[fd];
		// file_t *file;
		unsigned int total = 0;
		// write to the file
		while ( total < value_size ) {   // more to write
    		int index = xcb->xattrs[i].value_offset / (FS_BLOCKSIZE - sizeof(dblock_t));
    		unsigned int block = xcb->value_blocks[index];
    		unsigned int block_bytes;

    		// if block has not been brought into memory, copy it 
    		if ( block == BLK_INVALID ) {
        		errorMessage("diskSetAttr: INVALID LOGIC REACHED, NEED TO MODIFY");			                
        		exit(1);			            
        		//block = diskGetBlock( file, index );
        	//file->blocks[index] = block;
  
        	//if ( block == BLK_INVALID ) {
        	//    errorMessage("fileWrite: Could get block from the disk");
        	//    return -1;
        	//}
    		}

    		if ( index >= XATTR_BLOCKS ) {
        		errorMessage("diskSetAttr: Max size of value file reached");
        		return total;
    		}

    		// write to this block 
    		// TODO The first argument of diskWrite is a pointer to the offset for the file we are going to write 						
        	// some info to disk for. Considering we are on a log-based filesystem, this makes sense, since we want 					
        	// to modify the offset to the newly written location, where the file's data is now stored. However, why 						
        	// would we pass in a pointer to the file->diskfile->size if we are supposed to be passing in an offset 					
        	// to modify? Is the offset the disk location of the start of the file?
    		// TODO Why is an offset passed into diskWrite as well as a disk_offset?
    		block_bytes = diskWrite( &(xcb->size), block, value, value_size, 
                xcb->xattrs[i].value_offset, total );

    		// update the total written and the file offset as well 
    		total += block_bytes; 
    		// TODO Mirrored fileWrite by modifying offset by block_bytes, but it really doesn't make sense because
    		//      we didn't just write to value_offset
    		// TODO Is value_offset an indicator of where the value starts on the disk? or an indicator of where it 					
            	// ends?
    		xcb->xattrs[i].value_offset += block_bytes;
    		value += block_bytes;
	}
	
	///* update the file's size (if necessary) */
	if ( xcb->xattrs[i].value_offset > xcb->size ) {
		xcb->size = xcb->xattrs[i].value_offset;
	}
    	
    	// Update number of xattrs
    	xcb->no_xattrs += 1;
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
	xcb = (xcb_t *)&dblk->data;   /* convert from blank chars to a structure containing xcb and a bunch of dxattrs - union */
	// xcb->no_xattrs = 0;
	// xcb->size = 0;   

    	// TODO Fill in xcb->xattrs with next no_xattrs*sizeof(dxattr_t) bytes within the attr_block,
    	//      because xcb gets written to disk and does not take arrays in dynamic memory with it (only pointer
    	//      is written)

	// TODO In for loop, need to compare name with dxattr->name and name_size with dxattr->name_size

	for ( i = 0; i < xcb->no_xattrs; i++ ) 
	{  
        // TODO RESPOND TO PIAZZA POST ABOUT xattrs[] POINTER ARITHMETIC
        //      By using pointer arithmetic, are you saying that each
        //      array entry in xcb->xattrs is actually of a different size?
        //      From a bit of reading on stack overflow, it looks like the 
        //      c idiom of automatically allocated arrays of size zero is
        //      used to create variable length structs that are still a
        //      contiguous block of memory. This would mean that the entire
        //      xcb struct and its array of xattrs would be one giant block
        //      of contiguous memory. [[ NON CRITICAL : I don't think we would have to reallocate
        //      memory for the xcb_t struct in our programs when we get more
        //      dxattrs_t's in our xattrs[] array because the memory all gets
        //      allocated at the beginning from our giant pool of disk memory. ]]
        //      However, the dxattr_t struct itself is of variable size because
        //      its name is also an automatically allocated array of length 0.
        //      Does this mean that indexing into the xcb's array of xattrs will
        //      no longer be possible using the standard C array indexing notation?
        //      It seems like we would have to use pointer arithmetic as you mentioned,
        //      indexing into our pointer xattrs by the size of the dxattr_t struct
        //      and the additional size of the name retrieved from the beginning of 
        //      the dxattr_t struct.
        //      Will we be making extensive use of the disk2addr function here to
        //      facilitate this pointer arithmetic, or is there another approach that will
        //      allow for more simplified abstractions? 
        //      Also, will allocating space for new xattrs and xattr names just mean providing an index
        //      into the next available space in the giant block allocated for the xcb? I don't
        //      think a malloc would make sense.
        //      OVERALL PROCESS TO ITERATE INTO the xcb's xattrs array
        //      for i in no_xattrs
        //          name_size = (*xattrs).name_size
        //          // Do something with current xattr
        //          xattrs = xattrs + sizeof(dxattr) + name_size;
        	if (xcb->xattrs[i].name != NULL)
       		{
            		char * nameToCompare = (char*) malloc(name_size * sizeof(char));
            		memcpy(nameToCompare, xcb->xattrs[i].name, name_size);
            		if (strcmp(nameToCompare, name) == 0)
            		{
                		if (existsp == 1)
                		{       
                    			return 1;
                		}
                		else
                		{
				    	unsigned int value_block_index = xcb->xattrs[i].value_offset / (FS_BLOCKSIZE - sizeof(dblock_t));
				    	unsigned int value_block = xcb->value_blocks[value_block_index];
				    	unsigned int xattr_value_offset = xcb->xattrs[i].value_offset;
				    	char * buf = (char*) malloc(size*sizeof(char));
					// The answer to these questions in piazza was not specific enough. Need more of a concrete answer.
				    	// TODO PROBABLY RESOLVED How can we read values that span multiple blocks using diskRead?
				    	//      Will this just rely on us manually calculating the individual parts of each block
				    	//      that will be read? (e.g. For a diskGetAttr requesting an 800 byte value, doing 
				    	//      one diskRead on the last 200 bytes of the first value_block starting from the computed offset,
				    	//      then another disk_read on the first 600 bytes of the next value block,
				    	//      memcpy'ing each diskRead separately into a buffer of the desired size 800)
				    	// TODO RESOLVED Also, is the value_offset data member a memory offset into the overall value_blocks
				    	//      series of contiguous memory blocks, or is it an offset only in the current value_block
				    	//      that the dxattr_t's value is a part of? If the latter is true, what other indicators exist
				    	//      to denote the value_block that the dxattr_t is a part of? 
				    
				    	/* read limit is either size of buffer or distance to end of file */
					    int num_bytes_to_read = min( size, xcb->xattrs[i].value_size);

				    	// TODO RESOLVED Why are we considering the "distance to end of file" as file->size - fstat->offset? Shouldn't it 						
                        // just be file->size? And, is this relevant to our fileGetAttr call? Or is our "distance to end of file" 						
                        // just the xattr's value size?                    
				    	/* read limit is either size of buffer or distance to end of file */
				    	// 	bytes = min( bytes, ( file->size - fstat->offset ));
					
					    // Professor's answer: The end of the data is sum of all values that were written.
					    // Need to take the modulus of that because we need a block
					    // Not relevant to fileGetAttr because it should be the value

				    	unsigned int total_bytes_read = 0;                    
				    	while (total_bytes_read < num_bytes_to_read)
				    	{
				        	value_block_index = xattr_value_offset / (FS_BLOCKSIZE - sizeof(dblock_t));
						    value_block = xcb->value_blocks[value_block_index];

				            	/* if block has not been brought into memory, copy it */
						    if ( value_block == BLK_INVALID ) {
				                		errorMessage("diskGetAttr: INVALID LOGIC REACHED, NEED TO MODIFY");			                
				                		// value_block = diskGetBlock( file, index );
							    //file->blocks[index] = block;
				          
							    if ( value_block == BLK_INVALID ) {
								    errorMessage("fileRead: Could get block from the disk");
								    return -1;
							    }
						    }

						    if ( value_block_index >= XATTR_BLOCKS ) {
							    errorMessage("diskGetAttr: Max size of value file reached");
							    return -1;
						    }

						    /* read this block */
				            	unsigned int bytes_read = diskRead(value_block, buf, num_bytes_to_read, xattr_value_offset, 
										    total_bytes_read);                        

						    /* update the total written and the file offset as well */
						    total_bytes_read += bytes_read;
                            // TODO Change this to modify xcb->xattrs[i].value_offset if this doesn't work
						    xattr_value_offset += bytes_read;
						    buf += bytes_read;
				    	}
				    	return total_bytes_read;
                		}
            		}
        	}
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
