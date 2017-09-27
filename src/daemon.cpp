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
		//停止服务（不重启）
		CRIT_LOG("SIG_TERM FROM [stop:true, restart:false]");

		//结束日志,处理未处理完毕的日志
//		SAFE_DELETE(g_log);

		g_daemon->stop     = true;
		g_daemon->restart  = false;
	}

	void sighup_handler(int signo) 
	{
		//停止&&重启服务
		CRIT_LOG("SIGHUP FROM [restart:true, stop:true]");

		//结束日志,处理未处理完毕的日志
//		SAFE_DELETE(g_log);

		g_daemon->restart  = true;
		g_daemon->stop     = true;
	}

	//为了在core之前输出所有的日志
	void sigsegv_handler(int signo) 
	{
		CRIT_LOG("SIGSEGV FROM");

		//结束日志,处理未处理完毕的日志
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
		/*	服务端关闭已连接客户端，客户端接着发数据产生问题，
		1. 当服务器close一个连接时，若client端接着发数据。根据TCP协议的规定，
		会收到一个RST响应，client再往这个服务器发送数据时，系统会发出一个SIGPIPE信号给进程，
		告诉进程这个连接已经断开了，不要再写了。根据信号的默认处理规则SIGPIPE信号的
		默认执行动作是terminate(终止、退出),所以client会退出。若不想客户端退出可以把SIGPIPE设为SIG_IGN
		如:    signal(SIGPIPE,SIG_IGN);
		这时SIGPIPE交给了系统处理。
		2. 客户端write一个已经被服务器端关闭的sock后，返回的错误信息Broken pipe.
		1）broken pipe的字面意思是“管道破裂”。broken pipe的原因是该管道的读端被关闭。
		2）broken pipe经常发生socket关闭之后（或者其他的描述符关闭之后）的write操作中
	　　　	  3）发生broken pipe错误时，进程收到SIGPIPE信号，默认动作是进程终止。
		   4）broken pipe最直接的意思是：写入端出现的时候，另一端却休息或退出了，
		   因此造成没有及时取走管道中的数据，从而系统异常退出；*/
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
// 使用默认的处理方式 [2/5/2015 kevin]		sigaction(SIGSEGV, &sa, NULL);

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
		//在关闭服务器时,防止子进程先收到信号退出,父进程再次创建子进程.
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
