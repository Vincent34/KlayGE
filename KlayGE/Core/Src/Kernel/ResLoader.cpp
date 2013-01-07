/**
 * @file ResLoader.cpp
 * @author Minmin Gong
 *
 * @section DESCRIPTION
 *
 * This source file is part of KlayGE
 * For the latest info, see http://www.klayge.org
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * You may alternatively use this source under the terms of
 * the KlayGE Proprietary License (KPL). You can obtained such a license
 * from http://www.klayge.org/licensing/.
 */

#include <KlayGE/KlayGE.hpp>
#include <KFL/Util.hpp>
#include <KlayGE/Extract7z.hpp>
#include <KFL/Thread.hpp>

#include <fstream>
#include <sstream>
#ifdef KLAYGE_TR2_LIBRARY_FILESYSTEM_V2_SUPPORT
	#include <filesystem>
	namespace KlayGE
	{
		namespace filesystem = std::tr2::sys;
	}
#else
	#include <boost/filesystem.hpp>
	namespace KlayGE
	{
		namespace filesystem = boost::filesystem;
	}
#endif

#if defined KLAYGE_PLATFORM_WINDOWS
#include <windows.h>
#elif defined KLAYGE_PLATFORM_LINUX
#elif defined KLAYGE_PLATFORM_ANDROID
#include <KlayGE/Context.hpp>
#include <android/asset_manager.h>
#endif

#include <KlayGE/ResLoader.hpp>

namespace KlayGE
{
	ResLoader& ResLoader::Instance()
	{
		static ResLoader resLoader;
		return resLoader;
	}

	ResLoader::ResLoader()
	{
#if defined KLAYGE_PLATFORM_WINDOWS
#if defined KLAYGE_PLATFORM_WINDOWS_DESKTOP
		char buf[MAX_PATH];
		GetModuleFileNameA(nullptr, buf, sizeof(buf));
		exe_path_ = buf;
		exe_path_ = exe_path_.substr(0, exe_path_.rfind("\\"));
#endif
#elif defined KLAYGE_PLATFORM_LINUX || defined KLAYGE_PLATFORM_ANDROID
		{
			char line[1024];
			void const * symbol = "";
			FILE* fp = fopen("/proc/self/maps", "r");
			if (fp != nullptr)
			{
				while (!feof(fp))
				{
					unsigned long start, end;
					if (!fgets(line, sizeof(line), fp))
					{
						continue;
					}
					if (!strstr(line, " r-xp ") || !strchr(line, '/'))
					{
						continue;
					}

					sscanf(line, "%lx-%lx ", &start, &end);
					if ((symbol >= reinterpret_cast<void const *>(start)) && (symbol < reinterpret_cast<void const *>(end)))
					{
						exe_path_ = strchr(line, '/');
						exe_path_ = exe_path_.substr(0, exe_path_.rfind("/"));
					}
				}
				fclose(fp);
			}

#ifdef KLAYGE_PLATFORM_ANDROID
			exe_path_ = exe_path_.substr(0, exe_path_.find_last_of("/"));
			exe_path_ = exe_path_.substr(exe_path_.find_last_of("/") + 1);
			exe_path_ = exe_path_.substr(0, exe_path_.find_last_of("-"));
			exe_path_ = "/data/data/" + exe_path_;
#endif
		}
#endif

		paths_.push_back("");

#if defined KLAYGE_PLATFORM_WINDOWS_METRO
		this->AddPath("Assets/");
#else
		this->AddPath("");
		this->AddPath("../");
		this->AddPath("../../media/RenderFX/");
		this->AddPath("../../media/Models/");
		this->AddPath("../../media/Textures/2D/");
		this->AddPath("../../media/Textures/3D/");
		this->AddPath("../../media/Textures/Cube/");
		this->AddPath("../../media/Textures/Juda/");
		this->AddPath("../../media/Fonts/");
		this->AddPath("../../media/PostProcessors/");
#endif
	}

	void ResLoader::AddPath(std::string const & path)
	{
		filesystem::path new_path(path);
#ifdef KLAYGE_TR2_LIBRARY_FILESYSTEM_V2_SUPPORT
		if (!new_path.is_complete())
#else
		if (!new_path.is_absolute())
#endif
		{
			filesystem::path full_path = filesystem::path(exe_path_) / new_path;
			if (!filesystem::exists(full_path))
			{
#ifndef KLAYGE_PLATFORM_ANDROID
#ifdef KLAYGE_TR2_LIBRARY_FILESYSTEM_V2_SUPPORT
				full_path = filesystem::current_path<filesystem::path>() / new_path;
#else
				full_path = filesystem::current_path() / new_path;
#endif
				if (!filesystem::exists(full_path))
				{
					return;
				}
#else
				return;
#endif
			}
			new_path = full_path;
		}
		std::string path_str = new_path.string();

		if (path_str[path_str.length() - 1] != '/')
		{
			path_str.push_back('/');
		}
		paths_.push_back(path_str);
	}

	std::string ResLoader::Locate(std::string const & name)
	{
		if (('/' == name[0]) || ('\\' == name[0]))
		{
			if (filesystem::exists(filesystem::path(name)))
			{
				return name;
			}
		}
		else
		{
			typedef KLAYGE_DECLTYPE(paths_) PathsType;
			KLAYGE_FOREACH(PathsType::const_reference path, paths_)
			{
				std::string const res_name(path + name);

				if (filesystem::exists(filesystem::path(res_name)))
				{
					return res_name;
				}
				else
				{
					std::string::size_type const pkt_offset(res_name.find("//"));
					if (pkt_offset != std::string::npos)
					{
						std::string pkt_name = res_name.substr(0, pkt_offset);
						filesystem::path pkt_path(pkt_name);
						if (filesystem::exists(pkt_path)
								&& (filesystem::is_regular_file(pkt_path)
										|| filesystem::is_symlink(pkt_path)))
						{
							std::string::size_type const password_offset = pkt_name.find("|");
							std::string password;
							if (password_offset != std::string::npos)
							{
								password = pkt_name.substr(password_offset + 1);
								pkt_name = pkt_name.substr(0, password_offset - 1);
							}
							std::string const file_name = res_name.substr(pkt_offset + 2);

							uint64_t timestamp = filesystem::last_write_time(pkt_path);
							ResIdentifierPtr pkt_file = MakeSharedPtr<ResIdentifier>(name, timestamp,
								MakeSharedPtr<std::ifstream>(pkt_name.c_str(), std::ios_base::binary));
							if (*pkt_file)
							{
								if (Find7z(pkt_file, password, file_name) != 0xFFFFFFFF)
								{
									return res_name;
								}
							}
						}
					}
				}
			}
		}

#ifdef KLAYGE_PLATFORM_ANDROID
		android_app* state = Context::Instance().AppState();
		AAssetManager* am = state->activity->assetManager;
		AAsset* asset = AAssetManager_open(am, name.c_str(), AASSET_MODE_UNKNOWN);
		if (asset != nullptr)
		{
			AAsset_close(asset);
			return name;
		}
#endif

#ifndef KLAYGE_PLATFORM_WINDOWS_METRO
		return "";
#else
		std::string::size_type pos = name.rfind('/');
		if (std::string::npos == pos)
		{
			pos = name.rfind('\\');
		}
		if (pos != std::string::npos)
		{
			std::string file_name = name.substr(pos + 1);
			return this->Locate(file_name);
		}
		else
		{
			return "";
		}
#endif
	}

	ResIdentifierPtr ResLoader::Open(std::string const & name)
	{
		if (('/' == name[0]) || ('\\' == name[0]))
		{
			filesystem::path res_path(name);
			if (filesystem::exists(res_path))
			{
				uint64_t timestamp = filesystem::last_write_time(res_path);
				return MakeSharedPtr<ResIdentifier>(name, timestamp,
					MakeSharedPtr<std::ifstream>(name.c_str(), std::ios_base::binary));
			}
		}
		else
		{
			typedef KLAYGE_DECLTYPE(paths_) PathsType;
			KLAYGE_FOREACH(PathsType::const_reference path, paths_)
			{
				std::string const res_name(path + name);

				filesystem::path res_path(res_name);
				if (filesystem::exists(res_path))
				{
					uint64_t timestamp = filesystem::last_write_time(res_path);
					return MakeSharedPtr<ResIdentifier>(name, timestamp,
						MakeSharedPtr<std::ifstream>(res_name.c_str(), std::ios_base::binary));
				}
				else
				{
					std::string::size_type const pkt_offset(res_name.find("//"));
					if (pkt_offset != std::string::npos)
					{
						std::string pkt_name = res_name.substr(0, pkt_offset);
						filesystem::path pkt_path(pkt_name);
						if (filesystem::exists(pkt_path)
								&& (filesystem::is_regular_file(pkt_path)
										|| filesystem::is_symlink(pkt_path)))
						{
							std::string::size_type const password_offset = pkt_name.find("|");
							std::string password;
							if (password_offset != std::string::npos)
							{
								password = pkt_name.substr(password_offset + 1);
								pkt_name = pkt_name.substr(0, password_offset - 1);
							}
							std::string const file_name = res_name.substr(pkt_offset + 2);

							uint64_t timestamp = filesystem::last_write_time(pkt_path);
							ResIdentifierPtr pkt_file = MakeSharedPtr<ResIdentifier>(name, timestamp,
								MakeSharedPtr<std::ifstream>(pkt_name.c_str(), std::ios_base::binary));
							if (*pkt_file)
							{
								boost::shared_ptr<std::iostream> packet_file = MakeSharedPtr<std::stringstream>();
								Extract7z(pkt_file, password, file_name, packet_file);
								return MakeSharedPtr<ResIdentifier>(name, timestamp, packet_file);
							}
						}
					}
				}
			}
		}

#ifdef KLAYGE_PLATFORM_ANDROID
		android_app* state = Context::Instance().AppState();
		AAssetManager* am = state->activity->assetManager;
		AAsset* asset = AAssetManager_open(am, name.c_str(), AASSET_MODE_UNKNOWN);
		if (asset != nullptr)
		{
			boost::shared_ptr<std::stringstream> asset_file = MakeSharedPtr<std::stringstream>(std::ios_base::in | std::ios_base::out | std::ios_base::binary);

			int bytes = 0;
			char buf[1024];
			while ((bytes = AAsset_read(asset, buf, sizeof(buf))) > 0)
			{
				asset_file->write(buf, bytes);
			}

			AAsset_close(asset);

			return MakeSharedPtr<ResIdentifier>(name, 0, asset_file);
		}
#endif


#ifndef KLAYGE_PLATFORM_WINDOWS_METRO
		return ResIdentifierPtr();
#else
		std::string::size_type pos = name.rfind('/');
		if (std::string::npos == pos)
		{
			pos = name.rfind('\\');
		}
		if (pos != std::string::npos)
		{
			std::string file_name = name.substr(pos + 1);
			return this->Open(file_name);
		}
		else
		{
			return ResIdentifierPtr();
		}
#endif		
	}

	void* ResLoader::SyncQuery(ResLoadingDescPtr const & res_desc)
	{
		if (res_desc->HasSubThreadStage())
		{
			res_desc->SubThreadStage();
		}
		return res_desc->MainThreadStage();
	}

	boost::function<void*()> ResLoader::ASyncQuery(ResLoadingDescPtr const & res_desc)
	{
		boost::shared_ptr<joiner<void> > async_thread;
		if (res_desc->HasSubThreadStage())
		{
			async_thread = MakeSharedPtr<joiner<void> >(GlobalThreadPool()(boost::bind(&ResLoader::ASyncSubThreadFunc, this, res_desc)));
		}
		return boost::bind(&ResLoader::ASyncFunc, this, res_desc, async_thread);
	}

	void ResLoader::ASyncSubThreadFunc(ResLoadingDescPtr const & res_desc)
	{
		res_desc->SubThreadStage();
	}

	void* ResLoader::ASyncFunc(ResLoadingDescPtr const & res_desc, boost::shared_ptr<joiner<void> > const & loading_thread)
	{
		if (res_desc->HasSubThreadStage())
		{
			(*loading_thread)();
		}
		return res_desc->MainThreadStage();
	}
}