/*
This source file is part of KBEngine
For the latest info, see http://www.kbengine.org/

Copyright (c) 2008-2012 KBEngine.

KBEngine is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

KBEngine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with KBEngine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "resmgr.hpp"
#include "helper/watcher.hpp"
#include "thread/threadguard.hpp"

#if KBE_PLATFORM != PLATFORM_WIN32
#include <unistd.h>
#include <fcntl.h>
#endif

namespace KBEngine{
KBE_SINGLETON_INIT(Resmgr);

uint64 Resmgr::respool_timeout = 0;
uint32 Resmgr::respool_buffersize = 0;
uint32 Resmgr::respool_checktick = 0;

//-------------------------------------------------------------------------------------
Resmgr::Resmgr():
kb_env_(),
respaths_(),
isInit_(false),
respool_(),
mutex_()
{
}

//-------------------------------------------------------------------------------------
Resmgr::~Resmgr()
{
	respool_.clear();
}

//-------------------------------------------------------------------------------------
bool Resmgr::initializeWatcher()
{
	WATCH_OBJECT("syspaths/KBE_ROOT", kb_env_.root);
	WATCH_OBJECT("syspaths/KBE_RES_PATH", kb_env_.res_path);
	WATCH_OBJECT("syspaths/KBE_HYBRID_PATH", kb_env_.hybrid_path);
	return true;
}

//-------------------------------------------------------------------------------------
bool Resmgr::initialize()
{
	//if(isInit())
	//	return true;

	// 获取引擎环境配置
	kb_env_.root			= getenv("KBE_ROOT") == NULL ? "" : getenv("KBE_ROOT");
	kb_env_.res_path		= getenv("KBE_RES_PATH") == NULL ? "" : getenv("KBE_RES_PATH"); 
	kb_env_.hybrid_path		= getenv("KBE_HYBRID_PATH") == NULL ? "" : getenv("KBE_HYBRID_PATH"); 

	//kb_env_.root				= "/home/kbe/kbengine/";
	//kb_env_.res_path			= "/home/kbe/kbengine/kbe/res/;/home/kbe/kbengine/demo/;/home/kbe/kbengine/demo/res/"; 
	//kb_env_.hybrid_path		= "/home/kbe/kbengine/kbe/bin/Hybrid/"; 
	
	if(kb_env_.res_path.size() == 0 || kb_env_.root.size() == 0)
	{
		printf("[ERROR] Resmgr::initialize: not set environment, (KBE_ROOT, KBE_RES_PATH, KBE_HYBRID_PATH) invalid!\n");
#if KBE_PLATFORM == PLATFORM_WIN32
		::MessageBox(0, L"Resmgr::initialize: not set environment, (KBE_ROOT, KBE_RES_PATH, KBE_HYBRID_PATH) invalid!\n", L"ERROR", MB_ICONERROR);
#endif
	}

	char ch;
	
	if(kb_env_.root.size() > 0)
	{
		ch =  kb_env_.root.at(kb_env_.root.size() - 1);
		if(ch != '/' && ch != '\\')
			kb_env_.root += "/";

		strutil::kbe_replace(kb_env_.root, "\\", "/");
		strutil::kbe_replace(kb_env_.root, "//", "/");
	}

	if(kb_env_.hybrid_path.size() > 0)
	{
		ch =  kb_env_.hybrid_path.at(kb_env_.hybrid_path.size() - 1);
		if(ch != '/' && ch != '\\')
			kb_env_.hybrid_path += "/";

		strutil::kbe_replace(kb_env_.hybrid_path, "\\", "/");
		strutil::kbe_replace(kb_env_.hybrid_path, "//", "/");
	}

	respaths_.clear();
	std::string tbuf = kb_env_.res_path;
	char splitFlag = ';';
	strutil::kbe_split<char>(tbuf, splitFlag, respaths_);

	if(respaths_.size() < 2)
	{
		respaths_.clear();
		splitFlag = ':';
		strutil::kbe_split<char>(tbuf, splitFlag, respaths_);
	}

	kb_env_.res_path = "";
	std::vector<std::string>::iterator iter = respaths_.begin();
	for(; iter != respaths_.end(); iter++)
	{
		if((*iter).size() <= 0)
			continue;

		ch =  (*iter).at((*iter).size() - 1);
		if(ch != '/' && ch != '\\')
			(*iter) += "/";

		kb_env_.res_path += (*iter);
		kb_env_.res_path += splitFlag;
		strutil::kbe_replace(kb_env_.res_path, "\\", "/");
		strutil::kbe_replace(kb_env_.res_path, "//", "/");
	}

	if(kb_env_.res_path.size() > 0)
		kb_env_.res_path.erase(kb_env_.res_path.size() - 1);

	isInit_ = true;

	respool_.clear();
	return true;
}

//-------------------------------------------------------------------------------------
void Resmgr::pirnt(void)
{
	INFO_MSG(boost::format("Resmgr::initialize: KBE_ROOT=%1%\n") % kb_env_.root.c_str());
	INFO_MSG(boost::format("Resmgr::initialize: KBE_RES_PATH=%1%\n") % kb_env_.res_path.c_str());
	INFO_MSG(boost::format("Resmgr::initialize: KBE_HYBRID_PATH=%1%\n") % kb_env_.hybrid_path.c_str());
}

//-------------------------------------------------------------------------------------
std::string Resmgr::matchRes(std::string res)
{
	return matchRes(res.c_str());
}

//-------------------------------------------------------------------------------------
std::string Resmgr::matchRes(const char* res)
{
	std::vector<std::string>::iterator iter = respaths_.begin();

	for(; iter != respaths_.end(); iter++)
	{
		std::string fpath = ((*iter) + res);

		strutil::kbe_replace(fpath, "\\", "/");
		strutil::kbe_replace(fpath, "//", "/");

		FILE * f = fopen (fpath.c_str(), "r");
		if(f != NULL)
		{
			fclose(f);
			return fpath;
		}
	}
	return res;
}

//-------------------------------------------------------------------------------------
std::string Resmgr::matchPath(std::string path)
{
	return matchPath(path.c_str());
}

//-------------------------------------------------------------------------------------
std::string Resmgr::matchPath(const char* path)
{
	std::vector<std::string>::iterator iter = respaths_.begin();
	
	std::string npath = path;
	strutil::kbe_replace(npath, "\\", "/");
	strutil::kbe_replace(npath, "//", "/");

	for(; iter != respaths_.end(); iter++)
	{
		std::string fpath = ((*iter) + npath);

		strutil::kbe_replace(fpath, "\\", "/");
		strutil::kbe_replace(fpath, "//", "/");
		
		if(!access(fpath.c_str(), 0))
		{
			return fpath;
		}
	}

	return "";
}

//-------------------------------------------------------------------------------------
std::string Resmgr::getPySysResPath()
{
	if(respaths_.size() > 0)
	{
		return respaths_[0];
	}

	return "";
}

//-------------------------------------------------------------------------------------
ResourceObjectPtr Resmgr::openResource(const char* res, const char* model, uint32 flags)
{
	std::string respath = matchRes(res);

	if(Resmgr::respool_checktick == 0)
	{
		return new FileObject(respath.c_str(), flags, model);
	}

	KBEngine::thread::ThreadGuard tg(&mutex_); 
	KBEUnordered_map< std::string, ResourceObjectPtr >::iterator iter = respool_.find(respath);
	if(iter == respool_.end())
	{
		FileObject* fobj = new FileObject(respath.c_str(), flags, model);
		respool_[respath] = fobj;
		fobj->update();
		return fobj;
	}

	iter->second->update();
	return iter->second;
}

//-------------------------------------------------------------------------------------
void Resmgr::update()
{
	KBEngine::thread::ThreadGuard tg(&mutex_); 
	KBEUnordered_map< std::string, ResourceObjectPtr >::iterator iter = respool_.begin();
	for(; iter != respool_.end();)
	{
		if(!iter->second->valid())
		{
			respool_.erase(iter++);
		}
		else
		{
			iter++;
		}
	}
}

//-------------------------------------------------------------------------------------
void Resmgr::handleTimeout(TimerHandle handle, void * arg)
{
	update();
}

//-------------------------------------------------------------------------------------		
}
