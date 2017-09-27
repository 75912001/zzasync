/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	
	brief:		用户接口
*********************************************************************/

#pragma once

#include <lib_include.h>
#include <lib_net/lib_tcp_server.h>
#include <lib_net/lib_tcp_client.h>
#include <lib_proto/lib_msg.h>
#include <lib_proto/lib_msgbuf.h>
#include <lib_proto/lib_cmd_map.h>
#include <lib_err_code.h>
#include <lib_proto/lib_proto.h>
#include <lib_query.h>
#include <lib_log.h>
#include <lib_linux_version.h>

#include "bench_conf.h"
#include "handle_msg_base.h"
#include "net_tcp.h"

namespace el_async{
	//获取当前秒数(每次el_async循环时生成)
	uint32_t get_now_sec();
	//获取当前微秒数(每次el_async循环时生成)
	uint32_t get_now_usec();
	bool is_parent();
	uint32_t get_server_id();
	const std::string& get_server_ip();
	uint16_t get_server_port();
	const char* get_server_name();

	int s2peer(el::lib_tcp_peer_info_t* peer_info, const void* data, uint32_t len);
	int s2peer_msg(el::lib_tcp_peer_info_t* peer_info, el::lib_proto_head_base_t* head, el::lib_msg_t* msg = NULL);

	el::lib_tcp_peer_info_t* connect(const std::string& ip, uint16_t port);
	el::lib_tcp_peer_info_t* connect(const char* svr_name);

	//************************************
	// @brief:	关闭对方(client/server)
	// return:	void
	// parameter:	el::lib_tcp_peer_info_t * peer_info
	// parameter:	bool do_calback true:调用注册的回调函数(on_cli_conn_closed/on_svr_conn_closed).通知被关闭, 
	//								false:不做通知
	//************************************
//	void close_peer(el::lib_tcp_peer_info_t* peer_info, bool do_calback = true);
	//主动关闭对方	
	void active_close_peer( el::lib_tcp_peer_info_t* peer_info);
	//************************************
	// @brief:	Disables further receive operations.
	//************************************
	int shutdown_peer_rd(el::lib_tcp_peer_info_t* peer_info);

	//////////////////////////////////////////////////////////////////////////
	//处理服务端返回消息的基础类
	template <typename T_SERVICE_TYPE, typename T_PROTO_HEAD_TYPE>
	class service_base_t : public handle_msg_base_t<T_SERVICE_TYPE, T_PROTO_HEAD_TYPE>{
	public:
		std::string ip;
		uint16_t port;
		std::string name;
	public:
		service_base_t(){
			this->port = 0;
		}
		virtual~service_base_t(){}

		//处理来自服务的消息(解包/分发到各自处理函数中)
		//返回el::ERR_SYS::CMD_OUT_OF_RANGE 表示命令号超出处理范围(不属于该对象处理范围内)
		virtual int do_handle_dispatcher(const void* data, uint32_t len, 
			el::lib_tcp_peer_info_t* peer_fd_info) = 0;

		virtual int do_s2(el::lib_proto_head_base_t* head, el::lib_msg_t* msg = NULL){
			if (SUCC != this->connect()){
				return ERR;
			}
			return s2peer_msg(this->peer, head, msg);
		}
		virtual int do_s2(const void* data, uint32_t len){
			if (SUCC != this->connect()){
				return ERR;
			}
			return s2peer(this->peer, data, len);
		}
		//************************************
		// @brief:	断开连接时调用
		//************************************
		virtual void do_disconnect(){
			ERROR_LOG("[ip:%s, port:%d]", this->ip.c_str(), this->port);
			this->peer = NULL;
		}

		//connected success do , add by Brandon
		virtual void on_connected(){}

		int connect(){
			if (!this->is_connect()){
				if (!this->name.empty()){
					this->peer = el_async::connect(this->name.c_str());
				} else {
					this->peer = el_async::connect(this->ip, this->port);
				}
				if (!this->is_connect()){
					ERROR_LOG(" ");
					return ERR;
				} else {
					this->on_connected();
				}
			}
			return SUCC;
		}
		bool is_connect() const{
			return NULL != this->peer;
		}
	protected:
	private:
	};

	//////////////////////////////////////////////////////////////////////////
	//处理服务端返回消息的基础类 protobuf 简称pb
	class service_base_pb_t{
	public:
		el::lib_tcp_peer_info_t* peer;
		std::string ip;
		uint16_t port;
		std::string name;
		uint32_t service_id;
	public:
		service_base_pb_t(){
			this->port = 0;
			this->peer = NULL;
			this->service_id = 0;
		}
		virtual~service_base_pb_t(){}

		virtual int do_s2(const void* data, uint32_t len){
			if (SUCC != this->connect()){
				return ERR;
			}
			return s2peer(this->peer, data, len);
		}
		//************************************
		// @brief:	断开连接时调用
		//************************************
		virtual void do_disconnect(){
			ERROR_LOG("[ip:%s, port:%d]", this->ip.c_str(), this->port);
			this->peer = NULL;
		}

		//connected success do
		virtual void on_connected(){}
		int connect(){
			if (!this->is_connect()){
				if (!this->name.empty()){
					this->peer = el_async::connect(this->name.c_str());
				} else {
					this->peer = el_async::connect(this->ip, this->port);
				}
				if (!this->is_connect()){
					ERROR_LOG(" ");
					return ERR;
				} else {
					this->on_connected();
				}
			}
			return SUCC;
		}
		bool is_connect() const{
			return NULL != this->peer;
		}
	protected:
	private:
	};
/*
	//////////////////////////////////////////////////////////////////////////
	//处理服务端发来的消息的基础类(带查询功能)
	// s==>c的主动通知返回包中seq必须为0
	// s==>c的返回包中seq为c向s发送的请求包中的seq
	//返回el::ERR_SYS::CMD_OUT_OF_RANGE 表示命令号超出处理范围(不属于该对象处理范围内)
	template <typename T_SERVICE_TYPE, typename T_PROTO_HEAD_TYPE>
	class service_base_query_t : public service_base_t<T_SERVICE_TYPE, T_PROTO_HEAD_TYPE>{
	public:
		service_base_query_t(){}
		virtual ~service_base_query_t(){}
		virtual int do_handle_dispatcher(const void* data, uint32_t len, 
			el::lib_tcp_peer_info_t* peer_fd_info){
			el::lib_recv_data_cli_t in(data);
			in>>this->head->magic>>this->head->len>>this->head->op>>
				this->head->param>>this->head->seq>>this->head->ret;
			this->peer = peer_fd_info;
			this->ret = 0;

			TRACE_LOG("s=>[fd:%d]", peer_fd_info->get_fd());
			this->head->trace_log();
			TRACE_MSG_HEX_LOG(data, len);

			this->p_cmd_item = this->get_cmd_item(this->head->op);
			if (NULL == this->p_cmd_item){
				WARN_LOG("cli handle empty");
				WARN_LOG("query memory leak!!!");
				this->head.error_log();
				return el::ERR_SYS::CMD_OUT_OF_RANGE;
			} else {
				el::lib_query_t* query = NULL;
				if (0 != this->head->seq){
					//代表查询结果的回包(s->c)
					query = this->query_mgr.find_query(this->head->seq);
					if (NULL == query){
						WARN_LOG("query miss");
						this->head.trace_log();
						return 0;
					}
				} else {
					//处理s->c 主动发送的返回包
					if (0 != this->restore_msg((char*)data + T_PROTO_HEAD_TYPE::PROTO_HEAD_LEN, 
						this->p_cmd_item->msg_in, this->p_cmd_item->msg_out)){
						return 0;
					} else {
#ifdef SHOW_ALL_LOG
						TRACE_MSG_LOG("s=>");
#endif
						return this->handle_msg(this->p_cmd_item->func, 
							this->p_cmd_item->msg_in, this->p_cmd_item->msg_out);
					}
				}
				
				if (0 != this->head->ret){
					if (NULL != query){
						query->call_cb_handle(this->head->seq, this->head, NULL);
					} else {
						return this->do_handle_ret();
					}
				} else {
					//还原对象
					if (0 != this->restore_msg((char*)data + T_PROTO_HEAD_TYPE::PROTO_HEAD_LEN, 
						this->p_cmd_item->msg_in, this->p_cmd_item->msg_out)){
					} else {
						if (NULL != query){
							query->call_cb_handle(this->head->seq, this->head, this->p_cmd_item->msg_out);
						} else {
#ifdef SHOW_ALL_LOG
							TRACE_MSG_LOG("s=>");
#endif
							return this->handle_msg(this->p_cmd_item->func, this->p_cmd_item->msg_in, this->p_cmd_item->msg_out);
						}
					}
				}
				if (NULL != query){
					query->update(this->query_mgr, this->head->seq, this->head->ret);
					//之后不能使用query,在update中可能已经被delete
				}
			}
			return 0;
		}

		 //////////////////////////////////////////////////////////////////////////
		 * @brief 对该服务请求数据，发送报文的包头中会有序列号和该请求一一对应
		 *
		 * @param c_in 请求的报文
		 * @param q 发起该请求的查询对象
		 *
		 * @return  发送出去的字节数,    -1, 发送失败
		 //////////////////////////////////////////////////////////////////////////
		template<typename T_CB_CLASS>
		int query(el::lib_proto_head_base_t* head, el::lib_msg_t* c_in, 
			el::lib_query_t* q, int (T_CB_CLASS::*cb)(el::lib_proto_head_base_t* phead, el::lib_msg_t* c_out))
		{
			uint32_t seq = q->do_send(this->query_mgr);
			if (0 == seq){
				SAFE_DELETE(q);
				return -1;
			}
			
			head->param = q->get_peer_id();

			q->registe_cb(seq, cb);
			head->seq = seq;
			int ret = this->do_s2(head, c_in);
			if (ret < 0){
				//发送失败后,清理插入的回调函数
				q->del_cb(seq);
				SAFE_DELETE(q);
			} else {
				//发送成功
			}
			return ret;
		}
		//无回调函数的查询
		int query(el::lib_proto_head_base_t* head, el::lib_msg_t* c_in, el::lib_query_t* q)
		{
			uint32_t seq = q->do_send(this->query_mgr);
			if (0 == seq){
				SAFE_DELETE(q);
				return -1;
			}

			head->param = q->get_peer_id();
			head->seq = seq;

			int ret = this->do_s2(head, c_in);
			if (ret < 0){
				SAFE_DELETE(q);
			}
			return ret;
		}

		virtual void do_disconnect(){
			service_base_t<T_SERVICE_TYPE>::do_disconnect();
			//该服务失效
			this->query_mgr.del_all_querys();
		};
		el::lib_querys_t query_mgr;
	protected:
		
	private:
	};
*/


	//////////////////////////////////////////////////////////////////////////
	//处理客户端发来消息的基础类
	// 1.处理消息返回>0 , 则发送这个错误码到对方.(包头中其它信息不变)
	template <typename T_CLIENT_TYPE, typename T_PROTO_HEAD_TYPE>
	class client_base_t : public handle_msg_base_t<T_CLIENT_TYPE, T_PROTO_HEAD_TYPE>{
	public:
		virtual int do_s2(const void* data, uint32_t len){
			return s2peer(this->peer, data, len);
		}
		virtual int do_s2(el::lib_proto_head_base_t* head, el::lib_msg_t* msg = NULL){
			return s2peer_msg(this->peer, head, msg);
		}
		client_base_t(){}
		virtual ~client_base_t(){}
		//////////////////////////////////////////////////////////////////////////
		//处理来自客户的消息(解包/分发到各自处理函数中)(do_parse_handle_cmd,do_handle 的连续调用)
		virtual int do_handle_dispatcher(const void* data, uint32_t len,
			el::lib_tcp_peer_info_t* peer_fd_info){

			if (SUCC != this->do_parse_handle_cmd(data, len, peer_fd_info)){
				WARN_LOG("cli handle empty");
				return el::ERR_SYS::CMD_OUT_OF_RANGE;
			}

			return this->do_handle(data, this->p_cmd_item->msg_in, this->p_cmd_item->msg_out);
		}
	protected:
	private:
	};
}//end namespace el_async


