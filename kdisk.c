#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include "kdisk.h"

struct proc_dir_entry *proc_file;
struct file_operations proc_file_fops = {
	.owner 			= THIS_MODULE,
	.unlocked_ioctl	= procfile_ioctl
};

struct FileDesc *fd_table[1024];
struct Ramdisk *disk;

static long procfile_ioctl(struct file *f, unsigned int cmd, unsigned long arg){
	int result = 0;
	int size;
	char *pathname;

	//File ops
	struct Params params;
	char *data_address;

	switch(cmd){
		case RD_CREAT:
			size = strnlen_user((char *) arg, 64);
			pathname = (char *)kmalloc(size, GFP_KERNEL);
			copy_from_user(pathname, (char *)arg, size);
			result = rd_creat(pathname);
			break;
		case RD_MKDIR:
			size = strnlen_user((char *) arg, 64);
			pathname = (char *)kmalloc(size, GFP_KERNEL);
			copy_from_user(pathname, (char *)arg, size);
			result = rd_mkdir(pathname);
			break;
		case RD_OPEN:
			size = strnlen_user((char *) arg, 64);
			pathname = (char *)kmalloc(size, GFP_KERNEL);
			copy_from_user(pathname, (char *)arg, size);
			result = rd_open(pathname);
			break;
		case RD_CLOSE:
			result = rd_close((int) arg);
			break;
		case RD_READ:
			//Get params
			copy_from_user(&params, (struct Params*)arg, sizeof(struct Params));

			//Allocate space to read data
			data_address = (char *) kmalloc(params.count, GFP_KERNEL);

			//Read data
			result = rd_read(params.fd, data_address, params.count);

			printk("Bytes read: %d\n", result);

			//Check if successfull
			if(params.count != result){
				result = -1;
				break;
			}

			//Copy read data to user space
			printk("Going to read '%d' chars\n", params.count);
			printk("Read: %.*s\n", params.count, data_address);
			copy_to_user(params.addr, data_address, params.count);
			printk("RD_Read successfull\n");
			break;
		case RD_WRITE:
			//Get params
			copy_from_user(&params, (struct Params*)arg, sizeof(struct Params));

			//Get Data to write
			data_address = (char *) kmalloc(params.count, GFP_KERNEL);
			copy_from_user(data_address, params.addr, params.count);

			//Write data
			result = rd_write(params.fd, data_address, params.count);
			
			printk("Bytes wrote: %d\n", result);

			//Check if successfull
			if(params.count != result){
				result = -1;
				break;
			}

			printk("RD_Write successfull\n");

			break;
		case RD_LSEEK:
			//Get params
			copy_from_user(&params, (struct Params*)arg, sizeof(struct Params));
			result = rd_lseek(params.fd, params.count);
			break;
		case RD_UNLINK:
			//Get absolute directory path
			size = strnlen_user((char *) arg, 64);
			pathname = (char *)kmalloc(size, GFP_KERNEL);
			copy_from_user(pathname, (char *)arg, size);
			result = rd_unlink(pathname);
			break;
		case RD_READDIR:
			//Get params
			copy_from_user(&params, (struct Params*)arg, sizeof(struct Params));

			//Allocate space to read data
			data_address = (char *) kmalloc(16, GFP_KERNEL);

			//Read data
			result = rd_readdir(params.fd, data_address);

			//Copy to user space
			copy_to_user(params.addr, data_address, 16);

			break;
		default:
			return -ENOTTY;
	}
	printk("~~~~~~~~~~ Status ~~~~~~~~~~\n");
	printk("\tResult:\t%d\n", result);
	printk("\tInode:\t%d\n", disk->superBlock.freeInode);
	printk("\tBlock:\t%d\n", disk->superBlock.freeblock);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
	return result;
}

int rd_creat(char *pathname){
	char **data = parse_path(pathname);
	char *dirname = data[0];
	char *parentdir = data[1];
	printk("==MK_CREAT=========================\n");
	printk("Creating file: %s\n", pathname);
	printk("File name: '%s'\tDir name: '%s'\n", dirname, parentdir);

	//Check if file or dir already exists or not
	if(get_inode_number(pathname) != -1)
		return -1;

	//Get end path inode
	int parent_inode;
	if((parent_inode = get_inode_number(parentdir)) == -1)
		return -1; //Inode not found
	printk("Dir inode at: %d\n", parent_inode);

	//Create the file
	int newInode;
	if((newInode = find_free_inode()) == -1)
		return -1; //Inodes are full
	strncpy(disk->inode[newInode].type, "reg", 4);
	disk->inode[newInode].size = 0;
	printk("File inode at: %d\n", newInode);

	//Insert new Inode into parent's
	insert_Inode(parent_inode, newInode, dirname);
	
	//Debug: check inode number of newly create dir "/dir1"
	printk("'Testing: /%s' at: %d\n", dirname, get_inode_number(pathname));
	printk("=================================\n");
	return 0;
}

int rd_mkdir(char *pathname){	
	char **data = parse_path(pathname);
	char *dirname = data[0];
	char *parentdir = data[1];
	printk("==MK_DIR=========================\n");
	printk("Creating dir: %s\n", pathname);
	printk("Dir name: '%s'\tParent name: '%s'\n", dirname, parentdir);

	//Check if file or dir already exists or not
	if(get_inode_number(pathname) != -1)
		return -1;

	//Get end path inode
	int parent_inode;
	if((parent_inode = get_inode_number(parentdir)) == -1)
		return -1; //Inode not found
	printk("Parent inode at: %d\n", parent_inode);

	//Create the directory
	int newInode;
	if((newInode = find_free_inode()) == -1)
		return -1; //inodes are full
	strncpy(disk->inode[newInode].type, "dir", 4);
	disk->inode[newInode].size = 0;
	printk("Child inode at: %d\n", newInode);


	//Insert new Inode into parent's
	insert_Inode(parent_inode, newInode, dirname);
	
	//Debug: check inode number of newly create dir "/dir1"
	printk("'Testing: /%s' at: %d\n", dirname, get_inode_number(pathname));
	printk("=================================\n");
	return 0;
}


int rd_open(char *pathname){
	printk("==RD_OPEN========================\n");

	//Get Inode index for the path name
	int inode_index;
	if((inode_index = get_inode_number(pathname)) == -1)
		return -1; //file doesn't exist
	printk("File is at inode: %d\n", inode_index);

	//Some other process is using it. lol
	if(fd_table[inode_index] != NULL)
		return -1;

	//Create new file Descriptor
	struct FileDesc* fd = (struct FileDesc*) kmalloc(sizeof(struct FileDesc), GFP_KERNEL);
	fd->read_pos = 0;
	fd->write_pos = 0;
	fd->inode = &(disk->inode[inode_index]);

	//Insert file descriptor into fd table
	fd_table[inode_index] = fd;
	printk("fd for inode '%d' is pointing to file_desc with addr:0x%p\n", inode_index, fd_table[inode_index]);

	printk("=================================\n");

	return inode_index;
}

int rd_close(int fd){
	printk("==RD_CLOSE=======================\n");
	
	printk("int fd = %d\n", fd);
	if(fd_table[fd] == NULL)
		return -1;

	//Free memory
	printk("Freeing memory at: 0x%p\n", fd_table[fd]);
	kfree(fd_table[fd]);

	//Remove pointer from file descriptor table
	printk("Setting fd_table[%d] = NULL\n", fd);
	fd_table[fd] = NULL;

	printk("=================================\n");
	return 0;
}

int rd_read(int fd, char *address, int num_bytes){
	printk("==RD_READ=======================\n");
	printk("Need to read '%d bytes' from inode '%d' into address: 0x%p\n", num_bytes, fd, address);
	printk("fd_table[%d]->position is: %d\n", fd, fd_table[fd]->read_pos);
	printk("File size is: %d\n", fd_table[fd]->inode->size);
	//Error checking
	if(fd_table[fd] == NULL)
		return -1; // Non-existant is hasn't been open yet
	
	if(!strncmp(fd_table[fd]->inode->type,"dir",3))
		return -1; //This is a directory file

	int bytes_read= 0;									//For tracking *address
	int offset = 0;
	int bytes_left = num_bytes;							//For writting loops
	int start_block;
	struct Inode *inode = &disk->inode[fd];
	
	printk("Reading from direct block pointer\n");
	
	//Blocks 0...7
	start_block = (fd_table[fd]->read_pos/256) % 4168;
	for(int i=start_block; i<8; i++){
		//Check if block is allocated or not. If not, return -1
		if(inode->location[i] == NULL) return -1;

		//Calculate offset
		offset = bytes_left < 256 ? fd_table[fd]->read_pos % 256 : 0;

		//Read data
		printk("Reading from inode.location[%d] into addr: %p\n", i, inode->location[i]);
		union Block *block = inode->location[i];
		for(int j=offset; (j<256) && (j<(bytes_left+offset)); j++){
			printk("block[%d] byte[%d]\n", i, j);
			address[bytes_read] = block->file.byte[j];

			bytes_read++;
			fd_table[fd]->read_pos++;

			//Return if all data has been read
			if((num_bytes <= bytes_read)) return bytes_read;
		}
		bytes_left = num_bytes - bytes_read;
	}
	
	printk("Reading from single indirect block pointer\n");

	//Blocks 8...71
	if(inode->location[8] == NULL) inode->location[8] = allocate_block();
	start_block = (fd_table[fd]->read_pos/256) % 4168;
	for(int i=start_block-8; i<64; i++){
		//Check if block is allocated or not. If not, allocate it a block
		if(inode->location[8]->ptr.loc[i] == NULL) inode->location[8]->ptr.loc[i] = allocate_block();

		//Calculate offset
		offset = bytes_left < 256 ? fd_table[fd]->read_pos % 256 : 0;

		//Write data
		union Block *block = inode->location[8]->ptr.loc[i];
		for(int j=offset; (j<256) && (j<(bytes_left+offset)); j++){
			printk("block[%d] byte[%d]\n", i, j);
			address[bytes_read] = block->file.byte[j];
			
			bytes_read++;
			fd_table[fd]->read_pos++;

			//Return if all data has been read
			if((num_bytes <= bytes_read)) return bytes_read;
		}

		bytes_left = num_bytes - bytes_read;
		printk("Left: %d\n", bytes_left);
	}
	
	printk("Reading in double indirect block pointer\n");
	
	//Blocks 72...4167
	if(inode->location[9] == NULL) inode->location[9] = allocate_block();
	start_block = (fd_table[fd]->read_pos/256) % 4168;
	for(int i=(start_block-72)/64; i<64; i++){			//0-63

		//Check if block is allocated or not. If not, allocate it a block
		if(inode->location[9]->ptr.loc[i] == NULL) inode->location[9]->ptr.loc[i] = allocate_block();

		for(int j=(start_block-72)%64; j<64; j++){		//0-63
			//Check if block is allocated or not. If not, allocate it a block
			if(inode->location[9]->ptr.loc[i]->ptr.loc[j] == NULL) inode->location[9]->ptr.loc[i]->ptr.loc[j] = allocate_block();

			//Calculate offset
			offset = bytes_left < 256 ? fd_table[fd]->read_pos % 256 : 0;

			//Read data
			union Block *block = inode->location[9]->ptr.loc[i]->ptr.loc[j];
			for(int k=offset; (k<256) && (k<(bytes_left+offset)); k++){
				address[bytes_read] = block->file.byte[k];

				bytes_read++;
				fd_table[fd]->read_pos++;

				//Return if all data has been read
				if((num_bytes <= bytes_read)) return bytes_read;
			}

			bytes_left = num_bytes - bytes_read;
			printk("Left: %d\n", bytes_left);
		}
	}
	
	//Return if all data has been read
	return bytes_read;

}

int rd_write(int fd, char *address, int num_bytes){
	printk("==RD_WRITE=======================\n");
	printk("Need to write '%d bytes' into inode '%d' from address: 0x%p\n", num_bytes, fd, address);
	printk("Address '0x%p' contains: %s\n", address, address);
	printk("fd_table[%d]->position is: %d\n", fd, fd_table[fd]->write_pos);
	printk("File size is: %d\n", fd_table[fd]->inode->size);
	
	//Error checking
	if(fd_table[fd] == NULL)
		return -1; // Non-existant is hasn't been open yet
	
	if(!strncmp(fd_table[fd]->inode->type,"dir",3))
		return -1; //This is a directory file

	int bytes_written = 0;								//For tracking *address
	int offset = 0;
	int bytes_left = num_bytes;							//For writting loops
	int start_block;
	struct Inode *inode = &disk->inode[fd];
	
	printk("Writting in direct block pointer\n");
	
	//Blocks 0...7
	start_block = (fd_table[fd]->write_pos/256) % 4168;
	printk("==> %d\n", start_block);
	for(int i=start_block; i<8; i++){
		//Check if block is allocated or not. If not, allocate it a block
		if(inode->location[i] == NULL) inode->location[i] = allocate_block();

		//Calculate offset
		offset = bytes_left < 256 ? fd_table[fd]->write_pos % 256 : 0;

		//Write data
		printk("Writting to inode.location[%d] at addr: %p\n", i, inode->location[i]);
		union Block *block = inode->location[i];
		for(int j=offset; (j<256) && (j<(bytes_left+offset)); j++){
			block->file.byte[j] = address[bytes_written];

			inode->size += 1;
			bytes_written++;
			fd_table[fd]->write_pos++;
		}
		bytes_left = num_bytes - bytes_written;
		
		//printk("Size:%d\n", inode->size);
		//printk("Left: %d\n", bytes_left);

		//Return if all data has been written
		if(bytes_left <= 0) return bytes_written;
	}

	printk("Writting in single indirect block pointer\n");

	//Blocks 8...71
	if(inode->location[8] == NULL) inode->location[8] = allocate_block();
	start_block = (fd_table[fd]->write_pos/256) % 4168;
	for(int i=start_block-8; i<64; i++){
		//Check if block is allocated or not. If not, allocate it a block
		if(inode->location[8]->ptr.loc[i] == NULL) inode->location[8]->ptr.loc[i] = allocate_block();

		//Calculate offset
		offset = bytes_left < 256 ? fd_table[fd]->write_pos % 256 : 0;

		//Write data
		union Block *block = inode->location[8]->ptr.loc[i];
		for(int j=offset; (j<256) && (j<(bytes_left+offset)); j++){
			block->file.byte[j] = address[bytes_written];

			inode->size += 1;			
			bytes_written++;
			fd_table[fd]->write_pos++;
		}

		bytes_left = num_bytes - bytes_written;
		//printk("Size:%d\n", inode->size);
		//printk("Left: %d\n", bytes_left);

		//Return if all data has been written
		if(bytes_left <= 0) return bytes_written;
	}
	
	printk("Writting in double indirect block pointer\n");
	
	//Blocks 72...4167
	if(inode->location[9] == NULL) inode->location[9] = allocate_block();
	start_block = (fd_table[fd]->write_pos/256) % 4168;
	for(int i=(start_block-72)/64; i<64; i++){			//0-63

		//Check if block is allocated or not. If not, allocate it a block
		if(inode->location[9]->ptr.loc[i] == NULL) inode->location[9]->ptr.loc[i] = allocate_block();

		for(int j=(start_block-72)%64; j<64; j++){		//0-63
			//Check if block is allocated or not. If not, allocate it a block
			if(inode->location[9]->ptr.loc[i]->ptr.loc[j] == NULL) inode->location[9]->ptr.loc[i]->ptr.loc[j] = allocate_block();

			//Calculate offset
			offset = bytes_left < 256 ? fd_table[fd]->write_pos % 256 : 0;

			//Write data
			union Block *block = inode->location[9]->ptr.loc[i]->ptr.loc[j];
			for(int k=offset; (k<256) && (k<(bytes_left+offset)); k++){
				block->file.byte[k] = address[bytes_written];

				inode->size += 1;
				bytes_written++;
				fd_table[fd]->write_pos++;
			}

			bytes_left = num_bytes - bytes_written;
			//printk("Size:%d\n", inode->size);
			//printk("Left: %d\n", bytes_left);

			//Return if all data has been written
			if(bytes_left <= 0) return bytes_written;
		}
	}
	
	//Return if all data has been written
	return bytes_written;

}


int rd_lseek(int fd, int offset){
	printk("Moving fd_table[%d]->read_pos to %d\n",fd, offset);
	fd_table[fd]->read_pos = offset;
	return 0;
}

int rd_unlink(char *pathname){
	printk("==RD_UNLINK======================\n");
	printk("Unlinking file/dir: %s\n", pathname);

	//Get inodes for the given pathname and its parent
	char **data = parse_path(pathname);
	int parent_inode = get_inode_number(data[1]);
	int file_inode = get_inode_number(pathname);

	printk("Path to delete has inode index: %d\n", file_inode);
	//Error checking
	//Does it exist?
	if(file_inode == -1)
		return -1;
	if(file_inode == 0)
		return -1;
	//Does the dir contain files?
	if(strcmp(disk->inode[file_inode].type, "dir") == 0
		&& disk->inode[file_inode].location[0] != NULL)
		return -1;
	//Is this file open?
	if(strcmp(disk->inode[file_inode].type, "reg") == 0
		&& fd_table[file_inode] != NULL)
		return -1;
	printk("Path pass all error checks\n");

	//Delete blocks accociated to the path's inode
	struct Inode *inode = &(disk->inode[file_inode]);
	delete_blocks(inode);

	//Delete file/dir inodes
	
	delete_Inode(parent_inode, file_inode);

	printk("=================================\n");
	return 0;
}

int rd_readdir(int fd, char *address){
	printk("==RD_READDIR=====================\n");
	printk("Reading entries in inode '%d'\n", fd);

	if(fd_table[fd] == NULL)
		return -1;

	printk("Dir is open\n");
	struct Inode *inode = fd_table[fd]->inode;

	int block_num_start = (fd_table[fd]->read_pos) / 16;
	int dir_num_start = (fd_table[fd]->read_pos) % 16;

	for(int i=block_num_start; i<8; i++){
		if(inode->location[i] == 0)
			continue;
		printk("Reading entry in inode block location '%d'\n", i);
		for(int j=dir_num_start; j<16; j++){
			fd_table[fd]->read_pos++;
			if(inode->location[i]->dir.entry[j].filename[0] == 0)
				continue;
			printk("Found entry: %.14s\n", inode->location[i]->dir.entry[j].filename);
			strncpy(address, inode->location[i]->dir.entry[j].filename, 16);
			memcpy(address+14, &(inode->location[i]->dir.entry[j].inode_number), 2);
			return 1;
		}
	}
	
	return 0;
}

//Returns an array of 2 string containing the parent directory
//and end file. Example: char *pathname = "/dir1/dir2/file1"
//will return; parent: `/dir1/dir2`, child: `file1`
char **parse_path(char *pathname){
	//Get new directory name
	char *dirname = (strrchr(pathname, '/')) +1;

	//Trim to get parent
	int size = strlen(pathname)-strlen(dirname)+1;
	char *parent = kmalloc(size, GFP_KERNEL);
	memset(parent, '\0', size);
	strncpy(parent, pathname, size-2);
	if(parent[0] == '\0')
		parent[0] = '/';

	char **data = kmalloc(2*sizeof(char*), GFP_KERNEL);
	data[0] = dirname;
	data[1] = parent;

	return data;
}

//Scans the inode blocks to find a free inode
//Return the index of the free inode it found
int find_free_inode(){
	//Check if all inodes are full or not
	if(disk->superBlock.freeInode == 0)
		return -1;

	for(int i=0; i < INODE_COUNT; i++){
		if(disk->inode[i].type[0] == 0){ //inode is free
			disk->superBlock.freeInode--;
			return i;
		}
	}

	return -1;
}

//Returns the inode index of the provided path
int get_inode_number(char* pathname){
	printk("\t\tGetting inode for: %s\n", pathname);
	if(pathname[0] != '/')
		return -1;
	if(strlen(pathname) == 1)
		return 0;

	pathname++;

	//Get inode index of path be going through the path name
	int nodeIndex = 0; //inode of root dir
  	char *token, *strpos = pathname;
  	while ((token=strsep(&strpos,"/")) != NULL){
    	if((nodeIndex = get_inode_number_helper(nodeIndex, token)) == -1)
    		break;
    	printk("\t\tNode Index for '%s': %d\n", token ,nodeIndex);
  	}
  	

  	return nodeIndex;
}

//returns the inode of dir_name if it is present in inode
//of the given index
int get_inode_number_helper(int index, char *dir_name){
	printk("\t\tNeed to look for '%s' in inode '%d'\n", dir_name, index);
	if(dir_name[0] == '/')
		return 0;

	struct Inode *inode = &(disk->inode[index]);
	
	//Check in first 8 block pointers
	for(int i=0; i<8; i++){
		union Block *b = inode->location[i];

		//Search for entires in the block
		if(b != 0){
			for(int j=0; j<16; j++){
				//Some entry in this...?
				if(b->dir.entry[j].filename[0] != 0){
					if(!strncmp(b->dir.entry[j].filename, dir_name, 14))
						return b->dir.entry[j].inode_number;
				}
			}
		}
	}
	
	//Check in 9th single indirect block pointer
	if(inode->location[8] == NULL) return -1;
	for(int i=0; i<64; i++){
		union Block *b = inode->location[8]->ptr.loc[i];
		//Search for entires in the block
		if(b != 0){
			for(int j=0; j<16; j++){
				//Some entry in this...?
				if(b->dir.entry[j].filename[0] != 0){
					if(!strncmp(b->dir.entry[j].filename, dir_name, 14))
						return b->dir.entry[j].inode_number;
				}
			}
		}
	}
	
	return -1;
}

//Inserts an child inode under a parent inode. For example:
//if 	parent inode = 123 which links to (/dir1/dir2/dir3)
//if 	child  		 = 124 which links to (file1)
//then the following struction is formed /dir1/dir2/dir3/file1
int insert_Inode(int parent, int child, char *fileName){
	printk("\t\tInserting '%s' at inode %d in parent with inode %d\n", fileName, child, parent);

	//Within the first 8 direct block pointers
	for(int i=0;i<8;i++){
		union Block *b = disk->inode[parent].location[i];
		//Allocate new block
		if(b == 0){
			disk->inode[parent].location[i] = allocate_block();
			b = disk->inode[parent].location[i];
		}
		for(int j=0; j<16; j++){
			if(b->dir.entry[j].filename[0] == 0){ //found a free area
				strncpy(b->dir.entry[j].filename, fileName, 13); //truncate any long strings
				b->dir.entry[j].inode_number = child;
				return 0;
			}
		}
	}

	//In the 9 single indirect block pointer
	if(disk->inode[parent].location[8] == NULL) disk->inode[parent].location[8] = allocate_block();

	for(int i=0; i<64; i++){
		if(disk->inode[parent].location[8]->ptr.loc[i] == NULL)
			disk->inode[parent].location[8]->ptr.loc[i] = allocate_block();

		union Block *b = disk->inode[parent].location[8]->ptr.loc[i];
		for(int j=0; j<16; j++){
			if(b->dir.entry[j].filename[0] == 0){ //found a free area
				strncpy(b->dir.entry[j].filename, fileName, 13); //truncate any long strings
				b->dir.entry[j].inode_number = child;
				printk("Inserted\n");
				return 0;
			}
		}
	}

	return -1;
}

int delete_Inode(int parent, int child){
	printk("\t\tDeleting inode '%d' in parent with inode %d\n",child, parent);

	//Within the first 8 direct block pointers
	for(int i=0; i<8; i++){
		union Block *b = disk->inode[parent].location[i];
		if(b != 0){
			for(int j=0; j<16; j++){
				if(b->dir.entry[j].inode_number == child){
					memset(&(b->dir.entry[j]), 0, 16); //remove entry from parent
					memset(&(disk->inode[child]), 0, 64); //free inode
					disk->superBlock.freeInode++;
					printk("\t\tDeleted inode\n");
					return 0;
				}
			}
		}
	}

	if(disk->inode[parent].location[8] == NULL) return -1;

	//In the 9 single indirect block pointer
	for(int i=0; i<64; i++){
		union Block *b = disk->inode[parent].location[8]->ptr.loc[i];
		if(b != NULL){
			for(int j=0; j<16; j++){
				if(b->dir.entry[j].inode_number == child){
					memset(&(b->dir.entry[j]), 0, 16); //remove entry from parent
					memset(&(disk->inode[child]), 0, 64); //free inode
					disk->superBlock.freeInode++;
					printk("\t\tDeleted inode\n");
					return 0;
				}
			}
		}
	}

	return -1;
}

//allocates a free block in the disk.
//Returns the address of the newly allocated block
union Block* allocate_block(){
	for(int i=0; i<7931; i++){
		if(isBlockFree(i)){
			disk->bitmap.map[i / 8] = (disk->bitmap.map[i / 8] | SET_BIT_1(i%8));
			disk->superBlock.freeblock--;
			memset(&disk->part[i], 0, BLOCKSIZE);
			return &disk->part[i];
		}
	}
	return NULL;
}

int delete_blocks(struct Inode *inode){
	union Block *block;

	//Delete direct block  pointer blocks
	printk("Deleting blocks in inode->location[i]\n");
	for(int i=0; i<8; i++){
		//Search block with address *block
		block = inode->location[i];
		delete_blocks_helper(block);
	}
	
	//Delete single indirect block pointer
	printk("Deleting blocks in inode->location[8]->ptr.loc[i]\n");
	block = inode->location[8];
	if(block == 0) return 0;
	for(int i=0; i<64; i++){
		delete_blocks_helper(inode->location[8]->ptr.loc[i]);
	}
	delete_blocks_helper(block);

	//Delete double indirect block pointer
	printk("Deleting blocks in inode->location[9]->ptr.loc[i]->ptr.loc[j]\n");
	block = inode->location[9];
	if(block == 0) return 0;
	for(int i=0; i<64; i++){
		for(int j=0; j<64; j++){
			if(inode->location[9]->ptr.loc[i] == 0) return 0;
			delete_blocks_helper(inode->location[9]->ptr.loc[i]->ptr.loc[j]);
		}
		delete_blocks_helper(inode->location[9]->ptr.loc[i]);
	}
	delete_blocks_helper(block);
	printk("Deleting done!\n");
	return 0;
}

void delete_blocks_helper(union Block *block){
	for(int j=0; j<7931; j++){
		if(block == &(disk->part[j])){
			//printk("\t\tDeleting block: %d\n", j);
			disk->bitmap.map[j / 8] = (disk->bitmap.map[j / 8] | SET_BIT_0(j%8));
			disk->superBlock.freeblock++;
			memset(block, 0, 256);
		}
	}
}

//Uses the bitmap to check if a block is free is not
int isBlockFree(int block){
	uint8_t *bitmap = &(disk->bitmap.map[block / 8]);
	return ((*bitmap >> (7-block%8)) & 0x1) == 0;
}

int init_module(){
	//Create process file
	proc_file = proc_create(MODULE_NAME, 0, NULL, &proc_file_fops);

	//Initialize Ramdisk
	disk = (struct Ramdisk*) vmalloc(sizeof(struct Ramdisk));
	memset(disk, 0, sizeof(struct Ramdisk));
	
	//Init Inode
	memcpy(disk->inode[0].type, "dir", 4);
	disk->inode[0].size = 0;

	//Init supper block
	disk->superBlock.freeInode = INODE_COUNT - 1; //Account for root
	disk->superBlock.freeblock = PART_COUNT;

	printk("Ram disk Initialized: %s\n", MODULE_NAME);
	printk("~~~~ Super Block Status ~~~~\n");
	printk("\tInode count: %d\n", disk->superBlock.freeInode);
	printk("\tBlock count: %d\n", PART_COUNT);
	printk("~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	return 0;
}

void cleanup_module(){
	remove_proc_entry(MODULE_NAME, NULL);
	printk("Unloaded %s\n", MODULE_NAME);
}