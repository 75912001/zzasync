#include <lib_include.h>
#include <lib_log.h>
#include <lib_file.h>

#include "util.h"
#include "daemon.h"
#include "dll.h"
#include "bench_conf.h"
#include "bind_conf.h"
#include "service_logic_thread.h"

#ifdef EL_ASYNC_USE_THREAD

namespace el_async{
	el_async::service_logic_thread_t* g_service_logic = NULL;
}

void el_async::service_logic_thread_t::do_work_fn( void* data )
{
	//////////////////////////////////////////////////////////////////////////
	//定时器
	this->timer->renew_now();
	if (!g_is_parent){
		g_addr_mcast->syn_info();
	}
	g_dll->functions.on_events();
	//////////////////////////////////////////////////////////////////////////
	//处理消息
	// 编译不过 [5/29/2014 MengLingChao]
	/*
	for (HANDLE_PEER_LIST::iterator it = this->handle_peer_msg_list.begin();
		this->handle_peer_msg_list.end() != it;){
		el::lib_tcp_peer_info_t* peer = *it;
		while (){//这个fd的消息处理完毕
			switch (peer->get_fd_type())
			{
			case el::FD_TYPE_LISTEN:
				break;
			case el::FD_TYPE_CLI:
			case el::FD_TYPE_SVR:
				break;
			case el::FD_TYPE_MCAST:
				break;
			case el::FD_TYPE_ADDR_MCAST:
				break;
			case el::FD_TYPE_UDP:
				break;
			default:
				break;
			}
		}
		

		
		
		if (1){
			this->handle_peer_msg_list.erase(it++);
		}else{
			++it;
		}
		
	}
	
	FOREACH(this->handle_peer_msg_list, it){

	}
	*/
}

int el_async::service_logic_thread_t::do_work_begin( void* data )
{
	if (SUCC != g_dll->functions.on_init()){
		ALERT_LOG("");
		exit(0);
	}

	return SUCC;
}

int el_async::service_logic_thread_t::do_work_end( void* data )
{
	return g_dll->functions.on_fini();
}

el_async::service_logic_thread_t::service_logic_thread_t()
{
	this->timer = new el::lib_timer_t;
}

el_async::service_logic_thread_t::~service_logic_thread_t()
{
	SAFE_DELETE(this->timer);
}



#endif
