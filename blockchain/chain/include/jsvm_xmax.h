#pragma once
#include <blockchain_exceptions.hpp>
#include <message_xmax.hpp>
#include <message_context_xmax.hpp>

#include <libplatform/libplatform.h>
#include <v8.h>

#include "jsvm_util.h"

namespace Xmaxplatform {
	namespace Chain {
		class jsvm_xmax {
		public:
			enum key_type {
				str,
				i64,
				i128i128,
				i64i64i64,
				invalid_key_type
			};
			typedef map<name, key_type> TableMap;
			struct ModuleState {
				//one contract one context map.
				v8::Local<v8::Script>			 current_script;		
				PersistentCpyableContext           current_context;
			
				fc::sha256               code_version;
				TableMap                 table_key_types;
				bool                     tables_fixed = false;
			};

			static jsvm_xmax& get();

			void V8SetInstructionCallBack(const char* name,void* foo);
			void V8SetupGlobalObjTemplate(v8::Local<v8::ObjectTemplate>* pGlobalTemp);
			void V8EnvInit();
			void V8EnvDiscard();
			v8::Isolate* V8GetIsolate();
			
			void StoreInstruction(int ins);
			void CleanInstruction();

			int GetExecutedInsCount();
			std::list<int>& GetExecutedIns();

			void init(message_context_xmax& c);
			void apply(message_context_xmax& c, uint32_t execution_time, bool received_block);
			void validate(message_context_xmax& c);
			void precondition(message_context_xmax& c);

			int64_t current_execution_time();

			void checktime();

			static key_type to_key_type(const Basetypes::type_name& type_name);
			static std::string to_type_name(key_type key_type);

			message_context_xmax*       current_message_context = nullptr;
			message_context_xmax*       current_validate_context = nullptr;
			message_context_xmax*       current_precondition_context = nullptr;

// 			Runtime::MemoryInstance*   current_memory = nullptr;
// 			Runtime::ModuleInstance*   current_module = nullptr;
// 			wasm_memory*               current_memory_management = nullptr;
			ModuleState*               current_state = nullptr;
			TableMap*                  table_key_types = nullptr;
			bool                       tables_fixed = false;
			int64_t                    table_storage = 0;

			uint32_t                   checktime_limit = 0;

			int32_t                    per_code_account_max_db_limit_mbytes = Config::default_per_code_account_max_db_limit_mbytes;
			uint32_t                   row_overhead_db_limit_bytes = Config::default_row_overhead_db_limit_bytes;

		private:
			
			void load(const account_name& name, const Basechain::database& db);
			void  vm_validate();
			void  vm_precondition();
			void  vm_apply(char* code);
			void  vm_onInit(char* code);
			//U32   vm_pointer_to_offset(char*);



			map<account_name, ModuleState> instances;
			fc::time_point checktimeStart;

			v8::Isolate* m_pIsolate;
			v8::Local<v8::ObjectTemplate>* m_pGlobalObjectTemplate;

			int m_instructionCount;
			std::list<int> m_Intrunctions;

			v8::Isolate::CreateParams m_CreateParams;
			v8::Platform* m_pPlatform;


			jsvm_xmax();
		};
		inline v8::Isolate* jsvm_xmax::V8GetIsolate()
		{
			return m_pIsolate;
		}
		inline int jsvm_xmax::GetExecutedInsCount()
		{
			return m_instructionCount;
		}

		inline std::list<int>& jsvm_xmax::GetExecutedIns()
		{
			return m_Intrunctions;
		}

	}
}
