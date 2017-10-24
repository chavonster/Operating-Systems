//
// Created by marina92 on 4/16/17.
//
#include <vector>
#include <stdlib.h>
#include <malloc.h>
#include <iostream>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include "uthreads.h"

using namespace std;

#define RUNNING 0
#define BLOCKED 1
#define READY 2
#define FREE_ID -1
#define ERROR_MAX_THREADS_EXCEEDED -1
#define MAIN_THREAD_ID 0
#define MIN_ID 0
#define MICROSEC_IN_SEC 1000000
#define THREAD_NOT_FOUND -1
#define ERROR_CODE -1
#define SUCCESS_CODE 0
#define SYS_ERR "system error: "
#define LIB_ERR "thread library error: "


sigset_t set;
sigset_t pendingSet;

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
            "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

/*
 * A struct of the user thread with it's variables.
 */
typedef struct user_thread
{
    int id;
    char stack[STACK_SIZE];
    int status;
    sigjmp_buf thread_env;
    int running_quantums_cnt = 0;
    vector<int> sync_with_ids;
    bool is_sync = false;
    bool is_blocked = false;
} user_thread;

static void schedule();
static void terminateProcess();


/**
 * A function that handle the system error, and prints the correct massege.
 * @param msg
 */
static void handleSystemErr(string msg)
{
    cerr << SYS_ERR << msg << endl;
    terminateProcess();
    exit(ERROR_CODE);
}

/**
 * A function that handle the library error, and prints the correct massege.
 * @param msg
 */
static int handleThreadLibraryErr(string msg)
{
    cerr << LIB_ERR << msg << endl;
    return ERROR_CODE;
}

//Library variables:
static vector<user_thread*> readyQueue;
static vector<user_thread*> blockedThreads;
static user_thread* runningThread;
static int allActiveThreadsIds[MAX_THREAD_NUM];
static int quantums_counter = 0;
static struct sigaction sa;
static struct itimerval timer;

/**
 * A function that blocks signals.
 */
static void blockSig()
{
    if (sigprocmask(SIG_BLOCK, &set, NULL) == ERROR_CODE)
    {
        handleSystemErr("sigpromask failed");
    }
}

/**
 * A function that unblocks signals.
 */
static void unblockSig()
{
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) == ERROR_CODE)
    {
        handleSystemErr("sigpromask failed");
    }
}

/**
 * A function that finds the first free id in the range.
 * @return the free id, else -1 for error
 */
static int findFirstFreeId()
{
    for(int i=0; i < MAX_THREAD_NUM; i++)
    {
        if (allActiveThreadsIds[i] == FREE_ID)
        {
            return i;
        }
    }
    return ERROR_CODE;
}

/**
 * A function that searchs the thread in the given vector (ready or block)
 * @param id of a thread
 * @param vec
 * @return the index of the thread in the vector, else -1 for error
 */
static int searchThreadinVector(int id, vector<user_thread*> vec)
{
    for(unsigned int i = 0; i < vec.size(); i++)
    {
        if(vec.at(i)->id == id)
        {
            return i;
        }
    }
    return THREAD_NOT_FOUND;
}

/**
 * A function that cheks if the given id is valid, if it's in the range 0-99 or
 * if the id is already in use.
 * @param tid
 * @return true if it's valid, else false
 */
static bool checkIdValidity(int tid)
{
    if(MIN_ID > tid || tid >= MAX_THREAD_NUM)
    {
        handleThreadLibraryErr("wrong thread id, id is not in range 0-99.");
        return false;
    }
    if(allActiveThreadsIds[tid] == FREE_ID)
    {
        handleThreadLibraryErr("wrong thread id, thread does not exist.");
        return false;
    }
    return true;
}

/**
 * A function that update the array of the id's, if we created a thread with
 * this id
 * @param id
 */
static void updateActiveThreads(int id)
{
    allActiveThreadsIds[id] = id;
}

/**
 * A function that finds a thread by the given id in the vectors.
 * @param id
 * @return pointer to the thread with this id, else return NULL
 */
static user_thread* findThreadById(int id)
{
    if(runningThread->id == id)
    {
        return runningThread;
    }
    for (unsigned int i = 0; i < readyQueue.size(); i++)
    {
        if (readyQueue.at(i)->id == id)
        {
            return readyQueue.at(i);
        }
    }
    for (unsigned int i = 0; i < blockedThreads.size(); i++)
    {
        if (blockedThreads.at(i)->id == id)
        {
            return blockedThreads.at(i);
        }
    }
    return NULL;
}

/**
 * A function that terminate the whole procss, goes over all thread that existed
 * and terminate it.
 */
static void terminateProcess()
{
    blockSig();
    for(vector<user_thread*>::iterator it = readyQueue.begin();
        it != readyQueue.end(); it++)
    {
        delete(*it);
    }
    readyQueue.clear();
    for(vector<user_thread*>::iterator it = blockedThreads.begin();
        it != blockedThreads.end(); it++)
    {
        delete(*it);
    }
    blockedThreads.clear();
    delete(runningThread);
    unblockSig();
}

/**
 * A function that realease the threads that syncked with a thread that was
 * terminated or strted to run.
 */
static int releaseThreadDependencies(user_thread* thread)
{
    for (unsigned int i = 0; i < thread->sync_with_ids.size(); i++)
    {
        user_thread *thread_to_resume =
                findThreadById(thread->sync_with_ids.at(i));
        if (uthread_resume(thread_to_resume->id))
        {
            return ERROR_CODE;
        }
    }
    thread->sync_with_ids.clear();
    return SUCCESS_CODE;
}

/**
 * A function that handles when the time was expired after quantum, moves the
 * running thread (if wasn't terminated or blocked) to the last plce in the
 * ready queue. And calls the the schedule to decide whicj thread will turn to
 * the running thread now.
 */
static void timerHandler(int sig)
{
    if(sig)
    {

    }
    blockSig();
    quantums_counter++;
    if (runningThread != NULL)
    {
        if (runningThread->status != BLOCKED)
        {
            runningThread->status = READY;
            readyQueue.push_back(runningThread);
        }
    }
    unblockSig();
    schedule();
}

/**
 * A function that schedules the actions, and moves the first element in ready
 * to running, it happens if the time was exipired (the quantum), or the running
 * thread was termineted or blocked. Based on the Round-Robin algorithm.
 */
static void schedule()
{
    blockSig();
    if(runningThread != NULL) //then we want to save the env
    {
        int ret_val = sigsetjmp(runningThread->thread_env,1);
        if (ret_val != 0) { //non zero came from long jump
            unblockSig();
            return;
        }
    }
    if(!readyQueue.empty())
    {
        runningThread = readyQueue.front();
        readyQueue.erase(readyQueue.begin());
    }
    runningThread->status = RUNNING;
    runningThread->running_quantums_cnt ++;
    if(!runningThread->sync_with_ids.empty())
    {
        releaseThreadDependencies(runningThread);
    }
    if (sigemptyset(&pendingSet) == ERROR_CODE)
    {
        handleSystemErr("failed in killing pending signals with sigemptyset");
    }
    if (sigpending(&pendingSet) == ERROR_CODE)
    {
        handleSystemErr("failed in killing pending signals with sigpending");
    }
    if (sigismember(&pendingSet, SIGVTALRM))
    {
        int sigint = 1;
        if (sigwait(&set, &sigint))
        {
            handleSystemErr("failed in killing pending signals with sigwait");
        }
    }
    unblockSig();
    siglongjmp(runningThread->thread_env, 1);
}

/**
 * A function that create a main thread when the init() called, with id = 0.
 * @return true if the creation was succsesful, else false
 */
static bool createMainThread()
{
    blockSig();
    try
    {
        user_thread* new_thread = new user_thread;
        new_thread->id = MAIN_THREAD_ID;
        updateActiveThreads(MAIN_THREAD_ID);
        new_thread->status = RUNNING;
        runningThread = new_thread;
        runningThread->running_quantums_cnt ++;
        sigsetjmp(new_thread->thread_env, 1);
        unblockSig();
        return true;
    }
    catch (bad_alloc& err)
    {
        handleSystemErr("bad allocating memory");
        return false;
    }
}

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        handleThreadLibraryErr("the quantum_usecs can't be non positive");
    }
    for (unsigned int i = 0; i < MAX_THREAD_NUM; i++)
    {
        allActiveThreadsIds[i] = FREE_ID;
    }
    quantums_counter++;
    if (!createMainThread())
    {
        handleThreadLibraryErr("failed to create Main thread");
        return ERROR_CODE;
    }
    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &timerHandler;
    if(sigemptyset(&set) == ERROR_CODE)
    {
        handleSystemErr("failed in blocking SIGVTALARM with sigemptyset");
        return ERROR_CODE;
    }
    if(sigaddset(&set, SIGVTALRM) == ERROR_CODE)
    {
        handleSystemErr("failed in blocking SIGVTALARM with sigaddset");
        return ERROR_CODE;
    }
    if (sigaction(SIGVTALRM, &sa,NULL) < 0)
    {
        handleThreadLibraryErr("sigaction error.");
        delete(runningThread);
        exit(ERROR_CODE);
    }
    // Configure the timer to expire after 1 quantum unit.
    // first time interval, seconds part:
    timer.it_value.tv_sec = quantum_usecs / MICROSEC_IN_SEC;
    // first time interval, microseconds part:
    timer.it_value.tv_usec = quantum_usecs % MICROSEC_IN_SEC;
    // configure the timer to expire every quantum unit after that.
    // following time intervals, seconds part
    timer.it_interval.tv_sec = quantum_usecs / MICROSEC_IN_SEC;
    // following time intervals, microseconds part
    timer.it_interval.tv_usec = quantum_usecs % MICROSEC_IN_SEC;
    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
    {
        handleThreadLibraryErr("setitimer error");
        delete(runningThread);
        exit(ERROR_CODE);
    }
    return SUCCESS_CODE;
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void))
{
    blockSig();
    address_t sp, pc;
    int id = findFirstFreeId();
    if (id == ERROR_MAX_THREADS_EXCEEDED)
    {
        handleThreadLibraryErr("Maximum number of threads was exceeded");
        return ERROR_CODE;
    }
    try
    {
        user_thread* new_thread = new user_thread;
        new_thread->id = id;
        new_thread->status = READY;
        readyQueue.push_back(new_thread);
        updateActiveThreads(id);
        sp = (address_t)new_thread->stack + STACK_SIZE - sizeof(address_t);
        pc = (address_t)f;
        sigsetjmp(new_thread->thread_env, 1);
        (new_thread->thread_env->__jmpbuf)[JB_SP] = translate_address(sp);
        (new_thread->thread_env->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&new_thread->thread_env->__saved_mask);
        unblockSig();
        return new_thread->id;
    }
    catch (bad_alloc& err)
    {
        handleSystemErr("bad allocating memory");
    }
    return ERROR_CODE;
}

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered as an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
    blockSig();
    if(tid == MAIN_THREAD_ID)
    {
        terminateProcess();
        exit(SUCCESS_CODE);
    }
    if(!checkIdValidity(tid))
    {
        return ERROR_CODE;
    }
    if (findThreadById(tid) == NULL)
    {
        handleThreadLibraryErr("wrong thread id, thread does not exist.");
        return ERROR_CODE;
    }
    user_thread* threadToBeDeleted = findThreadById(tid);
    allActiveThreadsIds[tid] = FREE_ID;
    if(threadToBeDeleted->status == RUNNING)
    {
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
        {
            handleThreadLibraryErr("setitimer error.");
            terminateProcess();
            exit(ERROR_CODE);
        }
        if(!runningThread->sync_with_ids.empty())
        {
            releaseThreadDependencies(runningThread);
        }
        runningThread = NULL;
        delete(threadToBeDeleted);
        unblockSig();
        timerHandler(SIGVTALRM);
        return SUCCESS_CODE;
    }
    else if(threadToBeDeleted->status == READY)
    {
        int index = searchThreadinVector(tid, readyQueue);
        releaseThreadDependencies(readyQueue.at(index));
        delete(readyQueue.at(index));
        readyQueue.erase(readyQueue.begin() + index);
        allActiveThreadsIds[tid] = FREE_ID;
    }
    else
    {
        int index = searchThreadinVector(tid, blockedThreads);
        releaseThreadDependencies(blockedThreads.at(index));
        delete(blockedThreads.at(index));
        blockedThreads.erase(blockedThreads.begin() + index);
        allActiveThreadsIds[tid] = FREE_ID;
    }
    unblockSig();
    return SUCCESS_CODE;
}

/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    blockSig();
    if(!checkIdValidity(tid))
    {
        return ERROR_CODE;
    }
    if(tid == MAIN_THREAD_ID)
    {
        handleThreadLibraryErr("it is an error to try blocking the main "
                                       "thread.");
        return ERROR_CODE;
    }
    if(runningThread->id == tid)
    {
        runningThread->status = BLOCKED;
        runningThread->is_blocked = true;
        blockedThreads.push_back(runningThread);
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
        {
            handleThreadLibraryErr("setitimer error.");
            terminateProcess();
            exit(ERROR_CODE);
        }
        unblockSig();
        timerHandler(SIGVTALRM);
    }
    else
    {
        user_thread* threadToBlock = findThreadById(tid);
        if(threadToBlock->status == READY) // if it's already blocked- no action
        {
            int index = searchThreadinVector(tid, readyQueue);
            if(index == THREAD_NOT_FOUND)
            {
                handleThreadLibraryErr("wrong thread id,thread does not exist");
                return ERROR_CODE;
            }
            readyQueue.at(index)->status = BLOCKED;
            readyQueue.at(index)->is_blocked = true;
            blockedThreads.push_back(readyQueue.at(index));
            readyQueue.erase(readyQueue.begin() + index);
        }
        else if (threadToBlock->status == BLOCKED && threadToBlock->is_sync)
        {
            threadToBlock->is_blocked = true;
        }
    }
    unblockSig();
    return SUCCESS_CODE;
}

/**
 * Used only by sync function. When syncing with another thread we want to avoid
 * overriding the action of the independent block, hence we call this
 * func instead of default block.
 */
static int sync_block(int tid)
{
    blockSig();
    if(!checkIdValidity(tid))
    {
        return ERROR_CODE;
    }
    if(tid == MAIN_THREAD_ID)
    {
        handleThreadLibraryErr("it is an error to try blocking the main "
                                       "thread.");
        return ERROR_CODE;
    }
    if(runningThread->id == tid)
    {
        runningThread->is_sync = true;
        runningThread->status = BLOCKED;
        blockedThreads.push_back(runningThread);
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
        {
            handleThreadLibraryErr("setitimer error.");
            terminateProcess();
            exit(ERROR_CODE);
        }
        unblockSig();
        timerHandler(SIGVTALRM);
    }
    else
    {
        user_thread* threadToBlock = findThreadById(tid);
        if(threadToBlock->status == READY) // if it's already blocked- no action
        {
            int index = searchThreadinVector(tid, readyQueue);
            if(index == THREAD_NOT_FOUND)
            {
                handleThreadLibraryErr("wrong thread id,thread does not exist");
                return ERROR_CODE;
            }
            threadToBlock->is_sync = true;
            readyQueue.at(index)->status = BLOCKED;
            blockedThreads.push_back(readyQueue.at(index));
            readyQueue.erase(readyQueue.begin() + index);
        }
    }
    unblockSig();
    return SUCCESS_CODE;
}

/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered as an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
    blockSig();
    if(!checkIdValidity(tid))
    {
        return ERROR_CODE;
    }
    int index = searchThreadinVector(tid, blockedThreads);
    //Thread is indeed blocked, otherwise it's running or ready and we ignore.
    if (index != THREAD_NOT_FOUND)
    {
        if((blockedThreads.at(index)->is_blocked == true &&
                blockedThreads.at(index)->is_sync == false) ||
                (blockedThreads.at(index)->is_blocked == false &&
                        blockedThreads.at(index)->is_sync == true))
        {
            blockedThreads.at(index)->status = READY;
            blockedThreads.at(index)->is_blocked = false;
            blockedThreads.at(index)->is_sync = false;
            readyQueue.push_back(blockedThreads.at(index));
            blockedThreads.erase(blockedThreads.begin() + index);
        }
        else if(blockedThreads.at(index)->is_blocked == true)
        {
            blockedThreads.at(index)->is_blocked = false;
        }
    }
    unblockSig();
    return SUCCESS_CODE;
}

/*
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will move to RUNNING state (i.e.right after the next time that
 * thread tid will stop running, the calling thread will be resumed
 * automatically). If thread with ID tid will be terminated before RUNNING
 * again, the calling thread should move to READY state right after thread
 * tid is terminated (i.e. it won’t be blocked forever). It is considered
 * as an error if no thread with ID tid exists or if the main thread (tid==0)
 * calls this function. Immediately after the RUNNING thread transitions to
 * the BLOCKED state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid)
{
    blockSig();
    if(!checkIdValidity(tid))
    {
        return ERROR_CODE;
    }
    if(runningThread->id == MAIN_THREAD_ID)
    {
        handleThreadLibraryErr("main thread can't call this function");
        return ERROR_CODE;
    }
    if(runningThread->id == tid)
    {
        handleThreadLibraryErr("thread can't sync with itself");
        return ERROR_CODE;
    }
    findThreadById(tid)->sync_with_ids.push_back(runningThread->id);
    sync_block(runningThread->id);
    unblockSig();
    return SUCCESS_CODE;
}

/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid()
{
    return runningThread->id;
}

/*
 * Description: This function returns the total number of quantums that were
 * started since the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
    return quantums_counter;
}

/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered as an error.
 * Return value: On success, return the number of quantums of the thread with ID
 * tid. On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
    if(!checkIdValidity(tid))
    {
        return ERROR_CODE;
    }
    return findThreadById(tid)->running_quantums_cnt;
}
