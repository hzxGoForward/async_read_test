#include <string>
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
		std::string file_dir = "D:\\test.idx1-ubyte";
#endif
#ifndef WIN32
		std::string file_dir = "/home/zxhu/gitLab/dataset/testdata.manual.2009.06.14.csv";
#endif
		/*std::cout << "please input a path:\n";
		std::cin >> file_dir;*/


		void* handle = create_item_reader(file_dir.data());

		std::cout << "start to read...\n";
		char* buf = nullptr;

		int len = 0;
		int read_len = 0;
		do
		{
			std::cout << "total fetch " << read_len << " Bytes\n";
			read_item_data(handle, buf, &len);
			read_len += len;
			delete[] buf;
			buf = nullptr;

		} while (len > 0);

		std::cout << "reading finish, total reading " << read_len << " Bytes----\n";
		std::cout << "file length " << get_file_size(file_dir.data()) << " Bytes----\n";
	}
	catch (std::exception e) {
		std::cerr<<"ERROR:   "<< e.what() << std::endl;
	}
	return 0;
}
