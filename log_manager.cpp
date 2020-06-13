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
    char cmd_buffer[50];
    char tar[50];
    string cur_k;
    int cur_v;
    vector<CheckPointInfo> cptv;
    set<int> committed_txns;
    vector<int> last_active_txns; // active for the has-end checkpoint
    set<int> last_active_txns_set;

    int cur_txn;

    // scan for the first time
    while (fscanf(this->fp, "%s", cmd_buffer)) {
        switch (cmd_buffer[0]) {
            case 'B':
                fscanf(this->fp, "%d", &cur_txn);
                break;
            case 'C':
                fscanf(this->fp, "%d", &cur_txn);
                committed_txns.insert(cur_txn);
                break;
            case 'U':
                fscanf(this->fp, "%d", &cur_txn);
                fscanf(this->fp, "%s", tar);
                cur_k = tar;
                fscanf(this->fp, "%d", &cur_v);
                break;
            case 'D':
                fscanf(this->fp, "%d", &cur_txn);
                fscanf(this->fp, "%s", tar);
                break;
            case 'K':
                fscanf(this->fp, "%s", cmd_buffer);
                if (cmd_buffer[0] = 'B') {
                    int active_num = 0;
                    CheckPointInfo cur_cpt;
                    cur_cpt.hasEnd = false;
                    fscanf(this->fp, "%d", &active_num);
                    for (int k = 0; k < active_num; ++k) {
                        int cur_t = 0;
                        fscanf(this->fp, "%d", &cur_t);
                        cur_cpt.active_txns.push_back(cur_t);
                    }
                } else if (cmd_buffer[0] == 'E') {
                    cptv[cptv.size() - 1].hasEnd = true;
                } else {
                    printf("Invalid checkpoint format!\n");
                    exit(-1);
                }
                break;
            default:
                printf("Unrecognized command in log file.\n");
                exit(-1);       
        }
    }  

    if (cptv.size() >= 1) {
        if (cptv[cptv.size() - 1].hasEnd) {
            last_active_txns = cptv[cptv.size() - 1].active_txns;
        } else {
            if (cptv.size() > 1) {
                last_active_txns = cptv[cptv.size() - 2].active_txns;
            }
        }
    }

    for (int i = 0; i < last_active_txns.size(); ++i) {
        last_active_txns_set.insert(last_active_txns[i]);
    }

    std::set<int>::iterator it;
    for (it = committed_txns.begin(); it != committed_txns.end(); ++it) {
        if (last_active_txns_set.find(*it) != last_active_txns_set.end()) {
            committed_txns.erase(it);
        }
    }

    // now the txns in the committed_txns is what we need to redo
    rewind(this->fp);

    while (fscanf(this->fp, "%s", cmd_buffer)) {
        switch (cmd_buffer[0]) {
            case 'B':
                fscanf(this->fp, "%d", &cur_txn);
                break;
            case 'C':
                fscanf(this->fp, "%d", &cur_txn);
                break;
            case 'U':
                fscanf(this->fp, "%d", &cur_txn);
                fscanf(this->fp, "%s", tar);
                cur_k = tar;
                fscanf(this->fp, "%d", &cur_v);
                if (committed_txns.find(cur_txn) != committed_txns.end()) {
                    this->dp->AddOrUpdate(cur_k, cur_v);
                }
                break;
            case 'D':
                fscanf(this->fp, "%d", &cur_txn);
                fscanf(this->fp, "%s", tar);
                if (committed_txns.find(cur_txn) != committed_txns.end()) {
                    this->dp->Delete(cur_k);
                }
                break;
            case 'K':
                fscanf(this->fp, "%s", cmd_buffer);
                if (cmd_buffer[0] = 'B') {
                    int active_num = 0;
                    fscanf(this->fp, "%d", &active_num);
                    for (int k = 0; k < active_num; ++k) {
                        int cur_t = 0;
                        fscanf(this->fp, "%d", &cur_t);
                    }
                } else if (cmd_buffer[0] == 'E') {
                    cptv[cptv.size() - 1].hasEnd = true;
                } else {
                    printf("Invalid checkpoint format!\n");
                    exit(-1);
                }
                break;
            default:
                printf("Unrecognized command in log file.\n");
                exit(-1);       
        }
    }   

    this->dp->flushMem();

    return 0;
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