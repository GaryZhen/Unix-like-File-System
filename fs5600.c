/*
 * file:        fs5600.c
 * description: skeleton file for CS 5600 system
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, November 2019
 *
 * Modified by CS5600 staff, fall 2021.
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "fs5600.h"

/* if you don't understand why you can't use these system calls here,
 * you need to read the assignment description another time
 */
#define stat(a,b) error do not use stat()
#define open(a,b) error do not use open()
#define read(a,b,c) error do not use read()
#define write(a,b,c) error do not use write()


// === block manipulation functions ===

/* disk access.
 * All access is in terms of 4KB blocks; read and
 * write functions return 0 (success) or -EIO.
 *
 * read/write "nblks" blocks of data
 *   starting from block id "lba"
 *   to/from memory "buf".
 *     (see implementations in misc.c)
 */
extern int block_read(void *buf, int lba, int nblks);
extern int block_write(void *buf, int lba, int nblks);

/* bitmap functions
 */
void bit_set(unsigned char *map, int i)
{
    map[i/8] |= (1 << (i%8));
}
void bit_clear(unsigned char *map, int i)
{
    map[i/8] &= ~(1 << (i%8));
}
int bit_test(unsigned char *map, int i)
{
    return map[i/8] & (1 << (i%8));
}


/*
 * Allocate a free block from the disk.
 *
 * success - return free block number
 * no free block - return -ENOSPC
 *
 * hint:
 *   - bit_set/bit_test might be useful.
 */
int alloc_blk() {
    unsigned char map[FS_BLOCK_SIZE];
    block_read(map, 1, 1);
    for (int i = 3; i < FS_BLOCK_SIZE * 8; i++)
    {
        if (!bit_test(map, i)){
            bit_set(map, i);
            block_write(map, 1, 1);
            return i;
        }
    }
    
    return -ENOSPC;
}

/*
 * Return a block to disk, which can be used later.
 *
 * hint:
 *   - bit_clear might be useful.
 */
void free_blk(int i) {
    unsigned char map[FS_BLOCK_SIZE];
    block_read(map, 1, 1);
    bit_clear(map, i);
    block_write(map, 1, 1);
}


// === FS helper functions ===


/* Two notes on path translation:
 *
 * (1) translation errors:
 *
 *   In addition to the method-specific errors listed below, almost
 *   every method can return one of the following errors if it fails to
 *   locate a file or directory corresponding to a specified path.
 *
 *   ENOENT - a component of the path doesn't exist.
 *   ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *             /a/b/c) is not a directory
 *
 * (2) note on splitting the 'path' variable:
 *
 *   the value passed in by the FUSE framework is declared as 'const',
 *   which means you can't modify it. The standard mechanisms for
 *   splitting strings in C (strtok, strsep) modify the string in place,
 *   so you have to copy the string and then free the copy when you're
 *   done. One way of doing this:
 *
 *      char *_path = strdup(path);
 *      int inum = ... // translate _path to inode number
 *      free(_path);
 */


/* EXERCISE 2:
 * convert path into inode number.
 *
 * how?
 *  - first split the path into directory and file names
 *  - then, start from the root inode (which inode/block number is that?)
 *  - then, walk the dirs to find the final file or dir.
 *    when walking:
 *      -- how do I know if an inode is a dir or file? (hint: mode)
 *      -- what should I do if something goes wrong? (hint: read the above note about errors)
 *      -- how many dir entries in one inode? (hint: read Lab4 instructions about directory inode)
 *
 * hints:
 *  - you can safely assume the max depth of nested dirs is 10
 *  - a bunch of string functions may be useful (e.g., "strtok", "strsep", "strcmp")
 *  - "block_read" may be useful.
 *  - "S_ISDIR" may be useful. (what is this? read Lab4 instructions or "man inode")
 *
 * programing hints:
 *  - there are several functionalities that you will reuse; it's better to
 *  implement them in other functions.
 */

int path2inum(const char *path) {
    /* your code here */
    size_t n = strlen(path);
    if (n == 0 || path[0] != '/') return -ENOENT; // empty 

    if (strcmp(path, "/") == 0) return 2;

    char path_copy[300];
    strcpy(path_copy, &path[1]);

    char * paths[11] = {0};
    int i = 0;
    char *temp = strtok(path_copy, "/");
    while(temp){
        paths[i++] = temp;        
        temp = strtok(NULL, "/");
    }

    if (i == 0) return 2;
    // extract node
    inode_t root_inode;
    int ret = block_read((unsigned char *)&root_inode, 2, 1);

    if (ret < 0) {
       
        return -EOPNOTSUPP;
    }
    
    inode_t current = root_inode;
    int j = 0;
    while(j < i){
        if (strlen(paths[j]) > 27){
            if(j < i-1) return -ENOTDIR;
            return -ENOENT;
        }

        if (!S_ISDIR(current.mode) && j < i - 1){
            
            return -ENOTDIR;
        }
            
        if (current.size == 0) {
            //printf("size\n");
            if(j < i-1) return -ENOTDIR;
            return -ENOENT;
        }

        dirent_t dirs[128];

        ret = block_read((unsigned char *)dirs, current.ptrs[0], 1);
        
        if (ret != 0){
            
            if(j < i-1) return -ENOTDIR;
            return -ENOENT;
        }
        
        // find the matched dir
        int k = 0;
        while(k < 128){            
            if(dirs[k].valid && strcmp(paths[j], dirs[k].name) == 0){
                break;
            }
            k ++;
        }
        if (k >= 128){
            // printf("\n not find dir %d %s %d\n", j, paths[j], i);
            if (i > 1 && j == i-1) return -ENOTDIR;
            
            return -ENOENT;            
        } 
        

        // if the last one, return the inode
        if (j == i - 1) return dirs[k].inode;

        // else read next inode
        ret = block_read((unsigned char *)&current, dirs[k].inode, 1);
        if (ret != 0) return -EOPNOTSUPP;
        if (!S_ISDIR(current.mode)){
            return -ENOTDIR;
        }
        j ++;
    }   
       
    return -EOPNOTSUPP;
}


/* EXERCISE 2:
 * Helper function:
 *   copy the information in an inode to struct stat
 *   (see its definition below, and the "full definition" in 'man lstat'.)
 *
 *  struct stat {
 *        ino_t     st_ino;         // Inode number
 *        mode_t    st_mode;        // File type and mode
 *        nlink_t   st_nlink;       // Number of hard links
 *        uid_t     st_uid;         // User ID of owner
 *        gid_t     st_gid;         // Group ID of owner
 *        off_t     st_size;        // Total size, in bytes
 *        blkcnt_t  st_blocks;      // Number of blocks allocated
 *                                  // (note: block size is FS_BLOCK_SIZE;
 *                                  // and this number is an int which should be round up)
 *
 *        struct timespec st_atim;  // Time of last access
 *        struct timespec st_mtim;  // Time of last modification
 *        struct timespec st_ctim;  // Time of last status change
 *    };
 *
 *  [hints:
 *
 *    - read fs_inode in fs5600.h and compare with struct stat.
 *
 *    - you can safely ignore the types of attributes (e.g., "nlink_t",
 *      "uid_t", "struct timespec"), treat them as "unit32_t".
 *
 *    - the above "struct stat" does not show all attributes, but we don't care
 *      the rest attributes.
 *
 *    - for several fields in 'struct stat' there is no corresponding
 *    information in our file system:
 *      -- st_nlink - always set it to 1  (recall that fs5600 doesn't support links)
 *      -- st_atime - set to same value as st_mtime
 *  ]
 */

void inode2stat(struct stat *sb, struct fs_inode *in, uint32_t inode_num)
{
    memset(sb, 0, sizeof(*sb));

    sb->st_ino = inode_num;   
    sb->st_mode = in->mode;  
    sb->st_nlink = 1; 
    sb->st_uid = in->uid;   
    sb->st_gid = in->gid;   
    sb->st_size = in->size;  
    
    if (S_ISDIR(in->mode)){
        sb->st_blocks = 1;
    }else{
        sb->st_blocks = DIV_ROUND_UP(in->size, (FS_BLOCK_SIZE)); 
    }

    sb->st_atim.tv_sec = in->ctime;
    sb->st_mtim.tv_sec = in->mtime;
    sb->st_ctim.tv_sec = in->ctime;
}




// ====== FUSE APIs ========

/* EXERCISE 1:
 * init - this is called once by the FUSE framework at startup.
 *
 * The function should:
 *   - read superblock
 *   - check if the magic number matches FS_MAGIC
 *   - initialize whatever in-memory data structure your fs5600 needs
 *     (you may come back later when requiring new data structures)
 *
 * notes:
 *   - ignore the 'conn' argument.
 *   - use "block_read" to read data (if you don't know how it works, read its
 *     implementation in misc.c)
 *   - if there is an error, exit(1)
 */
void* fs_init(struct fuse_conn_info *conn)
{
    unsigned char buffer[FS_BLOCK_SIZE];
    int ret = block_read(buffer, 0, 1);
    if (ret != 0) 
        exit(1);
    
    uint32_t magic = *((uint32_t *) buffer);
    if (magic != FS_MAGIC){
        exit(1);
    }
    

    return NULL;
}


/* EXERCISE 1:
 * statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (ignore others):
     *   [DONE] f_bsize = FS_BLOCK_SIZE
     *   [DONE] f_namemax = <whatever your max namelength is>
     *   [TODO] f_blocks = total image - (superblock + block map)
     *   [TODO] f_bfree = f_blocks - blocks used
     *   [TODO] f_bavail = f_bfree
     *
     * it's okay to calculate this dynamically on the rare occasions
     * when this function is called.
     */

    st->f_bsize = FS_BLOCK_SIZE;
    st->f_namemax = 27;  // why? see fs5600.h

    /* your code here */
    int i;
    unsigned char super[FS_BLOCK_SIZE];
    unsigned char map[FS_BLOCK_SIZE];
    int ret = block_read(super, 0, 1);
    if (ret != 0)
        return -EOPNOTSUPP;
    ret = block_read(map, 1, 1);
    if (ret != 0)
        return -EOPNOTSUPP;

    super_t *super_block_p = (super_t *) super;
    st->f_blocks = super_block_p->disk_size - 2;

    unsigned int used_blocks = 0;
    for(i = 2; i < FS_BLOCK_SIZE * 8; i++){
        if (bit_test(map, i)){
            used_blocks ++;
        }
    }
    st->f_bfree = st->f_blocks - used_blocks;
    st->f_bavail = st->f_bfree;
    return 0;
}


/* EXERCISE 2:
 * getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', read 'man 2 stat'.
 *
 * You should:
 *  1. parse the path given by "const char * path",
 *     find the inode of the specified file,
 *       [note: you should implement the helfer function "path2inum"
 *       and use it.]
 *  2. copy inode's information to "struct stat",
 *       [note: you should implement the helper function "inode2stat"
 *       and use it.]
 *  3. and return:
 *     ** success - return 0
 *     ** errors - path translation, ENOENT
 */


int fs_getattr(const char *path, struct stat *sb)
{
    int inode_num = path2inum(path);
    
    if (inode_num <= 0){
        if (inode_num == -ENOTDIR)
            return -ENOTDIR;
        return -ENOENT;
    }
        

    inode_t in;
    int ret = block_read((unsigned char *)&in, inode_num, 1);
    if (ret != 0){
        if (ret == -ENOTDIR)
            return -ENOTDIR;
        return -ENOENT;
    }
        
    
    inode2stat(sb, &in, inode_num);
    return 0;

}

/* EXERCISE 2:
 * readdir - get directory contents.
 *
 * call the 'filler' function for *each valid entry* in the
 * directory, as follows:
 *     filler(ptr, <name>, <statbuf>, 0)
 * where
 *   ** "ptr" is the second argument
 *   ** <name> is the name of the file/dir (the name in the direntry)
 *   ** <statbuf> is a pointer to the struct stat (of the file/dir)
 *
 * success - return 0
 * errors - path resolution, ENOTDIR, ENOENT
 *
 * hints:
 *   - this process is similar to the fs_getattr:
 *     -- you will walk file system to find the dir pointed by "path",
 *     -- then you need to call "filler" for each of
 *        the *valid* entry in this dir
 */
int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
    int inode_num = path2inum(path);

    if (inode_num <= 0)
        return -ENOENT;
    
    inode_t in;
    int ret = block_read((unsigned char *)&in, inode_num, 1);
    if (ret != 0 || !S_ISDIR(in.mode))
        return -ENOTDIR;

    dirent_t dirs[128];
    
    ret = block_read((unsigned char *)dirs, in.ptrs[0], 1);
    if (ret != 0) return -ENOENT;
    
    // find the matched dir
    int k = 0;
    struct stat statbuf;
    inode_t child_in;
    while(k < 128){            
        if(dirs[k].valid){
            
            
            ret = block_read((unsigned char *)&child_in, dirs[k].inode, 1);
            if (ret != 0){
                k ++;
                continue;
            }

            inode2stat(&statbuf, &child_in, dirs[k].inode);
            
            filler(ptr, dirs[k].name, &statbuf, 0);
        }
        k ++;
    }
    
    return 0;
}


/* EXERCISE 3:
 * read - read data from an open file.
 * success: should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return #bytes from offset to end
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
int fs_read(const char *path, char *buf, size_t len, off_t offset,
        struct fuse_file_info *fi)
{
    int inode_num = path2inum(path);
    //printf("%s: %d\n", path, inode_num);
    if (inode_num <= 0){
        if (inode_num == -ENOTDIR)
            return -ENOTDIR;
        return -ENOENT;
    }
        

    inode_t in;
    int ret = block_read((unsigned char *)&in, inode_num, 1);
    if (ret != 0){
        return -EOPNOTSUPP;
    }

    if (S_ISDIR(in.mode)) return -EISDIR;

    if (offset >= in.size) return 0;

    size_t nread = 0;
    size_t block_num = offset / FS_BLOCK_SIZE;
    size_t block_offset = offset % FS_BLOCK_SIZE;
    unsigned char buffer[FS_BLOCK_SIZE];
    while (nread < len && nread + offset < in.size)
    {
        block_read(buffer, in.ptrs[block_num], 1);
        size_t readlength = FS_BLOCK_SIZE - block_offset;
        if (nread + readlength > len){
            readlength = len - nread;            
        }

        if (nread + offset + readlength > in.size){
            readlength = in.size - offset - nread; 
        }
        // printf("offset = %lu nread = %lu readlength=%lu\n", offset, nread, readlength);
        for (size_t i = 0; i <  readlength; i++)
        {
            buf[nread + i] = buffer[block_offset + i];
        }
        nread += readlength;
        block_num ++;
        block_offset = 0;
                
    }
    
    

    return (int) nread;
}


int tokenise(char *path, char **tokens){
    int i = 0;
    char *temp = strtok(path, "/");
    while(temp){
        tokens[i++] = temp;        
        temp = strtok(NULL, "/");
    }

    return i;

}

/* EXERCISE 3:
 * rename - rename a file or directory
 * success - return 0
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
int fs_rename(const char *src_path, const char *dst_path)
{
    int inode_num1 = path2inum(src_path);
    if (inode_num1 <= 0) {
        if (inode_num1 == -ENOTDIR) return -EINVAL;
        return -ENOENT;
    }

    int inode_num2 = path2inum(dst_path);
    if (inode_num2 > 0) {
        return -EEXIST;
    }

    char * src_tokens[11];
    char src_copy[300];
    strcpy(src_copy, src_path + 1);
    int src_count = tokenise(src_copy, src_tokens);

    char * dst_tokens[11];
    char dst_copy[300];
    strcpy(dst_copy, dst_path + 1);
    int dst_count = tokenise(dst_copy, dst_tokens);
    // printf("\nsrccount = %d, dstcount = %d\n", src_count, dst_count);
    if (dst_count != src_count) return -EINVAL;
    int length = 0;
    for (int i = 0; i < src_count - 1; i++)
    {
        
        if(strcmp(src_tokens[i], dst_tokens[i]) != 0) return -EINVAL;
        length += strlen(src_tokens[i]) + 1;
    }

    

    length += 1;
    char parent[300];
    strncpy(parent, src_path, length);
    parent[length] = '\0';

    int inode_no_parent = path2inum(parent);

    if (inode_no_parent < 0 ){
        printf("inode_no_parent should > 0");
        return -EINVAL;
    }    

    // find parent
    inode_t parent_inode;
    dirent_t dirs[128];
    int ret = block_read((unsigned char *)&parent_inode, inode_no_parent, 1);
    if (ret != 0) return -EINVAL;
    ret = block_read((unsigned char *)dirs, parent_inode.ptrs[0], 1);
    if (ret != 0) return -EINVAL;
    int k = 0;
    while (k < 128)
    {
        if (dirs[k].valid && strcmp(dirs[k].name, src_tokens[src_count - 1]) == 0){
            strcpy(dirs[k].name, dst_tokens[src_count - 1]);
            break;
        }
        k ++;
    }
    
    // write back directory data
    ret = block_write((unsigned char *)dirs, parent_inode.ptrs[0], 1);
    if (ret != 0) return -EINVAL;

    return 0;

}

/* EXERCISE 3:
 * chmod - change file permissions
 *
 * success - return 0
 * Errors - path resolution, ENOENT.
 *
 * hints:
 *   - You can safely assume the "mode" is valid.
 *   - notice that "mode" only contains permissions
 *     (blindly assign it to inode mode doesn't work;
 *      why? check out Lab4 instructions about mode)
 *   - S_IFMT might be useful.
 */
int fs_chmod(const char *path, mode_t mode)
{
    int inode_num = path2inum(path);
    if (inode_num < 0){
        return - ENOENT;
    }

    inode_t node;
    
    block_read((unsigned char*)&node, inode_num, 1);
    node.mode &= 0xfffff000;
    node.mode |= 0x00000fff & mode;

    block_write((unsigned char*)&node, inode_num, 1);
    return 0;

}


/* EXERCISE 4:
 * create - create a new file with specified permissions
 *
 * success - return 0
 * errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 128 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 * If the name is too long (longer than 27 letters), return -EINVAL.
 *
 * notes:
 *   - that 'mode' only has the permission bits. You have to OR it with S_IFREG
 *     before setting the inode 'mode' field.
 *   - Ignore the third parameter.
 *   - you will have to implement the helper funciont "alloc_blk" first
 *   - when creating a file, remember to initialize the inode ptrs.
 */
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    uint32_t cur_time = time(NULL);
    struct fuse_context *ctx = fuse_get_context();
    uint16_t uid = ctx->uid;
    uint16_t gid = ctx->gid;

    // check it path is empty
    if (strlen(path) == 0 || path[0] != '/'){
        return -ENOENT;
    }

    //check whether we can find the path 
    int inode_n = path2inum(path);

    if (inode_n > 0){
        return -EEXIST;
    }




    char * src_tokens[11];
    char src_copy[300];
    strcpy(src_copy, path + 1);
    // copy the path and get the items counter   
    //src count == pathc
    int src_count = tokenise(src_copy, src_tokens);
    int length = 0;
    //get the string of parent address length
    for (int i = 0; i < src_count - 1; i++){       
        // +1 means the last '0'
        length += strlen(src_tokens[i]) + 1;
    }
    
    // get the last one string 's len to judge whether the name too long 
    if (strlen(src_tokens[src_count - 1]) > 27){
        return -EINVAL;
    }

    // now I have no idea why here add 1
    length += 1;
    char parent[300];
    //copy n char into parent then I can get parent 
    strncpy(parent, path, length);
    parent[length] = '\0';



    // check whether parent exist, not exist error 
    int inode_no_parent = path2inum(parent);
     
    if (inode_no_parent < 0 ){
        
        return -ENOENT;
    }    

    // find parent
    inode_t parent_inode;
    dirent_t dirs[128];
    block_read((unsigned char *)&parent_inode, inode_no_parent, 1);
    
    //if parent not is dir. ERROR
    if (!S_ISDIR(parent_inode.mode)){
        return -ENOTDIR;
    }

    //! we had check all the items before 

    // time to create 
    int new_inode_num = alloc_blk();
    inode_t new_file;
    new_file.uid = uid;
    new_file.gid = gid;
    new_file.mode = 0;
    new_file.mode |= 0x00000fff & mode;
    new_file.mode |= 1 << 15;
    //printf("MODE = %x\n", new_file.mode);
    new_file.size = 0;
    new_file.ctime = cur_time;
    new_file.mtime = cur_time;
    block_write((unsigned char *)&new_file, new_inode_num, 1);  
    // write the value into its block

    
    // parent must be a dir 
    int k = 0;
    block_read((unsigned char *)dirs, parent_inode.ptrs[0], 1); 
    

    while (k < 128)
    {
         // == 0 means not this is empty, and we will change it 
        if (dirs[k].valid == 0){
            dirs[k].valid = 1;
            
            strcpy(dirs[k].name, src_tokens[src_count-1]);
            
            dirs[k].inode = new_inode_num;
            
            break;
        }
        k ++;
    }
   
    if (k >= 128)
        return -EINVAL;
    //initial parent's dirent 
    block_write((unsigned char *)dirs, parent_inode.ptrs[0], 1);  

       
    // write     
    block_write((unsigned char *)&parent_inode, inode_no_parent, 1);
    return 0;

   
}



/* EXERCISE 4:
 * mkdir - create a directory with the given mode.
 *
 * Note that 'mode' only has the permission bits. You
 * have to OR it with S_IFDIR before setting the inode 'mode' field.
 *
 * success - return 0
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 *
 * hint:
 *   - there is a lot of similaries between fs_mkdir and fs_create.
 *     you may want to reuse many parts (note: reuse is not copy-paste!)
 */
int fs_mkdir(const char *path, mode_t mode)
{
   uint32_t cur_time = time(NULL);
    struct fuse_context *ctx = fuse_get_context();
    uint16_t uid = ctx->uid;
    uint16_t gid = ctx->gid;

    if (strlen(path) == 0 || path[0] != '/'){
        return -ENOENT;
    }

    int inode_n = path2inum(path);

    if (inode_n > 0){
        return -EEXIST;
    }


    char * src_tokens[11];
    char src_copy[300];
    strcpy(src_copy, path + 1);
    int src_count = tokenise(src_copy, src_tokens);
    int length = 0;
    for (int i = 0; i < src_count - 1; i++){       
        
        length += strlen(src_tokens[i]) + 1;
    }
    
    if (strlen(src_tokens[src_count - 1]) > 27){
        return -EINVAL;
    }

    length += 1;
    char parent[300];
    strncpy(parent, path, length);
    parent[length] = '\0';

    int inode_no_parent = path2inum(parent);

    if (inode_no_parent < 0 ){
        
        return -ENOENT;
    }    

    // find parent
    inode_t parent_inode;
    dirent_t dirs[128];
    block_read((unsigned char *)&parent_inode, inode_no_parent, 1);
    
    if (!S_ISDIR(parent_inode.mode)){
        return -ENOTDIR;
    }


    int new_inode_num = alloc_blk();
    inode_t new_file;
    new_file.uid = uid;
    new_file.gid = gid;
    new_file.mode = 0;
    new_file.mode |= 0x00000fff & mode;
    new_file.mode |= 1 << 14;
    //printf("MODE = %x\n", new_file.mode);
    new_file.size = 4096;
    new_file.ctime = cur_time;
    new_file.mtime = cur_time;

    new_file.ptrs[0] = alloc_blk();  

    // initiate the data
    dirent_t new_dires[128];
    memset(new_dires, 0, 128*sizeof(dirent_t));
    //write to disk
    block_write((unsigned char *)new_dires, new_file.ptrs[0], 1);   
    block_write((unsigned char *)&new_file, new_inode_num, 1); 
    

    // add entry in the parent dir
    int k = 0;
    block_read((unsigned char *)dirs, parent_inode.ptrs[0], 1); 
    

    while (k < 128)
    {
         
        if (dirs[k].valid == 0){
            dirs[k].valid = 1;
            
            strcpy(dirs[k].name, src_tokens[src_count-1]);
            
            dirs[k].inode = new_inode_num;
            
            break;
        }
        k ++;
    }
   
    if (k >= 128)
        return -EINVAL;
    block_write((unsigned char *)dirs, parent_inode.ptrs[0], 1);  

       
        
    block_write((unsigned char *)&parent_inode, inode_no_parent, 1);
    return 0;
}


/* EXERCISE 5:
 * unlink - delete a file
 *  success - return 0
 *  errors - path resolution, ENOENT, EISDIR
 *
 * hint:
 *   - you will have to implement the helper funciont "free_blk" first
 *   - remember to delete all data blocks as well
 *   - remember to update "mtime"
 */
int fs_unlink(const char *path)
{
    
    char * src_tokens[11];
    char src_copy[300];
    strcpy(src_copy, path + 1);
    int src_count = tokenise(src_copy, src_tokens);
    int length = 0;
    for (int i = 0; i < src_count - 1; i++){       
        
        length += strlen(src_tokens[i]) + 1;
    }   
    

    length += 1;
    char parent[300];
    strncpy(parent, path, length);
    parent[length] = '\0';

    int inode_no_parent = path2inum(parent);
    if (inode_no_parent < 0){
        return - ENOENT;
    }

    // find parent
    inode_t parent_inode;
    dirent_t dirs[128];
    block_read((unsigned char *)&parent_inode, inode_no_parent, 1);

    if (!S_ISDIR(parent_inode.mode)){
        return -ENOTDIR;
    }
    

    int inode_n = path2inum(path);

    if (inode_n < 0){
        
        return - ENOENT;
    }


    inode_t fileinode;
    block_read((unsigned char *)&fileinode, inode_n, 1);    
    if (S_ISDIR(fileinode.mode)){
        return -EISDIR;
    }

    for (int i = 0; i < DIV_ROUND_UP(fileinode.size, FS_BLOCK_SIZE); i++)
    {
        free_blk(fileinode.ptrs[i]);
    }

    free_blk(inode_n);

    int k = 0;
    block_read((unsigned char *)dirs, parent_inode.ptrs[0], 1);
    
    while (k < 128)
    {
         
        if (dirs[k].valid == 1 && dirs[k].inode == inode_n){
            dirs[k].valid = 0;
            break;
        }
        k ++;
    }

    block_write((unsigned char *)dirs, parent_inode.ptrs[0], 1);
    
    uint32_t cur_time = time(NULL);
    parent_inode.mtime = cur_time;
    block_write((unsigned char *)&parent_inode, inode_no_parent, 1);


    return 0;

}

/* EXERCISE 5:
 * rmdir - remove a directory
 *  success - return 0
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 *
 * hint:
 *   - fs_rmdir and fs_unlink have a lot in common; think of reuse the code
 */
int fs_rmdir(const char *path)
{
   


    char * src_tokens[11];
    char src_copy[300];
    strcpy(src_copy, path + 1);
    int src_count = tokenise(src_copy, src_tokens);
    int length = 0;
    for (int i = 0; i < src_count - 1; i++){       
        
        length += strlen(src_tokens[i]) + 1;
    }   
    

    length += 1;
    char parent[300];
    strncpy(parent, path, length);
    parent[length] = '\0';

    int inode_no_parent = path2inum(parent);
    
    if (inode_no_parent < 0) {
        //printf("PARENT %s does not exists\n", parent);
        return -ENOENT;
    }

    // find parent
    inode_t parent_inode;
    dirent_t dirs[128];
    block_read((unsigned char *)&parent_inode, inode_no_parent, 1);

    if (!S_ISDIR(parent_inode.mode)){
        return -ENOTDIR;
    }

     int inode_n = path2inum(path);

    if (inode_n < 0){        
        return - ENOENT;
    }
    
    inode_t fileinode;
    block_read((unsigned char *)&fileinode, inode_n, 1);    
    if (!S_ISDIR(fileinode.mode)){
        return -ENOTDIR;
    }

    int k = 0;
    block_read((unsigned char *)dirs, fileinode.ptrs[0], 1);
    
    while (k < 128){
         
        if (dirs[k].valid == 1){
            return -ENOTEMPTY;
        }
        k ++;
    }
    
    
    
    free_blk(fileinode.ptrs[0]);
    

    free_blk(inode_n);

    k = 0;
    block_read((unsigned char *)dirs, parent_inode.ptrs[0], 1);
    
    while (k < 128)
    {
         
        if (dirs[k].valid == 1 && dirs[k].inode == inode_n){
            dirs[k].valid = 0;
            break;
        }
        k ++;
    }

    block_write((unsigned char *)dirs, parent_inode.ptrs[0], 1);
    
    uint32_t cur_time = time(NULL);
    parent_inode.mtime = cur_time;
    block_write((unsigned char *)&parent_inode, inode_no_parent, 1);


    return 0;
}

/* EXERCISE 6:
 * write - write data to a file
 * success - return number of bytes written. (this will be the same as
 *           the number requested, or else it's an error)
 *
 * Errors - path resolution, ENOENT, EISDIR, ENOSPC
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them,
 *   but we don't)
 *  return ENOSPC when the data exceed the maximum size of a file.
 */
int fs_write(const char *path, const char *buf, size_t len,
         off_t offset, struct fuse_file_info *fi)
{
    int inode_num = path2inum(path);
    if (inode_num <= 0){
        if (inode_num == -ENOTDIR)
            return -ENOTDIR;
        return -ENOENT;
    }
    
    inode_t in;
    int ret = block_read((unsigned char *)&in, inode_num, 1);
    if (ret != 0){
        return -EOPNOTSUPP;
    }

    if (S_ISDIR(in.mode)) return -EISDIR;

    if (offset > in.size){        
        return 0;
    } 

    if (offset + len > FS_BLOCK_SIZE * NUM_PTRS_INODE)
        return -ENOSPC;
    
    size_t new_size = offset + len;
    

    size_t nread = 0;
    size_t block_num = offset / FS_BLOCK_SIZE;
    size_t block_offset = offset % FS_BLOCK_SIZE;
    unsigned char buffer[FS_BLOCK_SIZE];

    
    while (nread < len && nread + offset < new_size)
    {
        if (block_num >= DIV_ROUND_UP(in.size, FS_BLOCK_SIZE)){
            in.ptrs[block_num] = alloc_blk();
            if (in.ptrs[block_num] <= 0) return -ENOSPC;
        }
        block_read(buffer, in.ptrs[block_num], 1);
        size_t readlength = FS_BLOCK_SIZE - block_offset;
        if (nread + readlength > len){
            readlength = len - nread;  
                      
        }

        
        // printf("offset = %lu nread = %lu readlength=%lu\n", offset, nread, readlength);
        for (size_t i = 0; i <  readlength; i++)
        {
            buffer[block_offset + i] = buf[nread + i];
        }
        nread += readlength;       
                
        block_write(buffer, in.ptrs[block_num], 1);
        block_num ++;
        block_offset = 0;
                
    }
    
    // if (DIV_ROUND_UP(in.size, FS_BLOCK_SIZE) > DIV_ROUND_UP(new_size, FS_BLOCK_SIZE))
    // for (int i = DIV_ROUND_UP(in.size, FS_BLOCK_SIZE); i >= DIV_ROUND_UP(new_size, FS_BLOCK_SIZE); i--)
    // {
    //     free_blk(in.ptrs[i-1]);
    // }

    if (new_size > in.size)
        in.size = new_size;
    uint32_t cur_time = time(NULL);
    in.mtime = cur_time;

    block_write((unsigned char *)&in, inode_num, 1);    

    return (int) nread;

}

/* EXERCISE 6:
 * truncate - truncate file to exactly 'len' bytes
 * note that CS5600 fs only allows len=0, meaning discard all data in this file.
 *
 * success - return 0
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
int fs_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0) {
        return -EINVAL;        /* invalid argument */
    }

    int inode_num = path2inum(path);
    if (inode_num <= 0){
        if (inode_num == -ENOTDIR)
            return -ENOTDIR;
        return -ENOENT;
    }
    
    inode_t in;
    int ret = block_read((unsigned char *)&in, inode_num, 1);
    if (ret != 0){
        return -EOPNOTSUPP;
    }

    if (S_ISDIR(in.mode)) return -EISDIR;

    for (int i = 0; i < DIV_ROUND_UP(in.size, FS_BLOCK_SIZE); i++)
    {
        free_blk(in.ptrs[i]);        
    }

    in.size = 0;
    block_write((unsigned char *)&in, inode_num, 1);
    return 0;
}

/* EXERCISE 6:
 * Change file's last modification time.
 *
 * notes:
 *  - read "man 2 utime" to know more.
 *  - when "ut" is NULL, update the time to now (i.e., time(NULL))
 *  - you only need to use the "modtime" in "struct utimbuf" (ignore "actime")
 *    and you only have to update "mtime" in inode.
 *
 * success - return 0
 * Errors - path resolution, ENOENT
 */
int fs_utime(const char *path, struct utimbuf *ut)
{
    int inode_num = path2inum(path);
    if (inode_num <= 0){
        if (inode_num == -ENOTDIR)
            return -ENOTDIR;
        return -ENOENT;
    }
    
    inode_t in;
    int ret = block_read((unsigned char *)&in, inode_num, 1);
    if (ret != 0){
        return -EOPNOTSUPP;
    }

    if (S_ISDIR(in.mode)) return -EISDIR;

    if (!ut){
        in.mtime = time(NULL);
    }else{
        in.mtime = (uint32_t) ut->modtime;
    }

    block_write((unsigned char *)&in, inode_num, 1);
    return 0;

}



/* operations vector. Please don't rename it, or else you'll break things
 */
struct fuse_operations fs_ops = {
    .init = fs_init,            /* read-mostly operations */
    .statfs = fs_statfs,
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read,
    .rename = fs_rename,
    .chmod = fs_chmod,

    .create = fs_create,        /* write operations */
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .write = fs_write,
    .truncate = fs_truncate,
    .utime = fs_utime,
};

