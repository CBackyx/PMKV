#ifndef ENGINE_H
#define ENGINE_H

#include <string>
#include <map>
#include <mutex>

#include "door_plate.h"
#include "log_manager.h"

using namespace std;

enum RetCode {
  kSucc = 0,
  kNotFound = 1,
  kCorruption = 2,
  kNotSupported = 3,
  kInvalidArgument = 4,
  kIOError = 5,
  kIncomplete = 6,
  kTimedOut = 7,
  kFull = 8,
  kOutOfMemory = 9,
};

// A single record(one version)
struct Record {
    int value;
    int createdXid;
    int expiredXid;
};

class RecordLine {
    public:
        struct Record records[2]; 
        std::mutex mtx;
        RecordLine() {
            records[0].createdXid = -1;
            records[0].expiredXid = -1;
            records[1].createdXid = -1;
            records[1].expiredXid = -1;
        }
        ~RecordLine() {}
};

//locations->insert(std::pair<std::string, Location>(key, it->location));
//table.find()
class Engine {

    public:
        std::mutex mtx;
        std::mutex x_mtx;

        DoorPlate dp;
        LogManager lm;

        int doRecovery();

        bool recordIsVisible(struct Record* r, int xid);
        bool recordIsLocked(struct Record* r, int xid);
        int addRecord(string key, int value, int xid);
        int deleteRecord(string key, int xid, int& base);
        int updateRecord(string key, int value, int xid);
        int getValue(string key, int xid, long long& ct);
        int reclaim(string key, int xid);
        int doRollback(vector<pair<string, char>>& rbas, int xid);
        void copyData(std::map<std::string, int>& cur_table, int xid);
        // void nsUpdate(std::map<std::string, int>& cur_table);
        Engine(string path): dp(path), lm(path) {}
        ~Engine() {
            for (std::map<std::string, struct RecordLine*>::iterator it=table.begin(); it!=table.end(); ++it)
                delete it->second;
            table.clear();
        }

    std::map<std::string, struct RecordLine*> table;
};

#endif