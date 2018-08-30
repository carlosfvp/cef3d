//----------------------------------------------------------------------------
// Cef3D
// Copyright (C) 2017 arkenthera
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// https://github.com/arkenthera/cef3d
// Main.cc
// Date: 18.04.2017
//---------------------------------------------------------------------------

#include "include/cef_render_handler.h"
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

#include <Cef3DPCH.h>

#include <iostream>

class MyV8Handler : public CefV8Handler {
public:
	MyV8Handler() {}

	virtual bool Execute(const CefString& name,
		CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception) OVERRIDE {
		if (name == "test") {
			// Return my string value.
			retval = CefV8Value::CreateString("My Value!");
			return true;
		}

		if (name == "test2") {
			// Return my string value.
			retval = CefV8Value::CreateString("My Value2!");
			return true;
		}

		// Function does not exist.
		return false;
	}

	// Provide the reference counting implementation for this class.
	IMPLEMENT_REFCOUNTING(MyV8Handler);
};

class MyDelegate : public Cef3D::Cef3DRendererDelegate
{
	virtual void OnWebKitInitialized(CefRefPtr<Cef3D::Cef3DRenderer> app)
	{
		std::string extensionCode = Cef3D::Cef3DFileSystem::Get().ReadFile("D:/cef3d/Cef3D/Binaries/Win64/test/lib.js");

		// Create an instance of my CefV8Handler object.
		CefRefPtr<CefV8Handler> handler = new MyV8Handler();

		// Register the extension.
		CefRegisterExtension("v8/test", extensionCode, handler);
	}

	virtual bool OnProcessMessageReceived(
		CefRefPtr<Cef3D::Cef3DRenderer> app,
		CefRefPtr<CefBrowser> browser,
		CefProcessId source_process,
		CefRefPtr<CefProcessMessage> message)
	{

		return true;
	}

	virtual void OnContextCreated(CefRefPtr<Cef3D::Cef3DRenderer> app,
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefV8Context> context)
	{
		//CefRefPtr<CefV8Handler> handler = new MyV8Handler();

		// Create the "myfunc" function.
		//CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction("test", handler);

		// Add the "myfunc" function to the "window" object.
		//context->GetGlobal()->SetValue("test", func, V8_PROPERTY_ATTRIBUTE_NONE);


		
		
		//frame->ExecuteJavaScript("alert('ExecuteJavaScript works!');", frame->GetURL(), 0);
	}
	IMPLEMENT_REFCOUNTING(MyDelegate);
};

int main(int argc,char* argv[])
{
#ifdef _WIN32
	CefMainArgs main_args(GetModuleHandle(NULL));
#else
	CefMainArgs main_args(argc,argv);
#endif

	CefRefPtr<Cef3D::Cef3DRenderer> app(new Cef3D::Cef3DRenderer());

	CefRefPtr<Cef3D::Cef3DRendererDelegate> del(new MyDelegate());

	app->RegisterDelegate(del);

	return CefExecuteProcess(main_args,app.get(),nullptr);
}