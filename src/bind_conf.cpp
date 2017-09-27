#include <lib_include.h>
#include <lib_log.h>
#include <lib_util.h>
#include <lib_xmlparser.h>

#include "daemon.h"
#include "bind_conf.h"

namespace {
#ifndef WIN32
	const char* s_bind_config_path =  "./bind.xml";
#else
	const char* s_bind_config_path =  "bind.xml";
#endif//WIN32
}//end of namespace

el_async::bind_config_elem_t::bind_config_elem_t()
{
	this->id = 0;
	this->port = 0;
	this->restart_cnt = 0;
	this->net_type = 0;
}


int el_async::bind_config_t::load()
{
	el::lib_xmlparser_t xml_parser;
	if (0 != xml_parser.open(s_bind_config_path)){
		ALERT_LOG("open file [%s]", s_bind_config_path);
		return ERR;
	}

	xml_parser.move2children_node();
	xmlNodePtr cur = xml_parser.node_ptr;

	while(cur != NULL){
		//取出节点中的内容
		if (!xmlStrcmp(cur->name, (const xmlChar *)"Date")){
			el_async::bind_config_elem_t elem;
			elem.id = xml_parser.get_xml_prop(cur, "id");
			elem.net_type = xml_parser.get_xml_prop(cur, "net_type");
			xml_parser.get_xml_prop(cur, elem.name, "name");
			xml_parser.get_xml_prop(cur, elem.ip, "ip");
			elem.port = xml_parser.get_xml_prop(cur, "port");
			xml_parser.get_xml_prop(cur, elem.data, "data");
			DEBUG_LOG("bind config [id:%d, name:%s, net_type:%d, ip:%s, port:%d, data:%s]",
				elem.id, elem.name.c_str(), elem.net_type,
				elem.ip.c_str(), elem.port, elem.data.c_str());
			this->elems.push_back(elem);
		}
		cur = cur->next;
	}
	atomic_t t;
	atomic_set(&t, 0);
	g_daemon->child_pids.resize(this->elems.size(), t);
	return 0;
}
