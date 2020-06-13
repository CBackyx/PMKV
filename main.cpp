#include "headers.h"

using namespace std;
using namespace std::chrono;

string threadFiles[MAX_THREAD_NUM];
int threadIDs[MAX_THREAD_NUM];
thread ths[MAX_THREAD_NUM];
thread checkPointThread;
bool x_is_active[MAX_TRANSACTION_NUM + 1];
vector<pair<string, char>> rollBackActions[MAX_TRANSACTION_NUM + 1];

Engine *engine;

time_point<high_resolution_clock> start;

std::mutex gt_mtx;

string attr_A = "attr_A";
string attr_B = "attr_B";
string attr_C = "attr_C";
string attr_D = "attr_D";

long long fakeTimer = 0;

int doThread(int id);
long long getTime();

int committed_txn_num  = 0;
std::mutex committed_txn_num_mtx;

void modifyCommittedTxnNum(int incre);

int doThread(int id) {
    int cur_v = 0;
    int cur_xid = 0;
    char cmd_buffer[MAX_COMMAND_SIZE];
    char tar[MAX_KEY_SIZE];
    char cur_op;
    int cur_op_num;
    string cur_k;
    std::map<std::string, int> cur_table;
    std::vector<std::string> to_delete;

    // FILE *fp;
    ifstream fin(("threads/" + threadFiles[id]).c_str());
    // fp = fopen(threadFiles[id].c_str(), "rb");
    // if (fp == NULL) {
    //     printf("Open input file for thread %d failed!\n", id);
    //     return -1;
    // }

    FILE *wfp;
    char ofNameBuffer[100];
    sprintf(ofNameBuffer, "output_thread_%d.csv", threadIDs[id]);
    wfp = fopen(ofNameBuffer, "w");
    if (wfp == NULL) {
        printf("Open output file for thread %d failed!\n", id);
        return -1;
    }

    fprintf(wfp, "transaction_id,type,time,value\n");

    time_t cur_t;
    vector<string> toReclaim;

    char xbuffer[MAX_TRANSACTION_SIZE][MAX_COMMAND_SIZE]; // for roll back
    char obuffer[MAX_TRANSACTION_SIZE][MAX_COMMAND_SIZE];
    int ocnt = 0;
    int xcnt = 0;
    int scnt = 0;
    long long curTime = 0;

    while (fin.getline(cmd_buffer, MAX_COMMAND_SIZE)) {
        if (cmd_buffer[0] != 'C') {
            strcpy(xbuffer[xcnt], cmd_buffer);
            xcnt++;
            continue;
        }
        strcpy(xbuffer[xcnt], cmd_buffer);
        xcnt++;
        // printf("shit thread %d\n", id);
        while (true) {
            // printf("shit\n");
            // printf("%d\n", cur_xid);
            bool committed = false;
            bool needRedo = false;
            for (int i=0; i<xcnt; ++i) {
                // printf("%c\n", xbuffer[i][0]);
                // printf("%d\n", i);
                switch (xbuffer[i][0]) {
                    case 'B':
                        // Begin
                        sscanf(xbuffer[i] + 6, "%d", &cur_xid);
                        printf("%d\n", cur_xid);
                        engine->x_mtx.lock();
                        // engine->copyData(cur_table, cur_xid);
                        cur_table.clear();
                        to_delete.clear();

                        x_is_active[cur_xid] = true;
                        sprintf(obuffer[ocnt++], "%d,BEGIN,%lld,", cur_xid, getTime());

                        engine->lm.mtx.lock();
                        fprintf(engine->lm.fp, "BEGIN %d\n", cur_xid);
                        engine->lm.flushLogs();
                        engine->lm.mtx.unlock();
                        // printf("Thread %d Begin transaction %d\n", id, cur_xid);
                        break;
                    case 'C':
                        // Commit
                        sscanf(xbuffer[i] + 7, "%d", &cur_xid);
                        engine->mtx.lock();
                        for (auto x:toReclaim) {
                            engine->reclaim(x, cur_xid);
                        }
                        toReclaim.clear();
                        cur_table.clear();
                        x_is_active[cur_xid] = false;
                        sprintf(obuffer[ocnt++], "%d,END,%lld,", cur_xid, getTime());
                        engine->mtx.unlock();

                        for (int k=0; k<ocnt; ++k) {
                            fprintf(wfp, "%s\n", obuffer[k]);
                            fflush(wfp);
                        }

                        engine->lm.mtx.lock();
                        fprintf(engine->lm.fp, "COMMIT %d\n", cur_xid);
                        engine->lm.flushLogs();
                        engine->lm.mtx.unlock();

                        for (std::map<std::string, int>::iterator it = cur_table.begin(); it != cur_table.end(); ++it) {
                            string cur_k = it->first;
                            // cout<<"cur_k "<<cur_k<<endl;
                            int cur_v = it->second;
                            // cout<<currl->records[0].createdXid<<" "<<currl->records[0].expiredXid<<" "<<currl->records[0].value<<endl;
                            // cout<<currl->records[1].createdXid<<" "<<currl->records[1].expiredXid<<" "<<currl->records[1].value<<endl;
                            engine->dp.AddOrUpdate(cur_k, cur_v);
                        }

                        for (int kk = 0; kk < to_delete.size(); ++kk) {
                            engine->dp.Delete(to_delete[kk]);
                        }

                        modifyCommittedTxnNum(1);

                        ocnt = 0;
                        committed = true;
                        break;
                    case 'R':
                        // Read
                        sscanf(xbuffer[i] + 5, "%s", tar);
                        cur_k = tar;

                        if (cur_table.find(cur_k) == cur_table.end()) {
                            engine->dp.Read(cur_k, cur_v);
                            cur_table.insert(std::pair<std::string, int>(cur_k, cur_v));
                        }

                        sprintf(obuffer[ocnt++], "%d,%s,%lld,%d", cur_xid, tar, getTime(), cur_v);
                        // printf("Thread %d Read %s %d\n", id, tar, cur_v);
                        break;
                   case 'I':
                        // Insert
                        sscanf(xbuffer[i] + 7, "%s %d", tar, &cur_v);
                        cur_k = tar;
                        if (cur_table.find(cur_k) == cur_table.end()) {
                            cur_table.insert(std::pair<std::string, int>(cur_k, cur_v));
                        } else {
                            cur_table[cur_k] = cur_v;
                        }
                        // sprintf(obuffer[ocnt++], "%d,%s,%lld,%d", cur_xid, tar, getTime(), cur_table[cur_k]);
                        
                        engine->lm.mtx.lock();
                        fprintf(engine->lm.fp, "Update %d %s %d\n", cur_xid, tar, cur_v);
                        engine->lm.flushLogs();
                        engine->lm.mtx.unlock();

                        // printf("Thread %d Read %s %d\n", id, tar, cur_v);
                        break;
                   case 'D':
                        // Delete
                        sscanf(xbuffer[i] + 5, "%s", tar);
                        cur_k = tar;

                        engine->dp.Delete(cur_k);

                        engine->lm.mtx.lock();
                        fprintf(engine->lm.fp, "Delete %d %s\n", cur_xid, tar);
                        engine->lm.flushLogs();
                        engine->lm.mtx.unlock();

                        to_delete.push_back(cur_k);

                        // sprintf(obuffer[ocnt++], "%d,%s,%lld,%d", cur_xid, tar, getTime(), cur_table[cur_k]);
                        // printf("Thread %d Read %s %d\n", id, tar, cur_v);
                        break;
                    case 'S':
                        cur_op_num = 0;
                        // Set
                        for (int k=0; k<100; k++) {
                            if (xbuffer[i][k] == ',') {
                                sscanf(xbuffer[i] + k + 2, "%s %c %d", tar, &cur_op, &cur_op_num);
                                break;
                            }
                        }
                        // printf("%s, ++ %s ++ %c ++ %d\n", tar, tar, cur_op, cur_op_num);
                        cur_k = tar;
                        if (cur_op == '+') cur_op_num = cur_op_num;
                        else if (cur_op == '-') cur_op_num = -cur_op_num;
                        else {
                            printf("Operator error!\n");
                            exit(-1);
                        }
                        cur_table[cur_k] += cur_op_num;
                        cur_v = cur_table[cur_k];
                        scnt = 0;

                        if (cur_table.find(cur_k) == cur_table.end()) {
                            cur_table.insert(std::pair<std::string, int>(cur_k, cur_v));
                        } else {
                            cur_table[cur_k] = cur_v;
                        }

                        engine->lm.mtx.lock();
                        fprintf(engine->lm.fp, "Update %d %s %d\n", cur_xid, tar, cur_v);
                        engine->lm.flushLogs();
                        engine->lm.mtx.unlock();
                        // while (engine->updateRecord(cur_k, cur_op_num, cur_xid) != 0) {
                        //     std::this_thread::sleep_for(std::chrono::microseconds(rand() % 10 + 1));
                        //     if (++scnt > 5) {
                        //         needRedo = true;
                        //         break;
                        //     } 
                        // }
                        // printf("Thread %d Set %s %d\n", id, tar, cur_v);
                        if (!needRedo) toReclaim.push_back(cur_k);
                        // sprintf(obuffer[ocnt++], "SET %d,%s,%lld,%d", cur_xid, tar, getTime(), cur_v);
                        break;
                    default:
                        printf("Unrecognized command!\n");
                        exit(-1);
                }
                if (needRedo) {
                    // printf("halo\n");
                    break;
                }
                if (committed) {
                    break;
                }
            }
            if (needRedo) { 
                engine->doRollback(rollBackActions[cur_xid], cur_xid);
                // redo
                rollBackActions[cur_xid].clear();
                ocnt = 0;
                toReclaim.clear();
                x_is_active[cur_xid] = false;
                std::this_thread::sleep_for (std::chrono::microseconds(rand()%1000 + 1));
                x_is_active[cur_xid] = true;
                cur_table.clear();
                engine->x_mtx.unlock();
                continue;
            } else {
                fflush(wfp);
                rollBackActions[cur_xid].clear();
                xcnt = 0;
                ocnt = 0;
                toReclaim.clear();
                engine->x_mtx.unlock();
                break;
            }
        }
    }
    
    fin.close();
    // fclose(fp);
    fclose(wfp);
    return 0;
}

void modifyCommittedTxnNum(int incre) {
    committed_txn_num_mtx.lock();
    committed_txn_num += incre;
    committed_txn_num_mtx.unlock();

}

int doCheckPointThread(int id) {
    // need some condition to do checkpoint

    while (true) {
        std::this_thread::sleep_for(std::chrono::microseconds(rand() % 1000 + 1));

        if (committed_txn_num >= 10) {
            modifyCommittedTxnNum(-10);

            engine->lm.mtx.lock();

            fprintf(engine->lm.fp, "KPT BEGIN ");
            vector<int> active_txns;
            for (int i = 0; i < MAX_TRANSACTION_NUM; ++i) {
                if (x_is_active[i]) active_txns.push_back(i);
            }

            fprintf(engine->lm.fp, "%d ", (int)active_txns.size());

            for (int i = 0; i < active_txns.size(); ++i) {
                fprintf(engine->lm.fp, "%d ", active_txns[i]);
            }


            fprintf(engine->lm.fp, "\n");
            fflush(engine->lm.fp);

            if (engine->dp.flushMem() != 0) {
                printf("Flush door plate failed\n");
                exit(-1);
            }

            fprintf(engine->lm.fp, "KPT END\n");
            fflush(engine->lm.fp);

            engine->lm.mtx.unlock();
        }

    }
    
    return 0;
}

// Actually this is a duration since start point
long long getTime() {
    gt_mtx.lock();
    time_point<high_resolution_clock> end;
    double duration;
    end = high_resolution_clock::now();//获取当前时间
	auto dur = duration_cast<nanoseconds>(end - start);
	duration = double(dur.count()) * nanoseconds::period::num / nanoseconds::period::den * 1000000000;
    gt_mtx.unlock();
    return (long long)duration;
}

int main(int argc, char* argv[]) {
    // Initialize the KV engine
    engine = new Engine("data");

    RetCode ret = engine->dp.Init();
    if (ret != kSucc) {
        printf("Create/Open door plate file failed!\n");
        exit(-1);
    }

    int ret1 = engine->lm.init();
    if (ret1 != 0) {
        printf("Create/Open log file failed!\n");
        exit(-1);
    }

    engine->lm.setDoorPlate(&engine->dp);

    if (argv[1][0] == 'R') {
        engine->lm.doRecovery();
    } 

    srand(1);

    char threadFilesPath[] = "threads/";
    int threadNum = getFiles(threadFilesPath, threadFiles);

    printf("threadNum %d\n", threadNum);
    for (int i = 0; i < threadNum; ++i) {
        sscanf(threadFiles[i].c_str(), "thread_%d.txt", &threadIDs[i]);
    }

    printf("Thread num is %d\n", threadNum);
    // Initialize transactions info
    for (int i = 0; i < MAX_TRANSACTION_NUM + 1; ++ i) {
        x_is_active[i] = false;
    }

    FILE *fp;
    
    if (argv[1][0] == 'I') {

        // Now initialize the KV engine using "data_prepare.txt"
        printf("Initializing KV engine content\n");
        
        fp = fopen("data_prepare.txt", "r");
        if (fp == NULL) {
            printf("Open data_prepare.txt failed!\n");
            return -1;
        }

        // Attention!
        // The MAX_TRANSACTION_NUM transaction is preserved for initialize the KV, so it is always not active
        int cur_v = 0;
        char cmd_buffer[MAX_COMMAND_SIZE];
        while (fscanf(fp, "%s", cmd_buffer) != EOF) {
            fscanf(fp, "%s", cmd_buffer);
            fscanf(fp, "%d", &cur_v);
            string curs = cmd_buffer;
            cout << curs << endl;
            // engine->addRecord(curs, cur_v, MAX_TRANSACTION_NUM);
            engine->dp.AddOrUpdate(curs, cur_v);
        }

        engine->dp.flushMem();

        fclose(fp);
    }

    start = high_resolution_clock::now();

    checkPointThread = thread(doCheckPointThread, threadNum); // ?

    // Do all the threads
    printf("Initializing threads!\n");
    for (int i = 0; i < threadNum; ++i) {
        ths[i] = thread(doThread, i);
    }
    for (int i = 0; i < threadNum; ++i) {
        ths[i].join();
    }

    checkPointThread.detach();
    checkPointThread.~thread();

    printf("Total time cost is %lf\n", (double)getTime()/1000000000);

    // Output the final KV state
    fp = fopen("final_state.csv", "w");
    if (fp == NULL) {
        printf("Open final_state.txt failed!\n");
        return -1;
    }

    for (std::map<std::string, struct RecordLine*>::iterator it = engine->table.begin(); it != engine->table.end(); ++it) {
        string cur_k = it->first;
        // cout<<"cur_k "<<cur_k<<endl;
        struct RecordLine* currl = it->second;
        // cout<<currl->records[0].createdXid<<" "<<currl->records[0].expiredXid<<" "<<currl->records[0].value<<endl;
        // cout<<currl->records[1].createdXid<<" "<<currl->records[1].expiredXid<<" "<<currl->records[1].value<<endl;
        if (currl->records[0].createdXid != -1) {
            fprintf(fp, "%s,%d\n", cur_k.c_str(), currl->records[0].value);
        } else if (currl->records[1].createdXid != -1) {
            fprintf(fp, "%s,%d\n", cur_k.c_str(), currl->records[1].value);
        } else continue;
    }

    fclose(fp);

    delete engine;

    return 0;
}