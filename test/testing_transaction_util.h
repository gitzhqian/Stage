//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// testing_transaction_util.h
//
// Identification: test/include/concurrency/testing_transaction_util.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

/**
 * How to use the transaction test utilities
 *
 * These utilities are used to construct test cases for transaction and
 * concurrency control related tests. It makes you be able to describe the
 * schedule of each transaction (when to do what), i.e. you can describe the
 * serialized orders of each operation among the transactions.
 *
 * To schedule a txn tests, you need a TransactionScheduler (scheduler). Then
 * write the schedule in the following way: scheduler.Txn(n).ACTION(args)
 * scheduler.Txn(0).Insert(0, 1);
 * scheduler.Txn(0).Read(0);
 * scheduler.Commit();
 *  => Notice that this order will be the serial order to execute the operations
 *
 * There's a CreateTable() method, it will create a table with two columns:
 * key and value, and a primary index on the key column. The table is pre-
 * populated with the following tuples:
 * (0, 0), (1, 0), (2, 0), (3, 0), (4, 0), (5, 0), (6, 0), (7, 0), (8, 0),(9, 0)
 *
 * ACTION supported:
 * * Insert(key, value): Insert (key, value) into DB, key must be unique
 * * Read(key): Read value from DB, if the key does not exist, will read a value
 * *            of -1
 * * Update(key, value): Update the value of *key* to *value*
 * * Delete(key): delete tuple with key *key*
 * * Scan(key): Scan the table for key >= *key*, if nothing satisfies key >=
 * *            *key*, will scan a value of -1
 * * ReadStore(key, modify): Read value with key *key* from DB, and store the
 * *                         (result+*modify*) temporarily, the stored value
 * *                         can be further referred as TXN_STORED_VALUE in
 * *                         any above operations.
 * * Commit(): Commit the txn
 * * Abort(): Abort the txn
 *
 * Then, run the schedules by scheduler.Run(), it will schedule the txns to
 * execute corresponding operations.
 * The results of executing Run() can be fetched from
 * scheduler.schedules[TXN_ID].results[]. It will store the results from Read()
 * and Scan(), in the order they executed. The txn result (SUCCESS, FAILURE)
 * can be retrieved from scheduler.schedules[TXN_ID].txn_result.
 *
 * See isolation_level_test.cpp for examples.
 */

#include <map>
#include "../include/common/logger.h"
#include "../include/common/sync.h"
#include "../include/common/catalog.h"
#include "../include/execute/executor.h"
//#pragma once

namespace mvstore {

namespace test {
enum txn_op_t {
    TXN_OP_READ,
    TXN_OP_INSERT,
    TXN_OP_UPDATE,
    TXN_OP_DELETE,
    TXN_OP_SCAN,
    TXN_OP_ABORT,
    TXN_OP_COMMIT,
    TXN_OP_READ_STORE,
    TXN_OP_UPDATE_BY_VALUE
};

#define TXN_STORED_VALUE -10000

struct TestTuple {
    uint64_t key;
    uint64_t value;

    uint64_t Key() const { return key; }
    const char *GetData(){
        const char *data = reinterpret_cast<const char *>(&value);

        return data;
    }
};


typedef BTree TestDataTable;

class TestingTransactionUtil {
public:
    // Create a simple table with two columns: the id column and the value column
    // Further add a unique index on the id column. The table has `num_key` tuples
    // when created
    static TestDataTable * CreateTable(int num_key = 10, VersionStore *buf_mgr = nullptr,
                                       DramBlockPool *leaf_node_pool= nullptr,
                                       InnerNodeBuffer *inner_node_pool= nullptr,
                                       EphemeralPool *conflict_buffer= nullptr);

    static bool ExecuteInsert(TransactionContext *txn,
                              TestDataTable * table, int id, int value, VersionStore * buf_mgr);
    static bool ExecuteRead(TransactionContext *txn,
                            TestDataTable * table, int id, int &result,
                            bool select_for_update = false, VersionStore * buf_mgr = nullptr);
    static bool ExecuteDelete(TransactionContext *txn,
                              TestDataTable * table, int id,
                              bool select_for_update = false, VersionStore * buf_mgr = nullptr);
    static bool ExecuteUpdate(TransactionContext *txn,
                              TestDataTable * table, int id, int value,
                              bool select_for_update = false, VersionStore * buf_mgr = nullptr);
//    static bool ExecuteUpdateByValue(TransactionContext *txn,
//                                     TestDataTable * table, int old_value,
//                                     int new_value,
//                                     bool select_for_update = false, VersionStore * buf_mgr = nullptr);
    static bool ExecuteScan(TransactionContext *txn,
                            std::vector<int> &results, TestDataTable * table,
                            int id, bool select_for_update = false, VersionStore * buf_mgr = nullptr);
};

struct TransactionOperation {
    // Operation of the txn
    txn_op_t op;
    // id of the row to be manipulated
    int id;
    // value of the row, used by INSERT and DELETE operation
    int value;
    // Whether the read is declared as select_for_update
    bool is_for_update;

    TransactionOperation(txn_op_t op_, int id_, int value_,
                         bool is_for_udpate_ = false)
            : op(op_), id(id_), value(value_), is_for_update(is_for_udpate_){};
};

// The schedule for transaction execution
struct TransactionSchedule {
    ResultType txn_result;
    std::vector<TransactionOperation> operations;
    std::vector<int> results;
    int stored_value;
    int schedule_id;
    bool declared_ro;
    TransactionSchedule(int schedule_id_, bool ro = false)
            : txn_result(ResultType::FAILURE),
              stored_value(0),
              schedule_id(schedule_id_),
              declared_ro(ro) {}
};

// A thread wrapper that runs a transaction
class TransactionThread {
public:
    TransactionThread(TransactionSchedule *sched, TestDataTable *table_,
                      SSNTransactionManager *txn_manager_,
                      VersionStore *version_store)
            : schedule(sched),
              txn_manager(txn_manager_),
              table(table_),
              cur_seq(0),
              go(false),
              v_store(version_store){
        LOG_TRACE("Thread has %d ops", (int)sched->operations.size());
    }

    void RunLoop() {
        while (true) {
            while (!go) {
                std::chrono::milliseconds sleep_time(1);
                std::this_thread::sleep_for(sleep_time);
            };
            ExecuteNext();
            if (cur_seq == (int)schedule->operations.size()) {
                go = false;
                return;
            }
            go = false;
        }
    }

    void RunNoWait() {
        while (true) {
            ExecuteNext();
            if (cur_seq == (int)schedule->operations.size()) {
                break;
            }
        }
    }

    std::thread Run(bool no_wait = false) {
        if (!no_wait)
            return std::thread(&TransactionThread::RunLoop, this);
        else
            return std::thread(&TransactionThread::RunNoWait, this);
    }

    void ExecuteNext() {
        // Prepare data for operation
        txn_op_t op = schedule->operations[cur_seq].op;
        int id = schedule->operations[cur_seq].id;
        int value = schedule->operations[cur_seq].value;
        bool is_for_update = schedule->operations[cur_seq].is_for_update;

        if (id == TXN_STORED_VALUE) id = schedule->stored_value;
        if (value == TXN_STORED_VALUE) value = schedule->stored_value;

        if (cur_seq == 0) {
            if (schedule->declared_ro == true) {
                txn = txn_manager->BeginTransaction(IsolationLevelType::READ_ONLY);
            } else {
                txn = txn_manager->BeginTransaction(IsolationLevelType::SERIALIZABLE);
            }
        }

        if (schedule->txn_result == ResultType::ABORTED) {
            cur_seq++;
            return;
        }

        cur_seq++;
        bool execute_result = true;

        // Execute the operation
        switch (op) {
            case TXN_OP_INSERT: {
                LOG_TRACE("Execute Insert");
                execute_result =
                        TestingTransactionUtil::ExecuteInsert(txn, table, id, value, v_store);
                break;
            }
            case TXN_OP_READ: {
                LOG_TRACE("Execute Read");
                int result;
                execute_result = TestingTransactionUtil::ExecuteRead(
                        txn, table, id, result, is_for_update, v_store);
                schedule->results.push_back(result);
                break;
            }
            case TXN_OP_DELETE: {
                LOG_TRACE("Execute Delete");
                execute_result = TestingTransactionUtil::ExecuteDelete(txn, table, id,
                                                                       is_for_update, v_store);
                break;
            }
            case TXN_OP_UPDATE: {
                execute_result = TestingTransactionUtil::ExecuteUpdate(
                        txn, table, id, value, is_for_update, v_store);
                LOG_INFO("Txn %d Update %d's value to %d, %d", schedule->schedule_id,
                         id, value, execute_result);
                break;
            }
            case TXN_OP_SCAN: {
                LOG_TRACE("Execute Scan");
                execute_result = TestingTransactionUtil::ExecuteScan(
                        txn, schedule->results, table, id, is_for_update, v_store);
                break;
            }
//            case TXN_OP_UPDATE_BY_VALUE: {
//                int old_value = id;
//                int new_value = value;
//                execute_result = TestingTransactionUtil::ExecuteUpdateByValue(
//                        txn, table, old_value, new_value, is_for_update, v_store);
//                break;
//            }
            case TXN_OP_ABORT: {
                LOG_INFO("Txn %d Abort", schedule->schedule_id);
                // Assert last operation
                assert(cur_seq == (int)schedule->operations.size());
                auto next_end_commit_id = txn_manager->GetNextCurrentTidCounter();
                txn->SetCommitId(next_end_commit_id);
                schedule->txn_result = txn_manager->AbortTransaction(txn);
                txn = NULL;
                break;
            }
            case TXN_OP_COMMIT: {
                schedule->txn_result = txn_manager->CommitTransaction(txn);
                LOG_INFO(
                        "Txn %d commits: %s", schedule->schedule_id,
                        schedule->txn_result == ResultType::SUCCESS ? "Success" : "Fail");
                txn = NULL;
                break;
            }
            case TXN_OP_READ_STORE: {
                int result;
                execute_result = TestingTransactionUtil::ExecuteRead(
                        txn, table, id, result, is_for_update, v_store);
                schedule->results.push_back(result);
                LOG_INFO(
                        "Txn %d READ_STORE, key: %d, read: %d, modify and stored as: %d",
                        schedule->schedule_id, id, result, result + value);
                schedule->stored_value = result + value;
                break;
            }
        }

        if (txn != NULL && txn->GetResult() == ResultType::FAILURE) {
            txn_manager->AbortTransaction(txn);
            txn = NULL;
            LOG_TRACE("ABORT NOW");
            if (execute_result == false) LOG_TRACE("Executor returns false");
            schedule->txn_result = ResultType::ABORTED;
        }
    }

    TransactionSchedule *schedule;
    SSNTransactionManager *txn_manager;
    TestDataTable *table;
    int cur_seq;
    bool go;
    TransactionContext *txn;
    VersionStore *v_store;
};

// Transaction scheduler, to make life easier for writing txn test
class TransactionScheduler {
public:
    TransactionScheduler(size_t num_txn, TestDataTable *datatable_,
                         SSNTransactionManager *txn_manager_,
                         VersionStore *version_store,
                         bool first_as_ro = false)
            : txn_manager(txn_manager_),
              v_store(version_store),
              table(datatable_),
              time(0),
              concurrent(false) {
        for (size_t i = 0; i < num_txn; i++) {
            if (first_as_ro && i == 0) {
                schedules.emplace_back(i, true);
            } else {
                schedules.emplace_back(i, false);
            }
        }
    }

    void Run() {
        // Run the txns according to the schedule
        for (int i = 0; i < (int)schedules.size(); i++) {
            tthreads.emplace_back(&schedules[i], table, txn_manager, v_store);
        }
        if (!concurrent) {
            for (int i = 0; i < (int)schedules.size(); i++) {
                tp.enqueue([i, this](){
                    tthreads[i].RunLoop();
                });
            }
            for (auto itr = sequence.begin(); itr != sequence.end(); itr++) {
                LOG_TRACE("Execute %d", (int)itr->second);
                tthreads[itr->second].go = true;
                while (tthreads[itr->second].go) {
                    std::chrono::milliseconds sleep_time(1);
                    std::this_thread::sleep_for(sleep_time);
                }
                LOG_TRACE("Done %d", (int)itr->second);
            }
        } else {
            // Run the txns concurrently
            sync::CountDownLatch latch(schedules.size());
            for (int i = 0; i < (int)schedules.size(); i++) {
                tp.enqueue([i, &latch, this](){
                    tthreads[i].RunNoWait();
                    latch.CountDown();
                });
            }
            latch.Await();
            LOG_TRACE("Done concurrent transaction schedule");
        }
    }

    TransactionScheduler &Txn(int txn_id) {
        assert(txn_id < (int)schedules.size());
        cur_txn_id = txn_id;
        return *this;
    }

    void Insert(int id, int value, bool is_for_update = false) {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_INSERT, id, value,
                                                      is_for_update);
        sequence[time++] = cur_txn_id;
    }
    void Read(int id, bool is_for_update = false) {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_READ, id, 0,
                                                      is_for_update);
        sequence[time++] = cur_txn_id;
    }
    void Delete(int id, bool is_for_update = false) {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_DELETE, id, 0,
                                                      is_for_update);
        sequence[time++] = cur_txn_id;
    }
    void Update(int id, int value, bool is_for_update = false) {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_UPDATE, id, value,
                                                      is_for_update);
        sequence[time++] = cur_txn_id;
    }
    void Scan(int id, bool is_for_update = false) {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_SCAN, id, 0,
                                                      is_for_update);
        sequence[time++] = cur_txn_id;
    }
    void Abort() {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_ABORT, 0, 0);
        sequence[time++] = cur_txn_id;
    }
    void Commit() {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_COMMIT, 0, 0);
        sequence[time++] = cur_txn_id;
    }
    void UpdateByValue(int old_value, int new_value, bool is_for_update = false) {
        schedules[cur_txn_id].operations.emplace_back(
                TXN_OP_UPDATE_BY_VALUE, old_value, new_value, is_for_update);
        sequence[time++] = cur_txn_id;
    }
    // ReadStore will store the (result of read + modify) to the schedule, the
    // schedule may refer it by using TXN_STORED_VALUE in adding a new operation
    // to a schedule. See usage in isolation_level_test SIAnomalyTest.
    void ReadStore(int id, int modify, bool is_for_update = false) {
        schedules[cur_txn_id].operations.emplace_back(TXN_OP_READ_STORE, id, modify,
                                                      is_for_update);
        sequence[time++] = cur_txn_id;
    }

    void SetConcurrent(bool flag) { concurrent = flag; }

    SSNTransactionManager *txn_manager;
    VersionStore *v_store;
    TestDataTable *table;
    int time;
    std::vector<TransactionSchedule> schedules;
    std::vector<TransactionThread> tthreads;
    std::map<int, int> sequence;
    int cur_txn_id;
    bool concurrent;
    static sync::ThreadPool tp;
};
}
}
