/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	��ȡbench.ini�����ļ�
	brief:		ok
*********************************************************************/
#pragma once

#include <lib_include.h>

#include <lib_util.h>
#include <lib_file_ini.h>

namespace el_async{

	class bench_conf_t
	{
	public:
		uint32_t max_fd_num;//��fd���������
		bool is_daemon;//�Ƿ��̨����
		std::string liblogic_path;////�����SO·��
		uint32_t log_level;//��־�ȼ�
		time_t fd_time_out;//���ӵ�FD��ʱ����,0:�޳�ʱ
		uint32_t page_size_max;//���ݰ�������ֽ���
		uint32_t listen_num;//tcp listen ����.Ĭ��1024
		std::string log_dir;//��־Ŀ¼
		uint32_t log_save_next_file_interval_min;//ÿ����ʱ��(����)���±����ļ���(���ļ�)
		uint32_t core_size;//core�ļ��Ĵ�С���ֽ�
		uint32_t restart_cnt_max;//�����������
		std::string daemon_tcp_ip;//�ػ����̵�ַ//Ĭ��Ϊ0
		uint16_t daemon_tcp_port;//�ػ����̶˿�//Ĭ��Ϊ0
		uint32_t daemon_tcp_max_fd_num;//�ػ����̴�fd���������//Ĭ��Ϊ20000

		std::string mcast_incoming_if;//�鲥 ����//Ĭ��Ϊ0
		std::string mcast_outgoing_if;//�鲥 ����//Ĭ��Ϊ0
		std::string mcast_ip;//239.X.X.X�鲥��ַ//Ĭ��Ϊ0
		uint32_t mcast_port;//239.X.X.X�鲥�˿�//Ĭ��Ϊ0

		std::string addr_mcast_incoming_if;//��ַ��Ϣ �鲥 ����//Ĭ��Ϊ0
		std::string addr_mcast_outgoing_if;//��ַ��Ϣ �鲥 ����//Ĭ��Ϊ0
		std::string addr_mcast_ip;//239.X.X.X��ַ��Ϣ �鲥��ַ//Ĭ��Ϊ0
		uint32_t addr_mcast_port;//239.X.X.X��ַ��Ϣ �鲥�˿�//Ĭ��Ϊ0
		virtual ~bench_conf_t(){}
		static bench_conf_t* get_instance(){
			static bench_conf_t bench_conf;
			return &bench_conf;
		}
	public:
		//so���û�ȡ������bench.ini�е�����(��������)
		//��ȡ����������
		std::string get_strval(const char* section, const char* name);
		bool get_uint32_val(const char* section, const char* name, uint32_t& val);
		uint32_t get_uint32_def_val(const char* section, const char* name, uint32_t def = 0);
		//************************************
		// Brief:     ���������ļ�bench.ini
		// Returns:   int(0:��ȷ,����:����)
		//************************************
		int load(const char* path_suf);
	private:
		bench_conf_t();
		el::lib_file_ini_t file_ini;
	};
}

#define g_bench_conf el_async::bench_conf_t::get_instance()
