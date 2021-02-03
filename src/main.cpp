#include <string>
#include "async_reader.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main()
{

    std::string file_dir;
    std::cout << "please input a path:\n";
    std::cin >> file_dir;
    int *fm = (int *)(create_item_reader(file_dir.data()));
    if (!fm || *fm < 0)
    {
        std::cout << "can not open " << file_dir << std::endl;
        return -1;
    }
    std::cout << "start to read\n";
    char *buf = nullptr;

    int len = 0;
    int read_len = 0;
    do
    {
        std::cout << "total fetch " << read_len << " Bytes\n";
        read_item_data(fm, buf, &len);
        read_len += len;
        delete[] buf;
        buf = nullptr;

    } while (len > 0);

    std::cout << "reading finish, total reading " << read_len << " Bytes----\n";
    std::cout << "file length " << get_file_size(file_dir.data()) << " Bytes----\n";
    return 0;
}
