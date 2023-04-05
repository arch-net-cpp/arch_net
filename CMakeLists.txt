set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-variable")

add_compile_options(-Wno-deprecated-declarations)

FILE(GLOB SRC_FILES  *.h *.cpp *.hpp
        udp/*.h udp/*.c udp/*.cpp
        utils/*.cpp utils/*.h utils/*.hpp
        utils/murmurhash3/*.cpp utils/murmurhash3/*.h
        utils/future/*.h)

cc_library(arch_net_core SRCS ${SRC_FILES} DEPS libacl_fiber libacl libacl_cpp libprotocol libfiber libfiber_cpp glog ssl crypto lz4 snappy zstd zlib)

FILE(GLOB COIN_SRC_FILES  http/*.h http/*.cpp http/*.cc http/router/*.cpp)
cc_library(arch_coin SRCS ${COIN_SRC_FILES} DEPS arch_net_core gflags)

FILE(GLOB ROBIN_SRC_FILES  rpc/*.h rpc/*.cpp)
cc_library(arch_robin SRCS ${ROBIN_SRC_FILES} DEPS arch_net_core)

thrift_library(twitter_test SRCS test/twitter.thrift)
cc_test(arch_Test_thrift SRCS test/robin_thrift_test.cpp DEPS arch_robin twitter_test)

cc_test(arch_Test_http SRCS test/http_test.cpp DEPS arch_coin)

cc_test(arch_Test_http_router SRCS test/http_router_test.cpp DEPS arch_coin)

cc_test(arch_Test_utils SRCS test/utils_test.cpp DEPS arch_coin)

cc_test(arch_Test_UDP SRCS test/udp_test.cpp DEPS arch_net_core)

cc_test(arch_Test_Socket SRCS test/socket_test.cpp DEPS arch_net_core)

cc_test(arch_Test_Load_balance SRCS test/loadbalance_resolver_test.cpp DEPS arch_net_core)

cc_test(arch_Test_Future SRCS test/future_test.cpp DEPS arch_net_core)

cc_test(arch_Test_Mux SRCS test/multiplexing_test.cpp DEPS arch_net_core)

cc_test(arch_Test_SSL SRCS test/ssl_socket_test.cpp DEPS arch_net_core)

#proto_library(echo_lib SRCS test/echo.proto)
#cc_test(arch_Test_pbrpc_codec SRCS test/pbrpc_codec_test.cpp DEPS arch_robin echo_lib)

proto_library(echo_lib SRCS test/echo.proto)
cc_test(arch_Test_PBrpc SRCS test/robin_pb_test.cpp DEPS arch_robin echo_lib)

cc_test(arch_Test_rpc_streaming SRCS test/robin_rpcstreaming_test.cpp DEPS arch_robin echo_lib)

cc_binary(mtls_test
        SRCS test/mtls_test/client.c test/mtls_test/client.h
             test/mtls_test/server.h test/mtls_test/server.c
             test/mtls_test/master.c
        DEPS ssl crypto)

robin_library(mylib IDL test/echo.proto)

robin_library(hislib IDL test/twitter.thrift)