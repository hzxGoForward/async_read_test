#pragma once
#ifndef _MINST_READER_H
#define _MINST_READER_H
#include "stdint.h"
#include <algorithm>
#include "threadSafeQueue.h"
#include <atomic>
#include <functional>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <errno.h>
#include <fcntl.h>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>

#ifndef WIN32
#include <unistd.h>
#include <boost/asio/posix/stream_descriptor.hpp>
#else
#include <boost/asio/windows/random_access_handle.hpp>
#include <windows.h>
#endif


#ifdef __cplusplus
extern "C"
{
#endif

	void* create_item_reader(const char* file_path);
	int reset_for_read(void* handle);
	int read_item_data(void* handle, char* buf, int* len);
	int close_item_reader(void* handle);
	int64_t get_file_size(const char* file_name);
	uint64_t get_item_number(void* handle);

#ifdef __cplusplus
}

inline bool is_little_endian()
{
	int x = 1;
	return *reinterpret_cast<char*>(&x) != 0;
}

template <typename T>
T* reverse_endian(T* p)
{
	std::reverse(reinterpret_cast<char*>(p),
		reinterpret_cast<char*>(p) + sizeof(T));
	return p;
}
#endif

#endif


enum class STATUS
{
	READY,
	READING,
	FINISH,
};


class asio_read {

protected:
#ifndef WIN32
	boost::asio::posix::stream_descriptor* m_stream_ptr = nullptr;
#else
	boost::asio::windows::random_access_handle* m_stream_ptr = nullptr;
#endif
	
	void* m_handle = nullptr;
	int64_t m_buff_len = 512;
	int64_t m_total_read = 0;
	int64_t m_read_len;
	boost::asio::io_service m_ios;
	CThreadSafeQueue<CDataPkg_ptr_t> m_buff_queue;
	enum STATUS m_run_state;
	std::atomic<bool> m_stop_read = false;
	std::unique_ptr<std::thread> m_read_thread_ptr;

public:

	asio_read(void* handle, const int64_t buff_len, const int64_t read_len, const int64_t total_read) :m_handle(handle), m_buff_len(buff_len),m_total_read(total_read),m_read_len(read_len), m_ios(),m_buff_queue(256) {
		m_run_state = STATUS::READY;
		m_stop_read.store(false);
		m_read_thread_ptr = nullptr;
		
	}
	~asio_read() {
		if (m_read_thread_ptr->joinable()) {
			m_stop_read.store(true);
			m_read_thread_ptr->join();
		}
	}

	inline void run() {
		if(m_run_state == STATUS::READY&& !m_read_thread_ptr)
			m_read_thread_ptr = std::make_unique<std::thread>(&asio_read::read_data_daemon, this);
	}

	inline void stop() {
		m_stop_read.store(true);
		m_read_thread_ptr->join();
	}

	inline bool pop(CDataPkg_ptr_t& read_buff_ptr ) {
		bool pop_state = m_buff_queue.pop(read_buff_ptr);
		return pop_state;
	}

protected:

	inline void read_data_daemon()
	{
#ifndef WIN32
		m_stream_ptr = new boost::asio::posix::stream_descriptor(m_ios, *(int*)(m_handle));
#else
		m_stream_ptr = new boost::asio::windows::random_access_handle(m_ios, *(HANDLE*)m_handle);
#endif
		std::cout << "start reading thread..........\n";
		CDataPkg_ptr_t read_buff_ptr = std::make_shared<CDataPkg>(0);
		read_handler(read_buff_ptr, boost::system::error_code(), 0);
		m_ios.run();
		m_buff_queue.set_end();
		std::cout << "reading finish, reading thread exit ------------------\n";
	}


	void read_handler(const CDataPkg_ptr_t datapkg, const boost::system::error_code e, size_t read)
	{
		if (m_stop_read.load() == true)
			return;
		else if (datapkg->data && read > 0)
		{
			datapkg->length = read;
			m_buff_queue.push(datapkg);
			m_read_len += read;
		}

		if (m_read_len < m_total_read)
		{
			CDataPkg_ptr_t read_buff_ptr = std::make_shared<CDataPkg>(m_buff_len);
			boost::asio::mutable_buffers_1 dataBuff(static_cast<void*>(read_buff_ptr->data.get()), m_buff_len);
#ifndef WIN32
			boost::asio::async_read(*m_stream_ptr, dataBuff,
				std::bind(&asio_read::read_handler, this, read_buff_ptr, std::placeholders::_1, std::placeholders::_2));
#else
			boost::asio::async_read_at(*m_stream_ptr, m_read_len, dataBuff,std::bind(&asio_read::read_handler,this, read_buff_ptr, std::placeholders::_1, std::placeholders::_2));
#endif
		}
		else
		{
			m_run_state = STATUS::FINISH;
			std::cout << "total read " << m_read_len << " bytes\n";
		}
	}
};
