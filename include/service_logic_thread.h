/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	
	brief:		子进程服务
*********************************************************************/

#pragma once

#include <lib_include.h>
#include <lib_thread.h>
#include <lib_timer.h>
#include "net_tcp.h"


#ifndef WIN32
//#define EL_ASYNC_USE_THREAD
#endif

#ifdef EL_ASYNC_USE_THREAD
namespace el_async{
	enum E_FD_EVENT_TYPE{
		E_FD_EVENT_READ = 1,//可读
		E_FD_EVENT_WRITE = 2,//可写
		E_FD_EVENT_CLOSE = 3,//关闭
	};
	struct fd_event 
	{
		el::lib_tcp_peer_info_t* peer;
		E_FD_EVENT_TYPE fd_event_type;
	};
	
	class service_logic_thread_t : public el::lib_thread_t
	{
	public:
		service_logic_thread_t();
		virtual ~service_logic_thread_t();
		virtual int do_work_begin(void* data);
		virtual void do_work_fn(void* data);
		virtual int do_work_end(void* data);
		typedef std::list<fd_event> HANDLE_FD_EVENT_LIST;
		HANDLE_FD_EVENT_LIST handle_fd_event_list;
		void add_fd_event(el::lib_tcp_peer_info_t* peer, E_FD_EVENT_TYPE fd_event){
			switch (fd_event)
			{
			case E_FD_EVENT_READ:
				break;
			case E_FD_EVENT_WRITE:
				break;
			case E_FD_EVENT_CLOSE:
				break;
			default:
				break;
			}
		}
		el::lib_timer_t* timer;
	protected:
		
	private:
		service_logic_thread_t(const service_logic_thread_t& cr);
		service_logic_thread_t& operator=(const service_logic_thread_t& cr);
	};

	extern el_async::service_logic_thread_t* g_service_logic;
}

#endif