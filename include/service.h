/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	
	brief:		子进程服务
*********************************************************************/

#pragma once

#include <lib_include.h>
#include <lib_net/lib_multicast.h>

namespace el_async{
	class service_t
	{
	public:
		struct bind_config_elem_t*	bind_elem;
	public:
		virtual ~service_t(){}
		static service_t* get_instance(){
			static service_t service;
			return &service;
		}
		void run(bind_config_elem_t* bind_elem, int n_inited_bc);
		uint32_t get_bind_elem_id();
		const std::string& get_bind_elem_ip();
		uint16_t get_bind_elem_port();
		const char* get_bind_elem_name();
		const std::string& get_bind_elem_data();
	protected:
	private:
		service_t(){}
	};
}

#define g_service el_async::service_t::get_instance()

namespace el_async{
extern el::lib_mcast_t g_mcast;
}