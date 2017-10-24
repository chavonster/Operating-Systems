#ifndef EX4_BLOCK_H
#define EX4_BLOCK_H

#include <ctime>
#include <string>

#define TO_NANO 1000000000

using namespace std;

/*
 * The class of the block in the cache that holds the block variables and
 * functions.
 */
class CacheBlock
{
private:
    const char* data;
    string path;
    int blockInd;
    long int timeStamp;
    int counter;
public:
    /**
     * Constructor of the cache block, each block has a data, the path of the
     * file, the index, a time stamp and a count refence.
     * @param data
     * @param path
     * @param blockInd
     * @param timeStamp
     * @param counter
     * @return
     */
    CacheBlock(const char* data, string path, int blockInd, time_t
    timeStamp, int counter);
    /**
     * Destructor of the cache block.
     */
    ~CacheBlock();
    /**
     * Getter to the data.
     * @return const char* the data.
     */
    const char* getData();
    /**
     * Updates the time stamp of the block in nanoseconds.
     */
    void updateTimeStamp();
    /**
     * Getter to the path of the file, of the block.
     * @return string- the path
     */
    string getPath();
    /**
     * Getter of the block index.
     * @return int- the index.
     */
    int getBlockInd();
    /**
     * Increments the count refence of the block.
     */
    void incrementCount();
    /**
     * Getter of the count refence of the block.
     * @return
     */
    int getCounter();
    /**
     * Getter of the time stamp of the block.
     * @return long int- the time stamp in nanoseconds.
     */
    long int getTimeStamp();
    /**
     * Sets the data of the block.
     * @param data
     */
    void setData(char* data);
};


#endif //EX4_BLOCK_H
