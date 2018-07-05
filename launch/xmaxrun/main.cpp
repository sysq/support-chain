/**
 *  @file
 *  @copyright defined in xmax/LICENSE
 */
#include <blockchain_exceptions.hpp>
#include <application.hpp>



#include <fc/log/logger_config.hpp>
#include <fc/log/appender.hpp>
#include <fc/exception/exception.hpp>
#include <fc/filesystem.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <blockchain_plugin.hpp>
#include <blockbuilder_plugin.hpp>
#include <chainhttp_plugin.hpp>
#include <chainnet_plugin.hpp>
#include <contractutil_plugin.hpp>
#include <mongodb_plugin.hpp>

#ifdef USE_V8
#include <jsvm_xmax.h>
#endif

using namespace Baseapp;

namespace fc {
    std::unordered_map<std::string,appender::ptr>& get_appender_map();
}

void initialize_logging()
{
    auto config_path = app().get_logging_conf();
    if(fc::exists(config_path))
        fc::configure_logging(config_path);
    for(auto iter : fc::get_appender_map())
        iter.second->initialize(app().get_io_service());
}

void regist_plugins()
{
    app().register_plugin<Xmaxplatform::chaindata_plugin>();
    app().register_plugin<Xmaxplatform::chainhttp_plugin>();
	app().register_plugin<Xmaxplatform::chainnet_plugin>();
    app().register_plugin<Xmaxplatform::blockchain_plugin>();
    app().register_plugin<Xmaxplatform::blockbuilder_plugin>();
	app().register_plugin<Xmaxplatform::contractutil_plugin>();
	app().register_plugin<Xmaxplatform::mongodb_plugin>();
}

int main(int argc, char** argv)
{
   //try {


       regist_plugins();

#ifdef USE_V8
	   Xmaxplatform::Chain::jsvm_xmax& jsvmInstance = Xmaxplatform::Chain::jsvm_xmax::get();

	   jsvmInstance.V8EnvInit();
	   {
		   v8::Isolate* pIsolate = jsvmInstance.V8GetIsolate();
		   v8::HandleScope handle_scope(pIsolate);
		   v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(pIsolate);
		   jsvmInstance.V8SetupGlobalObjTemplate(&global);
#endif

		   if (!app().init<Xmaxplatform::blockchain_plugin>(argc, argv))
			   return -1;
		   initialize_logging();
		   ilog("xmaxrun init success!");
		   app().startup();
		   app().exec();
		   //} catch (const fc::exception& e) {
		   //   elog("${e}", ("e",e.to_detail_string()));
		   //} catch (const boost::exception& e) {
		   //   elog("${e}", ("e",boost::diagnostic_information(e)));
		   //} catch (const std::exception& e) {
		   //   elog("${e}", ("e",e.what()));
		   //} catch (...) {
		   //   elog("unknown exception");
		   //}
#ifdef USE_V8
	   }
	   jsvmInstance.V8EnvDiscard();
#endif
   return 0;
}
