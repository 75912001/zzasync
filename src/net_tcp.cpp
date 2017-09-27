#include <lib_include.h>
#include <lib_util.h>
#include <lib_log.h>
#include <lib_linux_version.h>
#include <lib_timer.h>
#include <lib_time.h>
#include <lib_err_code.h>

#include "daemon.h"
#include "service.h"
#include "bind_conf.h"
#include "net_tcp.h"
#include "bench_conf.h"
#include "dll.h"

namespace {
	const uint32_t RECV_BUF_LEN = 1024*10240;//1024K*10
	char recv_buf_tmp[RECV_BUF_LEN] = {0};
	/**
	 * @brief	接收对方消息
	 * @param	ice::lib_tcp_client_t & cli_info
	 * @return	int 0:断开 >0:接收的数据长度
	 */
	int recv_peer_msg(el::lib_tcp_peer_info_t& cli_info)
	{
		int len = 0;

		int sum_len = 0;
		while (1) {
			len = cli_info.recv(recv_buf_tmp, sizeof(recv_buf_tmp));
			if (likely(len > 0)){
				if (len < (int)sizeof(recv_buf_tmp)){
					cli_info.recv_buf.push_back(recv_buf_tmp, len);
					return len;
				}else if (sizeof(recv_buf_tmp) == len){
					cli_info.recv_buf.push_back(recv_buf_tmp, len);
					sum_len += len;
					continue;
				}
			} else if (0 == len){
				return 0;
			} else if (len == -1) {
				return sum_len;
			}
		}
		return sum_len;
	}

	/**
	 * @brief	接收对方消息
	 * @param	ice::lib_udp_peer_info_t & cli_info
	 * @return	int 接收的数据长度
	 */
	int recv_peer_udp_msg(el::lib_tcp_peer_info_t& cli_info, sockaddr_in& peer_addr)
	{
		socklen_t from_len = sizeof(sockaddr_in);
		int len = HANDLE_EINTR(::recvfrom(cli_info.fd, recv_buf_tmp, sizeof(recv_buf_tmp), 0,
			(sockaddr*)&(peer_addr), &from_len));
		cli_info.recv_buf.push_back(recv_buf_tmp, len);
		return len;
	}

}//end namespace 

int el_async::net_server_t::create(uint32_t max_fd_num)
{
	this->server_epoll = new tcp_server_epoll_t(max_fd_num, (uint32_t)g_bench_conf->fd_time_out);
	if (NULL == this->server_epoll){
		ALERT_LOG("new tcp_server_epoll err [maxevents:%u]", max_fd_num);
		return -1;
	}
	int ret = 0;
	if (0 != (ret = this->server_epoll->create())){
		ALERT_LOG("new tcp_server_epoll create err [ret:%d]", ret);
		this->destroy();
		return -1;
	}
	return 0;
}

int el_async::net_server_t::destroy()
{
	SAFE_DELETE(this->server_epoll);
	return 0;
}

el_async::net_server_t::net_server_t( void )
{
	this->server_epoll = NULL;
}

#ifndef WIN32
int el_async::tcp_server_epoll_t::run()
{
	int event_num = 0;//事件的数量
	epoll_event evs[this->cli_fd_value_max];
#ifndef EL_ASYNC_USE_THREAD
	while(likely(!g_daemon->stop) || g_dll->functions.on_fini()){
#else
	while(likely(!g_daemon->stop)){
#endif//EL_ASYNC_USE_THREAD
		event_num = HANDLE_EINTR(::epoll_wait(this->fd, evs, this->cli_fd_value_max, this->epoll_wait_time_out));
#ifndef EL_ASYNC_USE_THREAD
		g_timer->renew_now();
		if (!g_is_parent){
			g_addr_mcast->syn_info();
		}
		g_dll->functions.on_events();
#else
		g_service_logic->notify_event();
#endif//EL_ASYNC_USE_THREAD
		if (0 == event_num){
			//time out
			continue;
		}else if (unlikely(-1 == event_num)){
			ALERT_LOG("epoll wait err [%s]", ::strerror(errno));
			return ERR;
		}
		//handling event
		for (int i = 0; i < event_num; ++i){
			el::lib_tcp_peer_info_t& fd_info = this->peer_fd_infos[evs[i].data.fd];
			uint32_t events = evs[i].events;
			el::E_FD_TYPE fd_type = fd_info.fd_type;
			if ( unlikely(el::FD_TYPE_PIPE == fd_type) ) {
				if (0 == this->on_pipe_event(fd_info.fd, evs[i])) {
					continue;
				} else {
					return ERR;
				}
			}
			//可读或可写(可同时发生,并不互斥)
			if(EPOLLOUT & events){//该套接字可写
				if (this->handle_send(fd_info) < 0){
					DEBUG_LOG("EPOLLOUT fd:%d", fd_info.fd);
					this->close_peer(fd_info);
					continue;
				}
			}
			if(EPOLLIN & events){//接收并处理其他套接字的数据
				switch (fd_type)
				{
				case el::FD_TYPE_LISTEN:
					this->handle_listen();
					continue;
					break;
				case el::FD_TYPE_CLI:
				case el::FD_TYPE_SVR:
					this->handle_peer_msg(fd_info);
					break;
				case el::FD_TYPE_MCAST:
					this->handle_peer_mcast_msg(fd_info);
					break;
				case el::FD_TYPE_ADDR_MCAST:
					this->handle_peer_add_mcast_msg(fd_info);
					break;
				case el::FD_TYPE_UDP:
					this->handle_peer_udp_msg(fd_info);
					break;
				default:
					break;
				}
			}
			if (EPOLLHUP & events){
				// EPOLLHUP: When close of a fd is detected (ie, after receiving a RST segment:
				//				   the client has closed the socket, and the server has performed
				//				   one write on the closed socket.)
				this->close_peer(fd_info);

				// After receiving EPOLLRDHUP or EPOLLHUP, we can still read data from the fd until
				// read() returns 0 indicating EOF is reached. So, we should alway call read on receving
				// this kind of events to aquire the remaining data and/or EOF. (Linux-2.6.18)
				// todo 可读 [10/17/2013 MengLingChao]
				continue;				
			}
			if(EPOLLERR & events){
				/*只有采取动作时，才能知道是否对方异常。即对方突然断掉，是不可能
				有此事件发生的。只有自己采取动作（当然自己此刻也不知道），read，
				write时，出EPOLLERR错，说明对方已经异常断开。

				EPOLLERR 是服务器这边出错（自己出错当然能检测到，对方出错你咋能
				直到啊）*/
				DEBUG_LOG("EPOLLERR fd:%d", fd_info.fd);
				this->close_peer(fd_info);
				continue;
			}

#if EL_IS_LINUX_VERSION_CODE_GE(2, 6, 17)
			if(EPOLLRDHUP & events){
				/*
				EPOLLRDHUP (since Linux 2.6.17)
				Stream socket peer closed connection, or shut down writing
				half of connection.  (This flag is especially useful for
				writing simple code to detect peer shutdown when using Edge
				Triggered monitoring.)
				EPOLLRDHUP: If a client send some data to a server and than close the connection
				immediately, the server will receive RDHUP and IN at the same time.
				Under ET mode, this can make a sockfd into CLOSE_WAIT state.
				对端close
				EPOLLRDHUP = 0x2000,
				#define EPOLLRDHUP EPOLLRDHUP
				*/
				DEBUG_LOG("EPOLLRDHUP fd:%d", fd_info.fd);
				this->close_peer(fd_info);
				// After receiving EPOLLRDHUP or EPOLLHUP, we can still read data from the fd until
				// read() returns 0 indicating EOF is reached. So, we should alway call read on receving
				// this kind of events to aquire the remaining data and/or EOF. (Linux-2.6.18)
				// todo 可读 [10/17/2013 MengLingChao]
				continue;
			}
#endif
			if(!(events & EPOLLOUT) && !(events & EPOLLIN)){
				ERROR_LOG("events:%u", events);
			}
		}
	}

	return 0;
}

#else

int el_async::tcp_server_epoll_t::run()
{
	int event_num = 0;//事件的数量
	fd_set fd_ReadSet;
	fd_set fd_WriteSet;
	timeval nTimeVal;

#ifndef EL_ASYNC_USE_THREAD
	while(likely(!g_daemon->stop) || g_dll->functions.on_fini()){
#else
	while(likely(!g_daemon->stop)){
#endif//EL_ASYNC_USE_THREAD
	FD_ZERO(&fd_ReadSet);
	FD_ZERO(&fd_WriteSet);
	int nTotalSockets = 0;//连接上服务器的SOCKET总数量
	int nMaxS = 0;//select  使用的第一个参数
	for (int i = 0; i < this->cli_fd_value_max; i++){
		el::lib_tcp_peer_info_t* peer = &this->peer_fd_infos[i];
		if (INVALID_FD != peer->fd){
			nTotalSockets++;
			FD_SET(peer->fd, &fd_ReadSet);
			peer->fd > nMaxS ? nMaxS = peer->fd : 0;
			if (peer->send_buf.total_len > 0){
				FD_SET(peer->fd, &fd_WriteSet);
			}
		}
	}
	nTimeVal.tv_sec = 0;
	nTimeVal.tv_usec = 1000 * this->epoll_wait_time_out;
	event_num = select(nMaxS + 1, &fd_ReadSet, &fd_WriteSet, NULL,&nTimeVal);

#ifndef EL_ASYNC_USE_THREAD
		g_timer->renew_now();
		if (!g_is_parent){
			g_addr_mcast->syn_info();
		}
		g_dll->functions.on_events();
#endif//EL_ASYNC_USE_THREAD
		if (0 == event_num){
			//time out
			continue;
		}else if (unlikely(-1 == event_num)){
			ALERT_LOG("epoll wait err [%s]", ::strerror(errno));
			return ERR;
		}
		for (int i = 0; event_num > 0 && i < this->cli_fd_value_max; i++){
			if (INVALID_FD == this->peer_fd_infos[i].fd){
				continue;
			}
			
			if (FD_ISSET(this->peer_fd_infos[i].fd, &fd_WriteSet)){
				event_num--;
				el::lib_tcp_peer_info_t& fd_info = this->peer_fd_infos[i];
				if (this->handle_send(fd_info) < 0){
					DEBUG_LOG("ccc7 fd:%d", fd_info.fd);
					this->close_peer(fd_info);
					continue;
				}
			}
			if (FD_ISSET(this->peer_fd_infos[i].fd, &fd_ReadSet)){
				event_num--;
				el::lib_tcp_peer_info_t& fd_info = this->peer_fd_infos[i];
				el::E_FD_TYPE fd_type = fd_info.fd_type;
				switch (fd_type)
				{
				case el::FD_TYPE_LISTEN:
					this->handle_listen();
					continue;
					break;
				case el::FD_TYPE_CLI:
				case el::FD_TYPE_SVR:
					this->handle_peer_msg(fd_info);
					break;
				case el::FD_TYPE_MCAST:
					this->handle_peer_mcast_msg(fd_info);
					break;
				case el::FD_TYPE_ADDR_MCAST:
					this->handle_peer_add_mcast_msg(fd_info);
					break;
				case el::FD_TYPE_UDP:
					this->handle_peer_udp_msg(fd_info);
					break;
				default:
					break;
				}
			}
		}
	}
	return 0;
}

#endif//WIN32

el_async::tcp_server_epoll_t::tcp_server_epoll_t(uint32_t max_events_num, uint32_t cli_time_out)
{
	this->cli_fd_value_max = max_events_num;
	this->epoll_wait_time_out = -1;
#ifndef WIN32
	this->on_pipe_event = NULL;
#endif//WIN32
	this->cli_time_out_sec = cli_time_out;
}

int el_async::tcp_server_epoll_t::listen(const char* ip, uint16_t port, uint32_t listen_num, int bufsize)
{
#ifndef WIN32
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
		this->listen_fd = ::socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	#else
		this->listen_fd = ::socket(PF_INET, SOCK_STREAM, 0);
	#endif
#else
	this->listen_fd = ::socket(PF_INET, SOCK_STREAM, 0);
#endif//WIN32

	if (INVALID_FD == this->listen_fd){
		ALERT_LOG("create socket err [%s]", ::strerror(errno));
		return -1;
	}
#ifndef WIN32
	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
		el::lib_file_t::set_io_block(this->listen_fd, false);
	#endif
	int ret = el::lib_tcp_t::set_reuse_addr(this->listen_fd);
	if (-1 == ret){
		el::lib_file_t::close_fd(this->listen_fd);
		return -1;

	}
	ret = el::lib_net_util_t::set_recvbuf(this->listen_fd, bufsize);
	if (-1 == ret){
		el::lib_file_t::close_fd(this->listen_fd);
		return -1;
	}
	ret = el::lib_net_util_t::set_sendbuf(this->listen_fd, bufsize);
	if(-1 == ret){
		el::lib_file_t::close_fd(this->listen_fd);
		return -1;
	}
#endif//WIN32
	if (0 != this->bind(ip, port)){
		el::lib_file_t::close_fd(this->listen_fd);
		ALERT_LOG("bind socket err [%s]", ::strerror(errno));
		return -1;
	}

	if (0 != ::listen(this->listen_fd, listen_num)){
		el::lib_file_t::close_fd(this->listen_fd);
		ALERT_LOG("listen err [%s]", ::strerror(errno));
		return -1;
	}

	if (NULL == this->add_connect(this->listen_fd, el::FD_TYPE_LISTEN, ip, port)){
		el::lib_file_t::close_fd(this->listen_fd);
		ALERT_LOG("add connect err[%s]", ::strerror(errno));
		return -1;
	}

	this->ip = el::lib_net_util_t::ip2int(ip);
	this->port = port;

	return 0;
}

int el_async::tcp_server_epoll_t::create()
{
#ifndef WIN32
	if ((this->fd = ::epoll_create(this->cli_fd_value_max)) < 0) {
		ALERT_LOG("EPOLL_CREATE FAILED [ERROR:%s]", strerror (errno));
		return -1;
	}
#endif//WIN32
	this->peer_fd_infos = (el::lib_tcp_peer_info_t*) new el::lib_tcp_peer_info_t[this->cli_fd_value_max];
	if (NULL == this->peer_fd_infos){
		el::lib_file_t::close_fd(this->fd);
		ALERT_LOG ("CALLOC CLI_FD_INFO_T FAILED [MAXEVENTS=%d]", this->cli_fd_value_max);
		return -1;
	}
	return 0;
}

int el_async::tcp_server_epoll_t::add_events( int fd, uint32_t flag )
{
#ifndef WIN32
	epoll_event ev;
	ev.events = flag;
	ev.data.fd = fd;

	int ret = HANDLE_EINTR(::epoll_ctl(this->fd, EPOLL_CTL_ADD, fd, &ev));
	if (0 != ret){
		ERROR_LOG ("epoll_ctl add fd:%d error:%s", fd, strerror(errno));
		return -1;
	}
#endif//WIN32
	return 0; 
}

el::lib_tcp_peer_info_t* el_async::tcp_server_epoll_t::add_connect( int fd,
	el::E_FD_TYPE fd_type, const char* ip, uint16_t port )
{
#ifndef WIN32
	uint32_t flag;

	switch (fd_type) {
	default:
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
			flag = EPOLLIN | EPOLLRDHUP;
	#else
			flag = EPOLLIN;
	#endif
		break;
	}

	if (0 != this->add_events(fd, flag)) {
		ERROR_LOG("add events err [fd:%d, flag:%u]", fd, flag);
		return NULL;
	}
#endif//WIN32
	el::lib_tcp_peer_info_t& cfi = this->peer_fd_infos[fd];
	cfi.open(fd, fd_type, ip, port);

	TRACE_LOG("fd:%d, fd type:%d, ip:%s, port:%u", 
		fd, fd_type, cfi.get_ip_str().c_str(), cfi.port);
	return &cfi;
}

void el_async::tcp_server_epoll_t::handle_peer_msg( el::lib_tcp_peer_info_t& fd_info )
{
	int ret = recv_peer_msg(fd_info);
	if (ret > 0){
#ifdef EL_ASYNC_USE_THREAD
		g_service_logic->add_handle_peer_msg(&fd_info);
#else
		int available_len = 0;
		while (0 != (available_len = g_dll->functions.on_get_pkg_len(&fd_info,
			fd_info.recv_buf.data, fd_info.recv_buf.write_pos))){	
			if (-1 == available_len){
				WARN_LOG("close socket! available_len [fd:%d, ip:%s, port:%u]",
					fd_info.fd, fd_info.get_ip_str().c_str(), fd_info.port);
				DEBUG_LOG("ccc8 fd:%d", fd_info.fd);
				this->close_peer(fd_info);
				break;
			} else if (available_len > 0 && (int)fd_info.recv_buf.write_pos >= available_len){
				if (el::FD_TYPE_CLI == fd_info.fd_type){
					if ((int)el::ERR_SYS::DISCONNECT_PEER == g_dll->functions.on_cli_pkg(fd_info.recv_buf.data, available_len, &fd_info)){
						WARN_LOG("close socket! ret [fd:%d, ip:%s, port:%u]",
							fd_info.fd, fd_info.get_ip_str().c_str(), fd_info.port);
						this->close_peer(fd_info);
					}
				} else if (el::FD_TYPE_SVR == fd_info.fd_type){
					g_dll->functions.on_srv_pkg(fd_info.recv_buf.data, available_len, &fd_info);
				}
				fd_info.recv_buf.pop_front(available_len);
			}else{
				break;
			}
			if (0 == fd_info.recv_buf.write_pos){
				break;
			}
		}
	}else if (0 == ret || -1 == ret){
		DEBUG_LOG("close socket by peer [fd:%d, ip:%s, port:%u, ret:%d]", 
			fd_info.fd, fd_info.get_ip_str().c_str(), fd_info.port, ret);
		this->close_peer(fd_info);
	}
#endif

}

void el_async::tcp_server_epoll_t::handle_listen()
{
	sockaddr_in peer;
	::memset(&peer, 0, sizeof(peer));
	int peer_fd = this->accept(peer, g_bench_conf->page_size_max,
		g_bench_conf->page_size_max);
	if (unlikely(peer_fd < 0) || unlikely(peer_fd >= this->cli_fd_value_max)){
		ALERT_LOG("accept err [%s] fd:%d", ::strerror(errno), peer_fd);
		if (peer_fd > 0){
			el::lib_file_t::close_fd(peer_fd);
		}
	}else{
		TRACE_LOG("client accept [ip:%s, port:%u, new_socket:%d]",
			inet_ntoa(peer.sin_addr), ntohs(peer.sin_port), peer_fd);
		el::lib_tcp_peer_info_t* peer_fd_info = this->add_connect(peer_fd, el::FD_TYPE_CLI, el::lib_net_util_t::ip2str(peer.sin_addr.s_addr).c_str(), ntohs(peer.sin_port));
		if (NULL != peer_fd_info){
			g_dll->functions.on_cli_conn(peer_fd_info);
		}
	}
}

int el_async::tcp_server_epoll_t::handle_send( el::lib_tcp_peer_info_t& fd_info )
{
#ifdef EL_ASYNC_USE_THREAD
	fd_info.lock_mutex.lock();
#endif
	int send_len = fd_info.send(fd_info.send_buf.data, fd_info.send_buf.write_pos);
	if (send_len > 0){
		if (0 == fd_info.send_buf.pop_front(send_len)){
#ifndef WIN32
			uint32_t flag;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
/*
EPOLLRDHUP (since Linux 2.6.17)
              Stream socket peer closed connection, or shut down writing half of connection.  (This flag is especially
              useful for writing simple code to detect peer shutdown when using Edge Triggered monitoring.)
*/
			flag = EPOLLIN | EPOLLRDHUP;
#else
			flag = EPOLLIN;
#endif
			this->mod_events(fd_info.fd, flag);
#endif//WIN32
		}
	}
#ifdef EL_ASYNC_USE_THREAD
	fd_info.lock_mutex.ulock();
#endif
	return send_len;
}

int el_async::tcp_server_epoll_t::mod_events( int fd, uint32_t flag )
{
#ifndef WIN32
	epoll_event ev;
	ev.events = flag;
	ev.data.fd = fd;

	int ret = HANDLE_EINTR(::epoll_ctl(this->fd, EPOLL_CTL_MOD, fd, &ev));
	if (0 != ret){
		ERROR_LOG ("epoll_ctl mod fd:%d error:%s", fd, strerror(errno));
		return -1;
	}
#endif//WIN32
	return 0; 
}

el_async::tcp_server_epoll_t::~tcp_server_epoll_t()
{

}

void el_async::tcp_server_epoll_t::close_peer( el::lib_tcp_peer_info_t& fd_info, bool do_calback /*= true*/ )
{
#ifdef EL_ASYNC_USE_THREAD
	fd_info.lock_mutex.lock();
#endif
	
	if (el::FD_TYPE_CLI == fd_info.fd_type){
		DEBUG_LOG("close socket cli [fd:%d, ip:%s, port:%u]", fd_info.fd, 
			fd_info.get_ip_str().c_str(), fd_info.port);
		if (do_calback){
			g_dll->functions.on_cli_conn_closed(fd_info.fd);
		}
	} else if (el::FD_TYPE_SVR == fd_info.fd_type){
		DEBUG_LOG("close socket svr [fd:%d, ip:%s, port:%u]", fd_info.fd, 
			fd_info.get_ip_str().c_str(), fd_info.port);
		if (do_calback){
			g_dll->functions.on_svr_conn_closed(fd_info.fd);
		}
	}

#ifndef WIN32
	epoll_event ev;
	int ret = HANDLE_EINTR(::epoll_ctl(this->fd, EPOLL_CTL_DEL, fd_info.fd, &ev));
	if (0 != ret){
		ERROR_LOG ("epoll_ctl del fd:%d error:%s", fd_info.fd, strerror(errno));
	}
#endif
	fd_info.close();
#ifdef EL_ASYNC_USE_THREAD
	fd_info.lock_mutex.ulock();
#endif
}

void el_async::tcp_server_epoll_t::active_close_peer( el::lib_tcp_peer_info_t& fd_info)
{
#ifdef EL_ASYNC_USE_THREAD
	fd_info.lock_mutex.lock();
#endif
	if (el::FD_TYPE_CLI == fd_info.fd_type){
		DEBUG_LOG("close socket cli [fd:%d, ip:%s, port:%u]", fd_info.fd, fd_info.get_ip_str().c_str(), fd_info.port);
	} else if (el::FD_TYPE_SVR == fd_info.fd_type){
		DEBUG_LOG("close socket svr [fd:%d, ip:%s, port:%u]", fd_info.fd, fd_info.get_ip_str().c_str(), fd_info.port);
	}

#ifndef WIN32
	epoll_event ev;
	int ret = HANDLE_EINTR(::epoll_ctl(this->fd, EPOLL_CTL_DEL, fd_info.fd, &ev));
	if (0 != ret){
		ERROR_LOG ("epoll_ctl del fd:%d error:%s", fd_info.fd, strerror(errno));
	}
#endif
	fd_info.close();
#ifdef EL_ASYNC_USE_THREAD
	fd_info.lock_mutex.ulock();
#endif
}

void el_async::tcp_server_epoll_t::handle_peer_mcast_msg( el::lib_tcp_peer_info_t& fd_info )
{
	recv_peer_msg(fd_info);
	if (fd_info.recv_buf.write_pos > 0){
		g_dll->functions.on_mcast_pkg(fd_info.recv_buf.data, fd_info.recv_buf.write_pos);
		fd_info.recv_buf.pop_front(fd_info.recv_buf.write_pos);
	}
}

void el_async::tcp_server_epoll_t::handle_peer_add_mcast_msg( el::lib_tcp_peer_info_t& fd_info )
{
	recv_peer_msg(fd_info);
	if (fd_info.recv_buf.write_pos > 0){
		g_addr_mcast->handle_msg(fd_info.recv_buf);// 注意线程安全 [11/14/2013 MengLingChao]
		fd_info.recv_buf.pop_front(fd_info.recv_buf.write_pos);
	}
}

void el_async::tcp_server_epoll_t::handle_peer_udp_msg( el::lib_tcp_peer_info_t& fd_info )
{		
	sockaddr_in peer_addr;
	::memset(&(peer_addr), 0, sizeof(peer_addr));
	recv_peer_udp_msg(fd_info, peer_addr);
	if (fd_info.recv_buf.write_pos > 0){
		g_dll->functions.on_udp_pkg(fd_info.fd, fd_info.recv_buf.data,
			fd_info.recv_buf.write_pos, &peer_addr, sizeof(peer_addr));
		fd_info.recv_buf.pop_front(fd_info.recv_buf.write_pos);
	}
}

int el_async::tcp_server_epoll_t::shutdown_peer_rd( el::lib_tcp_peer_info_t& fd_info )
{
	if (el::FD_TYPE_CLI == fd_info.fd_type){
		ERROR_LOG("close socket cli rd [fd:%d, ip:%s, port:%u]", fd_info.fd,
			fd_info.get_ip_str().c_str(), fd_info.port);
	} else if (el::FD_TYPE_SVR == fd_info.fd_type){
		ERROR_LOG("close socket svr rd [fd:%d, ip:%s, port:%u]", fd_info.fd,
			fd_info.get_ip_str().c_str(), fd_info.port);
	}
	return fd_info.shutdown_rd();
}

