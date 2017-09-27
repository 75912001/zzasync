/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	读取bench.ini配置文件
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
		uint32_t max_fd_num;//打开fd的最大数量
		bool is_daemon;//是否后台运行
		std::string liblogic_path;////代码段SO路径
		uint32_t log_level;//日志等级
		time_t fd_time_out;//连接的FD超时秒数,0:无超时
		uint32_t page_size_max;//数据包的最大字节数
		uint32_t listen_num;//tcp listen 数量.默认1024
		std::string log_dir;//日志目录
		uint32_t log_save_next_file_interval_min;//每多少时间(分钟)重新保存文件中(新文件)
		uint32_t core_size;//core文件的大小，字节
		uint32_t restart_cnt_max;//最大重启次数
		std::string daemon_tcp_ip;//守护进程地址//默认为0
		uint16_t daemon_tcp_port;//守护进程端口//默认为0
		uint32_t daemon_tcp_max_fd_num;//守护进程打开fd的最大数量//默认为20000

		std::string mcast_incoming_if;//组播 接收//默认为0
		std::string mcast_outgoing_if;//组播 发送//默认为0
		std::string mcast_ip;//239.X.X.X组播地址//默认为0
		uint32_t mcast_port;//239.X.X.X组播端口//默认为0

		std::string addr_mcast_incoming_if;//地址信息 组播 接收//默认为0
		std::string addr_mcast_outgoing_if;//地址信息 组播 发送//默认为0
		std::string addr_mcast_ip;//239.X.X.X地址信息 组播地址//默认为0
		uint32_t addr_mcast_port;//239.X.X.X地址信息 组播端口//默认为0
		virtual ~bench_conf_t(){}
		static bench_conf_t* get_instance(){
			static bench_conf_t bench_conf;
			return &bench_conf;
		}
	public:
		//so调用获取配置项bench.ini中的数据(自行配置)
		//获取配置项数据
		std::string get_strval(const char* section, const char* name);
		bool get_uint32_val(const char* section, const char* name, uint32_t& val);
		uint32_t get_uint32_def_val(const char* section, const char* name, uint32_t def = 0);
		//************************************
		// Brief:     加载配置文件bench.ini
		// Returns:   int(0:正确,其它:错误)
		//************************************
		int load(const char* path_suf);
	private:
		bench_conf_t();
		el::lib_file_ini_t file_ini;
	};
}

#define g_bench_conf el_async::bench_conf_t::get_instance()
