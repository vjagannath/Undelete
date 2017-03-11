#ifndef UNDELETE_H
#define UNDELETE_H

#define   MAX_STRING	60		/*  For all input lines	*/
#define   MAX_PREV	8		/*  For 'p' command	*/
#define   DEV	   "/dev"		/*  Where devices are	*/
#define   TMP      "/tmp"		/*  For temp output file	*/

#define   K			4096			/*  STD_BLK		*/
#define   K_MASK	(~(K-1))	/*  Round to K boundary	*/
#define   K_SHIFT	12		/*  Ie. 1<<12 = K	*/
#define   PAGE_MASK	0x1f		/*  Word mode: 32 bytes	*/

#define   _MAX_BLOCK_SIZE 		4096
#define   _STATIC_BLOCK_SIZE	1024

#ifndef I_MAP_SLOTS
/* Max number of inode and zone map blocks we can handle. */
#define I_MAP_SLOTS	8
#define Z_MAP_SLOTS	(sizeof(char *) == 2 ? 16 : 128)
#endif

/*  Terminal i/o codes  */

#define   CTRL_D	'\004'		/*  ASCII ^D		*/
#define   BELL		'\007'		/*  ASCII bell code     */
#define   BS		'\010'		/*  ASCII back space	*/
#define   CTRL_U	'\025'		/*  ASCII ^U		*/
#define	  ESCAPE  	'\033'		/*  ASCII escape code	*/
#define   DEL       '\177'		/*  ASCII delete code   */

/*  Visual modes  */

#define   WORD	   1
#define   MAP	   3



typedef  struct  _undelete_state		/*  State of undelete	*/
{
	/*  Information from super block  */
	/*  The types here are mostly promoted types for simplicity	*/
	/*  and efficiency.						*/

	unsigned inodes;			/*  Number of i-nodes	*/
	zone_t zones;				/*  Total # of blocks	*/
	unsigned inode_maps;			/*  I-node map blocks	*/
	unsigned zone_maps;			/*  Zone map blocks	*/
	unsigned inode_blocks;		/*  I-node blocks	*/
	unsigned first_data;			/*  Total non-data blks	*/
	int magic;				/*  Magic number	*/

	/* Numbers derived from the magic number */  

	unsigned char is_fs;			/*  Nonzero for good fs	*/
	unsigned char v1;			/*  Nonzero for V1 fs	*/
	unsigned inode_size;			/*  Size of disk inode	*/
	unsigned nr_indirects;		/*  # indirect blocks	*/
	unsigned zone_num_size;		/*  Size of disk z num	*/
	int block_size;			/*  FS block size       */

	/* Other derived numbers */  

	bit_t inodes_in_map;			/*  Bits in i-node map	*/
	bit_t zones_in_map;			/*  Bits in zone map	*/
	int ndzones;				/*  # direct zones in an inode */

	/*  Information from map blocks  */

	bitchunk_t inode_map[ I_MAP_SLOTS * K / sizeof (bitchunk_t) ];
	bitchunk_t zone_map[ Z_MAP_SLOTS * K / sizeof (bitchunk_t) ];

	/*  Information for current block  */

	off_t address;			/*  Current address	*/
	// off_t last_addr;			/*  For erasing ptrs	*/
	zone_t block;				/*  Current block (1K)	*/
	unsigned offset;			/*  Offset within block	*/

	char buffer[ _MAX_BLOCK_SIZE ];

	/*  Display state  */

	int  mode;				/*  WORD, BLOCK or MAP	*/
	int  output_base;			/*  2, 8, 10, or 16	*/

	/*  Search information  */

	char search_string[ MAX_STRING + 1 ];	/*  For '/' and 'n'	*/
	off_t prev_addr[ MAX_PREV ];		/*  For 'p' command	*/
	int   prev_mode[ MAX_PREV ];

	/*  File information  */

	char *device_name;			/*  From command line	*/
	int   device_d;
	int   device_mode;			/*  O_RDONLY or O_RDWR	*/
	zone_t device_size;			/*  Number of blocks	*/

	char  file_name[ MAX_STRING + 1 ];	/*  For 'w' and 'W'	*/
	FILE *file_f;
	int   file_written;			/*  Flag if written to	*/

}undelete_state;

#define MFS_DIRSIZ_CUST	60

struct direct_cust {
  uint32_t mfs_d_ino;
  char mfs_d_name[MFS_DIRSIZ_CUST];
} __packed;

#endif //UNDELETE_H