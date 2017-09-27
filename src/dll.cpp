#include <lib_include.h>
#include <lib_log.h>

#include "bench_conf.h"
#include "bind_conf.h"
#include "daemon.h"
#include "dll.h"
#include "dll_service.h"

namespace {

	#define DLFUNC(v, name) \
	{\
		v = (decltype(v))dlsym (this->handle, name);\
		if (NULL != (error = dlerror())) {\
			ALERT_LOG("DLSYM ERROR [error:%s, fn:%p]", error, v);\
			dlclose(this->handle);\
			this->handle = NULL;\
			goto out;\
		}\
	}

	/**
	 * @brief	重命名core文件(core.进程id)
	 */
	void rename_core(int pid)
	{
#ifndef WIN32
		::chmod("core", 777);
		::chown("core", 0, 0);
		char core_name[1024] ={0};
		::sprintf(core_name, "core.%d", pid);
		::rename("core", core_name);
#endif//WIN32
	}
}//end of namespace

int el_async::dll_t::register_plugin()
{
#ifndef WIN32
	char* error; 
	int   ret_code = ERR;

	this->handle = dlopen(g_bench_conf->liblogic_path.c_str(), RTLD_NOW);
	if ((error = dlerror()) != NULL) {
		BOOT_LOG(ret_code, "DLOPEN ERROR [error:%s]", error);
	}

	DLFUNC(this->functions.on_get_pkg_len, "on_get_pkg_len");
	DLFUNC(this->functions.on_cli_pkg, "on_cli_pkg");
 	DLFUNC(this->functions.on_srv_pkg, "on_srv_pkg");
 	DLFUNC(this->functions.on_cli_conn_closed, "on_cli_conn_closed");
	DLFUNC(this->functions.on_cli_conn, "on_cli_conn");
 	DLFUNC(this->functions.on_svr_conn_closed, "on_svr_conn_closed");

   	DLFUNC(this->functions.on_init, "on_init");
  	DLFUNC(this->functions.on_fini, "on_fini");
  	DLFUNC(this->functions.on_events, "on_events");
	DLFUNC(this->functions.on_mcast_pkg, "on_mcast_pkg");
	DLFUNC(this->functions.on_addr_mcast_pkg, "on_addr_mcast_pkg");
	DLFUNC(this->functions.on_udp_pkg, "on_udp_pkg");

	DLFUNC(this->functions.on_svr_conn, "on_svr_conn");
	ret_code = SUCC;
out:
	BOOT_LOG(ret_code, "dlopen [file name:%s, state:%s]",
		g_bench_conf->liblogic_path.c_str(), (0 != ret_code ? "FAIL" : "OK"));
#else
	this->functions.on_get_pkg_len = on_get_pkg_len;
	this->functions.on_cli_pkg = on_cli_pkg;
	this->functions.on_srv_pkg = on_srv_pkg;
	this->functions.on_cli_conn_closed = on_cli_conn_closed;
	this->functions.on_cli_conn = on_cli_conn;
	this->functions.on_svr_conn_closed = on_svr_conn_closed;
	this->functions.on_init = on_init;
	this->functions.on_fini = on_fini;
	this->functions.on_events = on_events;
	this->functions.on_mcast_pkg = reinterpret_cast<el::on_functions_tcp_srv::ON_MCAST_PKG>(on_mcast_pkg);
	this->functions.on_addr_mcast_pkg = reinterpret_cast<el::on_functions_tcp_srv::ON_ADDR_MCAST_PKG>(on_addr_mcast_pkg);
	this->functions.on_udp_pkg = reinterpret_cast<el::on_functions_tcp_srv::ON_UDP_PKG>(on_udp_pkg);
	this->functions.on_svr_conn = on_svr_conn;
	return SUCC;
#endif//WIN32
}

el_async::dll_t::dll_t()
{
	this->handle = NULL;
}

el_async::dll_t::~dll_t()
{
#ifndef WIN32
	if (NULL != this->handle){
		dlclose(this->handle);
		this->handle = NULL;
	}
#endif//WIN32
}

#ifndef WIN32
int el_async::dll_t::on_pipe_event( int fd, epoll_event& r_evs )
{
	if (r_evs.events & EPOLLHUP) {
		if (g_is_parent) {
			//////////////////////////////////////////////////////////////////////////
			pid_t pid;
			int	status;
			while ((pid = waitpid (-1, &status, WNOHANG)) > 0) {
				for (uint32_t i = 0; i < g_bind_conf->elems.size(); ++i) {
					if (atomic_read(&g_daemon->child_pids[i]) == pid) {
						atomic_set(&g_daemon->child_pids[i], 0);
						bind_config_elem_t& elem = g_bind_conf->elems[i];
						CRIT_LOG("child process crashed![olid:%u, olname:%s, fd:%d, restart_cnt;%u, restart_cnt_max:%u, pid=%d]",
							elem.id, elem.name.c_str(), fd, elem.restart_cnt, g_bench_conf->restart_cnt_max, pid);
						rename_core(pid);
						// prevent child process from being restarted again and again forever
						if (++elem.restart_cnt <= g_bench_conf->restart_cnt_max) {
							g_daemon->restart_child_process(&elem, i);
						}else{
							//关闭pipe(不使用PIPE,并且epoll_wait不再监控PIPE)
							elem.recv_pipe.close(el::E_PIPE_INDEX_WRONLY);
							elem.send_pipe.close(el::E_PIPE_INDEX_RDONLY);
						}
						break;
					}
				}
			}
		} else {
			// Parent Crashed
			CRIT_LOG("parent process crashed!");
			g_daemon->stop = true;
			g_daemon->restart = false;
			return 0;
		}
	} else {
		CRIT_LOG("unuse ???");
		//read
		if (g_is_parent){
		}else{
		}
// 		char trash[trash_size];
// 		while (trash_size == read(fd, trash, trash_size)) ;
	}
	return 0;
}
#endif//WIN32