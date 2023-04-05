

#include "../timer_provider.h"
#include "gtest/gtest.h"
#include "../utils/compression.h"
#include "../utils/util.h"

TEST(Utils_Test, test_TimerProvider)
{

    arch_net::TC_TimeProvider::getInstance().run();
    {
        int i = 0;
        arch_net::Time t1;
        while (i < 1000000000) {
            struct timeval tv;
            arch_net::TC_TimeProvider::getInstance().getNow(&tv);
            i++;
        }
        arch_net::Time t2;
        std::cout << arch_net::Time::since(t1, t2) << std::endl;
    }
    {
        int i = 0;
        arch_net::Time t1;
        while (i < 1000000000) {
            struct timeval tv;
            ::gettimeofday(&tv, nullptr);
            i++;
        }
        arch_net::Time t2;
        std::cout << arch_net::Time::since(t1, t2) << std::endl;
    }

    arch_net::TC_TimeProvider::getInstance().terminate();
}

TEST(test_Utils, Test_Compression)
{
    std::vector<char> inbuf;
    std::string data = "";

    for (int i = 0; i < data.size(); i++) {
        inbuf.push_back(data[i]);
    }
    {
        arch_net::Compression com(CompressType::ZSTD);
        char* outbuf = new char[24000];
        auto compress_size = com.compress(&inbuf[0], inbuf.size(), outbuf, 20000);
        std::cout << compress_size << std::endl;

        std::vector<char> unbuf(24000);
        std::cout << com.decompress(outbuf, compress_size, &unbuf[0], unbuf.size()) << std::endl;

        delete[] outbuf;
    }
    {
        arch_net::Compression com(CompressType::ZSTD_HIGH);
        char* outbuf = new char[24000];
        auto compress_size = com.compress(&inbuf[0], inbuf.size(), outbuf, 20000);
        std::cout << compress_size << std::endl;

        std::vector<char> unbuf(24000);
        std::cout << com.decompress(outbuf, compress_size, &unbuf[0], unbuf.size()) << std::endl;

        delete[] outbuf;
    }
    {
        arch_net::Compression com(CompressType::LZ4);
        char* outbuf = new char[24000];
        auto compress_size = com.compress(&inbuf[0], inbuf.size(), outbuf, 20000);
        std::cout << compress_size << std::endl;

        std::vector<char> unbuf(24000);
        std::cout << com.decompress(outbuf, compress_size, &unbuf[0], unbuf.size()) << std::endl;

        delete[] outbuf;
    }
    {
        arch_net::Compression com(CompressType::LZ4_FAST);
        char* outbuf = new char[24000];
        auto compress_size = com.compress(&inbuf[0], inbuf.size(), outbuf, 20000);
        std::cout << compress_size << std::endl;

        std::vector<char> unbuf(24000);
        std::cout << com.decompress(outbuf, compress_size, &unbuf[0], unbuf.size()) << std::endl;

        delete[] outbuf;
    }
    {
        arch_net::Compression com(CompressType::SNAPPY);
        char* outbuf = new char[24000];
        auto compress_size = com.compress(&inbuf[0], inbuf.size(), outbuf, 20000);
        std::cout << compress_size << std::endl;

        std::vector<char> unbuf(24000);
        std::cout << com.decompress(outbuf, compress_size, &unbuf[0], unbuf.size()) << std::endl;

        delete[] outbuf;
    }
}