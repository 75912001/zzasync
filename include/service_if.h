/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	
	brief:		�û��ӿ�
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
	//��ȡ��ǰ����(ÿ��el_asyncѭ��ʱ����)
	uint32_t get_now_sec();
	//��ȡ��ǰ΢����(ÿ��el_asyncѭ��ʱ����)
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
	// @brief:	�رնԷ�(client/server)
	// return:	void
	// parameter:	el::lib_tcp_peer_info_t * peer_info
	// parameter:	bool do_calback true:����ע��Ļص�����(on_cli_conn_closed/on_svr_conn_closed).֪ͨ���ر�, 
	//								false:����֪ͨ
	//************************************
//	void close_peer(el::lib_tcp_peer_info_t* peer_info, bool do_calback = true);
	//�����رնԷ�	
	void active_close_peer( el::lib_tcp_peer_info_t* peer_info);
	//************************************
	// @brief:	Disables further receive operations.
	//************************************
	int shutdown_peer_rd(el::lib_tcp_peer_info_t* peer_info);

	//////////////////////////////////////////////////////////////////////////
	//�������˷�����Ϣ�Ļ�����
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

		//�������Է������Ϣ(���/�ַ������Դ�������)
		//����el::ERR_SYS::CMD_OUT_OF_RANGE ��ʾ����ų�������Χ(�����ڸö�����Χ��)
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
		// @brief:	�Ͽ�����ʱ����
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
	//�������˷�����Ϣ�Ļ����� protobuf ���pb
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
		// @brief:	�Ͽ�����ʱ����
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
	//�������˷�������Ϣ�Ļ�����(����ѯ����)
	// s==>c������֪ͨ���ذ���seq����Ϊ0
	// s==>c�ķ��ذ���seqΪc��s���͵�������е�seq
	//����el::ERR_SYS::CMD_OUT_OF_RANGE ��ʾ����ų�������Χ(�����ڸö�����Χ��)
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
					//�����ѯ����Ļذ�(s->c)
					query = this->query_mgr.find_query(this->head->seq);
					if (NULL == query){
						WARN_LOG("query miss");
						this->head.trace_log();
						return 0;
					}
				} else {
					//����s->c �������͵ķ��ذ�
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
					//��ԭ����
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
					//֮����ʹ��query,��update�п����Ѿ���delete
				}
			}
			return 0;
		}

		 //////////////////////////////////////////////////////////////////////////
		 * @brief �Ը÷����������ݣ����ͱ��ĵİ�ͷ�л������кź͸�����һһ��Ӧ
		 *
		 * @param c_in ����ı���
		 * @param q ���������Ĳ�ѯ����
		 *
		 * @return  ���ͳ�ȥ���ֽ���,    -1, ����ʧ��
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
				//����ʧ�ܺ�,�������Ļص�����
				q->del_cb(seq);
				SAFE_DELETE(q);
			} else {
				//���ͳɹ�
			}
			return ret;
		}
		//�޻ص������Ĳ�ѯ
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
			//�÷���ʧЧ
			this->query_mgr.del_all_querys();
		};
		el::lib_querys_t query_mgr;
	protected:
		
	private:
	};
*/


	//////////////////////////////////////////////////////////////////////////
	//����ͻ��˷�����Ϣ�Ļ�����
	// 1.������Ϣ����>0 , ������������뵽�Է�.(��ͷ��������Ϣ����)
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
		//�������Կͻ�����Ϣ(���/�ַ������Դ�������)(do_parse_handle_cmd,do_handle ����������)
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


