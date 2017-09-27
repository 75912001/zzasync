/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	
	brief:		网络服务接口 
	socket:	socket(2)调用从2.6.27开始支持SOCK_NONBLOCK参数了	
	accept4:从Linux2.6.28开始的accept4支持SOCK_NOBLOCK参数，accept出来的fd就已经是非阻塞的了。这样就可以减少一次fcntl(2)的调用开销了	
*********************************************************************/
#pragma  once

#include <lib_include.h>

#include <lib_net/lib_tcp_server.h>
#include <lib_util.h>

#include "mcast.h"
namespace el_async{
	class tcp_server_epoll_t : public el::lib_tcp_srv_t
	{
	public:
#ifndef WIN32
		typedef int (*ON_PIPE_EVENT)(int fd, epoll_event& r_evs);
		ON_PIPE_EVENT on_pipe_event;
#endif//WIN32
		int epoll_wait_time_out;//epoll_wait函数调用时超时时间间隔
	public:
		tcp_server_epoll_t(uint32_t max_events_num, uint32_t cli_time_out);
		virtual ~tcp_server_epoll_t();
		virtual int create();
		/**
		* @brief Create a listening socket fd
		* @param ip the binding ip address. If this argument is assigned with 0,
		*                                           then INADDR_ANY will be used as the binding address.
		* @param port the binding port.
		* @param listen_num the maximum length to which the queue of pending connections for sockfd may grow.
		* @param bufsize maximum socket send and receive buffer in bytes, should be less than 10 * 1024 * 1024
		* @return the newly created listening fd on success, -1 on error.
		* @see create_passive_endpoint
		*/
		virtual int listen(const char* ip, uint16_t port, uint32_t listen_num, int bufsize);
		virtual int run();
		virtual el::lib_tcp_peer_info_t* add_connect(int fd, 
			el::E_FD_TYPE fd_type, const char* ip, uint16_t port);
	public:
		int mod_events(int fd, uint32_t flag);
		int add_events(int fd, uint32_t flag);
		/**
		 * @brief	关闭连接
		 * @param	bool do_calback true:调用客户端注册的回调函数.通知被关闭, false:不做通知
		 */
		void close_peer(el::lib_tcp_peer_info_t& fd_info, bool do_calback = true);
		//主动关闭连接	
		void active_close_peer( el::lib_tcp_peer_info_t& fd_info);
		int shutdown_peer_rd(el::lib_tcp_peer_info_t& fd_info);
	private:
		tcp_server_epoll_t(const tcp_server_epoll_t& cr);
		tcp_server_epoll_t& operator=(const tcp_server_epoll_t& cr);

		void handle_peer_msg(el::lib_tcp_peer_info_t& fd_info);
		void handle_peer_mcast_msg(el::lib_tcp_peer_info_t& fd_info);
		void handle_peer_add_mcast_msg(el::lib_tcp_peer_info_t& fd_info);
		void handle_peer_udp_msg(el::lib_tcp_peer_info_t& fd_info);
		void handle_listen();
		int handle_send(el::lib_tcp_peer_info_t& fd_info);
	};

	class net_server_t
	{
	public:
		tcp_server_epoll_t* server_epoll;
		virtual ~net_server_t(){}
		static net_server_t* get_instance(){
			static net_server_t net_server;
			return &net_server;
		}
		//************************************
		// Brief:     
		// Returns:   int 0:success,-1:error
		//************************************
		int create(uint32_t max_fd_num);
		int destroy();
	protected:
	private:
		net_server_t(void);
		net_server_t(const net_server_t &cr);
		net_server_t & operator=( const net_server_t &cr);
	};
}

#define g_net_server el_async::net_server_t::get_instance()



