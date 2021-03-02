#include "minst_reader.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include "threadSafeQueue.h"
#include <thread>
#include <future>
#include <errno.h>
#include <fcntl.h>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

#ifndef WIN32
#include <unistd.h>
#include <boost/asio/posix/stream_descriptor.hpp>
#endif

#ifdef WIN32
#include <boost/asio/windows/random_access_handle.hpp>
#include <windows.h>
#endif

enum class STATUS
{
    READY,
    READING,
    FINISH,
};

static enum STATUS m_read_state = STATUS::READY;
static std::string m_file_dir = "";
static CThreadSafeQueue<CDataPkg_ptr_t> m_read_buff(256);
static int m_read_len = 512;
static int64_t m_read_size = 0;
static int64_t m_file_size = -1;
static uint32_t item_num = 0;
static boost::asio::io_service m_ios;


#ifndef WIN32
static boost::asio::posix::stream_descriptor* m_stream_ptr = nullptr;
#else
static boost::asio::windows::random_access_handle* m_stream_ptr = nullptr;
#endif

void garbage_collect()
{
    if (m_read_buff.empty() == false)
    {
        m_read_buff.clear();
        m_read_buff.set_end();
    }

    if (m_stream_ptr)
    {
        delete m_stream_ptr;
        m_stream_ptr = nullptr;
    }
    item_num = -1;
    m_read_state = STATUS::READY;
}

int64_t get_file_size(const char *file_name)
{
    if (m_file_size != -1)
        return m_file_size;
    std::ifstream ifs(file_name, std::ios::in|std::ios::binary);
    if (!ifs)
        throw std::invalid_argument("can not open file: " + std::string(file_name));
    ifs.seekg(0, ifs.end);
    m_file_size = ifs.tellg();
    ifs.close();
    return m_file_size;
}

void *create_item_reader(const char *file_path)
{
    m_file_dir = std::string(file_path);
    m_file_size = get_file_size(m_file_dir.data());
    
    std::cout << file_path << " size: " << m_file_size << std::endl;
    
#ifndef WIN32
    int *fm = new int(open(m_file_dir.data(), O_RDONLY));
#else
    HANDLE* fm = new HANDLE(::CreateFile(
        m_file_dir.data(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 0));
    if (*fm == INVALID_HANDLE_VALUE) {
        throw std::invalid_argument("can not open " + std::string(file_path));
    }
#endif 
    
    m_read_size = 0;
    item_num = get_item_number(fm);
    return fm;
}

// 暂时不明白对应的iris.cpp中这个函数的作用是什么？
int reset_for_read(void *handle)
{

    if (!handle || *((int *)handle) < 0)
    {
        return -1;
    }
    garbage_collect();
    handle = create_item_reader(m_file_dir.data());
    return 0;
}

void read_handler(const CDataPkg_ptr_t datapkg, const boost::system::error_code e, size_t read)
{
    if (datapkg->data && read > 0)
    {
        std::string str(read, 0);
        memcpy((uint8_t*)&str[0], datapkg->data.get(), read);
        std::cout << "read " << read << " Bytes, data length:  "<< str.size()<<std::endl;
        datapkg->length = read;
        m_read_buff.push(datapkg);
    }
    m_read_size += read;

    if (m_read_size < m_file_size)
    {
        CDataPkg_ptr_t read_buff_ptr = std::make_shared<CDataPkg>(m_read_len);
        std::cout << "generate string length: " << std::string(read_buff_ptr->data.get()).size() << std::endl;
        boost::asio::mutable_buffers_1 dataBuff(static_cast<void*>(read_buff_ptr->data.get()), m_read_len);

#ifndef WIN32
        boost::asio::async_read(*m_stream_ptr, dataBuff,
                                std::bind(read_handler, read_buff_ptr, std::placeholders::_1, std::placeholders::_2));
#endif
#ifdef WIN32
        boost::asio::async_read_at(*m_stream_ptr, m_read_size, dataBuff,
            std::bind(read_handler, read_buff_ptr, std::placeholders::_1, std::placeholders::_2));
#endif
    }
    else
    {
        m_read_state = STATUS::FINISH;
        std::cout << "total read " << m_read_size << " bytes\n";
    }
}

void read_data_daemon(void *handle)
{
#ifndef WIN32

    m_stream_ptr = new boost::asio::posix::stream_descriptor(m_ios, *(int*)(handle));
#endif

#ifdef WIN32
    m_stream_ptr = new boost::asio::windows::random_access_handle(m_ios, *(HANDLE*)handle);
#endif

    std::cout << "start reading thread..........\n";
    CDataPkg_ptr_t read_buff_ptr = std::make_shared<CDataPkg>(m_read_len);
    read_handler(read_buff_ptr, boost::system::error_code(), 0);
    m_ios.run();
    m_read_buff.set_end();
    
    std::cout << "reading finish, reading thread exit ------------------\n";
}

int read_item_data(void *handle, char *buf, int *len)
{
    if (!handle || m_read_buff.is_end() && m_read_buff.empty())
    {
        close_item_reader(handle);
        garbage_collect();
        return -1;
        *len = 0;
    }
    
    if (m_read_state == STATUS::READY)
    {
        std::cout << "ready to read data \n";
        m_read_state = STATUS::READING;
        std::thread t(read_data_daemon, handle);
        t.detach();
    }

    CDataPkg_ptr_t read_buff_ptr = nullptr;
    /*std::cout << "popping data ...\n";*/
    bool pop_state = m_read_buff.pop(read_buff_ptr);
    if (pop_state)
    {
        memcpy((uint8_t*)buf, (read_buff_ptr->data.get()), read_buff_ptr->length);
        *len = read_buff_ptr->length;
        // std::cout << "fetch data length: " << buf.size() << std::endl;
    }

    std::cout << "fetch data from queue, pop state: " << pop_state << ", fetch Bytes: " << *len << std::endl;
    return *len;
}

int close_item_reader(void *handle)
{
    if (handle && *((int *)handle) < 0)
    {
#ifndef WIN32
        close(*(int *)handle);
#endif
#ifdef WIN32
        CloseHandle(handle);
#endif
    }
    return 0;
}


uint64_t get_item_number(void *handle)
{
    if (item_num != 0)
        return item_num;
    std::ifstream ifs(m_file_dir.data(), std::ios::in | std::ios::binary);
    uint32_t magic_number = 0;
    ifs.read(reinterpret_cast<char*>(&magic_number), 4);
    ifs.read(reinterpret_cast<char*>(&item_num), 4);
    if (is_little_endian()) {  // MNIST data is big-endian format
        reverse_endian(&magic_number);
        reverse_endian(&item_num);
    }
    m_read_size += 8;
    uint32_t label = 0x00000801, image = 0x00000803;
     if (magic_number == image) {
        uint32_t rows = 0, cols = 0;
        ifs.read(reinterpret_cast<char*>(&rows), 4);
        ifs.read(reinterpret_cast<char*>(&cols), 4);
        if (is_little_endian()) {  // MNIST data is big-endian format
            reverse_endian(&rows);
            reverse_endian(&cols);
        }
        m_read_size += 8;
        m_read_len = rows * cols;
    }
    else if(magic_number != label) {
        ifs.close();
        throw std::logic_error("error file format " + m_file_dir);
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
