#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <vector>
#include <string>
#include <string.h>

#include "door_plate.h"
#include "engine.h"

using namespace std;

struct CheckPointInfo {
    vector<int> active_txns;
    bool hasEnd;
};

class LogManager {

    public:

        FILE* fp;
        DoorPlate* dp;
        string dir_;

        std::mutex mtx;

        LogManager(const std::string& path): dir_(path) {}
        ~LogManager() {}
        int init();
        int checkPoint();
        int doRecovery();
        int getUsefulLogs(vector<string>& logs);
        int flushLogs();
        int appendLog(string log);
        void setDoorPlate(DoorPlate* dp);

};

#endif