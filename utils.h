#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <stdint.h>
#include <pthread.h>
#include <string>
#include <vector>

int getFiles(const char * dir, std::string *fileNames);

// Hash
uint32_t StrHash(const char* s, int size);

// Env
int GetDirFiles(const std::string& dir, std::vector<std::string>* result);
int GetFileLength(const std::string& file);
int FileAppend(int fd, const std::string& value);
bool FileExists(const std::string& path);

// FileLock
class FileLock  {
 public:
    FileLock() {}
    virtual ~FileLock() {}

    int fd_;
    std::string name_;

 private:
    // No copying allowed
    FileLock(const FileLock&);
    void operator=(const FileLock&);
};

int LockFile(const std::string& f, FileLock** l);
int UnlockFile(FileLock* l);

#endif