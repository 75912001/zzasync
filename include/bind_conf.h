/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	��ȡbind.xml�����ļ�
	brief:		ok
*********************************************************************/
#pragma once

#include <lib_include.h>
#include <lib_pipe.h>

namespace el_async{

	#pragma pack(1)

	//ÿ�����̵�����
	struct bind_config_elem_t {
		uint32_t		id;//���
		std::string		name;//����
		uint8_t			net_type;//��������
		std::string		ip;
		uint16_t		port;
		std::string		data;
		uint8_t			restart_cnt;//�������Ĵ���(��ֹ���ϵ�����)
#ifndef WIN32
		el::lib_pipe_t		send_pipe;//����ӽ��̵�д
		el::lib_pipe_t		recv_pipe;//����ӽ��̵Ķ�
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
		// Brief:     ���������ļ�
		// Returns:   int 0:success, -1:error
		//************************************
		int load();

	protected:
	private:
		bind_config_t(){};
	};
}

#define g_bind_conf el_async::bind_config_t::get_instance()
