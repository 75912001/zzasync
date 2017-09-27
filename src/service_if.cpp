#include <lib_include.h>
#include <lib_net/lib_tcp_client.h>
#include <lib_net/lib_tcp.h>
#include <lib_timer.h>

#include "service.h"
#include "daemon.h"
#include "service_if.h"

el::lib_tcp_peer_info_t* el_async::connect( const std::string& ip, uint16_t port )
{
	int fd = el::lib_tcp_t::connect(ip, port, 1, false, 
		g_bench_conf->page_size_max,
		g_bench_conf->page_size_max);
	if (-1 == fd){
		ERROR_LOG("[ip:%s, port:%u]", ip.c_str(), port);
	}else{
		TRACE_LOG("[ip:%s, port:%u, fd:%u]", ip.c_str(), port, fd);
		return g_net_server->server_epoll->add_connect(fd, el::FD_TYPE_SVR, ip.c_str(), port);
	}

	return NULL;
}

el::lib_tcp_peer_info_t* el_async::connect( const char* svr_name )
{
	std::string ip;
	uint16_t port = 0;
	if (!g_addr_mcast->get_1st_svr_ip_port(svr_name, ip, port)){
		ERROR_LOG("[svr_name:%s]", svr_name);
		return NULL;
	}
	
	return el_async::connect(ip, port);
}


uint32_t el_async::get_server_id()
{
	return g_service->get_bind_elem_id();
}

const std::string& el_async::get_server_ip(){
	return g_service->get_bind_elem_ip();
}

uint16_t el_async::get_server_port(){
	return g_service->get_bind_elem_port();
}

const char* el_async::get_server_name()
{
	return g_service->get_bind_elem_name();
}

bool el_async::is_parent()
{
	return g_is_parent;
}

//获取当前秒数(每次el_async循环时生成)
uint32_t el_async::get_now_sec(){
	return g_timer->now_sec();
}
//获取当前微秒数(每次el_async循环时生成)
uint32_t el_async::get_now_usec(){
	return g_timer->now_usec();
}

// void el_async::close_peer( el::lib_tcp_peer_info_t* peer_info, bool do_calback /*= true*/)
// {
// 	DEBUG_LOG("ccc11 fd:%d", peer_info->get_fd());
// 	g_net_server->get_server_epoll()->close_peer(*peer_info, do_calback);
// }

void el_async::active_close_peer( el::lib_tcp_peer_info_t* peer_info)
{
	DEBUG_LOG("ccc11 fd:%d", peer_info->fd);
	g_net_server->server_epoll->active_close_peer(*peer_info);
}

int el_async::shutdown_peer_rd( el::lib_tcp_peer_info_t* peer_info)
{
	return g_net_server->server_epoll->shutdown_peer_rd(*peer_info);
}


int el_async::s2peer(el::lib_tcp_peer_info_t* peer_info, const void* data, uint32_t len){
	int send_len = peer_info->send(data, len);
	if (-1 == send_len){
		WARN_LOG("close socket! send err [fd:%d, ip:%s, port:%u, len:%u, send_len:%d]",
			peer_info->fd, peer_info->get_ip_str().c_str(), peer_info->port, len, send_len);
		DEBUG_LOG("-1 == send_len fd:%d", peer_info->fd);
//		g_net_server->get_server_epoll()->close_peer(*peer_info);
	} else if (send_len < (int)len){
		WARN_LOG("close socket! send too long [fd:%d, ip:%s, port:%u, len:%u, send_len:%d]",
			peer_info->fd, peer_info->get_ip_str().c_str(), peer_info->port, len, send_len);
		peer_info->send_buf.push_back((char*)data + send_len, len - send_len);

		uint32_t flag = 0;
#ifndef WIN32
	#if EL_IS_LINUX_VERSION_CODE_GE(2, 6, 17)
			flag = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
	#else	
			flag = EPOLLIN | EPOLLOUT;
	#endif
#endif//WIN32
		g_net_server->server_epoll->mod_events(peer_info->fd, flag);
	}

	if (el::FD_TYPE_SVR == peer_info->fd_type){
//		TRACE_MSG_LOG("s2s======>[fd:%d, send_len:%d]", peer_info->get_fd(), send_len);
//		TRACE_MSG_HEX_LOG(data, len);
	} else if (el::FD_TYPE_CLI == peer_info->fd_type){
//		TRACE_MSG_LOG("s2c======>[fd:%d, send_len:%d]", peer_info->get_fd(), send_len);
//		TRACE_MSG_HEX_LOG(data, len);
	} else {
		ERROR_LOG(" ");
	}
	return send_len;
}

int el_async::s2peer_msg(el::lib_tcp_peer_info_t* peer_info, el::lib_proto_head_base_t* head, el::lib_msg_t* msg/* = NULL*/){
	head->set_len(0);
	el::lib_msg_byte_t msg_byte;
	el::lib_active_buf_t& w_buf = msg_byte.w_buf;

	w_buf.push_back((char*)head->get_data_pointer(), head->get_proto_head_len());

	if (NULL != msg){
		//有包体的情况
		if (!msg->write(msg_byte)){
			ERROR_LOG("msg write err!!!");
			head->error_log();
			return ERR;
		}
	}
	uint32_t len = 0;
	if (head->is_all_len()){
		len = msg_byte.w_buf.write_pos;
	} else {
		len = msg_byte.w_buf.write_pos - head->get_proto_head_len();
	}

	el::lib_packer_t::pack((void*)(w_buf.data + head->len_offset()), len);

#ifdef SHOW_ALL_LOG
	head->set_len(len);
	if (el::FD_TYPE_SVR == peer_info->get_fd_type()){
//		TRACE_MSG_LOG("s2s======>[fd:%d]", peer_info->get_fd());
		head->trace_log();
	} else if (el::FD_TYPE_CLI == peer_info->get_fd_type()){
//		TRACE_MSG_LOG("s2c======>[fd:%d]", peer_info->get_fd());
		head->trace_log();
	} else {
		ERROR_LOG(" ");
	}

	if (NULL != msg){
		msg->show();
		TRACE_MSG_LOG("++++++");
	}
#endif
	return s2peer(peer_info, w_buf.data, w_buf.write_pos);
}


