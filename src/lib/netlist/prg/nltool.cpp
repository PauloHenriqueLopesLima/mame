// license:GPL-2.0+
// copyright-holders:Couriersud
/***************************************************************************

    nltool.c

    Simple tool to debug netlists outside MAME.

****************************************************************************/

#include "netlist/plib/pmain.h"
#include "netlist/nl_setup.h"
#include "netlist/nl_parser.h"
#include "netlist/devices/net_lib.h"
#include "netlist/tools/nl_convert.h"
#include "netlist/solver/nld_solver.h"

class tool_app_t : public plib::app
{
public:
	tool_app_t() :
		plib::app(),
		opt_grp1(*this,     "General options",              "The following options apply to all commands."),
		opt_cmd (*this,     "c", "cmd",         "run",      "run:convert:listdevices:static:header:docheader", "run|convert|listdevices|static|header"),
		opt_file(*this,     "f", "file",        "-",        "file to process (default is stdin)"),
		opt_defines(*this,  "D", "define",                  "predefine value as macro, e.g. -Dname=value. If '=value' is omitted predefine it as 1. This option may be specified repeatedly."),
		opt_rfolders(*this, "r", "rom",                     "where to look for files"),
		opt_verb(*this,     "v", "verbose",                 "be verbose - this produces lots of output"),
		opt_quiet(*this,    "q", "quiet",                   "be quiet - no warnings"),
		opt_version(*this,  "",  "version",                 "display version and exit"),
		opt_help(*this,     "h", "help",                    "display help and exit"),
		opt_grp2(*this,     "Options for run and static commands",   "These options apply to run and static commands."),
		opt_name(*this,     "n", "name",        "",         "the netlist in file specified by ""-f"" option to run; default is first one"),
		opt_grp3(*this,     "Options for run command",      "These options are only used by the run command."),
		opt_ttr (*this,     "t", "time_to_run", 1.0,        "time to run the emulation (seconds)"),
		opt_logs(*this,     "l", "log" ,                    "define terminal to log. This option may be specified repeatedly."),
		opt_inp(*this,      "i", "input",       "",         "input file to process (default is none)"),
		opt_grp4(*this,     "Options for convert command",  "These options are only used by the convert command."),
		opt_type(*this,     "y", "type",        "spice",    "spice:eagle:rinf", "type of file to be converted: spice,eagle,rinf"),

		opt_ex1(*this,     "nltool -c run -t 3.5 -f nl_examples/cdelay.c -n cap_delay",
				"Run netlist \"cap_delay\" from file nl_examples/cdelay.c for 3.5 seconds"),
		opt_ex2(*this,     "nltool --cmd=listdevices",
				"List all known devices.")
		{}

	plib::option_group  opt_grp1;
	plib::option_str_limit opt_cmd;
	plib::option_str    opt_file;
	plib::option_vec    opt_defines;
	plib::option_vec    opt_rfolders;
	plib::option_bool   opt_verb;
	plib::option_bool   opt_quiet;
	plib::option_bool   opt_version;
	plib::option_bool   opt_help;
	plib::option_group  opt_grp2;
	plib::option_str    opt_name;
	plib::option_group  opt_grp3;
	plib::option_double opt_ttr;
	plib::option_vec    opt_logs;
	plib::option_str    opt_inp;
	plib::option_group  opt_grp4;
	plib::option_str_limit opt_type;
	plib::option_example opt_ex1;
	plib::option_example opt_ex2;

	int execute();
	pstring usage();

private:
	void run();
	void static_compile();

	void mac_out(const pstring s, const bool cont = true);
	void cmac(const netlist::factory::element_t *e);
	void mac(const netlist::factory::element_t *e);

	void create_header();
	void create_docheader();

	void listdevices();

};

static NETLIST_START(dummy)
	/* Standard stuff */

	CLOCK(clk, 1000) // 1000 Hz
	SOLVER(Solver, 48000)

NETLIST_END()

/***************************************************************************
    CORE IMPLEMENTATION
***************************************************************************/

class netlist_data_folder_t : public netlist::source_t
{
public:
	netlist_data_folder_t(netlist::setup_t &setup,
			pstring folder)
	: netlist::source_t(setup, netlist::source_t::DATA)
	, m_folder(folder)
	{
	}

	virtual std::unique_ptr<plib::pistream> stream(const pstring &file) override;

private:
	pstring m_folder;
};

std::unique_ptr<plib::pistream> netlist_data_folder_t::stream(const pstring &file)
{
	pstring name = m_folder + "/" + file;
	try
	{
		auto strm = plib::make_unique_base<plib::pistream, plib::pifilestream>(name);
		return strm;
	}
	catch (plib::pexception e)
	{

	}
	return std::unique_ptr<plib::pistream>(nullptr);
}

class netlist_tool_t : public netlist::netlist_t
{
public:

	netlist_tool_t(const pstring &aname)
	: netlist::netlist_t(aname)
	{
	}

	~netlist_tool_t()
	{
	}

	void init()
	{
	}

	void read_netlist(const pstring &filename, const pstring &name,
			const std::vector<pstring> &logs,
			const std::vector<pstring> &defines,
			const std::vector<pstring> &roms)
	{
		// read the netlist ...

		for (auto & d : defines)
			setup().register_define(d);

		for (auto & r : roms)
			setup().register_source(plib::make_unique_base<netlist::source_t, netlist_data_folder_t>(setup(), r));

		setup().register_source(plib::make_unique_base<netlist::source_t,
				netlist::source_file_t>(setup(), filename));
		setup().include(name);
		log_setup(logs);

		// start devices
		this->start();
		// reset
		this->reset();
	}

	void log_setup(const std::vector<pstring> &logs)
	{
		log().debug("Creating dynamic logs ...\n");
		for (auto & log : logs)
		{
			pstring name = "log_" + log;
			/*netlist_device_t *nc = */ setup().register_dev("LOG", name);
			setup().register_link(name + ".I", log);
		}
	}

protected:

	void vlog(const plib::plog_level &l, const pstring &ls) const override;

private:
};

void netlist_tool_t::vlog(const plib::plog_level &l, const pstring &ls) const
{
	pstring err = plib::pfmt("{}: {}\n")(l.name())(ls.c_str());
	// FIXME: ...
	//pout("{}", err);
	if (l == plib::plog_level::FATAL)
		throw netlist::nl_exception(err);
}


struct input_t
{
	input_t(const netlist::setup_t &setup, const pstring &line)
	{
		char buf[400];
		double t;
		int e = sscanf(line.c_str(), "%lf,%[^,],%lf", &t, buf, &m_value);
		if (e != 3)
			throw netlist::nl_exception(plib::pfmt("error {1} scanning line {2}\n")(e)(line));
		m_time = netlist::netlist_time::from_double(t);
		m_param = setup.find_param(pstring(buf, pstring::UTF8), true);
	}

	void setparam()
	{
		switch (m_param->param_type())
		{
			case netlist::param_t::STRING:
			case netlist::param_t::POINTER:
				throw netlist::nl_exception(plib::pfmt("param {1} is not numeric\n")(m_param->name()));
			case netlist::param_t::DOUBLE:
				static_cast<netlist::param_double_t*>(m_param)->setTo(m_value);
				break;
			case netlist::param_t::INTEGER:
				static_cast<netlist::param_int_t*>(m_param)->setTo(static_cast<int>(m_value));
				break;
			case netlist::param_t::LOGIC:
				static_cast<netlist::param_logic_t*>(m_param)->setTo(static_cast<bool>(m_value));
				break;
		}
	}

	netlist::netlist_time m_time;
	netlist::param_t *m_param;
	double m_value;
};

static std::vector<input_t> read_input(const netlist::setup_t &setup, pstring fname)
{
	std::vector<input_t> ret;
	if (fname != "")
	{
		plib::pifilestream f(fname);
		plib::putf8_reader r(f);
		pstring l;
		while (r.readline(l))
		{
			if (l != "")
			{
				input_t inp(setup, l);
				ret.push_back(inp);
			}
		}
	}
	return ret;
}

void tool_app_t::run()
{
	plib::chrono::timer<plib::chrono::system_ticks> t;
	t.start();

	netlist_tool_t nt("netlist");
	//plib::perftime_t<plib::exact_ticks> t;

	nt.init();

	if (!opt_verb())
		nt.log().verbose.set_enabled(false);
	if (opt_quiet())
		nt.log().warning.set_enabled(false);

	nt.read_netlist(opt_file(), opt_name(),
			opt_logs(),
			opt_defines(), opt_rfolders());

	std::vector<input_t> inps = read_input(nt.setup(), opt_inp());

	double ttr = opt_ttr();
	t.stop();

	pout("startup time ==> {1:5.3f}\n", t.as_seconds() );
	pout("runnning ...\n");

	t.reset();
	t.start();

	unsigned pos = 0;
	netlist::netlist_time nlt = netlist::netlist_time::zero();

	while (pos < inps.size() && inps[pos].m_time < netlist::netlist_time::from_double(ttr))
	{
		nt.process_queue(inps[pos].m_time - nlt);
		inps[pos].setparam();
		nlt = inps[pos].m_time;
		pos++;
	}
	nt.process_queue(netlist::netlist_time::from_double(ttr) - nlt);
	nt.stop();

	t.stop();

	double emutime = t.as_seconds();
	pout("{1:f} seconds emulation took {2:f} real time ==> {3:5.2f}%\n", ttr, emutime, ttr/emutime*100.0);
}

void tool_app_t::static_compile()
{
	netlist_tool_t nt("netlist");

	nt.init();

	nt.log().verbose.set_enabled(false);
	nt.log().warning.set_enabled(false);

	nt.read_netlist(opt_file(), opt_name(),
			opt_logs(),
			opt_defines(), opt_rfolders());

	plib::putf8_writer w(pout_strm);
	std::map<pstring, pstring> mp;

	nt.solver()->create_solver_code(mp);

	for (auto &e : mp)
	{
		w.write(e.second);
	}

	nt.stop();

}

void tool_app_t::mac_out(const pstring s, const bool cont)
{
	static const unsigned RIGHT = 72;
	if (cont)
	{
		unsigned adj = 0;
		for (auto x : s)
			adj += (x == '\t' ? 3 : 0);
		pout("{1}\\\n", s.rpad(" ", RIGHT-1-adj));
	}
	else
		pout("{1}\n", s);
}

void tool_app_t::cmac(const netlist::factory::element_t *e)
{
	auto v = plib::psplit(e->param_desc(), ",");
	pstring vs;
	for (auto s : v)
		vs += ", p" + s.replace("+","").replace(".","_");
	mac_out("#define " + e->name() + "(name" + vs + ")");
	mac_out("\tNET_REGISTER_DEV(" + e->name() +", name)");

	for (auto s : v)
	{
		pstring r(s.replace("+","").replace(".","_"));
		if (s.startsWith("+"))
			mac_out("\tNET_CONNECT(name, " + r + ", p" + r + ")");
		else
			mac_out("\tNETDEV_PARAMI(name, " + r + ", p" + r + ")");
	}
	mac_out("", false);
}

void tool_app_t::mac(const netlist::factory::element_t *e)
{
	auto v = plib::psplit(e->param_desc(), ",");
	pstring vs;
	for (auto s : v)
	{
		vs += ", " + s.replace("+","").replace(".","_");
	}
	pout("{1}(name{2})\n", e->name(), vs);
	if (v.size() > 0)
	{
		pout("/*\n");
		for (auto s : v)
		{
			pstring r(s.replace("+","").replace(".","_"));
			if (s.startsWith("+"))
				pout("{1:10}: Terminal\n",r);
			else
				pout("{1:10}: Parameter\n", r);
		}
		pout("*/\n");
	}
}

void tool_app_t::create_header()
{
	netlist_tool_t nt("netlist");

	nt.init();

	nt.log().verbose.set_enabled(false);
	nt.log().warning.set_enabled(false);

	nt.setup().register_source(plib::make_unique_base<netlist::source_t,
			netlist::source_proc_t>(nt.setup(), "dummy", &netlist_dummy));
	nt.setup().include("dummy");

	pout("// license:GPL-2.0+\n");
	pout("// copyright-holders:Couriersud\n");
	pout("#ifndef NLD_DEVINC_H\n");
	pout("#define NLD_DEVINC_H\n");
	pout("\n");
	pout("#include \"nl_setup.h\"\n");
	pout("#ifndef __PLIB_PREPROCESSOR__\n");
	pout("\n");
	pout("/* ----------------------------------------------------------------------------\n");
	pout(" *  Netlist Macros\n");
	pout(" * ---------------------------------------------------------------------------*/\n");
	pout("\n");

	pstring last_source("");

	for (auto &e : nt.setup().factory())
	{
		if (last_source != e->sourcefile())
		{
			last_source = e->sourcefile();
			pout("{1}\n", pstring("// ").rpad("-", 72));
			pout("{1}\n", pstring("// Source: ").cat(e->sourcefile().replace("../","")));
			pout("{1}\n", pstring("// ").rpad("-", 72));
		}
		cmac(e.get());
	}
	pout("#endif // __PLIB_PREPROCESSOR__\n");
	pout("#endif\n");
	nt.stop();

}

void tool_app_t::create_docheader()
{
	netlist_tool_t nt("netlist");

	nt.init();

	nt.log().verbose.set_enabled(false);
	nt.log().warning.set_enabled(false);

	nt.setup().register_source(plib::make_unique_base<netlist::source_t,
			netlist::source_proc_t>(nt.setup(), "dummy", &netlist_dummy));
	nt.setup().include("dummy");

	std::vector<pstring> devs;
	for (auto &e : nt.setup().factory())
		devs.push_back(e->name());
	std::sort(devs.begin(), devs.end(), [&](pstring &a, pstring &b) { return a < b; });

	pout("// license:GPL-2.0+\n");
	pout("// copyright-holders:Couriersud\n");
	pout("/* ----------------------------------------------------------------------------\n");
	pout(" *  Automatically created file. DO NOT MODIFY.\n");
	pout(" * ---------------------------------------------------------------------------*/\n");
	pout("/*!\n");
	pout(" * \\page devices Devices\n");
	pout(" *\n");
	pout(" * Below is a list of all the devices currently known to the system ...\n");
	pout(" *\n");

	for (auto &s : devs)
		pout(" *         - \\subpage {1}\n", s);

	pout(" *\n");

	for (auto &e : nt.setup().factory())
	{
		pout("//! [{1} csynopsis]\n", e->name());
		cmac(e.get());
		pout("//! [{1} csynopsis]\n", e->name());
		pout("//! [{1} synopsis]\n", e->name());
		mac(e.get());
		pout("//! [{1} synopsis]\n", e->name());
	}
	nt.stop();
}


/*-------------------------------------------------
    listdevices - list all known devices
-------------------------------------------------*/

void tool_app_t::listdevices()
{
	netlist_tool_t nt("netlist");
	nt.init();
	if (!opt_verb())
		nt.log().verbose.set_enabled(false);
	if (opt_quiet())
		nt.log().warning.set_enabled(false);

	netlist::factory::list_t &list = nt.setup().factory();

	nt.setup().register_source(plib::make_unique_base<netlist::source_t,
			netlist::source_proc_t>(nt.setup(), "dummy", &netlist_dummy));
	nt.setup().include("dummy");


	nt.start();

	std::vector<plib::owned_ptr<netlist::core_device_t>> devs;

	for (auto & f : list)
	{
		pstring out = plib::pfmt("{1} {2}(<id>")(f->classname(),"-20")(f->name());
		std::vector<pstring> terms;

		f->macro_actions(nt.setup().netlist(), f->name() + "_lc");
		auto d = f->Create(nt.setup().netlist(), f->name() + "_lc");
		// get the list of terminals ...

		for (auto & t : nt.setup().m_terminals)
		{
			if (t.second->name().startsWith(d->name()))
			{
				pstring tn(t.second->name().substr(d->name().len()+1));
				if (tn.find(".") == tn.end())
					terms.push_back(tn);
			}
		}

		for (auto & t : nt.setup().m_alias)
		{
			if (t.first.startsWith(d->name()))
			{
				pstring tn(t.first.substr(d->name().len()+1));
				//printf("\t%s %s %s\n", t.first.c_str(), t.second.c_str(), tn.c_str());
				if (tn.find(".") == tn.end())
				{
					terms.push_back(tn);
					pstring resolved = nt.setup().resolve_alias(t.first);
					//printf("\t%s %s %s\n", t.first.c_str(), t.second.c_str(), resolved.c_str());
					if (resolved != t.first)
					{
						auto found = std::find(terms.begin(), terms.end(), resolved.substr(d->name().len()+1));
						if (found!=terms.end())
							terms.erase(found);
					}
				}
			}
		}

		out += "," + f->param_desc();
		for (auto p : plib::psplit(f->param_desc(),",") )
		{
			if (p.startsWith("+"))
			{
				plib::container::remove(terms, p.substr(1));
			}
		}
		out += ")";
		printf("%s\n", out.c_str());
		if (terms.size() > 0)
		{
			pstring t = "";
			for (auto & j : terms)
				t += "," + j;
			printf("\tTerminals: %s\n", t.substr(1).c_str());
		}
		devs.push_back(std::move(d));
	}
}



/*-------------------------------------------------
    main - primary entry point
-------------------------------------------------*/

#if 0
static const pstring pmf_verbose[] =
{
	"NL_PMF_TYPE_VIRTUAL",
	"NL_PMF_TYPE_GNUC_PMF",
	"NL_PMF_TYPE_GNUC_PMF_CONV",
	"NL_PMF_TYPE_INTERNAL"
};
#endif

pstring tool_app_t::usage()
{
	return help(
			"nltool serves as the Swiss Army knife to run, test and convert netlists.",
			"nltool [options]");
}

int tool_app_t::execute()
{
	tool_app_t opts;

	/* make SIGFPE actually deliver signals on supoorted platforms */
	plib::fpsignalenabler::global_enable(true);
	plib::fpsignalenabler sigen(plib::FP_ALL & ~plib::FP_INEXACT & ~plib::FP_UNDERFLOW);

	//perr("{}", "WARNING: This is Work In Progress! - It may fail anytime\n");
	//perr("Update dispatching using method {}\n", pmf_verbose[NL_PMF_TYPE]);
	//printf("test2 %f\n", std::exp(-14362.38064713));

	if (opt_help())
	{
		pout(usage());
		return 0;
	}

	if (opt_version())
	{
		pout(
			"nltool (netlist) 0.1\n"
			"Copyright (C) 2017 Couriersud\n"
			"License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>.\n"
			"This is free software: you are free to change and redistribute it.\n"
			"There is NO WARRANTY, to the extent permitted by law.\n\n"
			"Written by Couriersud.\n");
		return 0;
	}

	try
	{
		pstring cmd = opt_cmd();
		if (cmd == "listdevices")
			listdevices();
		else if (cmd == "run")
			run();
		else if (cmd == "static")
			static_compile();
		else if (cmd == "header")
			create_header();
		else if (cmd == "docheader")
			create_docheader();
		else if (cmd == "convert")
		{
			pstring contents;
			plib::postringstream ostrm;
			if (opt_file() == "-")
			{
				plib::pstdin f;
				ostrm.write(f);
			}
			else
			{
				plib::pifilestream f(opt_file());
				ostrm.write(f);
			}
			contents = ostrm.str();

			pstring result;
			if (opt_type().equals("spice"))
			{
				nl_convert_spice_t c;
				c.convert(contents);
				result = c.result();
			}
			else if (opt_type().equals("eagle"))
			{
				nl_convert_eagle_t c;
				c.convert(contents);
				result = c.result();
			}
			else if (opt_type().equals("rinf"))
			{
				nl_convert_rinf_t c;
				c.convert(contents);
				result = c.result();
			}
			/* present result */
			pout.write(result);
		}
		else
		{
			perr("Unknown command {}\n", cmd.c_str());
			//FIXME: usage_short
			perr(usage());
			return 1;
		}
	}
	catch (netlist::nl_exception &e)
	{
		perr("Netlist exception caught: {}\n", e.text());
	}
	catch (plib::pexception &e)
	{
		perr("plib exception caught: {}\n", e.text());
	}

	pstring::resetmem();
	return 0;
}

PMAIN(tool_app_t)

//plib::app *appconstructor() { return new tool_app_t(); }
