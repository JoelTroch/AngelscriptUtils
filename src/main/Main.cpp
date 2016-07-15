#include <iostream>

#include <angelscript.h>

#include "Angelscript/CASManager.h"
#include "Angelscript/CASHook.h"
#include "Angelscript/CASModule.h"
#include "Angelscript/IASModuleBuilder.h"

#include "Angelscript/add_on/scriptbuilder.h"
#include "Angelscript/add_on/scriptstdstring.h"
#include "Angelscript/add_on/scriptarray.h"
#include "Angelscript/add_on/scriptdictionary.h"
#include "Angelscript/add_on/scriptany.h"

#include "Angelscript/ScriptAPI/CASScheduler.h"
#include "Angelscript/ScriptAPI/Reflection/ASReflection.h"

#include "Angelscript/util/ASUtil.h"
#include "Angelscript/util/CASRefPtr.h"

#include "Angelscript/wrapper/ASCallable.h"
#include "Angelscript/wrapper/CASContext.h"

namespace ModuleAccessMask
{
/**
*	Access masks for modules.
*/
enum ModuleAccessMask
{
	/**
	*	No access.
	*/
	NONE		= 0,

	/**
	*	Shared API.
	*/
	SHARED		= 1 << 0,

	/**
	*	Map script specific.
	*/
	MAPSCRIPT	= SHARED | 1 << 1,

	/**
	*	Plugin script specific.
	*/
	PLUGIN		= SHARED | 1 << 2,

	/**
	*	All scripts.
	*/
	ALL			= SHARED | MAPSCRIPT | PLUGIN
};
}

void Print( const std::string& szString )
{
	std::cout << szString;
}

/*
*	A hook to test out the hook system.
*	Stops as soon as it's handled.
*	Can be hooked by calling g_Hooks.HookFunction( Hooks::Main, @MainHook( ... ) );
*/
CASHook hook( "Main", "const string& in", "", ModuleAccessMask::ALL, HookStopMode::ON_HANDLED );

/**
*	Builder for the test script.
*/
class CASTestModuleBuilder : public IASModuleBuilder
{
public:

	bool AddScripts( CScriptBuilder& builder ) override
	{
		//By using a handle this can be changed, but since there are no other instances, it can only be made null.
		//TODO: figure out a better way.
		auto result = builder.AddSectionFromMemory( 
			"__Globals", 
			"CScheduler@ Scheduler;" );

		if( result < 0 )
			return false;

		return builder.AddSectionFromFile( "scripts/test.as" ) >= 0;
	}

	bool PostBuild( const bool bSuccess, CASModule* pModule ) override
	{
		if( !bSuccess )
			return false;

		auto& scriptModule = *pModule->GetModule();

		//Set the scheduler instance.
		if( !as::SetGlobalByName( scriptModule, "Scheduler", pModule->GetScheduler() ) )
			return false;

		return true;
	}
};

int main( int iArgc, char* pszArgV[] )
{
	std::cout << "Hello World!" << std::endl;

	CASManager manager;

	if( manager.Initialize() )
	{
		//Register the API.
		RegisterStdString( manager.GetEngine() );
		RegisterScriptArray( manager.GetEngine(), true );
		RegisterScriptDictionary( manager.GetEngine() );
		RegisterScriptAny( manager.GetEngine() );
		RegisterScriptScheduler( manager.GetEngine() );
		RegisterScriptReflection( *manager.GetEngine() );

		manager.GetEngine()->RegisterTypedef( "size_t", "uint32" );

		//Add a hook. Scripts will be able to hook these, when it's invoked by C++ code all hooked functions are called.
		manager.GetHookManager().AddHook( &hook );

		//Registers all hooks. One-time event that happens on startup.
		manager.GetHookManager().RegisterHooks( *manager.GetEngine() );

		//Printing function.
		manager.GetEngine()->RegisterGlobalFunction( "void Print(const string& in szString)", asFUNCTION( Print ), asCALL_CDECL );

		//Create some module types.

		//Map scripts are per-map scripts that always have their hooks executed before any other module.
		manager.GetModuleManager().AddDescriptor( "MapScript", ModuleAccessMask::MAPSCRIPT, as::ModulePriority::HIGHEST );

		//Plugins are persistent scripts that can keep running after map changes.
		manager.GetModuleManager().AddDescriptor( "Plugin", ModuleAccessMask::PLUGIN );

		//Make a map script.
		CASTestModuleBuilder builder;

		auto pModule = manager.GetModuleManager().BuildModule( "MapScript", "MapModule", builder );

		if( pModule )
		{
			//Call the main function.
			if( auto pFunction = pModule->GetModule()->GetFunctionByName( "main" ) )
			{
				//Add main as a hook.
				hook.AddFunction( pFunction );

				std::string szString = "Hello World!\n";

				//Note: main takes a const string& in, so pass the address here. References are handled as pointers.
				as::Call( pFunction, &szString );

				//Call the hook.
				hook.Call( CallFlag::NONE, &szString );
			}

			//Call a function using the different function call helpers.
			if( auto pFunction = pModule->GetModule()->GetFunctionByName( "NoArgs" ) )
			{
				{
					//Test the smart pointer.
					CASRefPtr<asIScriptFunction> func;

					CASRefPtr<asIScriptFunction> func2( pFunction );

					func = func2;

					func = std::move( func2 );

					CASRefPtr<asIScriptFunction> func3( func );

					CASRefPtr<asIScriptFunction> func4( std::move( func ) );

					func.Set( pFunction );

					auto pPtr = func.Get();
				}

				//Regular varargs.
				as::Call( pFunction );
				//Argument list.
				as::CallArgs( pFunction, CASArguments() );

				auto pFunc = [ = ]( CallFlags_t flags, ... )
				{
					va_list list;

					va_start( list, flags );

					as::VCall( flags, pFunction, list );

					va_end( list );
				};

				//va_list version.
				pFunc( CallFlag::NONE );
			}

			//Test the scheduler.
			pModule->GetScheduler()->Think( 10 );

			//Get the parameter types. Angelscript's type info support isn't complete yet, so not all types have an asITypeInfo instance yet.
			/*
			if( auto pFunc2 = pModule->GetModule()->GetFunctionByName( "Function" ) )
			{
				for( asUINT uiIndex = 0; uiIndex < pFunc2->GetParamCount(); ++uiIndex )
				{
					int iTypeId;
					const char* pszName;
					pFunc2->GetParam( uiIndex, &iTypeId, nullptr, &pszName );

					std::cout << "Parameter " << uiIndex << ": " << pszName << std::endl;
				
					if( auto pType = manager.GetEngine()->GetTypeInfoById( iTypeId ) )
					{
						std::cout << pType->GetNamespace() << "::" << pType->GetName() << std::endl;

						asDWORD uiFlags = pType->GetFlags();

						if( uiFlags & asOBJ_VALUE )
							std::cout << "Value" << std::endl;

						if( uiFlags & asOBJ_REF )
							std::cout << "Ref" << std::endl;

						if( uiFlags & asOBJ_ENUM )
							std::cout << "Enum" << std::endl;

						if( uiFlags & asOBJ_FUNCDEF )
							std::cout << "Funcdef" << std::endl;

						if( uiFlags & asOBJ_POD )
							std::cout << "POD" << std::endl;

						if( uiFlags & asOBJ_TYPEDEF )
							std::cout << "Typedef" << std::endl;
					}
					else //Only primitive types don't have type info right now.
						std::cout << "No type info" << std::endl;
				}
			}
			*/

			//Remove the module.
			manager.GetModuleManager().RemoveModule( pModule );
		}
	}

	//Shut down the Angelscript engine, frees all resources.
	manager.Shutdown();

	//Wait for input.
	getchar();

	return 0;
}