#include <sys/time.h>
#include "CacheBlock.h"

CacheBlock::CacheBlock(const char* data, string path, int blockInd,
                       time_t timeStamp, int counter):
        data(data), path(path), blockInd(blockInd), timeStamp(timeStamp),
        counter(counter)
        {

        }

CacheBlock::~CacheBlock()
{

}

const char* CacheBlock::getData() {
    return data;
}

void CacheBlock::updateTimeStamp()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    timeStamp = ts.tv_sec * TO_NANO + ts.tv_nsec;
}

int CacheBlock::getBlockInd()
{
    return blockInd;
}

string CacheBlock::getPath()
{
    return path;
}

void CacheBlock::incrementCount()
{
    counter++;
}

int CacheBlock::getCounter()
{
    return counter;
}

long int CacheBlock::getTimeStamp()
{
    return timeStamp;
}

void CacheBlock::setData(char *data)
{
    this->data = data;
}