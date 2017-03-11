# Undelete
Extending Minix Operating System to support recovery of deleted files

Project Overview:

The goal of the project is to implement an undelete command for MINIX operating system. When the command is invoked from the shell, it will attempt to recover a regular file that has been deleted by the unlink system call in the current directory, with a given file name.

Operations:

The project aims at providing a support for undeleting a deleted or unlinked file from the file system. This is achieved through a system call along with few modifications in the minix file system. The system call provided for this support is termed ‘undelete’

Design Considerations:

The undelete feature implementation provisions the flexibility for the user to recover a deleted or unlinked file from the file system. Recovery, not always being possible, the undelete command will try its best to recover and report to user if the recovery is not possible. The implementation specifics are as below:

1. When the user deletes or unlinks a file using the unlink system call, the inode structure associated with the file to be deleted, is cached globally if it has no other links which is used to recover the file at a later stage when the user tries to undelete the file. The file ‘link.c’ in the file system is modified to support this.

2. When a file gets unlinked, the ‘direct’ structure associated with the parent directory is updated in the file system to replicate the file deletion. The inode number associated with the deleted file is set to zero while having the inode number appended to the end of the file name in the file structure present in the directory. The changes pertain to modification in the file system - ‘path.c’.

3. On trying to undelete a file using the ‘undelete’ command, the super block of the file is queried along with the zone maps to retrieve the inode details from the file system. The file inode is searched for in the “direct” structure of the parent directory based on the file name, and corresponding inode details is fetched from global cache maintained at the file system to check if the file was deleted and can be recovered.

4. The start and end block address associated with the file is part of the inode structure. The block address is used to query the file system data blocks – direct, indirect, and double indirect data blocks to fetch the data content of the file.

5. The corresponding entry in the parent directory is queried and fetched to update the which inode number and name which was modified during the process of unlink system call.

6. A policy is enforced to allocate memory for a new file such that during the situation when the global undelete cache is full, and a new file system is trying to allocate but not able because of unavailable space, the entry in the global cache is freed thus allowing to allocate space the new file to be created. This being the case, if the user tries to undelete the file which was freed from the global cache, the undelete operation fails. The system implementation currently is limited to support a global cache of 100.

7. The policy ensures a successful memory allocation when in case the blocks occupied by the recoverable deleted files is needed by the system.
