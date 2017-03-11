#include <minix/config.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#undef ERROR			/* arrgghh, errno.h has this pollution */
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <minix/drivers.h>
#include <minix/const.h>
#include <minix/type.h>

#include "../../fs/mfs/const.h"
#include "../../fs/mfs/type.h"
#include "../../fs/mfs/inode.h"
#include <dirent.h>
#include <minix/fslib.h>
#include "undelete.h"

//static void new_icopy(register struct inode *rip, register d2_inode *dip, int direction, int norm);
/****************************************************************/
/*								*/
/*	Warning( text, arg1, arg2 )				*/
/*								*/
/*		Display a message for 2 seconds.		*/
/*								*/
/****************************************************************/

#if __STDC__
void Warning( const char *text, ... )
#else
void Warning( text )
  char *text;
#endif  

{
	va_list argp;

	//printf( "%c%s", BELL, Tclr_all );

	//Goto( WARNING_COLUMN, WARNING_LINE );

	//printf( "%s Warning: ", Treverse );
	printf( "Warning: ");
	va_start( argp, text );
	vprintf( text, argp );
	va_end( argp );
	//printf( " %s", Tnormal );
	printf("\n");

	fflush(stdout);		/* why does everyone forget this? */

	sleep( 2 );
}

/****************************************************************/
/*								*/
/*	Error( text, ... )					*/
/*								*/
/*		Print an error message on stderr.		*/
/*								*/
/****************************************************************/

#if __STDC__
void Error( const char *text, ... )
#else
void Error( text )
  char *text;
#endif  

{
	va_list argp;

	//Reset_Term();

	fprintf( stderr, "\nundelete: " );
	va_start( argp, text );
	vfprintf( stderr, text, argp );
	va_end( argp );
	
	if ( errno != 0 )
		fprintf( stderr, ": %s", strerror( errno ) );
	
	fprintf( stderr, "\n" );

	exit( 1 );
}



/*===========================================================================*
 *				conv2					     *
 *===========================================================================*/
unsigned conv2(norm, w)
int norm;			/* TRUE if no swap, FALSE for byte swap */
int w;				/* promotion of 16-bit word to be swapped */
{
/* Possibly swap a 16-bit word between 8086 and 68000 byte order. */

  if (norm) return( (unsigned) w & 0xFFFF);
  return( ((w&BYTE) << 8) | ( (w>>8) & BYTE));
}


/*===========================================================================*
 *				conv4					     *
 *===========================================================================*/
long conv4(norm, x)
int norm;			/* TRUE if no swap, FALSE for byte swap */
long x;				/* 32-bit long to be byte swapped */
{
/* Possibly swap a 32-bit long between 8086 and 68000 byte order. */

  unsigned lo, hi;
  long l;
  
  if (norm) return(x);			/* byte order was already ok */
  lo = conv2(FALSE, (int) x & 0xFFFF);	/* low-order half, byte swapped */
  hi = conv2(FALSE, (int) (x>>16) & 0xFFFF);	/* high-order half, swapped */
  l = ( (long) lo <<16) | hi;
  return(l);
}



/*===========================================================================*
 *				new_icopy				     *
 *===========================================================================*/
void new_icopy(rip, dip, direction, norm)
register struct inode *rip;	/* pointer to the in-core inode struct */
register d2_inode *dip;	/* pointer to the d2_inode struct */
int direction;			/* READING (from disk) or WRITING (to disk) */
int norm;			/* TRUE = do not swap bytes; FALSE = swap */
{
  int i;

  if (direction == READING) {
	/* Copy V2.x inode to the in-core table, swapping bytes if need be. */
	rip->i_mode    = (mode_t) conv2(norm,dip->d2_mode);
	rip->i_uid     = (uid_t) conv2(norm,dip->d2_uid);
	rip->i_nlinks  = (nlink_t) conv2(norm,dip->d2_nlinks);
	rip->i_gid     = (gid_t) conv2(norm,dip->d2_gid);
	rip->i_size    = (off_t) conv4(norm,dip->d2_size);
	rip->i_atime   = (time_t) conv4(norm,dip->d2_atime);
	rip->i_ctime   = (time_t) conv4(norm,dip->d2_ctime);
	rip->i_mtime   = (time_t) conv4(norm,dip->d2_mtime);
	rip->i_ndzones = V2_NR_DZONES;
	rip->i_nindirs = V2_INDIRECTS(rip->i_sp->s_block_size);
	for (i = 0; i < V2_NR_TZONES; i++)
		rip->i_zone[i] = (zone_t) conv4(norm, (long) dip->d2_zone[i]);
  } else {
	/* Copying V2.x inode to disk from the in-core table. */
	dip->d2_mode   = (u16_t) conv2(norm,rip->i_mode);
	dip->d2_uid    = (i16_t) conv2(norm,rip->i_uid);
	dip->d2_nlinks = (u16_t) conv2(norm,rip->i_nlinks);
	dip->d2_gid    = (u16_t) conv2(norm,rip->i_gid);
	dip->d2_size   = (i32_t) conv4(norm,rip->i_size);
	dip->d2_atime  = (i32_t) conv4(norm,rip->i_atime);
	dip->d2_ctime  = (i32_t) conv4(norm,rip->i_ctime);
	dip->d2_mtime  = (i32_t) conv4(norm,rip->i_mtime);
	for (i = 0; i < V2_NR_TZONES; i++)
		dip->d2_zone[i] = (zone_t) conv4(norm, (long) rip->i_zone[i]);
  }
}


int Path_Dir_File( path_name, dir_name, file_name )
  char  *path_name;
  char **dir_name;
  char **file_name;

{
	char *p;
	static char directory[ MAX_STRING + 1 ];
	static char filename[ MAX_STRING + 1 ];


	if ( (p = strrchr( path_name, '/' )) == NULL )
	{
		strcpy( directory, "." );
		strcpy( filename, path_name );
	}
	else
	{
		*directory = '\0';
		strncat( directory, path_name, p - path_name );
		strcpy( filename, p + 1 );
	}

	if ( *directory == '\0' )
		strcpy( directory, "/" );

	if ( *filename == '\0' )
	{
		Warning( "A file name must follow the directory name" );
		return( 0 );
	}

	*dir_name  = directory;
	*file_name = filename;

	return( 1 );
}

/****************************************************************/
/*								*/
/*	File_Device( file_name )				*/
/*								*/
/*		Return the name of the file system device	*/
/*		containing the file "file_name".		*/
/*								*/
/*		This is used if the "-r" option was specified.	*/
/*		In this case we have only been given a file	*/
/*		name, and must determine which file system	*/
/*		device to open.					*/
/*								*/
/*		NULL is returned on error conditions.		*/
/*								*/
/****************************************************************/



char *File_Device(file_name)
  char *file_name;

{
	struct stat file_stat;
	struct stat device_stat;
	DIR* dirp;
	struct dirent entry;
	struct dirent *result;
	static char device_name[NAME_MAX + 1];


	if ( access( file_name, R_OK ) != 0 )
	{
		Warning( "Can not find %s", file_name );
		return( NULL );
	}


	if ( stat( file_name, &file_stat ) == -1 )
	{
		Warning( "Can not stat(2) %s", file_name );
		return( NULL );
	}


	/*  Open /dev for reading  */

	if ((dirp = opendir(DEV)) == NULL)
	{
		Warning( "Can not read %s", DEV );
		return( NULL );
	}

	//printf("file_stat.st_dev: %llu\n",file_stat.st_dev);
	
	while (readdir_r(dirp, &entry, &result) == 0)
	{
		if (entry.d_fileno == 0 )
			continue;
		
		strcpy(device_name, DEV);
		strcat(device_name, "/");
		strncat(device_name, entry.d_name, NAME_MAX);

		//printf("Device name: %s\n",device_name);
		
		if (stat(device_name, &device_stat) == -1)
			continue;

		if ((device_stat.st_mode & S_IFMT) != S_IFBLK)
			continue;
		
		//printf("device_stat.st_rdev : %llu\n",device_stat.st_rdev);

		if (file_stat.st_dev == device_stat.st_rdev)
		{
		  closedir( dirp );
		  return( device_name );
		}
	}

	closedir(dirp);

	Warning("The device containing file %s is not in %s", file_name, DEV);

	return( NULL );
}


/****************************************************************/
/*								*/
/*	Read_Disk( state, block_addr, buffer )			*/
/*								*/
/*		Reads a 1k block at "block_addr" into "buffer".	*/
/*								*/
/****************************************************************/


void Read_Disk( s, block_addr, buffer )
  undelete_state *s;
  off_t  block_addr;
  char  *buffer;

{
	//printf("\nRead_Disk, block_addr = %lld\n", block_addr);
	
	if ( lseek( s->device_d, block_addr, SEEK_SET ) == -1 )
		Error( "Error seeking %s", s->device_name );

	//printf("\nRead_Disk, s->device_d = %u\n", s->device_d);
	
	if ( read( s->device_d, buffer, s->block_size ) != s->block_size )
		Error( "Error reading %s", s->device_name );
}

/****************************************************************/
/*								*/
/*	Read_Super_Block( state )				*/
/*								*/
/*		Read and check the super block.			*/
/*								*/
/****************************************************************/

void Read_Super_Block( s )
  undelete_state *s;

{
	struct super_block *super = (struct super_block *) s->buffer;
	unsigned inodes_per_block;
	off_t size;

	s->block_size = K;
	Read_Disk(s, (long)SUPER_BLOCK_BYTES, s->buffer);

	s->magic = super->s_magic;
	/*if ( s->magic == SUPER_MAGIC )
	{
		s->is_fs = TRUE;
		s->v1 = FALSE;
		s->inode_size = V1_INODE_SIZE;
		inodes_per_block = V1_INODES_PER_BLOCK;
		s->nr_indirects = V1_INDIRECTS;
		s->zone_num_size = V1_ZONE_NUM_SIZE;
		s->zones = super->s_nzones;
		s->ndzones = V1_NR_DZONES;
		s->block_size = _STATIC_BLOCK_SIZE;
	}
	else */
	if ( s->magic == SUPER_V2 || s->magic == SUPER_V3)
	{
		if(s->magic == SUPER_V3)
		{
			s->block_size = super->s_block_size;
			//printf("\ns->magic = SUPER_V3\n");
		}
		else
			s->block_size = _STATIC_BLOCK_SIZE;
		s->is_fs = TRUE;
		s->v1 = FALSE;
		s->inode_size = V2_INODE_SIZE;
		inodes_per_block = V2_INODES_PER_BLOCK(s->block_size);
		s->nr_indirects = V2_INDIRECTS(s->block_size);
		s->zone_num_size = V2_ZONE_NUM_SIZE;
		s->zones = super->s_zones;
		s->ndzones = V2_NR_DZONES;
		
		
	}
	else  
	{
		if (super->s_magic == SUPER_REV)
			Warning( "V1-bytes-swapped file system (?)" );
		else if (super->s_magic == SUPER_V2_REV)
			Warning( "V2-bytes-swapped file system (?)" );
		else  
			Warning( "Not a Minix file system" );
		
		Warning( "The file system features will not be available" );  
		s->zones = 100000L;
		return;
	}

	s->inodes = super->s_ninodes;
	s->inode_maps = bitmapsize((bit_t) s->inodes + 1 , s->block_size);
	
	//printf("\ns->inodes = %u, s->inode_maps = %d\n", s->inodes, s->inode_maps);
	
	if ( s->inode_maps != super->s_imap_blocks )
	{
		if ( s->inode_maps > super->s_imap_blocks )
			Error( "Corrupted inode map count or inode count in super block" );
		else  
			Warning( "Count of inode map blocks in super block suspiciously high" );
		
		s->inode_maps = super->s_imap_blocks;
	}

	s->zone_maps = bitmapsize( (bit_t) s->zones , s->block_size);
	
	
	//printf("\ns->zones = %u, s->zone_maps = %d\n", s->zones, s->zone_maps);
	if ( s->zone_maps != super->s_zmap_blocks )
	{
		if ( s->zone_maps > super->s_zmap_blocks )
			 Error( "Corrupted zone map count or zone count in super block" );
		else
			  Warning( "Count of zone map blocks in super block suspiciously high" );
		
		s->zone_maps = super->s_zmap_blocks;
	}
	
	//printf("\ninodes_per_block = %d\n", inodes_per_block);

	s->inode_blocks = (s->inodes + inodes_per_block - 1) / inodes_per_block;
	s->first_data   = 2 + s->inode_maps + s->zone_maps + s->inode_blocks;
	
	//printf("\ns->inode_blocks = %u, s->first_data = %u\n", s->inode_blocks, s->first_data);
	//printf("\nsuper->s_firstdatazone = %u\n", super->s_firstdatazone);
	//printf("\nsuper->s_firstdatazone_old = %u\n", super->s_firstdatazone_old);
	
	if ( s->first_data != super->s_firstdatazone_old)
	{
		
		if ( s->first_data > super->s_firstdatazone_old)
			Error( "Corrupted first data zone offset or inode count in super block" );
		else
			Warning( "First data zone in super block suspiciously high" );
		
		s->first_data = super->s_firstdatazone_old;
	}  

	s->inodes_in_map = s->inodes + 1;
	s->zones_in_map  = s->zones + 1 - s->first_data;

	/*
	if ( s->zones != s->device_size )
	Warning( "Zone count does not equal device size" );
	*/

	s->device_size = s->zones;

	if ( super->s_log_zone_size != 0 )
		Error( "Can not handle multiple blocks per zone" );
}


/****************************************************************/
/*								*/
/*	Read_Bit_Maps( state )					*/
/*								*/
/*		Read in the i-node and zone bit maps from the	*/
/*		specified file system device.			*/
/*								*/
/****************************************************************/


void Read_Bit_Maps( s )
  undelete_state *s;

{
	int i;

	if ( s->inode_maps > I_MAP_SLOTS  ||  s->zone_maps > Z_MAP_SLOTS )
	{
		Warning( "Super block specifies too many bit map blocks" );
		return;
	}

	for ( i = 0;  i < s->inode_maps;  ++i )
	{
		Read_Disk( s, (long) (2 + i) * K,
		(char *) &s->inode_map[ i * K / sizeof (bitchunk_t ) ] );
	}

	for ( i = 0;  i < s->zone_maps;  ++i )
	{
		Read_Disk( s, (long) (2 + s->inode_maps + i) * K,
		(char *) &s->zone_map[ i * K / sizeof (bitchunk_t ) ] );
	}
}


/****************************************************************/
/*								*/
/*	Find_Inode( state, filename )				*/
/*								*/
/*		Find the i-node for the given file name.	*/
/*								*/
/****************************************************************/


ino_t Find_Inode( s, filename )
  undelete_state *s;
  char *filename;

{
	struct stat device_stat;
	struct stat file_stat;
	ino_t inode;


	if ( fstat( s->device_d, &device_stat ) == -1 )
		Error( "Can not fstat(2) file system device" );

	#ifdef S_IFLNK
	if ( lstat( filename, &file_stat ) == -1 )
	#else
	if ( stat( filename, &file_stat ) == -1 )
	#endif
	{
		Warning( "Can not find file %s", filename );
		return( 0 );
	}

	if ( device_stat.st_rdev != file_stat.st_dev )
	{
		Warning( "File is not on device %s", s->device_name );
		return( 0 );
	}


	inode = file_stat.st_ino;

	if ( inode < 1  || inode > s->inodes )
	{
		Warning( "Illegal i-node number" );
		return( 0 );
	}

	return( inode );
}


/****************************************************************/
/*								*/
/*	Find_Deleted_Entry( state, path_name )			*/
/*								*/
/*		Split "path_name" into a directory name and	*/
/*		a file name. Then search the directory for	*/
/*		an entry that would match the deleted file	*/
/*		name. (Deleted entries have a zero i-node	*/
/*		number, but the original i-node number is 	*/
/*		placed at the end of the file name.)		*/
/*								*/
/*		If successful an i-node number is returned,	*/
/*		else zero is returned.				*/
/*								*/
/****************************************************************/


ino_t Find_Deleted_Entry( s, path_name )
  undelete_state *s;
  char *path_name;

{
	char *dir_name;
	char *file_name;

	/*  Check if the file exists  */

	if ( access( path_name, F_OK ) == 0 )
	{
		Warning( "File has not been deleted" );
		return( 0 );
	}


	/*  Split the path name into a directory and a file name  */

	if ( ! Path_Dir_File( path_name, &dir_name, &file_name ) )
		return( 0 );


	/*  Check to make sure the user has read permission on  */
	/*  the directory.					  */

	if ( access( dir_name, R_OK ) != 0 )
	{
		Warning( "Can not find %s", dir_name );
		return( 0 );
	}


	/*  Make sure "dir_name" is really a directory. */
	{
		struct stat dir_stat;

		if ( stat( dir_name, &dir_stat ) == -1   ||  (dir_stat.st_mode & S_IFMT) != S_IFDIR )
		{
			Warning( "Can not find directory %s", dir_name );
			return( 0 );
		}
	}


	/*  Make sure the directory is on the current  */
	/*  file system device.                        */

	if ( Find_Inode( s, dir_name ) == 0 )
		return( 0 );


	/*  Open the directory and search for the lost file name.  */
	{
		int   dir_d;
		int   count = 0;
		struct direct_cust entry;
		struct dirent *result;
		char buf[4096];
		
		int index = 0;
		if ( (dir_d = open( dir_name, O_RDONLY )) == -1 )
		{
			Warning( "Can not read directory %s", dir_name );
			return( 0 );
		}

		//printf("\nsize of dirent = %d, size of direct = %d\n, size of direct_cust = %d", sizeof(struct dirent), sizeof(struct direct), sizeof(struct direct_cust));
		
		errno = 0;
		//while ((count = getdents( dir_d, buf, 4096)) != -1 )
		//{
		while ( (count = read( dir_d, (char *) &entry, sizeof(struct direct_cust) ))
					      == sizeof(struct direct_cust) )
		{
			//printf("\nentry.d_ino = %u, filename = %s\n", entry.mfs_d_ino, entry.mfs_d_name);
			
			//printf("\nSize of data returned = %d\n", count);
			// if(count == 0)
			// {
				// printf("\ncount 0\n");
				// break;
			// }
			
			// if(count % sizeof(struct dirent) == 0)
			// {
				// result = (struct dirent *)buf;
				// while (index < (count/sizeof(struct dirent)))
				// {
					// printf("\nd_fileno = %llu, filename = %s\n", result[index].d_fileno, result[index].d_name);
					
					// if((strcmp(result[index].d_name, ".") == 0) || (strcmp(result[index].d_name, "..") == 0)) // skip "." and ".." file listings
					// {
						// continue;
					// }
					
					// if ( result[index].d_fileno == 0) {
						// printf("\nDeletedFile\n");
						
						// if (strncmp(file_name, result[index].d_name, MFS_NAME_MAX - sizeof(ino_t)) == 0 )
						// {
							// ino_t inode = *( (ino_t *) &result[index].d_name[ MFS_NAME_MAX - sizeof(ino_t) ] );

							// printf("\ninode = %llu\n", inode);
							// close(dir_d);

							// if ( inode < 1  || inode > s->inodes )
							// {
								// Warning( "Illegal i-node number" );
								// return( 0 );
							// }

							// return( inode );
						// }
					// }
				// }
				
			// }
			
			if((strcmp(entry.mfs_d_name, ".") == 0) || (strcmp(entry.mfs_d_name, "..") == 0)) // skip "." and ".." file listings
            {
				continue;
			}
			
			if ( entry.mfs_d_ino == 0) {
				//printf("\nDeletedFile\n");
				
				if (strncmp(file_name, entry.mfs_d_name, MFS_DIRSIZ_CUST - sizeof(ino_t)) == 0 )
				{
					ino_t inode = *( (ino_t *) &entry.mfs_d_name[ MFS_DIRSIZ_CUST - sizeof(ino_t) ] );

					//printf("\ninode = %llu\n", inode);
					close(dir_d);

					if ( inode < 1  || inode > s->inodes )
					{
						Warning( "Illegal i-node number" );
						return( 0 );
					}

					return( inode );
				}
			}
		}

		close(dir_d);

		if ( count == 0 )
			Warning( "Can not find a deleted entry for %s", file_name );
		else
			Warning( "Problem reading directory %s, return value : %d, errno = %d\n", dir_name, count, errno);

		return( 0 );
	}
}


/****************************************************************/
/*								*/
/*	Read_Block( state, buffer )				*/
/*								*/
/*		Reads a 1k block from "state->address" into	*/
/*		"buffer". Checks "address", and updates		*/
/*		"block" and "offset".				*/
/*								*/
/****************************************************************/


void Read_Block( s, buffer )
  undelete_state *s;
  char *buffer;

{
	off_t end_addr;
	off_t block_addr;
	end_addr = (long) s->device_size * s->block_size - 1;

	if ( s->address < 0 )
		s->address = 0L;

	if ( s->address > end_addr )
		s->address = end_addr;

	/*  The address must be rounded off for  */
	/*  certain visual display modes.        */

	if ( s->mode == WORD )
		s->address &= ~1L;
	else if ( s->mode == MAP )
		s->address &= ~3L;


	block_addr = s->address & K_MASK;

	s->block  = (zone_t) (block_addr >> K_SHIFT);
	s->offset = (unsigned) (s->address - block_addr);

	//printf("\nBefore Read_Disk\n");
	
	Read_Disk( s, block_addr, buffer );
}

/****************************************************************/
/*								*/
/*	In_Use( bit, map )					*/
/*								*/
/*		Is the bit set in the map?			*/
/*								*/
/****************************************************************/


int In_Use( bit, map )
  bit_t bit;
  bitchunk_t *map;

{
	return( map[ (int) (bit / (CHAR_BIT * sizeof (bitchunk_t))) ] &
	(1 << ((unsigned) bit % (CHAR_BIT * sizeof (bitchunk_t)))) );
}


/*  Free_Block( state, block )
 *
 *  Make sure "block" is a valid data block number, and it
 *  has not been allocated to another file.
 */


int Free_Block( s, block )
  undelete_state *s;
  zone_t  block;

  {
  if ( block < s->first_data  ||  block >= s->zones )
    {
    Warning( "Illegal block number" );
    return( 0 );
    }

  if ( In_Use( (bit_t) (block - (s->first_data - 1)), s->zone_map ) )
    {
    Warning( "Encountered an \"in use\" data block" );
    //return( 0 );
    }

  return( 1 );
  }


/*  Data_Block( state, block, &file_size )
 *
 *  If "block" is free then write  Min(file_size, k)
 *  bytes from it onto the current output file.
 *
 *  If "block" is zero, this means that a 1k "hole"
 *  is in the file. The recovered file maintains
 *  the reduced size by not allocating the block.
 *
 *  The file size is decremented accordingly.
 */


int Data_Block( s, block, file_size )
  undelete_state *s;
  zone_t   block;
  off_t    *file_size;

{
	char buffer[ K ];
	off_t block_size = *file_size > K ? K : *file_size;


	/*  Check for a "hole".  */

	if ( block == NO_ZONE )
	{
		if ( block_size < K )
		{
			Warning( "File has a hole at the end" );
			return( 0 );
		}

		if ( fseek( s->file_f, block_size, SEEK_CUR ) == -1 )
		{
			Warning( "Problem seeking %s", s->file_name );
			return( 0 );
		}

		*file_size -= block_size;
		return( 1 );
	}


	/*  Block is not a "hole". Copy it to output file, if not in use.  */

	if ( ! Free_Block( s, block ) )
		return( 0 );

	Read_Disk( s, (long) block << K_SHIFT, buffer );


	if ( fwrite( buffer, 1, (size_t) block_size, s->file_f ) != (size_t) block_size )
	{
		Warning( "Problem writing %s", s->file_name );
		return( 0 );
	}

	*file_size -= block_size;
	
	s->file_written = 1;
	return( 1 );
}



/*  Indirect( state, block, &file_size, double )
 *
 *  Recover all the blocks pointed to by the indirect block
 *  "block",  up to "file_size" bytes. If "double" is true,
 *  then "block" is a double-indirect block pointing to
 *  V*_INDIRECTS indirect blocks.
 *
 *  If a "hole" is encountered, then just seek ahead in the
 *  output file.
 */


int Indirect( s, block, file_size, dblind )
  undelete_state *s;
  zone_t   block;
  off_t    *file_size;
  int       dblind;

{
	union
	{
		//zone1_t ind1[ V1_INDIRECTS ];
		zone_t  ind2[ V2_INDIRECTS(_MAX_BLOCK_SIZE) ];
	} indirect;
	int  i;
	zone_t zone;

	/*  Check for a "hole".  */

	if ( block == NO_ZONE )
	{
		off_t skip = (off_t) s->nr_indirects * K;

		if ( *file_size < skip  ||  dblind )
		{
			Warning( "File has a hole at the end" );
			return( 0 );
		}

		if ( fseek( s->file_f, skip, SEEK_CUR ) == -1 )
		{
			Warning( "Problem seeking %s", s->file_name );
			return( 0 );
		}

		*file_size -= skip;
		return( 1 );
	}


	/*  Not a "hole". Recover indirect block, if not in use.  */

	if ( ! Free_Block( s, block ) )
		return( 0 );


	Read_Disk( s, (long) block << K_SHIFT, (char *) &indirect );

	for ( i = 0;  i < s->nr_indirects;  ++i )
	{
		if ( *file_size == 0 )
			return( 1 );

		zone = indirect.ind2[ i ];
		if ( dblind )
		{
			if ( ! Indirect( s, zone, file_size, 0 ) )
			return( 0 );
		}
		else
		{
			if ( ! Data_Block( s, zone, file_size ) )
			return( 0 );
		}
	}

	return( 1 );
}


/****************************************************************/
/*								*/
/*	Recover_Blocks( state )					*/
/*								*/
/*		Try to recover all the blocks for the i-node	*/
/*		currently pointed to by "s->address". The	*/
/*		i-node and all of the blocks must be marked	*/
/*		as FREE in the bit maps. The owner of the	*/
/*		i-node must match the current real user name.	*/
/*								*/
/*		"Holes" in the original file are maintained.	*/
/*		This allows moving sparse files from one device	*/
/*		to another.					*/
/*								*/
/*		On any error -1L is returned, otherwise the	*/
/*		size of the recovered file is returned.		*/
/*								*/
/*								*/
/*		NOTE: Once a user has read access to a device,	*/
/*		there is a security hole, as we lose the	*/
/*		normal file system protection. For convenience,	*/
/*		de(1) is sometimes set-uid root, this allows	*/
/*		anyone to use the "-r" option. When recovering,	*/
/*		Recover_Blocks() can only superficially check	*/
/*		the validity of a request.			*/
/*								*/
/****************************************************************/


off_t Recover_Blocks( s )
  undelete_state *s;

{
	struct inode core_inode;
	//d1_inode *dip1;
	d2_inode *dip2;
	struct inode *inode = &core_inode;
	bit_t node = (s->address - (s->first_data - s->inode_blocks) * K) /
		s->inode_size + 1;

	//dip1 = (d1_inode *) &s->buffer[ s->offset & ~ PAGE_MASK ];
	dip2 = (d2_inode *) &s->buffer[ s->offset & ~ PAGE_MASK
						& ~ (V2_INODE_SIZE-1) ];
	
	switch (s->magic) 
	{
		case SUPER_V3:
		case SUPER_V2:		new_icopy(inode, dip2, READING, TRUE);	break;
		case SUPER_V2_REV:	new_icopy(inode, dip2, READING, FALSE);	break;
	}

	if ( s->block < s->first_data - s->inode_blocks  ||
		s->block >= s->first_data )
	{
		Warning( "Not in an inode block" );
		return( -1L );
	}


	/*  Is this a valid, but free i-node?  */

	if ( node > s->inodes )
	{
		Warning( "Not an inode" );
		return( -1L );
	}

	if ( In_Use(node, s->inode_map) )
	{
		//Warning( "I-node is in use" );
		//return( -1L );
	}


	/*  Only recover files that belonged to the real user.  */

	// {
		// uid_t real_uid = getuid();
		// struct passwd *user = getpwuid( real_uid );

		// if ( real_uid != SU_UID  &&  real_uid != inode->i_uid )
		// {
			// Warning( "I-node did not belong to user %s", user ? user->pw_name : "" );
			// return( -1L );
		// }
	// }

	//printf("\nTrying to recover blocks\n");

	/*  Recover all the blocks of the file.  */

	{
		off_t file_size = inode->i_size;
		int i;


		/*  Up to s->ndzones pointers are stored in the i-node.  */

		//printf("\ns->ndzones : %d\n", s->ndzones);
		for ( i = 0;  i < s->ndzones;  ++i )
		{
			if ( file_size == 0 )
				return( inode->i_size );

			if ( ! Data_Block( s, inode->i_zone[ i ], &file_size ) )
				return( -1L );
		}

		if ( file_size == 0 )
			return( inode->i_size );
	

		//printf("\nTrying to recover indirect blocks\n");
		
		/*  An indirect block can contain up to inode->i_indirects more blk ptrs.  */

		if ( ! Indirect( s, inode->i_zone[ s->ndzones ], &file_size, 0 ) )
			return( -1L );

		if ( file_size == 0 )
			return( inode->i_size );


		//printf("\nTrying to recover double indirect blocks\n");
		
		/*  A double indirect block can contain up to inode->i_indirects blk ptrs. */

		if ( ! Indirect( s, inode->i_zone[ s->ndzones+1 ], &file_size, 1 ) )
			return( -1L );

		if ( file_size == 0 )
			return( inode->i_size );

		Error( "Internal fault (file_size != 0)" );
	}

	/* NOTREACHED */
	return( -1L );
}

int main( int argc, char* argv[] )
{
	char *command_name = argv[0];
	char *file_name, *dir_name;
	struct stat device_stat;
    struct stat tmp_stat;
	static undelete_state s;		 
	
	/*  Parse arguments  */
	if ( argc == 2  )
	{
		if ( ! Path_Dir_File( argv[1], &dir_name, &file_name ) )
			Error( "Path: Operation aborted" );
		//printf("\nDirectory name: %s", dir_name);
		//printf("\nFile name: %s", file_name);
	}
	else
    {
		fprintf( stderr, "Usage: %s <deleted_file_name>\n", command_name );
		exit( 1 );
    }
	
	if ( (s.device_name = File_Device( dir_name )) == NULL )
	{
		//printf("\nIn Main 1 Device Name : %s\n", s.device_name);
		Error( "Device: Operation aborted" );
	}
	
	/*  The output file will be in /tmp with the same file name.  */

    strcpy( s.file_name, TMP );
    strcat( s.file_name, "/" );
    strcat( s.file_name, file_name );
	
	if ( access( s.file_name, F_OK ) == 0 )
		Error( "Will not overwrite file %s", s.file_name );
	
	/*  Open the output file.  */
    if ( (s.file_f = fopen( s.file_name, "w" )) == NULL )
      Error( "Can not open file %s", s.file_name );
  
	/*  Open the device file.  */  
	{
		struct stat device_stat;
		off_t size;

		if ( stat( s.device_name, &device_stat ) == -1 )
			Error( "Can not find file %s", s.device_name );

		if ( (device_stat.st_mode & S_IFMT) != S_IFBLK  && 	(device_stat.st_mode & S_IFMT) != S_IFREG )
			Error( "Can only edit block special or regular files" );


		if ( (s.device_d = open( s.device_name, s.device_mode )) == -1 )
			Error( "Can not open %s", s.device_name );

		if ( (size = lseek( s.device_d, 0L, SEEK_END )) == -1 )
			Error( "Error seeking %s", s.device_name );

		if ( size % K != 0 )
		{
			Warning( "Device size is not a multiple of 4096" );
			Warning( "The (partial) last block will not be accessible" );
		}
	}
	
	/*  Initialize the rest of the state record  */
	s.mode = WORD;
	s.output_base = 10;
	s.search_string[ 0 ] = '\0';

	{
		int i;

		for ( i = 0;  i < MAX_PREV;  ++i )
		{
			s.prev_addr[ i ] = 0L;
			s.prev_mode[ i ] = WORD;
		}
	}
	
	sync();
	Read_Super_Block( &s );
	Read_Bit_Maps( &s );
	s.address = 0L;
	
	ino_t inode = Find_Deleted_Entry( &s, argv[1] );
	off_t size;

    if ( inode == 0 )
	{
		unlink( s.file_name );
		Error( "Recover aborted" );
	}
	
	//printf("\ndeleted inode : %llu\n", inode);
	
	s.address = ( (long) s.first_data - s.inode_blocks ) * K  + (long) (inode - 1) * s.inode_size;
	
	//printf("\ns.address : %lld\n", s.address);
	
	Read_Block( &s, s.buffer );
	
	//printf("\nAfter Read_Block\n");
	
	/*  Have found the lost i-node, now extract the blocks.  */
	if ( (size = Recover_Blocks( &s )) == -1L )
	{
		unlink( s.file_name );
		Error( "Recover aborted" );
	}

    //Reset_Term();

    printf( "\nRecovered %lld bytes, written to file %s\n", size, s.file_name );
	
	/*  If there is an open output file that was never written to  */
	/*  then remove its directory entry. This occurs when no 'w' 	 */
	/*  or 'W' command occurred between a 'c' command and exiting	 */
	/*  the program.						 */

	if ( s.file_name[0] != '\0'  &&  ! s.file_written )
		unlink( s.file_name );
	
	return 0;
}