/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	
	brief:		服务 管理器	
*********************************************************************/

#pragma once

#include <lib_include.h>
#include <lib_log.h>
#include "service_if.h"

namespace el_async{
	//T_SERVICE_TYPE 必须继承自 el_async::service_base_t	
	template <typename T_SERVICE_TYPE>
	class service_mgr_base_t
	{
	public:
		typedef std::map<uint32_t, T_SERVICE_TYPE*> ID_SERVICE_MAP;//key:服务序号ID	
		typedef std::map<int, T_SERVICE_TYPE*> FD_SERVICE_MAP;//key:服务FD	
	public:
		service_mgr_base_t(){
			this->service_count = 0;
		}
		virtual ~service_mgr_base_t(){}
		//************************************
		// @brief:	初始化 服务管理器所需的 服务资料(服务器名前缀,服务器总数)	
		// return:	int
		// parameter:	std::string major_tag
		// parameter:	std::string name_tag
		// parameter:	std::string count_tag
		//************************************
		virtual int do_init(std::string major_tag, std::string name_tag, std::string count_tag)
		{
			this->service_name = g_bench_conf->get_strval(major_tag.c_str(), name_tag.c_str());
			if (this->service_name.empty()){
				ALERT_LOG("service_name empty ");
				return -1;
			}

			if (!g_bench_conf->get_uint32_val(major_tag.c_str(),
						count_tag.c_str(), this->service_count)){
				ALERT_LOG("get service_count err %s,%s, %s", major_tag.c_str(), name_tag.c_str(), count_tag.c_str());
				return -1;
			}
			return 0;
		}
		//************************************
		// @brief:	添加一个服务 并尝试连接	
		// return:	int
		// parameter:	uint32_t service_id
		// parameter:	std::string ip
		// parameter:	uint16_t port
		//************************************
		virtual int do_add_service(uint32_t service_id, std::string ip, uint16_t port){
			auto it = this->id_service_map.find(service_id);
			T_SERVICE_TYPE* pservice = NULL;
			if (this->id_service_map.end() == it){
				pservice = new T_SERVICE_TYPE(service_id);
				pservice->ip = ip;
				pservice->port = port;
				this->id_service_map.insert(std::make_pair(service_id, pservice));
			} else {
				pservice = it->second;
			}
			if (!pservice->is_connect()){
				if (0 == pservice->connect()){
					this->fd_service_map.insert(std::make_pair(pservice->peer->get_fd(), pservice));
				}
			}

			return 0;
		}
		//************************************
		// @brief:	删除一个服务	
		// return:	int
		// parameter:	int fd
		//************************************
		virtual int do_del_service(int fd){
			auto it = this->fd_service_map.find(fd);
			if (this->fd_service_map.end() == it){
				ERROR_LOG("");
				return 0;
			}

			T_SERVICE_TYPE* pservice = it->second;

			this->id_service_map.erase(pservice->get_service_id());
			this->fd_service_map.erase(fd);

			SAFE_DELETE(pservice);
			return 0;
		}
		//************************************
		// @brief:	向服务发送消息(由route_id,来计算具体发送到几号服务中去)	
		//************************************
		virtual int do_s2(uint32_t route_id, el::lib_proto_head_base_t* head, el::lib_msg_t* msg = NULL){
			T_SERVICE_TYPE* pservice = this->do_get_service(route_id);
			if (NULL == pservice){
				ERROR_LOG("[route_id:%u]", route_id);
			} else {
				pservice->do_s2(head, msg);
			}
			return 0;
		}
		//************************************
		// @brief:	由传入的路由序号,获取对应的服务	
		// return:	NULL:无 非NULL:对应的服务	
		//************************************
		virtual T_SERVICE_TYPE* do_get_service(uint32_t route_id){
			FIND_MAP_SECOND_POINTER_RETURN_POINTER(this->id_service_map, this->get_service_idx(route_id));
		}
		uint32_t get_service_idx(uint32_t route_id){
			return (route_id % this->service_count) + 1;
		}
		//************************************
		// @brief:	处理服务发来的消息	
		// return:	int el::ERR_SYS::CMD_OUT_OF_RANGE 超出处理范围	
		//************************************
		int do_handle_dispatcher( const void* data, uint32_t len, el::lib_tcp_peer_info_t* peer)
		{
			auto it = this->fd_service_map.find(peer->fd);
			if (this->fd_service_map.end() != it){
				T_SERVICE_TYPE* pservive = it->second;
				int ret = pservive->check_has_cmd(data, len);
				if (0 != ret){
					return ret;
				}
				
				return pservive->do_handle_dispatcher(data, len, peer);
			} else {
				ERROR_LOG("[fd:%d]", peer->fd);
			}
			return 0;
		}

	protected:
	
	private:
		service_mgr_base_t(const service_mgr_base_t& cr);
		service_mgr_base_t& operator=(const service_mgr_base_t& cr);

		ID_SERVICE_MAP id_service_map;
		FD_SERVICE_MAP fd_service_map;
		std::string service_name;//服务名称	
		uint32_t service_count;//既定服务总数	
	};

	//////////////////////////////////////////////////////////////////////////
	//protobuf 简称pb	
	//T_SERVICE_TYPE 必须继承自 el_async::service_base_pb_t	
	template <typename T_SERVICE_TYPE>
	class service_mgr_base_pb_t
	{
	public:
		typedef std::map<uint32_t, T_SERVICE_TYPE*> ID_SERVICE_MAP;//key:服务序号ID	
		typedef std::map<int, T_SERVICE_TYPE*> FD_SERVICE_MAP;//key:服务FD	
	public:
		ID_SERVICE_MAP id_service_map;
		service_mgr_base_pb_t(){
		}
		virtual ~service_mgr_base_pb_t(){
			std::for_each(this->id_service_map.begin(), this->id_service_map.end(), el::lib_delete_pair_t());
		}

		//************************************
		// @brief:	添加一个服务 并尝试连接	
		// return:	int
		// parameter:	uint32_t service_id
		// parameter:	std::string ip
		// parameter:	uint16_t port
		//************************************
		virtual int do_add_service(uint32_t service_id, std::string ip, uint16_t port){
			auto it = this->id_service_map.find(service_id);
			T_SERVICE_TYPE* pservice = NULL;
			if (this->id_service_map.end() == it){
				pservice = new T_SERVICE_TYPE;
				pservice->service_id = service_id;
				pservice->ip = ip;
				pservice->port = port;
				this->id_service_map.insert(std::make_pair(service_id, pservice));
			} else {
				pservice = it->second;
			}
			if (!pservice->is_connect()){
				if (0 == pservice->connect()){
					pservice->peer->data = (void*)pservice;
					this->fd_service_map.insert(std::make_pair(pservice->peer->fd, pservice));
				} else {//connection fail
					SAFE_DELETE(pservice);
					this->id_service_map.erase(service_id);
					ALERT_LOG("");
					return ERR;
				}
			}

			return 0;
		}
		//************************************
		// @brief:	删除一个服务	
		// return:	int
		// parameter:	int fd
		//************************************
		virtual int do_del_service(int fd){
			auto it = this->fd_service_map.find(fd);
			if (this->fd_service_map.end() == it){
				ERROR_LOG("");
				return 0;
			}

			T_SERVICE_TYPE* pservice = it->second;

			this->id_service_map.erase(pservice->service_id);
			this->fd_service_map.erase(fd);

			SAFE_DELETE(pservice);
			return 0;
		}
		//************************************
		// @brief:	向服务发送消息(由route_id,来计算具体发送到几号服务中去)	
		//************************************
		virtual int do_s2(uint32_t route_id, const void* data, uint32_t len){
			T_SERVICE_TYPE* pservice = this->do_get_service(route_id);
			if (NULL == pservice){
				ERROR_LOG("[route_id:%u]", route_id);
			} else {
				pservice->do_s2(data, len);
			}
			return 0;
		}
		//************************************
		// @brief:	由传入的路由序号,获取对应的服务	
		// return:	NULL:无 非NULL:对应的服务	
		//************************************
		virtual T_SERVICE_TYPE* do_get_service(uint32_t route_id){
			if (this->id_service_map.empty()){
				return NULL;
			}
			
			FIND_MAP_SECOND_POINTER_RETURN_POINTER(this->id_service_map, this->get_service_idx(route_id));
		}

		T_SERVICE_TYPE* find(int fd){
			auto it = this->fd_service_map.find(fd);
			if (this->fd_service_map.end() != it){
				return it->second;
			}
			return NULL;
		}
		T_SERVICE_TYPE* find(uint32_t server_id){
			auto it = this->id_service_map.find(server_id);
			if (this->id_service_map.end() != it){
				return it->second;
			}
			return NULL;
		}
		

		uint32_t get_service_idx(uint32_t route_id){
			return (route_id % this->id_service_map.size()) + 1;
		}
	protected:
	
	private:
		
		FD_SERVICE_MAP fd_service_map;
	};
}//end namespace el_async


