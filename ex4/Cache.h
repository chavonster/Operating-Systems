#include <string>
#include <vector>
#include "CacheBlock.h"
#ifndef EX4_CACHE_H
#define EX4_CACHE_H
using namespace std;

// Defs
typedef pair<CacheBlock*, int> BLOCK_PAIR;
enum partitionFBR{
    NEW,
    MID,
    OLD
};

/*
* This class is the base class that holds the cache basic variables and
 * functions.
*/
class Cache {
public:
    /*
     * Constructor.
     */
    Cache();
    /*
     * Destructor.
     */
    virtual ~Cache();
    /**
     * Sorts the blocks in the buffer, according to the LRU algorithm.
     */
    virtual void sortBuffer();
    /**
     * Searchs a spesific block in the cache bu the path of the file and the
     * index.
     * @param path
     * @param index
     * @return if finds the block returns BLOCK_PAIR*, if not nullptr
     */
    virtual BLOCK_PAIR* searchBlock(string path, int index);
    /**
     * Adds a block to the cache.
     * @param block
     */
    virtual void addBlock(CacheBlock* block) = 0;
    /**
     * Checks if the cache is full.
     * @return true if it's full, else false.
     */
    virtual bool isCacheFull();
    /**
     * Removes a block from the cache according to the algorithm and frees the
     * allocated memory.
     */
    virtual void removeBlock();
    /**
     * Getter of the cache buffer.
     * @return vector<CacheBlock*>
     */
    virtual vector<CacheBlock*> getCacheBuffer();
    /**
     * Increments the reference count of the spesific block at the index in the
     * cache.
     * @param block
     * @param indexCache
     */
    virtual void incrementCount(CacheBlock* block, int indexCache) = 0;
    /**
     * Initilize the cache parametrs according to the algorithm at the spesific
     * constructor.
     * @param blocks_num
     */
    void initCacheParams(int blocks_num);
    /**
     * Getter to the hits number of the cache.
     * @return int- the hits num.
     */
    int getHitsNum();
    /**
     * If was a hit increment the hits number by one.
     */
    void incrementHitsNum();
    /**
     * Getter to the misses number of the cache.
     * @return int- the misses num.
     */
    int getMissesNum();
    /**
     * If was a miss increment the misses number by one.
     */
    void incrementMissesNum();
protected:
    vector<CacheBlock*> cacheBuf;
    unsigned int blocks_in_cache;
    int hits_num;
    int misses_num;
};

/*
 * This class holds the cache that uses the algorithm of LFU, and inherited from the Cache class.
 */
class CacheLFU: public Cache {
public:
    /**
     * Constructor of the cache that uses the LFU algorithm.
     * @param blocks_num
     * @return
     */
    CacheLFU(int blocks_num);
    /**
     * Destructor of the cache that uses the LFU algorithm.
     */
    virtual ~CacheLFU();
    /**
     * Sorts the cache bu the refence count.
     */
    void sortBuffer() override;
    /**
     * Adds a block to the cache that uses the LFU algorithm.
     * @param block
     */
    void addBlock(CacheBlock* block);
    /**
     * Increments the refence count.
     * @param block
     * @param indexCache
     */
    void incrementCount(CacheBlock* block, int indexCache);
};

/*
 * This class holds the cache that uses the algorithm of LRU, and inherited from the Cache class.
 */
class CacheLRU: public Cache {
public:
    /**
     * Constructor of the cache that uses the LRU algorithm.
     * @param blocks_num
     * @return
     */
    CacheLRU(int blocks_num);
    /**
     * Destructor of the cache that uses the LRU algorithm.
     */
    virtual ~CacheLRU();
    /**
     * Adds a block to the cache that uses the LFU algorithm.
     * @param block
     */
    void addBlock(CacheBlock* block);
    /**
     * Doesn't increment the count refence.
     * @param block
     * @param indexCache
     */
    void incrementCount(CacheBlock* block, int indexCache);
};

/*
 * This class holds the cache that uses the algorithm of FBR, and inherited from the Cache class.
 */
class CacheFBR: public Cache {
public:
    unsigned long old_size;
    unsigned long mid_size;
    unsigned long new_size;

    /**
     * Constructor of the cache that uses the FBR algorithm, and defines the
     * three partitions.
     * @param blocks_num
     * @param f_old
     * @param f_new
     * @return
     */
    CacheFBR(int blocks_num, double f_old, double f_new);
    /**
     * Destructor of the cache that uses the FBR algorithm.
     */
    virtual ~CacheFBR();
    /**
     * Adds a block to the cache that uses the FBR algorithm to the new
     * partititon.
     * @param block
     */
    void addBlock(CacheBlock* block) override;
    /**
     * Removes a block from the old section with the most lower reference
     * counts, if there are teo blocks with the same count, will remove by LRU.
     * Frees the allocated memory.
     */
    void removeBlock() override;
    /**
     * Increments the refence count if it's in old or middle partition, if it's
     * in new not increment, and moves the block to the start of the new
     * partition.
     * @param block
     * @param indexCache
     */
    void incrementCount(CacheBlock* block, int indexCache) override;
    /**
     * Gets the partition- OLD, NEW or MID at this index.
     * @param index
     * @return
     */
    partitionFBR getPartition(unsigned int index);
};
#endif //EX4_CACHE_H
