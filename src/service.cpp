#include <lib_include.h>
#include <lib_log.h>
#include <lib_file.h>
#include <lib_timer.h>

#include "net_tcp.h"
#include "util.h"
#include "daemon.h"
#include "dll.h"
#include "bench_conf.h"
#include "bind_conf.h"
#include "service.h"
#include "service_logic_thread.h"

namespace el_async{
	el::lib_mcast_t g_mcast;
}

void el_async::service_t::run( bind_config_elem_t* bind_elem, int n_inited_bc )
{
	g_is_parent = false;

	//释放资源(从父进程继承来的资源)
	SAFE_DELETE(g_timer);
	g_timer = new el::lib_timer_t;
	//这里释放父进程的日志
	g_log->clear_log_buf_list();
	SAFE_DELETE(g_log);
	g_log = new el::lib_log_t;
	// close fds inherited from parent process
	g_net_server->destroy();
#ifndef WIN32
	for (int i = 0; i != n_inited_bc; ++i ) {
		g_bind_conf->elems[i].recv_pipe.close(el::E_PIPE_INDEX_WRONLY);
		g_bind_conf->elems[i].send_pipe.close(el::E_PIPE_INDEX_RDONLY);
	}
#endif//WIN32

	this->bind_elem = bind_elem;
	char prefix[10] = { 0 };
	int  len = snprintf(prefix, 8, "%u", this->bind_elem->id);
	prefix[len] = '_';

	if (SUCC != g_log->init(g_bench_conf->log_dir.c_str(),
		(el::lib_log_t::E_LOG_LEVEL)g_bench_conf->log_level,
		prefix, g_bench_conf->log_save_next_file_interval_min)){
		g_log->boot(ERR, 0, "server log err");
		return;
	}
	g_log->start();

	//初始化子进程
	int ret = 0;
	if (0 != (ret = g_net_server->create(g_bench_conf->max_fd_num))){
		ALERT_LOG("g_net_server->create err [ret:%d]", ret);
		return;
	}
	tcp_server_epoll_t* ser = g_net_server->server_epoll;
#ifndef WIN32
	ser->on_pipe_event = dll_t::on_pipe_event;
	ser->add_connect(this->bind_elem->recv_pipe.read_fd(), el::FD_TYPE_PIPE, NULL, 0);
#endif//WIN32
	ser->epoll_wait_time_out = EPOLL_TIME_OUT;
	if (0 != ser->listen(this->bind_elem->ip.c_str(),
		this->bind_elem->port, g_bench_conf->listen_num, g_bench_conf->page_size_max)){
			g_log->boot(ERR, 0, "server listen err [ip:%s, port:%u]", this->bind_elem->ip.c_str(), this->bind_elem->port);
			return;
	}

#ifndef EL_ASYNC_USE_THREAD
	if ( SUCC != g_dll->functions.on_init()) {
		ALERT_LOG("FAIL TO INIT WORKER PROCESS. [id=%u, name=%s]", this->bind_elem->id, this->bind_elem->name.c_str());
		goto fail;
	}
#endif

	//创建组播
	if (!g_bench_conf->mcast_ip.empty()){
		if (SUCC != g_mcast.create(g_bench_conf->mcast_ip, 
			g_bench_conf->mcast_port, g_bench_conf->mcast_incoming_if,
			g_bench_conf->mcast_outgoing_if)){
				ALERT_LOG("mcast.create err[ip:%s]", g_bench_conf->mcast_ip.c_str());
			return;
		}else{
			ser->add_connect(g_mcast.fd,
				el::FD_TYPE_MCAST, g_bench_conf->mcast_ip.c_str(), g_bench_conf->mcast_port);
		}
	}

	//创建地址信息组播
	if (!g_bench_conf->addr_mcast_ip.empty()){
		if (SUCC != g_addr_mcast->create(g_bench_conf->addr_mcast_ip,
			g_bench_conf->addr_mcast_port, g_bench_conf->addr_mcast_incoming_if,
			g_bench_conf->addr_mcast_outgoing_if)){
				ALERT_LOG("addr mcast.create err [ip:%s]", g_bench_conf->addr_mcast_ip.c_str());
				return;
		} else {
			ser->add_connect(g_addr_mcast->fd,
				el::FD_TYPE_ADDR_MCAST, g_bench_conf->addr_mcast_ip.c_str(), g_bench_conf->addr_mcast_port);
			//取消第一次发包的标记		
			//g_addr_mcast->mcast_notify_addr(MCAST_CMD_ADDR_1ST);
			g_addr_mcast->mcast_notify_addr();
		}
	}
#ifdef EL_ASYNC_USE_THREAD	
	g_service_logic = new el_async::service_logic_thread_t;
	g_service_logic->start(NULL);
#endif

	// 这里网络线程和逻辑线程退出先后会影响到数据是否能发送出去
	// 先干掉监听端口,再设置所有的FD不接收数据,处理剩余的逻辑数据,关闭逻辑线程,关闭网络线程[11/15/2013 MengLingChao]
	ser->run();
#ifdef EL_ASYNC_USE_THREAD
	SAFE_DELETE(g_service_logic);
#endif
#ifndef EL_ASYNC_USE_THREAD	
fail:
#endif
	g_net_server->destroy();
	g_log->stop();
	SAFE_DELETE(g_log);
	SAFE_DELETE(g_timer);
#ifdef WIN32
	WSACleanup();
#endif//WIN32
	exit(0);
}

uint32_t el_async::service_t::get_bind_elem_id()
{
	return this->bind_elem->id;
}

const char* el_async::service_t::get_bind_elem_name()
{
	return this->bind_elem->name.c_str();
}

const std::string& el_async::service_t::get_bind_elem_ip()
{
	return this->bind_elem->ip;
}

uint16_t el_async::service_t::get_bind_elem_port()
{
	return this->bind_elem->port;
}
