#ifdef USE_V8

#include "jsvm_xmax.h"
/**
*  @file
*  @copyright defined in xmax/LICENSE
*/
#include <boost/function.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <jsvm_xmax.h>
#include <chain_xmax.hpp>
#include <vm_native_interface.hpp>
#include <objects/key_value_object.hpp>
#include <objects/account_object.hpp>
#include <objects/xmx_token_object.hpp>
#include <abi_serializer.hpp>
#include <chrono>
#include <boost/lexical_cast.hpp>
#include <fc/utf8.hpp>
#if WIN32
#include <fc/int128.hpp>
#endif

#include "jsvm_util.h"
#include "jsvm_objbind/Int64Bind.h"


using namespace v8;
namespace Xmaxplatform {

	namespace Chain {

		jsvm_xmax::jsvm_xmax() 
		:m_ContextMaxScriptCount(9999)
		,m_CurrentScriptCount(0)
		{

		}

		jsvm_xmax::key_type jsvm_xmax::to_key_type(const Basetypes::type_name& type_name)
		{
			if ("str" == type_name)
				return str;
			if ("i64" == type_name)
				return i64;
			if ("i128i128" == type_name)
				return i128i128;
			if ("i64i64i64" == type_name)
				return i64i64i64;

			return invalid_key_type;
		}

		std::string jsvm_xmax::to_type_name(key_type key_type)
		{
			switch (key_type)
			{
			case str:
				return "str";
			case i64:
				return "i64";
			case i128i128:
				return "i128i128";
			case i64i64i64:
				return "i64i64i64";
			default:
				return std::string("<invalid key type - ") + boost::lexical_cast<std::string>(int(key_type)) + ">";
			}
		}

		jsvm_xmax& jsvm_xmax::get() {
			static jsvm_xmax*  jsvm = nullptr;
			if (!jsvm)
			{
				//

				jsvm = new jsvm_xmax();
				//jsvm->InitV8();
			}
			return *jsvm;
	  }

#ifdef NDEBUG
		const int CHECKTIME_LIMIT = 3000;
#else
		const int CHECKTIME_LIMIT = 36000;
#endif

		void jsvm_xmax::init(message_context_xmax& c)
		{
			try {
				current_validate_context = &c;
				current_precondition_context = &c;
				current_message_context = &c;
				checktime_limit = CHECKTIME_LIMIT;

				load(c.code, c.db);
				const auto& recipient = c.db.get<account_object, by_name>(c.code);
				
				vm_onInit((char*)recipient.code.data());

			} FC_CAPTURE_AND_RETHROW()
		}


		void jsvm_xmax::V8SetupGlobalObjTemplate(v8::Local<v8::ObjectTemplate>* pGlobalTemp)
		{
			m_pGlobalObjectTemplate = pGlobalTemp;
			BindJsFoos(m_pIsolate, *m_pGlobalObjectTemplate, FooBind::GetBindFoos(m_pIsolate));
			SetupV8i64ObjectToJs(m_pIsolate, *m_pGlobalObjectTemplate);
		}

		void jsvm_xmax::V8EnvInit()
		{
			V8::InitializeICUDefaultLocation("");
			V8::InitializeExternalStartupData("");
			m_pPlatform = platform::CreateDefaultPlatform();
			V8::InitializePlatform(m_pPlatform);
			V8::Initialize();

			
			m_CreateParams.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
			m_pIsolate = v8::Isolate::New(m_CreateParams);
		}


		void jsvm_xmax::V8EnvDiscard()
		{
			if (!m_CurrentContext.IsEmpty())
			{
				m_CurrentContext.Get(m_pIsolate)->Exit();
			}
			m_pIsolate->Dispose();
			v8::V8::Dispose();
			v8::V8::ShutdownPlatform();
			delete m_pPlatform;
			delete m_CreateParams.array_buffer_allocator;
		}

		void  jsvm_xmax::vm_apply(char* code) {
		
			Handle<String> js_func_name = String::NewFromUtf8(current_state->current_isolate, "init", NewStringType::kNormal).ToLocalChecked();
			Handle<v8::Value>  js_func_val = current_state->current_context->Global()->Get(js_func_name);
			if (!js_func_val->IsFunction())
			{
				std::cerr << "Can't find js funcion init()" << std::endl;
			}
			Handle<Function> js_func = Handle<Function>::Cast(js_func_val);

			Handle<v8::Value> hResult = js_func->Call(current_state->current_context->Global(), 0, nullptr);
		
		}

		void vm_init( const Local<ObjectTemplate>& global, const Local<Context>& context, const Context::Scope& ctxScope)
		{
			Isolate* isolate = jsvm_xmax::get().current_state->current_isolate;

			message_context_xmax& msg_contxt = *jsvm_xmax::get().current_message_context;
			const auto& recipient = msg_contxt.db.get<account_object, by_name>(msg_contxt.code);

			CompileJsCode(isolate, context, (char*)recipient.code.data());
			
			message_context_xmax & validate_context = *jsvm_xmax::get().current_validate_context;
 			Handle<v8::Value> params[2];
 			params[0] = I64Cpp2JS(isolate, context, uint64_t(validate_context.msg.code));
 			params[1] = I64Cpp2JS(isolate, context, uint64_t(validate_context.msg.type));
			CallJsFoo(isolate, context,"init", 0, NULL);
		}

		void  jsvm_xmax::vm_onInit(char* code)
		{
			try {
				namespace  ph = std::placeholders;

				EnterJsContext(m_pIsolate,*m_pGlobalObjectTemplate, std::bind(&vm_init, ph::_1, ph::_2, ph::_3));

			}
			catch (const Runtime::Exception& e) {
				edump((std::string(describeExceptionCause(e.cause))));
				edump((e.callStack));
				throw;
			}
		}

		void jsvm_xmax::load(const account_name& name, const Basechain::database& db)
		{
			const auto& recipient = db.get<account_object, by_name>(name);
			

			auto& state = instances[name];
			if (state.code_version != recipient.code_version) {
 				
 				state.code_version = fc::sha256();
				state.table_key_types.clear();

				if (m_CurrentScriptCount>m_ContextMaxScriptCount)
				{
					m_CurrentScriptCount=0;
					if (!m_CurrentContext.IsEmpty())
					{
						ExitJsContext(m_pIsolate, m_CurrentContext);
					//	m_CurrentContext = EnterJsContext(m_pIsolate, *m_pGlobalObjectTemplate);
					}

					message_context_xmax& msg_contxt = *jsvm_xmax::get().current_message_context;
					const auto& recipient = msg_contxt.db.get<account_object, by_name>(msg_contxt.code);
					CompileJsCode(m_pIsolate, m_CurrentContext.Get(m_pIsolate), (char*)recipient.code.data());

				}
				else
				{
					if (m_CurrentContext.IsEmpty())
					{
						m_CurrentContext = EnterJsContext(m_pIsolate, *m_pGlobalObjectTemplate);
					}

					message_context_xmax& msg_contxt = *jsvm_xmax::get().current_message_context;
					const auto& recipient = msg_contxt.db.get<account_object, by_name>(msg_contxt.code);

					CompileJsCode(m_pIsolate, m_CurrentContext.Get(m_pIsolate), (char*)recipient.code.data());
					m_CurrentScriptCount++;
				}
				
				try
				{
					const auto init_time = fc::time_point::now();

					//init abi
					Basetypes::abi abi;
					if (Basetypes::abi_serializer::to_abi(recipient.abi, abi))
					{
						state.tables_fixed = true;
						for (auto& table : abi.tables)
						{
							const auto key_type = to_key_type(table.index_type);
							if (key_type == invalid_key_type)
								throw Serialization::FatalSerializationException("For code \"" + (std::string)name + "\" index_type of \"" +
									table.index_type + "\" referenced but not supported");

							state.table_key_types.emplace(std::make_pair(table.table_name, key_type));
						}
					}
				}
				catch (Serialization::FatalSerializationException exception)
				{
					std::cerr << "Error:" << std::endl;
					std::cerr << exception.message << std::endl;
					throw;
				}

				current_state = &state;
				table_key_types = &state.table_key_types;
				tables_fixed = state.tables_fixed;
				table_storage = 0;
			}
		}

	}
}

#endif

