//
// Created by zhangqian on 2022-03-1.
//

#ifndef MVSTORE_TPCC_LOADER_H
#define MVSTORE_TPCC_LOADER_H

//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// tpcc_loader.h
//
// Identification: src/include/benchmark/tpcc/tpcc_loader.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//


#include <memory>

#include "tpcc_configuration.h"
#include "tpcc_record.h"
#include "../../include/execute/txn.h"
#include "../../include/common/logger.h"
#include "../../include/common/sync.h"
#include "../../include/execute/executor.h"
#include "../../include/vstore/b_tree.h"

namespace mvstore {
namespace benchmark {
namespace tpcc {

extern configuration state;

void CreateTPCCDatabase(ParameterSet param, VersionStore *buf_mgr = nullptr,
                        DramBlockPool *leaf_node_pool= nullptr,
                        InnerNodeBuffer *inner_node_pool= nullptr,
                        EphemeralPool *conflict_buffer= nullptr);

void LoadTPCCDatabase(VersionStore *version_store);

void DestroyTPCCDatabase(VersionBlockManager *version_block_mng,
                         VersionStore *version_store,
                         SSNTransactionManager *txn_mng);

typedef BTree WarehouseTable;
typedef BTree DistrictTable;
typedef BTree ItemTable;
typedef BTree CustomerTable;
typedef BTree HistoryTable;
typedef BTree StockTable;
typedef BTree OrderTable;
typedef BTree NewOrderTable;
typedef BTree OrderLineTable;

typedef BTree OrderSKeyTable;
typedef BTree CustomerSKeyTable;

typedef BTree RegionTable;
typedef BTree NationTable;
typedef BTree SupplierTable;

/////////////////////////////////////////////////////////
// Tables
/////////////////////////////////////////////////////////

extern WarehouseTable *warehouse_table;
extern DistrictTable *district_table;
extern ItemTable *item_table;
extern CustomerTable *customer_table;
extern HistoryTable *history_table;
extern StockTable *stock_table;
extern OrderTable *orders_table;
extern NewOrderTable *new_order_table;
extern OrderLineTable *order_line_table;

extern OrderSKeyTable *orders_skey_table;
extern CustomerSKeyTable *customer_skey_table;

extern RegionTable *region_table;
extern NationTable *nation_table;
extern SupplierTable *supplier_table;

//extern sync::ThreadPool the_tp;


/////////////////////////////////////////////////////////
// Constants
/////////////////////////////////////////////////////////

extern const size_t name_length;
extern const size_t middle_name_length;
extern const size_t data_length;
extern const size_t state_length;
extern const size_t zip_length;
extern const size_t street_length;
extern const size_t city_length;
extern const size_t credit_length;
extern const size_t phone_length;
extern const size_t dist_length;

extern double item_min_price;
extern double item_max_price;

extern double warehouse_name_length;
extern double warehouse_min_tax;
extern double warehouse_max_tax;
extern double warehouse_initial_ytd;

extern double district_name_length;
extern double district_min_tax;
extern double district_max_tax;
extern double district_initial_ytd;

extern std::string customers_good_credit;
extern std::string customers_bad_credit;
extern double customers_bad_credit_ratio;
extern double customers_init_credit_lim;
extern double customers_min_discount;
extern double customers_max_discount;
extern double customers_init_balance;
extern double customers_init_ytd;
extern int customers_init_payment_cnt;
extern int customers_init_delivery_cnt;

extern double history_init_amount;
extern size_t history_data_length;

extern int orders_min_ol_cnt;
extern int orders_max_ol_cnt;
extern int orders_init_all_local;
extern int orders_null_carrier_id;
extern int orders_min_carrier_id;
extern int orders_max_carrier_id;

extern int new_orders_per_district;

extern int order_line_init_quantity;
extern int order_line_max_ol_quantity;
extern double order_line_min_amount;
extern size_t order_line_dist_info_length;

extern double stock_original_ratio;
extern int stock_min_quantity;
extern int stock_max_quantity;
extern int stock_dist_count;

extern double payment_min_amount;
extern double payment_max_amount;

extern int stock_min_threshold;
extern int stock_max_threshold;

extern double new_order_remote_txns;

extern const int syllable_count;
extern const char *syllables[];

extern const std::string data_constant;

struct NURandConstant {
    int c_last;
    int c_id;
    int order_line_itme_id;

    NURandConstant();
};

extern NURandConstant nu_rand_const;

/////////////////////////////////////////////////////////
// Tuple Constructors
/////////////////////////////////////////////////////////

Item BuildItemTuple(const int item_id);

Warehouse BuildWarehouseTuple(const int warehouse_id);

District BuildDistrictTuple(const int district_id, const int warehouse_id);

Customer BuildCustomerTuple(const int customer_id, const int district_id, const int warehouse_id);

History BuildHistoryTuple(const int customer_id, const int district_id, const int warehouse_id,
                          const int history_district_id, const int history_warehouse_id);

Order BuildOrdersTuple(const int orders_id,
                       const int district_id,
                       const int warehouse_id,
                       const bool new_order,
                       const int o_ol_cnt);

NewOrder BuildNewOrderTuple(const int orders_id,
                            const int district_id,
                            const int warehouse_id);

OrderLine BuildOrderLineTuple(const int orders_id, const int district_id, const int warehouse_id,
                              const int order_line_id, const int ol_supply_w_id, const bool new_order);

Stock BuildStockTuple(const int stock_id, const int s_w_id);

/////////////////////////////////////////////////////////
// Utils
/////////////////////////////////////////////////////////

std::string GetRandomAlphaNumericString(const size_t string_length);

int GetNURand(int a, int x, int y);

std::string GetLastName(int number);

std::string GetRandomLastName(int max_cid);

bool GetRandomBoolean(double ratio);

int GetRandomInteger(const int lower_bound, const int upper_bound);

int GetRandomIntegerExcluding(const int lower_bound, const int upper_bound,
                              const int exclude_sample);

double GetRandomDouble(const double lower_bound, const double upper_bound);

double GetRandomFixedPoint(int decimal_places, double minimum, double maximum);

std::string GetStreetName();

std::string GetZipCode();

std::string GetCityName();

std::string GetStateName();

int GetTimeStamp();

}  // namespace tpcc
}  // namespace benchmark
}  // namespace peloton

#endif