/*
 * dispatcher.cc
 * This file is part of dispatcher-blender
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 *
 * dispatcher-webots is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * dispatcher-webots is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dispatcher-webots; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */


#include "dispatcher.hh"

#include <jessevdk/os/environment.hh>
#include <jessevdk/os/filesystem.hh>
#include <optimization/messages.hh>
#include <glibmm.h>
#include <jessevdk/base/string.hh>
#include <signal.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/types.h>
#include "config.hh"

using namespace std;
using namespace blender;
using namespace jessevdk::os;
using namespace jessevdk::network;
using namespace optimization::messages;
using namespace jessevdk::base;

Dispatcher::~Dispatcher()
{
	KillBlender();
}

void
Dispatcher::Stop()
{
	KillBlender();
	optimization::Dispatcher::Stop();
}

Dispatcher::Dispatcher()
:
	d_pid(0)
{
	Config::Initialize(PREFIXDIR "/libexec/liboptimization-dispatchers-2.0/blender.conf");
}

void
Dispatcher::KillBlender()
{
	if (d_pid == 0)
	{
		return;
	}

	GPid pid = d_pid;
	d_pid = 0;

	d_terminator.Terminate(pid, true, false);
}

void
Dispatcher::OnBlenderKilled(GPid pid, int ret)
{
	Glib::spawn_close_pid(pid);

	task::Response response;

	response.set_status(task::Response::Success);
	response.set_id(Task().id());

	task::Response::Fitness *fitness = response.add_fitness();
	fitness->set_name("value");
	fitness->set_value(1);

	WriteResponse(response);

	d_pid = 0;
	Main()->quit();
}

map<string, string>
Dispatcher::SetupEnvironment()
{
	map<string, string> envp = Environment::All();

	string environment;
	if (Setting("environment", environment))
	{
		vector<string> vars = String(environment).Split(",");

		for (vector<string>::iterator iter = vars.begin(); iter != vars.end(); ++iter)
		{
			vector<string> parts = String(*iter).Split("=", 2);

			if (parts.size() == 2)
			{
				envp[parts[0]] = parts[1];
			}
			else if (parts.size() == 1)
			{
				envp[parts[0]] = "";
			}
		}
	}

	return envp;
}

bool
Dispatcher::SetupArguments(vector<string> &argv)
{
	Config &config(Config::Instance());

	string path = Glib::find_program_in_path(config.BlenderPath);

	if (path == "")
	{
		cerr << "Could not find blender executable" << endl;
		return false;
	}

	argv.push_back(path);

	// Append blender file
	string blenderfile;
	if (!Setting("blender-file", blenderfile))
	{
		cerr << "Missing blender-file" << endl;
		return false;
	}

	argv.push_back("-b");
	argv.push_back(blenderfile);

	// Append frame number
	task::Task::Parameter parameter;
	if (!Parameter("frame", parameter))
	{
		cerr << "Missing 'frame' parameter" << endl;
		return false;
	}

	size_t frame = static_cast<size_t>(parameter.value());
	argv.push_back("-f");

	stringstream s;
	s << frame;
	argv.push_back(s.str());

	string outputpath;
	argv.push_back("-o");

	if (Setting("output-path", outputpath))
	{
		argv.push_back(outputpath + "/frame#");
	}
	else
	{
		argv.push_back("//frame#");
	}

	string format;
	argv.push_back("-F");

	if (Setting("format", format))
	{
		argv.push_back(format);
	}
	else
	{
		argv.push_back("PNG");
	}

	argv.push_back("-x");
	argv.push_back("1");

	// Append additional arguments
	string args;

	if (Setting("arguments", args))
	{
		vector<string> splitted = Glib::shell_parse_argv(args);

		for (vector<string>::iterator iter = splitted.begin(); iter != splitted.end(); ++iter)
		{
			argv.push_back(*iter);
		}
	}

	return true;
}

bool
Dispatcher::RunTask()
{
	map<string, string> envp = SetupEnvironment();
	envp["OPTIMIZATION_BLENDER"] = "yes";

	vector<string> argv;

	if (!SetupArguments(argv))
	{
		return false;
	}

	gint sin;
	gint serr;

	try
	{
		Glib::spawn_async_with_pipes(WorkingDirectory(),
		                             argv,
		                             Environment::Convert(envp),
		                             Glib::SPAWN_DO_NOT_REAP_CHILD |
		                             Glib::SPAWN_STDOUT_TO_DEV_NULL,
		                             sigc::slot<void>(),
		                             &d_pid,
		                             &sin,
		                             0,
		                             &serr);
	}
	catch (Glib::SpawnError &e)
	{
		cerr << "Error while spawning blender: " << e.what() << endl;
		return false;
	}

	close(sin);
	close(serr);

	Glib::signal_child_watch().connect(sigc::mem_fun(*this, &Dispatcher::OnBlenderKilled), d_pid);

	return true;
}

std::string
Dispatcher::WorkingDirectory()
{
	string directory;
	return Setting("working-directory", directory) ? directory : "";
}
