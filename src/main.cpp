﻿#include <string>
#include "async_reader.h"
#ifndef WIN32
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <iostream>

int main()
{
	try
	{
#ifdef WIN32
		// t10k-images.idx3-ubyte train-labels-idx1-ubyte
		std::string file_dir = "D:\\t10k-images.idx3-ubyte";
#endif
#ifndef WIN32
		std::string file_dir = "/home/zxhu/gitLab/dataset/testdata.manual.2009.06.14.csv";
#endif
		/*std::cout << "please input a path:\n";
		std::cin >> file_dir;*/

		void* handle = create_item_reader(file_dir.data());
		std::cout << "item number: " << get_item_number(handle)<<"\n";
		std::cout << "start to read...\n";
		

		
		int read_len = 0;
		int len = 0;
		do
		{
			len = 0;
			char buf[1024] = { 0 };
			read_item_data(handle, buf, &len);
			read_len += len;
			std::cout << "current read: " << len << ", total fetch " << read_len << " Bytes\n";
		} while (len > 0);

		std::cout << "reading finish, total reading " << read_len << " Bytes----\n";
		std::cout << "file length :" << get_file_size(file_dir.data()) << " Bytes----\n";
	}
	catch (std::exception e) {
		std::cerr<<"ERROR:   "<< e.what() << std::endl;
	}
	return 0;
}
