#include "async_reader.h"
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include "threadSafeQueue.h"
#include <thread>
#include <future>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

enum class STATUS
{
    READY,
    READING,
    FINISH,
};

static enum STATUS m_read_state = STATUS::READY;
static std::string m_file_dir = "";
static CThreadSafeQueue<CDataPkg *> m_read_buff(2);
static const int m_read_len = 256;
static boost::asio::posix::stream_descriptor *m_stream_ptr = nullptr;
static int64_t m_read_size = 0;
static int64_t m_file_size = 0;
static boost::asio::io_service m_ios;

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

    m_read_state = STATUS::READY;
}

int64_t get_file_size(const char *file_name)
{
    std::ifstream ifs(file_name, std::ios::in);
    if (!ifs)
        throw std::invalid_argument("can not open file: " + std::string(file_name));
    ifs.seekg(0, ifs.end);
    int64_t size = ifs.tellg();
    ifs.close();
    return size;
}

void *create_item_reader(const char *file_path)
{
    m_file_dir = std::string(file_path);
    int *fm = new int(open(m_file_dir.data(), O_RDONLY));
    if (*fm < 0)
    {
        delete fm;
        return nullptr;
    }
    m_read_size = 0;
    m_file_size = get_file_size(m_file_dir.data());
    std::cout << file_path << " size: " << m_file_size << std::endl;
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

void read_handler(CDataPkg *datapkg, const boost::system::error_code e, size_t read)
{
    if (datapkg->data && read > 0)
    {
        std::cout << "read " << read << " Bytes \n";
        datapkg->length = read;
        m_read_buff.push(datapkg);
    }
    m_read_size += read;

    if (m_read_size < m_file_size)
    {
        CDataPkg *buff_ptr = new CDataPkg(m_read_len);
        boost::asio::mutable_buffers_1 dataBuff(buff_ptr->data, buff_ptr->length);
        boost::asio::async_read(*m_stream_ptr, dataBuff,
                                std::bind(read_handler, buff_ptr, std::placeholders::_1, std::placeholders::_2));
    }
    else
    {
        m_read_state = STATUS::FINISH;
        std::cout << "total read " << m_read_size << " bytes\n";
    }
}

void read_data_daemon(void *handle)
{
    m_stream_ptr = new boost::asio::posix::stream_descriptor(m_ios, *(int *)(handle));
    std::cout << "start reading thread\n";
    CDataPkg *data_pkg = new CDataPkg(m_read_len);
    read_handler(data_pkg, boost::system::error_code(), 0);
    m_ios.run();
    m_read_buff.set_end();
    std::cout << "reading finish, reading thread exit ------------------\n";
}

int read_item_data(void *handle, char *buf, int *len)
{
    buf = nullptr;
    *len = 0;

    if (!handle || m_read_buff.is_end() && m_read_buff.empty())
    {
        return -1;
    }
    std::cout << "ready to read data \n";
    if (m_read_state == STATUS::READY)
    {
        m_read_state = STATUS::READING;
        std::thread t(read_data_daemon, handle);
        t.detach();
    }
    CDataPkg *data_pkg = nullptr;
    std::cout << "popping data ...\n";
    bool pop_state = m_read_buff.pop(data_pkg);
    if (pop_state)
    {
        buf = data_pkg->data;
        *len = data_pkg->length;
    }
    std::cout << "fetch data from queue, pop state: " << pop_state << ",fetch Bytes: " << *len << std::endl;
    return 0;
}

int close_item_reader(void *handle)
{
    if (handle && *((int *)handle) < 0)
    {
        close(*(int *)handle);
    }
    return 0;
}

uint64_t get_item_number(void *handle)
{
    std::ifstream *is = (std::ifstream *)handle;
    is->seekg(0, is->beg);
    uint64_t n = 0;
    std::string s = "s";
    while (s.size() > 0 && !is->eof())
    {
        s.clear();
        std::getline(*is, s);
        if (s.size() > 0)
        {
            n++;
        }
    }
    is->seekg(0, is->beg);
    return n;
}
