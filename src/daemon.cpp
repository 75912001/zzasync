#include <lib_include.h>
#include <lib_util.h>
#include <lib_log.h>
#include <lib_file.h>
#include <lib_net/lib_tcp_server.h>

#include "bind_conf.h"
#include "bench_conf.h"
#include "service.h"
#include "dll.h"
#include "net_tcp.h"
#include "daemon.h"

namespace el_async{
	bool g_is_parent = true;
}

namespace {

	std::vector<std::string> g_argvs;
#ifndef WIN32
	void sigterm_handler(int signo) 
	{
		//ֹͣ���񣨲�������
		CRIT_LOG("SIG_TERM FROM [stop:true, restart:false]");

		//������־,����δ������ϵ���־
//		SAFE_DELETE(g_log);

		g_daemon->stop     = true;
		g_daemon->restart  = false;
	}

	void sighup_handler(int signo) 
	{
		//ֹͣ&&��������
		CRIT_LOG("SIGHUP FROM [restart:true, stop:true]");

		//������־,����δ������ϵ���־
//		SAFE_DELETE(g_log);

		g_daemon->restart  = true;
		g_daemon->stop     = true;
	}

	//Ϊ����core֮ǰ������е���־
	void sigsegv_handler(int signo) 
	{
		CRIT_LOG("SIGSEGV FROM");

		//������־,����δ������ϵ���־
//		SAFE_DELETE(g_log);

 		signal(SIGSEGV, SIG_DFL);
 		kill(getpid(), signo);
	}

#if 0
	//man 2 sigaction to see si->si_code
	void sigchld_handler(int signo, siginfo_t *si, void * p) 
	{
		CRIT_LOG("sigchld from [pid=%d, uid=%d", si->si_pid, si->si_uid);
		switch (si->si_code) {
		case CLD_DUMPED:
			::chmod("core", 700);
			::chown("core", 0, 0);
			char core_name[1024] ={0};
			::sprintf(core_name, "core.%d", si->si_pid);
			::rename("core", core_name);
			break;
		}
	}
#endif
	void rlimit_reset()
	{
		struct rlimit rlim;

		/* set open files */
		rlim.rlim_cur = g_bench_conf->max_fd_num;
		rlim.rlim_max = g_bench_conf->max_fd_num;
		if (-1 == setrlimit(RLIMIT_NOFILE, &rlim)) {
			CRIT_LOG("INIT FD RESOURCE FAILED [OPEN FILES NUMBER:%d]", g_bench_conf->max_fd_num);
		}

		/* set core dump */
		rlim.rlim_cur = g_bench_conf->core_size;
		rlim.rlim_max = g_bench_conf->core_size;
		if (-1 == setrlimit(RLIMIT_CORE, &rlim)) {
			CRIT_LOG("INIT CORE FILE RESOURCE FAILED [CORE DUMP SIZE:%d]",
				g_bench_conf->core_size);
		}
	}

	void save_argv(int argc, char** argv)
	{
		for (int i = 0; i < argc; i++){
			std::string str = argv[i];
			g_argvs.push_back(str);
		}
	}

	void set_signal()
	{
		struct sigaction sa;
		::memset(&sa, 0, sizeof(sa));
		/************************************************************************/
		/*	����˹ر������ӿͻ��ˣ��ͻ��˽��ŷ����ݲ������⣬
		1. ��������closeһ������ʱ����client�˽��ŷ����ݡ�����TCPЭ��Ĺ涨��
		���յ�һ��RST��Ӧ��client���������������������ʱ��ϵͳ�ᷢ��һ��SIGPIPE�źŸ����̣�
		���߽�����������Ѿ��Ͽ��ˣ���Ҫ��д�ˡ������źŵ�Ĭ�ϴ������SIGPIPE�źŵ�
		Ĭ��ִ�ж�����terminate(��ֹ���˳�),����client���˳���������ͻ����˳����԰�SIGPIPE��ΪSIG_IGN
		��:    signal(SIGPIPE,SIG_IGN);
		��ʱSIGPIPE������ϵͳ����
		2. �ͻ���writeһ���Ѿ����������˹رյ�sock�󣬷��صĴ�����ϢBroken pipe.
		1��broken pipe��������˼�ǡ��ܵ����ѡ���broken pipe��ԭ���Ǹùܵ��Ķ��˱��رա�
		2��broken pipe��������socket�ر�֮�󣨻����������������ر�֮�󣩵�write������
	������	  3������broken pipe����ʱ�������յ�SIGPIPE�źţ�Ĭ�϶����ǽ�����ֹ��
		   4��broken pipe��ֱ�ӵ���˼�ǣ�д��˳��ֵ�ʱ����һ��ȴ��Ϣ���˳��ˣ�
		   ������û�м�ʱȡ�߹ܵ��е����ݣ��Ӷ�ϵͳ�쳣�˳���*/
		/************************************************************************/
		signal(SIGPIPE,SIG_IGN);

		sa.sa_handler = sigterm_handler;
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGQUIT, &sa, NULL);

#if 0
		sa.sa_flags = SA_RESTART|SA_SIGINFO;
		sa.sa_sigaction = sigchld_handler;
		sigaction(SIGCHLD, &sa, NULL);
#endif

		sa.sa_handler = sighup_handler;
		sigaction(SIGHUP, &sa, NULL);

		sa.sa_handler = sigsegv_handler;
// ʹ��Ĭ�ϵĴ���ʽ [2/5/2015 kevin]		sigaction(SIGSEGV, &sa, NULL);

		sigset_t sset;
		sigemptyset(&sset);
		sigaddset(&sset, SIGSEGV);
		sigaddset(&sset, SIGBUS);
		sigaddset(&sset, SIGABRT);
		sigaddset(&sset, SIGILL);
		sigaddset(&sset, SIGCHLD);
		sigaddset(&sset, SIGFPE);
		sigprocmask(SIG_UNBLOCK, &sset, &sset);
	}
#endif//WIN32
}//end of namespace

el_async::daemon_t::daemon_t()
{
	this->stop = false;
	this->restart = false;
}

void el_async::daemon_t::prase_args( int argc, char** argv )
{
#ifndef WIN32
 	this->prog_name = argv[0];
	char* dir = ::get_current_dir_name();
	this->current_dir = dir;
	free(dir);

	rlimit_reset();
	set_signal();
	save_argv(argc, argv);
	if (g_bench_conf->is_daemon){
		daemon(1, 1);
	}
#endif//WIN32
}

el_async::daemon_t::~daemon_t()
{
#ifndef WIN32
	if (this->restart && !this->prog_name.empty()) {
		killall_children();

		ALERT_LOG("SERVER RESTARTING...");
		::chdir(this->current_dir.c_str());
		char* argvs[200];
		int i = 0;
		FOREACH(g_argvs, it){
			argvs[i] = const_cast<char*>(it->c_str());
			i++;
		}

		execv(this->prog_name.c_str(), argvs);
		ALERT_LOG("RESTART FAILED...");
	}
#endif//WIN32
}

void el_async::daemon_t::killall_children()
{
#ifndef WIN32
	for (uint32_t i = 0; i < g_bind_conf->elems.size(); ++i) {
		if (0 != atomic_read(&this->child_pids[i])) {
			kill(atomic_read(&this->child_pids[i]), SIGTERM/*SIGKILL*/);
		}
	}
#endif//WIN32
}

void el_async::daemon_t::restart_child_process( bind_config_elem_t* elem, uint32_t elem_idx)
{
#ifndef WIN32
	if (g_daemon->stop && !g_daemon->restart){
		//�ڹرշ�����ʱ,��ֹ�ӽ������յ��ź��˳�,�������ٴδ����ӽ���.
		return;
	}
	elem->recv_pipe.close(el::E_PIPE_INDEX_WRONLY);
	elem->send_pipe.close(el::E_PIPE_INDEX_RDONLY);

	if (0 != elem->recv_pipe.create()
		|| 0 != elem->send_pipe.create()){
		ERROR_LOG("pipe create err");
		return;
	}

	pid_t pid;

	if ( (pid = fork ()) < 0 ) {
		CRIT_LOG("fork failed: %s", strerror(errno));
	} else if (pid > 0) {
		//parent process
		g_bind_conf->elems[elem_idx].recv_pipe.close(el::E_PIPE_INDEX_RDONLY);
		g_bind_conf->elems[elem_idx].send_pipe.close(el::E_PIPE_INDEX_WRONLY);
		g_net_server->server_epoll->add_connect(elem->send_pipe.read_fd(), el::FD_TYPE_PIPE, NULL, 0);
		atomic_set(&g_daemon->child_pids[elem_idx], pid);
	} else { 
		//child process
		g_service->run(&g_bind_conf->elems[elem_idx], g_bind_conf->elems.size());
	}
#endif//WIN32
}

int el_async::daemon_t::run()
{
	return g_net_server->server_epoll->run();
}
