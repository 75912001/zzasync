/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	
	brief:		������Ϣ����	
*********************************************************************/

#pragma once

namespace el_async{

	#define  P_IN dynamic_cast<decltype(p_in)>(this->p_cmd_item->msg_in);
	#define  P_OUT dynamic_cast<decltype(p_out)>(this->p_cmd_item->msg_out);
	//////////////////////////////////////////////////////////////////////////
	//������Ϣ����	
	template <typename T_HANDLE_TYPE, typename T_PROTO_HEAD_TYPE>
	class handle_msg_base_t{
	public:
		//������ú�����ָ������	
		typedef int (T_HANDLE_TYPE::*P_FUN_NAME)();
		el::lib_tcp_peer_info_t* peer;
		int ret;//���ڱ����������ֵ��ֻ��Ϊ�˷���	
		
	public:
		handle_msg_base_t(){
			this->ret = 0;
			this->peer = NULL;
			this->p_cmd_item = NULL;
			this->head = new T_PROTO_HEAD_TYPE;
		}
		virtual ~handle_msg_base_t(){
			SAFE_DELETE(this->head);
		}
	protected:
		//����Э��ͷ,׼�������	
		virtual int do_parse_handle_cmd(const void* data, uint32_t len,
			el::lib_tcp_peer_info_t* peer_fd_info){
				TRACE_MSG_LOG("c======>[fd:%d]", peer_fd_info->fd);
				this->get_head()->unpack(data);
				this->get_head()->trace_log();
				TRACE_MSG_HEX_LOG(data, len);

				this->peer = peer_fd_info;
				this->ret = 0;

				//////////////////////////////////////////////////////////////////////////
				this->p_cmd_item = this->get_cmd_item(this->get_head()->get_cmd());
				if (NULL == this->p_cmd_item){
					return ERR;
				}
				return SUCC;
		}
		//�����ѽ�������Ϣͷ������(���ù�do_parse_handle_cmd,���ҷ���SUCC)	
		virtual int do_handle(const void* data, el::lib_msg_t* msg_restore_msg, el::lib_msg_t* msg_init){
			msg_init->init();
			this->ret = this->restore_msg((char*)data + T_PROTO_HEAD_TYPE::PROTO_HEAD_LEN, 
				msg_restore_msg);
			if (SUCC != this->ret){
				return this->ret;
			}
			this->ret = this->handle_msg();
			return this->ret;
		}
		//��ԭ��Ϣ	
		//0:����, ��0:������	
		virtual int restore_msg(const void* data, el::lib_msg_t* msg){
			//��ԭ����	
			msg->init();
			el::lib_msg_byte_t msg_byte((char*)data, this->get_head()->get_body_len());
			if (!msg->read(msg_byte)){
				WARN_LOG("p_msg read false");
				return el::ERR_SYS::PROTO_READ;
			} else if (!msg_byte.is_end()){
				WARN_LOG("p_msg len err");
				return el::ERR_SYS::PROTO_LEN;
			}
#ifdef SHOW_ALL_LOG
			msg->show();
			TRACE_MSG_LOG("++++++");
#endif
			return SUCC;
		}
		//����ԭ������Ϣ
		//0:����, ��0:������
		virtual int handle_msg(){
			if (NULL != this->p_cmd_item->func){
				this->do_handle_msg_pre();
				return (((T_HANDLE_TYPE*)this)->*(this->p_cmd_item->func))(); 
			}
			return el::ERR_SYS::UNDEFINED_CMD;
		}
		//************************************
		// @brief:	do_handle_dispatcher�д�����Ϣǰ����
		//************************************
		virtual int do_handle_msg_pre(){
			return 0;
		}
		T_PROTO_HEAD_TYPE* get_head(){
			return this->head;
		}
		el::lib_cmd_t<P_FUN_NAME>* get_cmd_item(uint32_t cmd){
			return this->cmd_map.get(cmd);
		}
		el::lib_cmd_map_t<P_FUN_NAME> cmd_map;
		el::lib_cmd_t<P_FUN_NAME>* p_cmd_item;
	private:
		T_PROTO_HEAD_TYPE* head;
	};
}//end namespace el_async




