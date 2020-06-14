#include "headers.h"

using namespace std;

static const string LogFileName = "REDOLOG";

int LogManager::init() {
    this->fp = fopen((this->dir_ + "/" + LogFileName).c_str(), "at+");
    if (this->fp == NULL) {
        printf("Open log file failed!\n");
        return -1;
    }

    return 0;
}

int LogManager::checkPoint() {
    return 0;
}

int LogManager::doRecovery() {
    char cmd_buffer[100];
    char tar[50];
    string cur_k;
    int cur_v;
    vector<CheckPointInfo> cptv;
    set<int> committed_txns;
    vector<int> last_active_txns; // active for the has-end checkpoint
    set<int> last_active_txns_set;
    set<int> cur_txns_after_CKPT;

    int cur_txn;

    int cnt = 0;

    // scan for the first time
    while (fgets(cmd_buffer, 100, this->fp) != NULL) {
        // printf("%d\n", cnt++);
        // printf("%s\n", cmd_buffer);
        string cmd_str = cmd_buffer;
        stringstream ss;
        ss<<cmd_str;
        string tmps;
        switch (cmd_buffer[0]) {
            case 'B':
                ss>>tmps;
                ss>>cur_txn;
                cur_txns_after_CKPT.insert(cur_txn);
                break;
            case 'C':
                ss>>tmps;
                ss>>cur_txn;
                committed_txns.insert(cur_txn);
                break;
            case 'U':
                ss>>tmps;
                ss>>cur_txn;
                ss>>cur_k;
                ss>>cur_v;
                break;
            case 'D':
                ss>>tmps;
                ss>>cur_txn;
                ss>>cur_k;
                break;
            case 'K':
                ss>>tmps;
                ss>>cmd_buffer;
                if (cmd_buffer[0] == 'B') {
                    int active_num = 0;
                    CheckPointInfo cur_cpt;
                    cur_cpt.hasEnd = false;
                    ss>>active_num;
                    for (int k = 0; k < active_num; ++k) {
                        int cur_t = 0;
                        ss>>cur_t;
                        cur_cpt.active_txns.push_back(cur_t);
                    }
                    cptv.push_back(cur_cpt);
                    cur_txns_after_CKPT.clear();
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

            fscanf(this->fp, "%[\n]", cmd_buffer);      
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

    for (it = cur_txns_after_CKPT.begin(); it != cur_txns_after_CKPT.end(); ++it) {
        cout<<"cur_txns_after_CKPT: "<<(*it)<<endl;
    }

    for (it = last_active_txns_set.begin(); it != last_active_txns_set.end(); ++it) {
        cout<<"last_active_txns_set: "<<(*it)<<endl;
    }

    // for (it = committed_txns.begin(); it != committed_txns.end(); ++it) {
    //     cout<<"committed txns: "<<(*it)<<endl;
    // }

    // cout<<"1"<<endl;

    for (it = committed_txns.begin(); it != committed_txns.end(); ) {
        if (last_active_txns_set.find(*it) == last_active_txns_set.end() && cur_txns_after_CKPT.find(*it) == cur_txns_after_CKPT.end()) {
            cout<<(*it)<<endl;
            std::set<int>::iterator itmp = it;
            it++;
            committed_txns.erase(itmp);
        } else {
            it++;
        }
    }

    for (it = committed_txns.begin(); it != committed_txns.end(); ++it) {
        cout<<"committed txns handled: "<<(*it)<<endl;
    }


    // now the txns in the committed_txns is what we need to redo
    rewind(this->fp);

    cnt = 0;
    while (fgets(cmd_buffer, 100, this->fp)) {
        // printf("%d\n", cnt++);
        string cmd_str = cmd_buffer;
        stringstream ss;
        ss<<cmd_str;
        string tmps;
        switch (cmd_buffer[0]) {
            case 'B':
                ss>>tmps;
                ss>>cur_txn;
                break;
            case 'C':
                ss>>tmps;
                ss>>cur_txn;
                break;
            case 'U':
                ss>>tmps;
                ss>>cur_txn;
                ss>>cur_k;
                ss>>cur_v;
                if (committed_txns.find(cur_txn) != committed_txns.end()) {
                    this->dp->AddOrUpdate(cur_k, cur_v);
                }
                break;
            case 'D':
                ss>>tmps;
                ss>>cur_txn;
                ss>>cur_k;
                if (committed_txns.find(cur_txn) != committed_txns.end()) {
                    this->dp->Delete(cur_k);
                }
                break;
            case 'K':
                ss>>tmps;
                ss>>cmd_buffer;
                if (cmd_buffer[0] = 'B') {
                    int active_num = 0;
                    ss>>active_num;
                    for (int k = 0; k < active_num; ++k) {
                        int cur_t = 0;
                        ss>>cur_t;
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
    return 0;
}

int LogManager::flushLogs() {
    return fflush(this->fp);
}

int LogManager::appendLog(string log) {
    fprintf(this->fp, "%s\n", log.c_str());
    return 0;
}

void LogManager::setDoorPlate(DoorPlate* dp) {
    this->dp = dp;
}