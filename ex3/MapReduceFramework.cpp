#include <vector>
#include <pthread.h>
#include <iostream>
#include <map>
#include <list>
#include <semaphore.h>
#include <algorithm>
#include <sys/time.h>
#include <stdlib.h>
#include <libltdl/lt_system.h>
#include "MapReduceFramework.h"

using namespace std;

//CONSTANTS
#define SIZE_OF_CHUNK 10
#define SEC_TO_NANOSEC 1000000000
#define MICRO_TO_NANOSEC 1000
#define BUFF_SIZE_FOR_TIME 80
#define INIT_FRAMEWORK_MSG "RunMapReduceFramework started with %d threads\n"
#define CREATE_THREAD_MSG "Thread %s created %s\n"
#define TERMINATE_THREAD_MSG "Thread %s terminated %s\n"
#define MAP_SHUFFLE_TIME_MSG "Map and Shuffle took %ld ns\n"
#define REDUCE_TIME_MSG "Reduce took %ld ns\n"
#define MAPREDUCE_DONE_MSG "RunMapReduceFramework finished\n"
#define EXEC_MAP_NAME "ExecMap"
#define SHUFFLE_NAME "Shuffle"
#define EXEC_REDUCE_NAME "ExecReduce"
#define LOG_FILE_NAME "MapReduceFramework.log" //TODO add dot!!!
#define ERROR_MSG_A "MapReduceFramework Failure: "
#define ERROR_MSG_B " failed."
#define ERROR_LOCK_MUTEX "pthread_mutex_lock"
#define ERROR_UNLOCK_MUTEX "pthread_mutex_unlock"
#define ERROR_INIT_MUTEX "pthread_mutex_init"
#define ERROR_DESTROY_MUTEX "pthread_mutex_destroy"
#define ERROR_CREATE "pthread_create"
#define ERROR_JOIN "pthread_join"
#define ERROR_GET_TIME "gettimeofday"
#define ERROR_OPEN_FILE "open ofstream"
#define ERROR_CLOSE_FILE "close ofstream"
#define ERROR_SEMAPHORE_INIT "semaphore_init"
#define ERROR_SEMAPHORE_DESTORY "semaphore_destory"
#define ERROR_SEMAPHORE_WAIT "Semaphore_wait"
#define ERROR_SEMAPHORE_POST "Semaphore_post"
#define ERROR_FPRINT "fprintf"

// Defs

/*
 * Defines a comparator for our map containers based on the implementation
 * of the < operator for k2 type.
 */
struct classcomp {
    bool operator() (const k2Base* first_item, const k2Base* second_item) const{
        return *first_item < *second_item;
    }
};
/*
 * Defines a comparator for our threads.
 */
struct compareThreads {
    bool operator() (const pthread_t& first_thread, const pthread_t& sec_thread)
    const {
        return (first_thread < sec_thread);
    }
};
typedef pthread_mutex_t mutex_t;
typedef pair<k2Base*, v2Base*> MAP_OUTPUT_TYPE;
typedef list<MAP_OUTPUT_TYPE> MAP_OUTPUT_LIST;
typedef map<pthread_t, MAP_OUTPUT_LIST, compareThreads> MAP_CONTAINERS;
typedef pair<k2Base*, std::vector<v2Base*>> SHUFFLE_ITEM;
typedef map<k2Base*, std::vector<v2Base*>, classcomp> SHUFFLE_LIST;
typedef map<pthread_t, OUT_ITEMS_VEC, compareThreads> REDUCE_CONTAINERS;

// GLOBALS
MapReduceBase* map_reduce_base;
mutex_t chunks_index_mutex;
mutex_t init_containers_mutex;
mutex_t reduce_mutex;
mutex_t log_file_mutex;
mutex_t exec_map_exist_mut;
map<pthread_t, mutex_t> container_mutexes_map;

unsigned long index_for_reading;
unsigned long index_for_reduce;

bool exec_map_exists;
bool toDealloc;

MAP_CONTAINERS pthreadToContainer;
REDUCE_CONTAINERS reduce_containers;
vector<SHUFFLE_ITEM> shuffle_vec;
SHUFFLE_LIST shuffle_output;
vector<k2Base*> k2_for_delete;
vector<v2Base*> v2_for_delete;
sem_t semaphore;
FILE* log_file_p;

// Functions declarations
void initMutex(mutex_t* mutex);
void* shuffleWork(void* ptr);
void* execReduce(void* ptr);
void* execMap(void* ptr);
bool outputComperator(const OUT_ITEM& first_item, const OUT_ITEM& second_item);
void openLog();
string returnCurrentTime();
long returnTimeDelta(timeval before, timeval after);
void writecontentToFile(const char* msg, const char* thread_name ,
                        int* threads_num, long* time_elapsed, int mode);
void deallocK2V2();
void cleanResources();
int shuffleCycle();

// IMPLEMENTATION

// LOG FUNCTIONS
/**
 * Computes the time delta in nano seconds.
 * @param pre
 * @param post
 * @return
 */
long returnTimeDelta(timeval pre, timeval post)
{
    long secDelta = (post.tv_sec - pre.tv_sec) * SEC_TO_NANOSEC;
    long microSecDelta = (post.tv_usec - pre.tv_usec) * MICRO_TO_NANOSEC;
    return secDelta + microSecDelta;
}

/**
 * Writes the masseges to the log file.
 * @param msg
 * @param thread_name
 * @param threads_num
 * @param time_elapsed
 * @param mode
 */
void writecontentToFile(const char* msg, const char* thread_name ,
                        int* threads_num, long* time_elapsed, int mode)
{
    if (pthread_mutex_lock(&log_file_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX <<ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    switch (mode)
    {
        case 0:
        {
            if (fprintf(log_file_p, msg, *threads_num) < 0)
            {
                cerr << ERROR_MSG_A << ERROR_FPRINT <<ERROR_MSG_B << endl;
                exit(EXIT_FAILURE);
            }
        }
        break;
        case 1:
        {
            string cur_time = returnCurrentTime();
            string str(thread_name);
            if (fprintf(log_file_p, msg, str.c_str(), cur_time.c_str()) < 0)
            {
                cerr << ERROR_MSG_A << ERROR_FPRINT << ERROR_MSG_B << endl;
                exit(EXIT_FAILURE);
            }
        }
        break;
        case 2:
        {
            if (fprintf(log_file_p, msg, *time_elapsed) < 0)
            {
                cerr << ERROR_MSG_A << ERROR_FPRINT << ERROR_MSG_B << endl;
                exit(EXIT_FAILURE);
            }
        }
        break;
        case 3:
        {
            if (fprintf(log_file_p, msg) < 0)
            {
                cerr << ERROR_MSG_A << ERROR_FPRINT << ERROR_MSG_B << endl;
                exit(EXIT_FAILURE);
            }
        }
        break;
        default:;
    }
    if (pthread_mutex_unlock(&log_file_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * Returns the current time and converts it to string.
 * @return string that represent the time
 */
string returnCurrentTime()
{
    time_t time_now = time(0);
    struct tm time_struct;
    char buf[BUFF_SIZE_FOR_TIME];
    time_struct = *localtime(&time_now);
    strftime(buf, sizeof(buf), "[%d.%m.%Y %X]", &time_struct);
    return buf;
}

/**
 * Open the file, if exists updates it, if there is a problem in the open,
 * prints an error and exit.
 */
void openLog(){
    log_file_p = fopen(LOG_FILE_NAME, "a+");
    if (log_file_p == NULL)
    {
        cerr << ERROR_MSG_A << ERROR_OPEN_FILE << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if(pthread_mutex_init(&log_file_mutex, NULL))
    {
        cerr << ERROR_MSG_A << ERROR_INIT_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * Initlize the given mutex, if there is a problem, prints an error and exit.
 * @param mutex
 */
void initMutex(mutex_t* mutex)
{
    if (pthread_mutex_init(mutex, NULL))
    {
        cerr << ERROR_MSG_A << ERROR_INIT_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
}

/**
 * Comparator for the sort of the result, using the operator < of the keys.
 * @param first_item
 * @param second_item
 * @return true according to the operator <.
 */
bool outputComperator(const OUT_ITEM& first_item, const OUT_ITEM& second_item)
{
    return (*(first_item.first) < *(second_item.first));
}

/**
 * The map function that the execMap threads runs.
 * @param ptr
 * @return null if everuthing's ok.
 */
void* execMap(void* ptr)
{

    writecontentToFile(CREATE_THREAD_MSG, EXEC_MAP_NAME, NULL, NULL, 1);
    if (pthread_mutex_lock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_unlock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    IN_ITEMS_VEC* in_items_vec;
    in_items_vec = (IN_ITEMS_VEC*) ptr;
    while(index_for_reading < in_items_vec->size())
    {
        if (pthread_mutex_lock(&chunks_index_mutex))
        {
            cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
        unsigned long index = index_for_reading;
        if (index >= in_items_vec->size())
        {
            if (pthread_mutex_unlock(&chunks_index_mutex))
            {
                cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B <<endl;
                exit(EXIT_FAILURE);
            }
            break;
        }
        index_for_reading += SIZE_OF_CHUNK;
        if (pthread_mutex_unlock(&chunks_index_mutex))
        {
            cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX <<ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
        for (unsigned long i = 0; i < SIZE_OF_CHUNK &&
                index + i < in_items_vec->size(); i++)
        {
            IN_ITEM cur_pair = in_items_vec->at(i + index);
            map_reduce_base->Map(cur_pair.first, cur_pair.second);
        }
    }
    writecontentToFile(TERMINATE_THREAD_MSG, EXEC_MAP_NAME, NULL, NULL, 1);
    pthread_exit(NULL);
}

 /**
 * The function that the shuffle thread runs, gets their pairs, shuffles them and creates to each key the list with the
  * values.
  * @return
  */
int shuffleCycle()
{
    for (MAP_CONTAINERS::iterator it = pthreadToContainer.begin(); it != pthreadToContainer.end(); ++it)
    {
        if(!it->second.empty())
        {
            if (pthread_mutex_lock(&container_mutexes_map.at(it->first)))
            {
                cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX << ERROR_MSG_B << endl;
                exit(EXIT_FAILURE);
            }
            for (MAP_OUTPUT_TYPE cur_pair : it->second)
            {
                shuffle_output[cur_pair.first].push_back(cur_pair.second);
                if (toDealloc)
                {
                    if (cur_pair.first != NULL)
                    {
                        k2_for_delete.push_back(cur_pair.first);
                    }
                    if (cur_pair.second != NULL)
                    {
                        v2_for_delete.push_back(cur_pair.second);
                    }
                }
            }
            while(!it->second.empty())
            {
                it->second.pop_front();
            }
            if (pthread_mutex_unlock(&container_mutexes_map.at(it->first)))
            {
                cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B
                     << endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    return 0;
}

/**
 * While execmap threads exists calss to the shuffle function, and one more time
 * again at the end. Then crestes a vector
 * wuth the items.
 * @param ptr
 * @return null if everythhing's ok.
 */
void* shuffleWork(void* ptr)
{
    if(ptr)
    {

    }
    while (exec_map_exists)
    {
        if (sem_wait(&semaphore))
        {
            cerr << ERROR_MSG_A << ERROR_SEMAPHORE_WAIT << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
        shuffleCycle();
    }
    shuffleCycle();
    for (SHUFFLE_LIST::iterator it = shuffle_output.begin();
         it != shuffle_output.end(); ++it)
    {
        shuffle_vec.push_back(*it);
    }
    writecontentToFile(TERMINATE_THREAD_MSG, SHUFFLE_NAME, NULL, NULL, 1);
    return NULL;
}

/**
 * The reduce function that the execReduce threads runs.
 * @param ptr
 * @return null if everithng's ok.
 */
void* execReduce(void* ptr)
{
    if(ptr)
    {

    }
    writecontentToFile(CREATE_THREAD_MSG, EXEC_REDUCE_NAME, NULL, NULL, 1);
    if (pthread_mutex_lock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX << ERROR_MSG_B <<endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_unlock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    while(index_for_reduce < shuffle_vec.size())
    {
        if (pthread_mutex_lock(&reduce_mutex))
        {
            cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX<<ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
        unsigned long index = index_for_reduce;
        if (index >= shuffle_vec.size())
        {
            if (pthread_mutex_unlock(&reduce_mutex))
            {
                cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX <<ERROR_MSG_B << endl;
                exit(EXIT_FAILURE);
            }
            break;
        }
        index_for_reduce += SIZE_OF_CHUNK;
        if (pthread_mutex_unlock(&reduce_mutex))
        {
            cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
        for (unsigned long i = 0; i < SIZE_OF_CHUNK &&
                index + i < shuffle_vec.size(); i++)
        {
            SHUFFLE_ITEM cur_pair = shuffle_vec.at(i + index);
            map_reduce_base->Reduce(cur_pair.first, cur_pair.second);
        }
    }
    writecontentToFile(TERMINATE_THREAD_MSG, EXEC_REDUCE_NAME, NULL, NULL, 1);
    pthread_exit(NULL);
}

/**
 * This function called at the end of the prigramm, close the file, clears all
 * data structures and destroy all mutexces.
 */
void cleanResources()
{
    if (fclose(log_file_p))
    {
        cerr << ERROR_MSG_A << ERROR_CLOSE_FILE<<ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    //Destroy all
    if (pthread_mutex_destroy(&chunks_index_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_DESTROY_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_DESTROY_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&log_file_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_DESTROY_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
   if (pthread_mutex_destroy(&exec_map_exist_mut))
    {
        cerr << ERROR_MSG_A << ERROR_DESTROY_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&reduce_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_DESTROY_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    for (map<pthread_t, mutex_t>::iterator it = container_mutexes_map.begin();
         it != container_mutexes_map.end(); ++it)
    {
        if (pthread_mutex_destroy(&(it->second)))
        {
            cerr << ERROR_MSG_A << ERROR_DESTROY_MUTEX << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
    }
    if (sem_destroy(&semaphore))
    {
        cerr << ERROR_MSG_A << ERROR_SEMAPHORE_DESTORY << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    //clean data structures
    container_mutexes_map.clear();
    pthreadToContainer.clear();

    for (SHUFFLE_LIST::iterator it = shuffle_output.begin();
         it != shuffle_output.end(); ++it)
    {
        it->second.clear();
    }
    shuffle_output.clear();

    for (vector<SHUFFLE_ITEM>::iterator it = shuffle_vec.begin();
         it != shuffle_vec.end(); ++it )
    {
        it->second.clear();
    }
    shuffle_vec.clear();

    for (REDUCE_CONTAINERS::iterator it = reduce_containers.begin();
         it != reduce_containers.end(); ++it)
    {
        it->second.clear();
    }
    reduce_containers.clear();
}

/**
 * Deletes k2, v2 according to the bollean flag that gave by the user.
 */
void deallocK2V2 ()
{
    if (toDealloc)
    {
        for (k2Base* k2 : k2_for_delete)
        {
            delete (k2);
        }
        for (v2Base* v2 : v2_for_delete)
        {
            delete (v2);
        }
    }
    k2_for_delete.clear();
    v2_for_delete.clear();
}

/**
 * The main function of the program, that recieves from the user the
 * implemention to the map and the reduce functions, and runs the execMap
 * threads, the shuffle thread and the execReduce in order to get Parallelism.
 * @param mapReduce object that contains map function and reduce function.
 * @param itemsVec the input of k1,v1.
 * @param multiThreadLevel number of threads
 * @param autoDeleteV2K2 boolean- if true the framework need to delete k2,v2.
 * @return OUT_ITEMS_VEC vector of pairs k3,v3.
 */
OUT_ITEMS_VEC RunMapReduceFramework(MapReduceBase& mapReduce, IN_ITEMS_VEC &itemsVec,
                                    int multiThreadLevel, bool autoDeleteV2K2)
{
    openLog();
    writecontentToFile(INIT_FRAMEWORK_MSG, NULL, &multiThreadLevel, NULL, 0);
    map_reduce_base = &mapReduce;
    OUT_ITEMS_VEC outItemsVec = OUT_ITEMS_VEC();
    pthread_t threadsArr[multiThreadLevel];
    pthread_t shuffle_thread;
    initMutex(&chunks_index_mutex);
    initMutex(&init_containers_mutex);
    initMutex(&exec_map_exist_mut);
    initMutex(&reduce_mutex);
    k2_for_delete = vector<k2Base*>();
    v2_for_delete = vector<v2Base*>();
    index_for_reading = 0;
    index_for_reduce = 0;
    if (sem_init(&semaphore, 0, 0))
    {
        cerr << ERROR_MSG_A << ERROR_SEMAPHORE_INIT << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    struct timeval beginning_time;
    struct timeval after_shuffle_time;
    struct timeval after_reduce_time;
    long timeElapsed;
    toDealloc = autoDeleteV2K2;
    exec_map_exists = true;
    if (gettimeofday(&beginning_time, NULL))
    {
        cerr << ERROR_MSG_A << ERROR_GET_TIME << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    container_mutexes_map = map<pthread_t, mutex_t>();
    if (pthread_mutex_lock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX << ERROR_MSG_B <<endl;
        exit(EXIT_FAILURE);
    }
    //Init threads
    for(int i = 0; i < multiThreadLevel; i++)
    {
        if (pthread_create(&threadsArr[i], NULL, execMap, &itemsVec))
        {
            cerr << ERROR_MSG_A << ERROR_CREATE << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < multiThreadLevel; i++)
    {
        pthreadToContainer[threadsArr[i]] = MAP_OUTPUT_LIST();
        initMutex(&container_mutexes_map[threadsArr[i]]);
    }
    if (pthread_mutex_unlock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&shuffle_thread, NULL, shuffleWork, NULL))
    {
        cerr << ERROR_MSG_A << ERROR_CREATE << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    writecontentToFile(CREATE_THREAD_MSG, SHUFFLE_NAME, NULL, NULL, 1);
    //JOIN with all other threads
    for (int i = 0; i < multiThreadLevel; i++)
    {
        if(pthread_join(threadsArr[i], NULL))
        {
            cerr << ERROR_MSG_A << ERROR_JOIN << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
    }
    exec_map_exists = false;
    if (sem_post(&semaphore))
    {
        cerr << ERROR_MSG_A << ERROR_SEMAPHORE_POST <<ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_join(shuffle_thread, NULL))
    {
        cerr << ERROR_MSG_A << ERROR_JOIN << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (gettimeofday(&after_shuffle_time, NULL))
    {
        cerr << ERROR_MSG_A << ERROR_GET_TIME << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_lock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX << ERROR_MSG_B  << endl;
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < multiThreadLevel; i++)
    {
        if (pthread_create(&threadsArr[i], NULL, execReduce, NULL))
        {
            cerr << ERROR_MSG_A << ERROR_CREATE << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < multiThreadLevel; i++)
    {
        reduce_containers[threadsArr[i]] = OUT_ITEMS_VEC();
    }
    if (pthread_mutex_unlock(&init_containers_mutex))
    {
        cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < multiThreadLevel; i++)
    {
        if (pthread_join(threadsArr[i], NULL))
        {
            cerr << ERROR_MSG_A << ERROR_JOIN << ERROR_MSG_B << endl;
            exit(EXIT_FAILURE);
        }
    }
    if (gettimeofday(&after_reduce_time, NULL))
    {
        cerr << ERROR_MSG_A << ERROR_GET_TIME << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    timeElapsed = returnTimeDelta(beginning_time, after_shuffle_time);
    writecontentToFile(MAP_SHUFFLE_TIME_MSG, NULL, NULL, &timeElapsed, 2);
    timeElapsed = returnTimeDelta(after_shuffle_time, after_reduce_time);
    writecontentToFile(REDUCE_TIME_MSG, NULL, NULL, &timeElapsed, 2);
    for (REDUCE_CONTAINERS::iterator it = reduce_containers.begin();
         it != reduce_containers.end(); ++it)
    {
        for (OUT_ITEM out_pair: it->second)
        {
            outItemsVec.push_back(out_pair);
        }
    }
    sort(outItemsVec.begin(), outItemsVec.end(), outputComperator);
    deallocK2V2();
    writecontentToFile(MAPREDUCE_DONE_MSG, NULL, NULL, NULL, 3);
    cleanResources();
    return outItemsVec;
}

/**
 * Function that the map use in order to insert the result for the shuffle
 * function.
 * @param key2 pointer
 * @param value2 pointer
 */
void Emit2(k2Base* key2, v2Base* value2)
{
    if (sem_post(&semaphore))
    {
        cerr << ERROR_MSG_A << ERROR_SEMAPHORE_POST << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    MAP_OUTPUT_TYPE cur_pair(key2, value2);
    if (pthread_mutex_lock(&(container_mutexes_map[pthread_self()])))
    {
        cerr << ERROR_MSG_A << ERROR_LOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }
    pthreadToContainer[pthread_self()].push_back(cur_pair);
    if (pthread_mutex_unlock(&(container_mutexes_map[pthread_self()])))
    {
        cerr << ERROR_MSG_A << ERROR_UNLOCK_MUTEX << ERROR_MSG_B << endl;
        exit(EXIT_FAILURE);
    }

}

/**
 * Function that the reduce uses in order to prepare the final result of the
 * program.
 * @param key3 pointer
 * @param val3 pointer
 */
void Emit3(k3Base* key3, v3Base* val3)
{
    OUT_ITEM cur_pair(key3,val3);
    reduce_containers[pthread_self()].push_back(cur_pair);
}