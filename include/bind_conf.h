/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	读取bind.xml配置文件
	brief:		ok
*********************************************************************/
#pragma once

#include <lib_include.h>
#include <lib_pipe.h>

namespace el_async{

	#pragma pack(1)

	//每个进程的数据
	struct bind_config_elem_t {
		uint32_t		id;//序号
		std::string		name;//名称
		uint8_t			net_type;//网络类型
		std::string		ip;
		uint16_t		port;
		std::string		data;
		uint8_t			restart_cnt;//重启过的次数(防止不断的重启)
#ifndef WIN32
		el::lib_pipe_t		send_pipe;//针对子进程的写
		el::lib_pipe_t		recv_pipe;//针对子进程的读
#endif//WIN32
		bind_config_elem_t();
	};
	#pragma pack()

	class bind_config_t
	{
	public:
		std::vector<bind_config_elem_t> elems;
	public:
		virtual ~bind_config_t(){}
		static bind_config_t* get_instance(){
			static bind_config_t bind_config;
			return &bind_config;
		}
		//************************************
		// Brief:     加载配置文件
		// Returns:   int 0:success, -1:error
		//************************************
		int load();

	protected:
	private:
		bind_config_t(){};
	};
}

#define g_bind_conf el_async::bind_config_t::get_instance()
