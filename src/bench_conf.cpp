#include <lib_include.h>
#include <lib_log.h>
#include <lib_util.h>

#include "bench_conf.h"

namespace {
}//end of namespace

int el_async::bench_conf_t::load(const char* path_suf)
{
	int ret = 0;

	this->file_ini.load(path_suf);

	if (0 != this->file_ini.get_val(this->liblogic_path, "plugin", "liblogic")){
		BOOT_LOG(ERR, "[%s]plugin,liblogic", path_suf);
		return -1;
	}
	this->file_ini.get_val(this->max_fd_num, "common", "max_fd_num");
	this->file_ini.get_val(this->is_daemon, "common", "is_daemon");
	this->file_ini.get_val(this->fd_time_out, "common", "fd_time_out");
	this->file_ini.get_val(this->page_size_max, "common", "page_size_max");
	this->file_ini.get_val(this->listen_num, "common", "listen_num");
	if (0 != this->file_ini.get_val(this->log_dir, "log", "dir")){
		BOOT_LOG(ERR, "log,dir");
		return -1;
	}
	this->file_ini.get_val(this->log_level, "log", "level");
	this->file_ini.get_val(this->log_save_next_file_interval_min, "log", "save_next_file_interval_min");
	this->file_ini.get_val(this->core_size, "core", "size");
	this->file_ini.get_val(this->restart_cnt_max, "core", "restart_cnt_max");
	this->file_ini.get_val(this->daemon_tcp_ip, "net", "daemon_tcp_ip");
	this->file_ini.get_val(this->daemon_tcp_port, "net", "daemon_tcp_port");
	this->file_ini.get_val(this->daemon_tcp_max_fd_num, "net", "daemon_tcp_max_fd_num");

	this->file_ini.get_val(this->mcast_incoming_if, "multicast", "mcast_incoming_if");
	this->file_ini.get_val(this->mcast_outgoing_if, "multicast", "mcast_outgoing_if");
	this->file_ini.get_val(this->mcast_ip, "multicast", "mcast_ip");
	this->file_ini.get_val(this->mcast_port, "multicast", "mcast_port");

	this->file_ini.get_val(this->addr_mcast_incoming_if, "addr_multicast", "mcast_incoming_if");
	this->file_ini.get_val(this->addr_mcast_outgoing_if, "addr_multicast", "mcast_outgoing_if");
	this->file_ini.get_val(this->addr_mcast_ip, "addr_multicast", "mcast_ip");
	this->file_ini.get_val(this->addr_mcast_port, "addr_multicast", "mcast_port");

	return ret;
}

el_async::bench_conf_t::bench_conf_t()
{
	this->max_fd_num = 20000;
	this->is_daemon = true;
	this->log_level = 8;
	this->log_save_next_file_interval_min = 0;
	this->fd_time_out = 0;
	this->page_size_max = 81920;
	this->core_size = 2147483648U;
	this->restart_cnt_max = 100;
	this->daemon_tcp_port = 0;
	this->daemon_tcp_max_fd_num = 20000;
	this->mcast_port = 0;
	this->addr_mcast_port = 0;
	this->listen_num = 1024;
}

std::string el_async::bench_conf_t::get_strval(const char* section, const char* name)
{
	std::string str;

	this->file_ini.get_val(str, section, name);

	return str;
}

bool el_async::bench_conf_t::get_uint32_val( const char* section, const char* name, uint32_t& val )
{
	if (0 != this->file_ini.get_val(val, section, name)){
		return false;
	}
	return true;
}

uint32_t el_async::bench_conf_t::get_uint32_def_val(const char* section, const char* name, uint32_t def)
{
	return this->file_ini.get_val_def(section, name, def);
}
