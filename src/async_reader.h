#pragma once
#ifndef _ASYNC_READER_H
#define _ASYNC_READER_H
#include "stdint.h"
#ifdef __cplusplus
extern "C"
{
#endif

    void *create_item_reader(const char *file_path);
    void read_data_daemon(void *handle);
    int reset_for_read(void *handle);
    int read_item_data(void *handle, char *buf, int *len);
    int close_item_reader(void *handle);
    void garbage_collect();
    int64_t get_file_size(const char *file_name);
    uint64_t get_item_number(void *handle);
#ifdef __cplusplus
}
#endif

#endif
