/********************************************************************
	platform:	
	author:		kevin
	copyright:	All rights reserved.
	purpose:	�ػ���������
	brief:		ok
*********************************************************************/
#pragma once

#include <lib_include.h>

#include <lib_iatomic.h>
#include <lib_util.h>

namespace el_async{
	class daemon_t
	{
	public:
		virtual ~daemon_t();
		static daemon_t* get_instance(){
			static daemon_t daemon;
			return &daemon;
		}
		/**
		 * @brief	�������������,���û���,�źŴ���
		 * @param	int argc
		 * @param	char * * argv
		 * @return	void
		 */
		void prase_args(int argc, char** argv);
		int run();
		//************************************
		// Brief:     ɱ�������ӽ���(SIGKILL),���ȴ����˳��󷵻�.
		// Returns:   void
		//************************************
		void killall_children();
		void restart_child_process(struct bind_config_elem_t* elem, uint32_t elem_idx);
		//true:ֹͣ.false:����(�޸ı�)
		volatile bool stop;
		//true:����.false:����(�޸ı�)
		volatile bool restart;
		//�ӽ���ID
		std::vector<atomic_t> child_pids;
		//��������
		std::string prog_name;
		//��ǰĿ¼
		std::string current_dir;
		std::string bench_config_path_suf;
	private:
		daemon_t();
	};
	extern bool	g_is_parent;
}

#define g_daemon el_async::daemon_t::get_instance()