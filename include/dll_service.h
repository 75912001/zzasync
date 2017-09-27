#pragma once
#ifndef WIN32
#else
extern "C"{
	int on_init();
	int on_fini();
	void on_events();
	int on_get_pkg_len(el::lib_tcp_peer_info_t* peer_fd_info, const void* data, uint32_t len);
	int on_cli_pkg(const void* data, uint32_t len, el::lib_tcp_peer_info_t* peer_fd_info);
	void on_srv_pkg(const void* data, uint32_t len, el::lib_tcp_peer_info_t* peer_fd_info);
	void on_cli_conn_closed(int fd);
	void on_cli_conn(el::lib_tcp_peer_info_t* peer_fd_info);
	void on_svr_conn_closed(int fd);
	void on_svr_conn(int fd);
	void on_mcast_pkg(const void* data, int len);
	void on_addr_mcast_pkg(uint32_t id, const char* name, const char* ip, uint16_t port, int flag);
	void on_udp_pkg(int fd, const void* data, int len ,struct sockaddr_in* from, socklen_t fromlen);
};
#endif//WIN32
