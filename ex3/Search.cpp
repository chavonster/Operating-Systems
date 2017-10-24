#include <iostream>
#include <libltdl/lt_system.h>
#include <vector>
#include <sys/stat.h>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <iterator>
#include <stdlib.h>
#include <algorithm>
#include "MapReduceClient.h"
#include "MapReduceFramework.h"

#define ERROR_ARGS "Usage: <substring to search> <folders, separated by space>"
#define MINIMAL_ARGS 2
#define SUBSTRING_ARG 1
#define FOLDERS_ARG 2
#define THREADS_POOL_SIZE 10
#define SPACE " "

using namespace std;

//input key and value.
//the key, value for the map function and the MapReduceFramework
class searchK1: public k1Base
{
    char* folder_path;
    char* substring_for_match;

public:
    searchK1(char* folder, char* substr): folder_path(folder),
                                          substring_for_match(substr) {};
    const char* getFolder() const
    {
        return this->folder_path;
    }

    const char* getSubstring() const
    {
        return this->substring_for_match;
    }

    bool operator<(const k1Base &other) const
    {
        int res = strcmp(this->folder_path, ((searchK1&)other).getFolder());
        return res < 0;
    }
};

//intermediate key and value.
//the key, value for the Reduce function created by the Map function
class searchK2: public k2Base
{
    const char* folder_path;
public:
    searchK2(const char* folder): folder_path(folder) {};

    const char* getFolder() const
    {
        return this->folder_path;
    }

    bool operator<(const k2Base &other) const
    {
        int res = strcmp(this->folder_path, ((searchK2&)other).getFolder());
        return res < 0;
    }
};

class searchV2: public v2Base
{
    string matched_file_name;
public:
    searchV2(string file): matched_file_name(file) {};

    string getMatchedFileName() const
    {
        return this->matched_file_name;
    }
};

//output key and value
//the key,value for the Reduce function created by the Map function
class searchK3: public k3Base
{
    string matched_string;
public:
    searchK3(const char* str): matched_string(str) {};

    string getMatchedString() const
    {
        return this->matched_string;
    }

    bool operator<(const k3Base &other) const
    {
        const char* this_str = matched_string.c_str();
        searchK3 otherk3 = (searchK3&)other;
        const char* other_str = otherk3.getMatchedString().c_str();
        int res = strcmp(this_str, other_str);
        return res < 0;
    }
};

class searchMapReduce: public MapReduceBase
{
    /**
     * checks if the file name and the given word is the same.
     * @param file_name
     * @param substringToSearch
     * @return true if they are equal, else false
     */
    bool isMatch(const char* file_name, const char* substringToSearch) const
    {
        string file_str(file_name);
        string subStr(substringToSearch);
        bool res = file_str.find(subStr) != string::npos;
        return res;
    }

    /**
     * checks if it's directory.
     * @param path
     * @return true, else false.
     */
    bool isDir(const char* path) const
    {
        struct stat buf;
        stat(path, &buf);
        return S_ISDIR(buf.st_mode);
    }

    /**
     * checks if it's file.
     * @param fileName
     * @return true if it is, else false.
     */
    bool isFile(const char* fileName) const
    {
        ifstream infile(fileName);
        return infile.good();
    }

public:
    /**
     * Opens the directory (if it is) and goes over the files, searchs fot the
     * files names that contains the given string, when finds creates a key2
     * with the folder nams, and the values are the correct files names.
     * @param key
     * @param val
     */
    void Map(const k1Base *const key, const v1Base *const val) const
    {
        if(val)
        {

        }
        searchK2* folderK2;
        searchV2* matchedFileV2;
        searchK1* searchKey = (searchK1*)key;
        const char* substringToSearch = searchKey->getSubstring();
        const char* path = searchKey->getFolder();
        if(isDir(path))
        {
            DIR* dir;
            struct dirent* file;

            if ((dir = opendir(path)) != NULL)
            {
                while ((file = readdir(dir)) != NULL)
                {
                    if(isMatch(file->d_name, substringToSearch))
                    {
                        string file_name(file->d_name);
                        matchedFileV2 = new searchV2(file_name);
                        folderK2 = new searchK2(path);
                        Emit2(folderK2, matchedFileV2);
                    }
                }
                closedir(dir);
            }
        }
        delete(key);
    }

    /**
     * creeates key3 with the file name and calls to Emit3 wuth the key, when
     * the value is null.
     * @param key
     * @param vals
     */
    void Reduce(const k2Base *const key, const V2_VEC &vals) const
    {
        if(key)
        {

        }
        string matched_string;
        const char* matched_char_arr;
        for(unsigned int i = 0; i < vals.size(); i++)
        {
            searchV2* searchValue = (searchV2*)vals.at(i);
            matched_string = searchValue->getMatchedFileName();
            matched_char_arr = matched_string.c_str();
            searchK3* matched_k3 = new searchK3(matched_char_arr);
            Emit3(matched_k3, nullptr);
        }
    }
};

/**
 * extract the file names from the result that the framework returns.
 * @param outItemsVector
 * @return vector of the names
 */
vector<string> extractStrings(OUT_ITEMS_VEC outItemsVector)
{
    std::vector <std::string> strings_vec;
    for (OUT_ITEMS_VEC::const_iterator it = outItemsVector.begin(),
                 end = outItemsVector.end(); it != end; ++it) {
        searchK3* filesInFolder = (searchK3*)it->first;
        string str(filesInFolder->getMatchedString()) ;
        std::istringstream iss(str);
        std::copy(std::istream_iterator<std::string>(iss),
                  std::istream_iterator<std::string>(),
                  std::back_inserter(strings_vec));
        delete(it->first);
    }
    return strings_vec;
}

/**
 * runs the programm with the given string, and the paths, ans searchs if their
 * is file name with this string, if it is exist prints the name of the file.
 * if the number if the arguments not correct' returns an error.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[])
{
    if (argc < MINIMAL_ARGS)
    {
        cerr << ERROR_ARGS << endl;
        exit(EXIT_FAILURE);
    }

    char* substringToSearch = argv[SUBSTRING_ARG];
    IN_ITEMS_VEC k1v1_vec;

    for(int i = FOLDERS_ARG; i < argc; i++)
    {
        searchK1* k1 = new searchK1(argv[i], substringToSearch);
        IN_ITEM pair(k1, nullptr);
        k1v1_vec.push_back(pair);
    }
    searchMapReduce mapReduceSearch;
    OUT_ITEMS_VEC out_items_vec = RunMapReduceFramework
            (mapReduceSearch, k1v1_vec, THREADS_POOL_SIZE, true);
    vector<string> k3_vals =  extractStrings(out_items_vec);
    std::sort(k3_vals.begin(), k3_vals.end());
    for (std::vector<std::string>::const_iterator it = k3_vals.begin();
         it != k3_vals.end(); ++ it)
    {
        std::cout << *it << SPACE;
    }
    return 0;
}
