#include "mnist_reader.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>


static std::string file_dir = "";
static uint64_t item_num = 0;
static int64_t file_size = -1;
static int64_t buff_len = 512;
static int64_t read_len = 0;
static std::shared_ptr<asio_read> asio_read_ptr;


int64_t get_file_size(const char* file_name)
{
    if (file_size != -1)
        return file_size;
    std::ifstream ifs(file_name, std::ios::in | std::ios::binary);
    if (!ifs)
        throw std::invalid_argument("can not open file: " + std::string(file_name));
    ifs.seekg(0, ifs.end);
    file_size = ifs.tellg();
    ifs.close();
    return file_size;
}

void* create_item_reader(const char* file_path)
{
    read_len = 0;
    file_dir = std::string(file_path);
    auto file_size = get_file_size(file_path);
    std::cout << file_path << " size: " << file_size << std::endl;

#ifndef WIN32
    int* fm = new int(open(file_path, O_RDONLY));
#else
    HANDLE* fm = new HANDLE(::CreateFile(
        file_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0));
    if (*fm == INVALID_HANDLE_VALUE)
    {
        throw std::invalid_argument("can not open " + std::string(file_path));
    }
#endif

    get_item_number(fm);
    asio_read_ptr = std::make_shared<asio_read>((void*)fm, buff_len, read_len, file_size);
    asio_read_ptr->run();
    return fm;
}

// 暂时不明白对应的iris.cpp中这个函数的作用是什么？
int reset_for_read(void* handle)
{

    if (!handle || *((int*)handle) < 0)
    {
        return -1;
    }
    handle = create_item_reader(file_dir.data());
    return 0;
}




int read_item_data(void* handle, char* buf, int* len)
{
    if (!handle || !buf)
    {
        return -1;
    }
    else if (!asio_read_ptr) {
        asio_read_ptr = std::make_shared<asio_read>(handle, read_len, buff_len, file_size);
        asio_read_ptr->run();
    }

    CDataPkg_ptr_t read_buff_ptr = nullptr;
    bool pop_state = asio_read_ptr->pop(read_buff_ptr);
    if (pop_state)
    {
        memcpy(buf, read_buff_ptr->data.get(), read_buff_ptr->length);
        *len = read_buff_ptr->length;
        std::string tmp(buf, *len);
        std::cout << "returned string length: " << tmp.size() << std::endl;
    }
    return 0;
}

int close_item_reader(void* handle)
{
    if (asio_read_ptr) {
        asio_read_ptr->stop();
    }
    if (handle && *((int*)handle) < 0)
    {
#ifndef WIN32
        close(*(int*)handle);
#endif
#ifdef WIN32
        CloseHandle(handle);
#endif
    }
    return 0;
}

uint64_t get_item_number(void* handle)
{
    if (item_num != 0)
        return item_num;
    std::ifstream ifs(file_dir.data(), std::ios::in | std::ios::binary);
    uint32_t magic_number = 0;
    ifs.read(reinterpret_cast<char*>(&magic_number), 4);
    ifs.read(reinterpret_cast<char*>(&item_num), 4);
    if (is_little_endian())
    { // MNIST data is big-endian format
        reverse_endian(&magic_number);
        reverse_endian(&item_num);
    }
    read_len += 8;
    uint32_t label = 0x00000801, image = 0x00000803;
    if (magic_number == image)
    {
        uint32_t rows = 0, cols = 0;
        ifs.read(reinterpret_cast<char*>(&rows), 4);
        ifs.read(reinterpret_cast<char*>(&cols), 4);
        if (is_little_endian())
        { // MNIST data is big-endian format
            reverse_endian(&rows);
            reverse_endian(&cols);
        }
        read_len += 8;
        buff_len = rows * cols;
    }
    else if (magic_number != label)
    {
        ifs.close();
        throw std::logic_error("error file format " + file_dir);
    }
    ifs.close();

#ifndef WIN32
    int offset = 8;
    if (magic_number == image)
        offset = 16;
    char buf[16] = { 0 };
    read(*reinterpret_cast<int*>(handle), buf, offset);
#endif
    return item_num;
}
