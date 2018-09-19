/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef LIBPIKSI_METRICS_TABLE_H
#define LIBPIKSI_METRICS_TABLE_H
// clang-format off

#include <libpiksi/metrics_foreach.h>

#define PK_METRICS_ENTRY(...) (__VA_ARGS__)

#define _ENTRY_GET_FOLDER(  _1, ...)                     _1
#define _ENTRY_GET_NAME(    _1, _2, ...)                 _2
#define _ENTRY_GET_TYPE(    _1, _2, _3, ...)             _3
#define _ENTRY_GET_UPDATER( _1, _2, _3, _4, ...)         _4
#define _ENTRY_GET_RESETER( _1, _2, _3, _4, _5, ...)     _5
#define _ENTRY_GET_SYMNAME( _1, _2, _3, _4, _5, _6, ...) _6

#define _ENTRY_GET_SYMTBLNAME(_1, ...) _1

#define _SYMBOL_TABLE_ENTRY(Context, TheArgs) \
  ssize_t _ENTRY_GET_SYMNAME TheArgs ;

#define _ENTRY_CONTEXT_6( _1, _2, _3, _4, _5, _6, ...) NULL
#define _ENTRY_CONTEXT_7( _1, _2, _3, _4, _5, _6, _7, ...) _7

#define _ENTRY_CONTEXT_(N, ...) _M_CONCAT(_ENTRY_CONTEXT_, N)(__VA_ARGS__)
#define _ENTRY_CONTEXT(...) _ENTRY_CONTEXT_(_M_FOREACH_NARG(__VA_ARGS__), __VA_ARGS__)

#define _METRICS_TABLE_ENTRY(Context, TheArgs) {                           \
  .folder  = _ENTRY_GET_FOLDER TheArgs,                                    \
  .name    = _ENTRY_GET_NAME TheArgs,                                      \
  .type    = _ENTRY_GET_TYPE TheArgs,                                      \
  .updater = _ENTRY_GET_UPDATER TheArgs,                                   \
  .reseter = _ENTRY_GET_RESETER TheArgs,                                   \
  .idx     = & _ENTRY_GET_SYMTBLNAME Context . _ENTRY_GET_SYMNAME TheArgs, \
  .context = _ENTRY_CONTEXT TheArgs                                        \
  },

#define PK_METRICS_TABLE(MetricsTableName, SymbolTableName, ...)     \
  struct {                                                           \
    _M_FOREACH((), _SYMBOL_TABLE_ENTRY, __VA_ARGS__)                 \
  } SymbolTableName;                                                 \
  _pk_metrics_table_entry_t MetricsTableName[] = {                   \
    _M_FOREACH((SymbolTableName), _METRICS_TABLE_ENTRY, __VA_ARGS__) \
  };

#define M_U32            METRICS_TYPE_U32
#define M_U64            METRICS_TYPE_U64
#define M_S32            METRICS_TYPE_S32
#define M_S64            METRICS_TYPE_S64
#define M_F64            METRICS_TYPE_F64
#define M_TIME           METRICS_TYPE_TIME
#define M_UPDATE_SUM     pk_metrics_updater_sum
#define M_UPDATE_ASSIGN  pk_metrics_updater_assign
#define M_UPDATE_DELTA   pk_metrics_updater_delta
#define M_UPDATE_MAX     pk_metrics_updater_max
#define M_UPDATE_COUNT   pk_metrics_updater_count
#define M_UPDATE_AVERAGE pk_metrics_updater_average
#define M_RESET_DEF      pk_metrics_reset_default
#define M_RESET_TIME     pk_metrics_reset_time

#define M_AVERAGE_OF(SymTable, Sym1, Sym2) \
  &((pk_metrics_average_t) { .index_of_num = & SymTable . Sym1, .index_of_dom = & SymTable . Sym2 })

// clang-format on
#endif//LIBPIKSI_METRICS_TABLE_H
