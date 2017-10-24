#include <cmath>
#include <algorithm>
#include <malloc.h>
#include "Cache.h"

/**
 * Comparator to the sort function of the LFU algorithm- compares by the
 * refence count.
 * @param first
 * @param second
 * @return true if the first element bigger than the second, else false.
 */
bool compareFreqLFU (CacheBlock* first, CacheBlock* second)
{
    return first->getCounter() > second->getCounter();
}

/**
 * Comparator to the sort function of the LRU algorithm- compares by the
 * time stamps.
 * @param first
 * @param second
 * @return true if the first element bigger than the second, else false.
 */
bool compareFreqLRU (CacheBlock* first, CacheBlock* second)
{
    return first->getTimeStamp() > second->getTimeStamp();
}

//Cache:
Cache::Cache()
{

}

Cache::~Cache()
{
    for (CacheBlock* block : cacheBuf)
    {
        free((void*)block->getData());
        block->setData(NULL);
        delete(block);
        block = nullptr;
    }
    cacheBuf.clear();
}

void Cache::sortBuffer()
{
    std::sort(cacheBuf.begin(), cacheBuf.end(), compareFreqLRU);
}

bool Cache::isCacheFull()
{
    if (blocks_in_cache == cacheBuf.size())
    {
        return true;
    }
    return false;
}

vector<CacheBlock*> Cache::getCacheBuffer()
{
    return this->cacheBuf;
}

int Cache::getHitsNum()
{
    return hits_num;
}

int Cache::getMissesNum()
{
    return misses_num;
}

void Cache::incrementHitsNum()
{
    hits_num++;
}

void Cache::incrementMissesNum()
{
    misses_num++;
}

BLOCK_PAIR* Cache::searchBlock(string path, int index)
{
    int i = 0;
    for (CacheBlock* cur_block : getCacheBuffer())
    {
        if (!(cur_block->getPath().compare(path)) && cur_block->getBlockInd()
                                                     == index)
        {
            BLOCK_PAIR* block_pair = new BLOCK_PAIR();
            block_pair->first = cur_block;
            block_pair->second = i;
            return block_pair;
        }
        i++;
    }
    return nullptr;
}

void Cache::removeBlock()
{
    this->sortBuffer();
    CacheBlock* block_to_delete = cacheBuf.back();
    free((void*)block_to_delete->getData());
    block_to_delete->setData(NULL);
    delete(block_to_delete);
    cacheBuf.pop_back();
}

void Cache::initCacheParams(int blocks_num)
{
    cacheBuf = vector<CacheBlock*>();
    blocks_in_cache = blocks_num;
    misses_num = 0;
    hits_num = 0;
}

//CacheLFU:
void CacheLFU::addBlock(CacheBlock *block)
{
    cacheBuf.push_back(block);
}

CacheLFU::CacheLFU(int blocks_num)
{
    initCacheParams(blocks_num);
}

CacheLFU::~CacheLFU()
{

}

void CacheLFU::incrementCount(CacheBlock *block, int cacheIndex)
{
    if (cacheIndex)
    {

    }
    block->incrementCount();
}

void CacheLFU::sortBuffer()
{
    std::sort(cacheBuf.begin(), cacheBuf.end(), compareFreqLFU);
}

//CacheLRU:
void CacheLRU::addBlock(CacheBlock *block)
{
    cacheBuf.push_back(block);
}

CacheLRU::CacheLRU(int blocks_num)
{
    initCacheParams(blocks_num);
}

CacheLRU::~CacheLRU()
{

}

void CacheLRU::incrementCount(CacheBlock *block, int cacheIndex)
{
    if (block && cacheIndex)
    {
        
    }
}

//CacheFBR:
CacheFBR::CacheFBR(int blocks_num, double f_old, double f_new)
{
    initCacheParams(blocks_num);
    old_size = (unsigned long) floor(blocks_num*f_old);
    new_size = (unsigned long) floor(blocks_num*f_new);
    mid_size = blocks_num - old_size - new_size;
}

CacheFBR::~CacheFBR()
{

}

partitionFBR CacheFBR::getPartition(unsigned int index)
{
    if (index < new_size)
    {
        return NEW;
    }
    else if (index > old_size)
    {
        return OLD;
    }
    else
    {
        return MID;
    }
}

void CacheFBR::incrementCount(CacheBlock *block, int cacheIndex)
{
    partitionFBR cur_partition = this->getPartition(cacheIndex);
    if(cur_partition == OLD || cur_partition == MID)
    {
        block->incrementCount();
    }
    this->cacheBuf.erase(cacheBuf.begin()+cacheIndex);
    this->cacheBuf.insert(cacheBuf.begin(), block);
}

void CacheFBR::addBlock(CacheBlock *block)
{
    cacheBuf.insert(cacheBuf.begin(), block);
}

void CacheFBR::removeBlock()
{
    unsigned int iterations_counter = 0;
    vector<CacheBlock*> minimal_counter;
    int start_index = new_size;
    minimal_counter.push_back(cacheBuf.at(start_index));
    for(unsigned int i = start_index+1; i < cacheBuf.size(); i++)
    {
        for(unsigned int j = 0; j < minimal_counter.size(); j ++)
        {
            if(cacheBuf.at(i)->getCounter() <
                    minimal_counter.at(j)->getCounter())
            {
                minimal_counter.erase(minimal_counter.begin()+j);
                iterations_counter++;
            }
            else if(cacheBuf.at(i)->getCounter() ==
               minimal_counter.at(j)->getCounter())
            {
                iterations_counter++;
            }

        }
        if (iterations_counter >= minimal_counter.size())
        {
            minimal_counter.push_back(cacheBuf.at(i));
        }
        iterations_counter = 0;
    }
    CacheBlock* cur_block = minimal_counter.at(0);
    for (CacheBlock* elem : minimal_counter)
    {
        if (cur_block->getTimeStamp() > elem->getTimeStamp())
        {
            cur_block = elem;
        }
    }
    vector<CacheBlock*>::iterator position =
            std::find(cacheBuf.begin(), cacheBuf.end(), cur_block);
    free((void*)cur_block->getData());
    cur_block->setData(NULL);
    delete(cur_block);
    cacheBuf.erase(position);
}