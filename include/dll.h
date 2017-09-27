/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	接口
	brief:		ok
*********************************************************************/

#pragma once

#include <lib_include.h>

#include <lib_util.h>

#include "net_tcp.h"
namespace el_async{
	class dll_t
	{
	public:
		void* handle;
		virtual ~dll_t();
		static dll_t* get_instance(){
			static dll_t dll;
			return &dll;
		}
		el::on_functions_tcp_srv functions;
		/**
		 * @brief	注册插件
		 * @return	int
		 */
		int  register_plugin();
#ifndef WIN32
		/**
		 * @brief	处理PIPE管道消息的回调函数
		 * @param	int fd
		 * @return	int
		 */
		static int on_pipe_event(int fd, epoll_event& r_evs);
#endif//WIN32
	protected:
	private:
		dll_t();
	};
}

#define g_dll el_async::dll_t::get_instance()

