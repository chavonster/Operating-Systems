/*
 * CacheFS.cpp
 */
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <zconf.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include "CacheFS.h"
#include "Cache.h"

#define SUCCESS 0
#define FAILURE -1
#define TEMP_FOLDER "/tmp"
#define HITS "Hits number: "
#define MISS "Misses number: "
#define FIRST_COUNTER 1
#define MINIMAL_SIZE_PARTITION 0
#define MAXIMAL_SIZE_PARTITION 1

int next_fileFd = 1;
Cache* cache;
int block_size;
std::map<string, int> path_counter;
std::map<string, int> path_fd;
std::map<int, string> fakeFd_path;
FILE* openLogFile(const char *log_path);

/**
 Initializes the CacheFS.
 Assumptions:
	1. CacheFS_init will be called before any other function.
	2. CacheFS_init might be called multiple times, but only with CacheFS_destroy
  	   between them.

 Parameters:
	blocks_num   - the number of blocks in the buffer cache
	cache_algo   - the cache algorithm that will be used
    f_old        - the percentage of blocks in the old partition (rounding down)
				   relevant in FBR algorithm only
    f_new        - the percentage of blocks in the new partition (rounding down)
				   relevant in FBR algorithm only
 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. new).
		2. invalid parameters.
	Invalid parameters are:
		1. blocks_num is invalid if it's not a positive number (zero is invalid too).
		2. f_old is invalid if it is not a number between 0 to 1 or
		   if the size of the partition of the old blocks is not positive.
		3. fNew is invalid if it is not a number between 0 to 1 or
		   if the size of the partition of the new blocks is not positive.
		4. Also, fOld and fNew are invalid if the fOld+fNew is bigger than 1.

		Pay attention: bullets 2-4 are relevant (and should be checked)
		only if cache_algo is FBR.

 For example:
 CacheFS_init(100, FBR, 0.3333, 0.5)
 Initializes a CacheFS that uses FBR to manage the cache.
 The cache contains 100 blocks, 33 blocks in the old partition,
 50 in the new partition, and the remaining 17 are in the middle partition.
 */
int CacheFS_init(int blocks_num, cache_algo_t cache_algo,
                 double f_old , double f_new  )
{
    if(blocks_num <= 0)
    {
        return FAILURE;
    }
    if(cache_algo == FBR)
    {
        if((f_old > MAXIMAL_SIZE_PARTITION || f_old < MINIMAL_SIZE_PARTITION)
           || (f_new > MAXIMAL_SIZE_PARTITION ||
                f_new < MINIMAL_SIZE_PARTITION) ||
                ((f_old + f_new) > MAXIMAL_SIZE_PARTITION))
        {
            return FAILURE;
        }
    }
    //create the cache with the given algorithm
    switch(cache_algo)
    {
        case LFU:
        {
            cache = new (nothrow) CacheLFU(blocks_num);
        }
        break;
        case LRU:
        {
            cache = new (nothrow) CacheLRU(blocks_num);
        }
        break;
        case FBR:
        {
            cache = new (nothrow) CacheFBR(blocks_num, f_old, f_new);
        }
        break;
        default:;
    }
    if(cache == nullptr)
    {
        return FAILURE;
    }
    //initilize the maps that controlling the open and the close of files
    path_counter = map<string, int>();
    path_fd = map<string, int>();
    fakeFd_path = map<int, string>();
    //gets the block size
    struct stat fi;
    stat(TEMP_FOLDER, &fi);
    block_size = fi.st_blksize;
    return SUCCESS;
}

/**
 Destroys the CacheFS.
 This function releases all the allocated resources by the library.

 Assumptions:
	1. CacheFS_destroy will be called only after CacheFS_init (one destroy per one init).
	2. After CacheFS_destroy is called,
	   the next CacheFS's function that will be called is CacheFS_init.
	3. CacheFS_destroy is called only after all the open files already closed.
	   In other words, it's the user responsibility to close the files before destroying
	   the CacheFS.

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail if a system call or a library function fails.
*/
int CacheFS_destroy()
{
    fakeFd_path.clear();
    path_counter.clear();
    path_fd.clear();
    delete(cache);
    return SUCCESS;
}

/**
 File open operation.
 Receives a path for a file, opens it, and returns an id
 for accessing the file later

 Notes:
	1. You must open the file with the following flags: O_RDONLY | O_DIRECT | O_SYNC
	2. The same file might be opened multiple times.
	   Like in POISX, it's valid.
	3. The pathname is not unique per file, because:
		a. relative paths are not unique: "myFolder/../tmp" and "tmp".
		b. we might open a link ("short-cut") to the file

 Parameters:
    pathname - the path to the file that will be opened

 Returned value:
    In case of success:
		Non negative value represents the id of the file.
		This may be the file descriptor, or any id number that you wish to create.
		This id will be used later to read from the file and to close it.

 	In case of failure:
		Negative number.
		A failure will occur if:
			1. System call or library function fails (e.g. open).
			2. Invalid pathname. Pay attention that we support only files under
			   "/tmp" due to the use of NFS in the Aquarium.
 */
int CacheFS_open(const char *pathname)
{
    char buf[PATH_MAX + 1];
    char* res = realpath(pathname, buf);
    if (res == nullptr)
    {
        return FAILURE;
    }
    string res_str(res);
    if(res_str.find(TEMP_FOLDER, 0) != 0)
    {
        return FAILURE;
    }
    //every open of a file gets fake fd
    fakeFd_path.insert(std::pair<int, string>(next_fileFd, res_str));
    int fakeFd = next_fileFd;
    next_fileFd++;
    //this file alredy opened
    if(path_counter.find(res_str) != path_counter.end())
    {
        path_counter[res_str]++;
    }
    else //first open of a file
    {
        int fd = open(res, O_RDONLY | O_DIRECT | O_SYNC);
        if (fd < 0)
        {
            return FAILURE;
        }
        path_counter.insert(std::pair<string, int>(res_str, FIRST_COUNTER));
        path_fd.insert(std::pair<string, int>(res_str, fd));
    }
    return fakeFd;
}

/**
 File close operation.
 Receives id of a file, and closes it.

 Returned value:
	0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. a system call or a library function fails (e.g. close).
		2. invalid file_id. file_id is valid if"f it was returned by
		CacheFS_open, and it is not already closed.
 */
int CacheFS_close(int file_id)
{
    int result;
    if(fakeFd_path.find(file_id) != fakeFd_path.end())
    {
        string path = fakeFd_path[file_id];
        // this is that last open file, so now can close the file and move out
        // from the map
        if(path_counter[path] == 1)
        {
            int fd = path_fd[path];
            result = close(fd);
            path_counter.erase(path);
            path_fd.erase(path);
        }
        else //can't close the file because it was opened several times
        {
            path_counter[path]--;
            result = SUCCESS;
        }
        fakeFd_path.erase(file_id);
        return result;
    }
    else
    {
        return FAILURE;
    }
}

/**
   Read data from an open file.

   Read should return exactly the number of bytes requested except
   on EOF or error. For example, if you receive size=100, offset=0,
   but the size of the file is 10, you will initialize only the first
   ten bytes in the buff and return the number 10.

   In order to read the content of a file in CacheFS,
   We decided to implement a function similar to POSIX's pread, with
   the same parameters.

 Returned value:
    In case of success:
		Non negative value represents the number of bytes read.
		See more details above.

 	In case of failure:
		Negative number.
		A failure will occur if:
			1. a system call or a library function fails (e.g. pread).
			2. invalid parameters
				a. file_id is valid if"f it was returned by
			       CacheFS_open, and it wasn't already closed.
				b. buf is invalid if it is NULL.
				c. offset is invalid if it's negative
				   [Note: offset after the end of the file is valid.
				    In this case, you need to return zero,
				    like posix's pread does.]
				[Note: any value of count is valid.]
 */
int CacheFS_pread(int file_id, void *buf, size_t count, off_t offset)
{
    if(buf == NULL || offset < 0)
    {
        return FAILURE;
    }
    bool fd_is_found = false;
    string path;
    if(fakeFd_path.find(file_id) != fakeFd_path.end())
    {
        fd_is_found = true;
        path = fakeFd_path[file_id];
    }
    if (!fd_is_found)
    {
        return FAILURE;
    }
    //gets the file size
    struct stat statbuf;
    int fd = path_fd[path];
    if(fstat(fd, &statbuf))
    {
        return FAILURE;
    }
    unsigned int file_size = statbuf.st_size;
    if (offset >= file_size)
    {
        return 0;
    }
    if (count <= 0)
    {
        return 0;
    }
    size_t size_remain = count;
    if (count > file_size)
    {
        size_remain = file_size;
    }
    unsigned int result = (file_size - offset);
    if (result < count)
    {
        size_remain = result;
    }
    //how many blocks need to read from the file
    int offset_block_idx = (int) floor((double) offset/block_size);
    int last_block_idx = (int) ceil((double) (offset + size_remain)
                                    /block_size);
    char* data;
    int how_much_written = 0;
    int how_much_to_copy;
    off_t loop_offset;
    size_t loop_count;
    CacheBlock* current_block;
    for(int i = offset_block_idx; i < last_block_idx; i++)
    {
        BLOCK_PAIR* block_pair = cache->searchBlock(path, i);
        //the block is not in the cache, need to read from the disk
        if(block_pair == nullptr) {
            cache->incrementMissesNum();
            data = (char *) aligned_alloc((size_t) block_size,
                                          (size_t) block_size);
            int res = pread(fd, data, block_size, i * block_size);
            if (!res) {
                return FAILURE;
            }
            long int timeStamp = 0;
            current_block = new CacheBlock(data, path, i, timeStamp, 1);
            current_block->updateTimeStamp();
            if (cache->isCacheFull()) {
                cache->removeBlock();
            }
            cache->addBlock(current_block);
        }
        else //the block is already in the cache
        {
            current_block = block_pair->first;
            int index_in_cache = block_pair->second;
            cache->incrementHitsNum();
            cache->incrementCount(current_block, index_in_cache);
            block_pair->first->updateTimeStamp();
            delete(block_pair);
        }
        if (i == offset_block_idx)
        {
            loop_offset = offset % block_size;
        }
        else
        {
            loop_offset = 0;
        }
        if (i == last_block_idx-1)
        {
            loop_count = size_remain - how_much_written;
        }
        else
        {
            loop_count = block_size;
        }
        if (i == offset_block_idx && i != last_block_idx -1)
        {
            how_much_to_copy = loop_count - loop_offset;
        }
        else
        {
            how_much_to_copy = loop_count;
        }
        memcpy((char*)buf + how_much_written, current_block->getData()+
                loop_offset, how_much_to_copy);
        how_much_written += how_much_to_copy;
    }
    return how_much_written;

}

/**
This function writes the current state of the cache to a file.
The function writes a line for every block that was used in the cache
(meaning, each block with at least one access).
Each line contains the following values separated by a single space.
	1. Full path of the file
	2. The number of the block. Pay attention: this is not the number in the cache,
	   but the enumeration within the file itself, starting with 0 for the first
	   block in each file.
The order of the entries is from the last block that will be evicted from the cache
to the first (next) block that will be evicted.

Notes:
	1. If log_path is a path to existed file - the function will append the cache
	   state (described above) to the cache.
	   Otherwise, if the path is valid, but the file doesn't exist -
	   a new file will be created.
	   For example, if "/tmp" contains a single folder named "folder", then
			"/tmp/folder/myLog" is valid, while "/tmp/not_a_folder/myLog" is invalid.
	2. Of course, this operation doesn't change the cache at all.
	3. log_path doesn't have to be under "/tmp".
	3. This function might be useful for debugging purpose as well as auto-tests.
	   Make sure to follow the syntax and order as explained above.

 Parameter:
	log_path - a path of the log file. A valid path is either: a path to an existing
			   log file or a path to a new file (under existing directory).

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. open, write).
		2. log_path is invalid.
 */
int CacheFS_print_cache (const char *log_path)
{
    FILE *fptr = openLogFile(log_path);

    if (fptr == NULL)
    {
        return FAILURE;
    }
    cache->sortBuffer();
    for (CacheBlock* cur_block : cache->getCacheBuffer())
    {
        fprintf(fptr, "%s %d\n", cur_block->getPath().c_str(),
                cur_block->getBlockInd());
    }

    if (fclose(fptr))
    {
        return FAILURE;
    }
    return SUCCESS;
}

/**
This function writes the statistics of the CacheFS to a file.
This function writes exactly the following lines:
Hits number: HITS_NUM.
Misses number: MISS_NUM.

Where HITS_NUM is the number of cache-hits, and MISS_NUM is the number of cache-misses.
A cache miss counts the number of fetched blocks from the disk.
A cache hit counts the number of required blocks that were already stored
in the cache (and therefore we didn't fetch them from the disk again).

Notes:
	1. If log_path is a path to existed file - the function will append the cache
	   state (described above) to the cache.
	   Otherwise, if the path is valid, but the file doesn't exist -
	   a new file will be created.
	   For example, if "/tmp" contains a single folder named "folder", then
			"/tmp/folder/myLog" is valid, while "/tmp/not_a_folder/myLog" is invalid.
	2. Of course, this operation doesn't change the cache at all.
	3. log_path doesn't have to be under "/tmp".

 Parameter:
	log_path - a path of the log file. A valid path is either: a path to an existing
			   log file or a path to a new file (under existing directory).

 Returned value:
    0 in case of success, negative value in case of failure.
	The function will fail in the following cases:
		1. system call or library function fails (e.g. open, write).
		2. log_path is invalid.
 */
int CacheFS_print_stat (const char *log_path)
{
    FILE *fptr = openLogFile(log_path);
    if (fptr == NULL)
    {
        return FAILURE;
    }

    fprintf(fptr, "%s%d\n", HITS, cache->getHitsNum());
    fprintf(fptr, "%s%d\n", MISS, cache->getMissesNum());

    if (fclose(fptr))
    {
        return FAILURE;
    }
    return SUCCESS;
}

/**
 * Opens a log file to print the cache in the file, if already exits opens it
 * at the mode of append.
 * @param log_path
 * @return FILE*
 */
FILE* openLogFile(const char *log_path)
{
    FILE *fptr;
    fptr = fopen(log_path, "a+");
    if (fptr == NULL) //if file doesn't exist, create it
    {
        fptr = fopen(log_path, "w");
    }
    return fptr;
}