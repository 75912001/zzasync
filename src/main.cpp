#include <lib_include.h>
#include <lib_log.h>
#include <lib_file.h>
#include <lib_util.h>
#include <lib_timer.h>
#include <lib_pipe.h>

#include "daemon.h"
#include "dll.h"
#include "bind_conf.h"
#include "service.h"
#include "bench_conf.h"
#include "net_tcp.h"
#include "util.h"
#include "versions.h"

int main(int argc, char* argv[])
{
	int ret = SUCC;
	g_timer = new el::lib_timer_t;
	g_log = new el::lib_log_t;
	static const char version[] = VERSION_NUM;
#ifndef WIN32
	DEBUG_LOG("Async Server version:\e[1m\e[32m%s\e[m (C) 2012-2016, report bugs to <19426792@qq.com>", version);
	DEBUG_LOG("Compiled at \e[1m\e[32m%s %s\e[m", __DATE__, __TIME__);
#endif
	if (el::is_little_endian()){
		DEBUG_LOG("LITTLE ENDIAN");
	} else {
#ifndef WIN32
		DEBUG_LOG("\e[5m\e[33mBIG ENDIAN\e[m");
#endif
	}

	std::vector<std::string> argvs;

	for (int i = 0; i < argc; i++){
		std::string str = argv[i];
		argvs.push_back(str);
		if (0 == i){
#ifndef WIN32
			DEBUG_LOG("\e[1m\e[32m%s\e[m", str.c_str());
#endif
		} else {
			DEBUG_LOG("%s", str.c_str());
		}
	}

	if (argvs.size() < 2){
		BOOT_LOG(ERR, "argv num ???[%u]",(uint32_t)argvs.size());
	}

	if (SUCC != g_bench_conf->load(argvs[1].c_str())){
		BOOT_LOG(ERR, "bench conf load ???");
	}

	if (SUCC != g_bind_conf->load()){
		BOOT_LOG(ERR, "bind conf load ???");
	}

	if (SUCC != g_log->init(g_bench_conf->log_dir.c_str(),
		(el::lib_log_t::E_LOG_LEVEL)g_bench_conf->log_level,
		NULL, g_bench_conf->log_save_next_file_interval_min)){
		BOOT_LOG(ERR, "lib_log_t::init ???");
	}

	if (SUCC != g_dll->register_plugin()){
		ret = ERR;
		goto loop_ret;
	}

	if (SUCC != g_net_server->create(g_bench_conf->daemon_tcp_max_fd_num)){
		BOOT_LOG(ERR, "net_server_t::init ???");
	}

	g_daemon->prase_args(argc, argv);
#ifndef WIN32
	g_net_server->server_epoll->on_pipe_event = el_async::dll_t::on_pipe_event;
#endif//WIN32
	g_net_server->server_epoll->epoll_wait_time_out = el_async::EPOLL_TIME_OUT;

	if (SUCC != g_dll->functions.on_init()) {
		g_log->boot(ERR, 0, "FAILED TO INIT PARENT PROCESS");
		ret = ERR;
		goto loop_ret;
	}

	::open("/dev/null", O_RDONLY);
	::open("/dev/null", O_WRONLY);
	::open("/dev/null", O_RDWR);

	g_log->start();

	WSAStartup();

	for (uint32_t i = 0; i != g_bind_conf->elems.size(); ++i ) {
		el_async::bind_config_elem_t& bc_elem = g_bind_conf->elems[i];
#ifndef WIN32
		if (0 != bc_elem.recv_pipe.create()
			|| 0 != bc_elem.send_pipe.create()){
			ERROR_LOG("pipe create err");
			ret = ERR;
			goto loop_ret;
		}
		pid_t pid;
		if ( (pid = fork()) < 0 ) {
			g_log->boot(ERR, 0, "fork child process err [id:%u]", bc_elem.id);
			ret = ERR;
			goto loop_ret;
		} else if (pid > 0) {
			//父进程
			g_bind_conf->elems[i].recv_pipe.close(el::E_PIPE_INDEX_RDONLY);
			g_bind_conf->elems[i].send_pipe.close(el::E_PIPE_INDEX_WRONLY);
			g_net_server->server_epoll->add_connect(bc_elem.send_pipe.read_fd(), el::FD_TYPE_PIPE, NULL, 0);
			atomic_set(&g_daemon->child_pids[i], pid);
		} else {
			//子进程
			g_service->run(&bc_elem, i + 1);
			return SUCC;
		}
#else
		//子进程	
		g_service->run(&bc_elem, i + 1);
		return SUCC;
#endif//WIN32
	}

	//parent process
	if (!g_bench_conf->daemon_tcp_ip.empty()){
		if (SUCC != g_net_server->server_epoll->listen(g_bench_conf->daemon_tcp_ip.c_str(),
			g_bench_conf->daemon_tcp_port, g_bench_conf->listen_num, g_bench_conf->page_size_max)){
			g_log->boot(ERR, 0, "daemon listen err [ip:%s, port:%u]", 
			g_bench_conf->daemon_tcp_ip.c_str(), g_bench_conf->daemon_tcp_port);
			ret = ERR;
			goto loop_ret;
		}
	}
	g_daemon->run();
loop_ret:
	g_daemon->killall_children();
	g_net_server->destroy();
	g_log->stop();
	SAFE_DELETE(g_log);
	SAFE_DELETE(g_timer);

#ifdef WIN32
	WSACleanup();
#endif
	return ret;
}
