#include "headers.h"

using namespace std;

static const string LogFileName = "REDOLOG";

int LogManager::init() {
    this->fp = fopen((this->dir_+ LogFileName).c_str(), "ab+");
    if (this->fp == NULL) {
        printf("Open log file failed!\n");
        return -1;
    }

    return 0;
}

int LogManager::checkPoint() {

}

int LogManager::doRecovery() {

}

int LogManager::getUsefulLogs(vector<string>& logs) {

}

int LogManager::flushLogs() {
    return fflush(this->fp);
}

int LogManager::appendLog(string log) {
    fprintf(this->fp, "%s\n", log.c_str());
    return 0;
}

void LogManager::setDoorPlate(DoorPlate* dp) {

}