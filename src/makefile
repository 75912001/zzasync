#选项 -rdynamic 用来通知链接器将所有符号添加到动态符号表中r(目的是能够通过使用 dlopen 来实现向后跟踪)
EXE=async-server.exe
CC=g++ -std=c++0x

#编译选项

EL_LIBS_FLAG=-Wl,-rpath,$(EL_LIB_PATH)
#FLAG=-Wall -DDEBUG -DSHOW_ALL_LOG -DSHOW_ALL_LOG_HEX -DENABLE_TRACE_LOG -g -O0 -o
FLAG=-Wall -DDEBUG -DENABLE_TRACE_LOG -g -O0 -o
LOCAL_INCLUDE=../include 

#包含的头文件
INCLUDES=-I/usr/include -I/usr/include/libxml2 -I../include -I$(EL_LIB_INCLUDE)/include 

LINK=-ldl -rdynamic -lxml2 -lpthread -lel $(EL_LIBS_FLAG)

LIBS=-L/usr/lib64 -L/usr/lib -L$(EL_LIB_PATH)

#编译的代码文件
LOCAL_SRCS =$(wildcard ./*.cpp)
OTHER_SRCS =

$(EXE):
	$(CC) $(LOCAL_SRCS) $(OTHER_SRCS) $(INCLUDES) $(LIBS) $(LINK) $(FLAG) $@
#	rm ./bin/$(EXE)
	mkdir -p $(EL_LIB_PATH)
	cp $(EXE) $(EL_LIB_PATH)
	mkdir -p $(AS_INCLUDE)
	cp -rf $(LOCAL_INCLUDE) $(AS_INCLUDE)
#	rm -rf $(EL_LIB_PATH)/async_include
	rm -f $(EXE)
clean:
	rm -f $(EXE)


