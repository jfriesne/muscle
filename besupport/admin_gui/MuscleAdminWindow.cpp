/*
 * Copyright 2006-2009, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */


#include "MuscleAdminWindow.h"

#include <Application.h>

#include "MuscleAdminView.h"
#include "constants.h"
#include "MuscleDaemon.h"

MuscleAdminWindow::MuscleAdminWindow()
	:
	BWindow(BRect(100, 150, 500, 300), "MuscleAdmin",
		B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_ASYNCHRONOUS_CONTROLS)
{
	BView* topView = new BView(Bounds(), NULL, B_FOLLOW_ALL, B_WILL_DRAW);
	topView->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(topView);

	topView->AddChild(new MuscleAdminReplicant(Bounds(), B_FOLLOW_ALL));
}


MuscleAdminWindow::~MuscleAdminWindow()
{
}


bool
MuscleAdminWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

void
MuscleAdminWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_PREF_BTN_DONE:
/*			PoorManWindow* win;
			PoorManServer* server;
			win = ((PoorManApplication*)be_app)->GetPoorManWindow();
			server = win->GetServer();

			PRINT(("Pref Window: sendDir CheckBox: %d\n",
				fSiteView->SendDirValue()));
			server->SetListDir(fSiteView->SendDirValue());
			win->SetDirListFlag(fSiteView->SendDirValue());
			PRINT(("Pref Window: indexFileName TextControl: %s\n",
				fSiteView->IndexFileName()));
			if (server->SetIndexName(fSiteView->IndexFileName()) == B_OK)
				win->SetIndexFileName(fSiteView->IndexFileName());
			PRINT(("Pref Window: webDir: %s\n", fSiteView->WebDir()));
			if (server->SetWebDir(fSiteView->WebDir()) == B_OK) {
				win->SetWebDir(fSiteView->WebDir());
				win->SetDirLabel(fSiteView->WebDir());
			}

			PRINT(("Pref Window: logConsole CheckBox: %d\n",
				fLoggingView->LogConsoleValue()));
			win->SetLogConsoleFlag(fLoggingView->LogConsoleValue());
			PRINT(("Pref Window: logFile CheckBox: %d\n",
				fLoggingView->LogFileValue()));
			win->SetLogFileFlag(fLoggingView->LogFileValue());
			PRINT(("Pref Window: logFileName: %s\n",
				fLoggingView->LogFileName()));
			win->SetLogPath(fLoggingView->LogFileName());

			PRINT(("Pref Window: MaxConnections Slider: %ld\n",
				fAdvancedView->MaxSimultaneousConnections()));
			server->SetMaxConns(fAdvancedView->MaxSimultaneousConnections());
			win->SetMaxConnections(
				(int16)fAdvancedView->MaxSimultaneousConnections());
*/
//			if (Lock())
				Quit();
			break;
		case MSG_PREF_BTN_CANCEL:
//			if (Lock())
				Quit();
			break;
		case MSG_PREF_SITE_BTN_SELECT:
		{
			// find it
			entry_ref appRef;
			status_t err = be_roster->FindApp(STR_MUSCLE_DEAMON_NAME, &appRef);

			if(err != B_OK)
				printf("Error %s\n", strerror(errno));
			else
				printf("OK %s\n", strerror(errno));


			// start it
//			team_id team;
//			const char* arg = "--addon-host";
			err = be_roster->Launch(
				&appRef
//				,1
//				,&arg
//				,&team
				);
			if(err != B_OK)
				printf("Error %s\n", strerror(errno));
			else
				printf("OK %s\n", strerror(errno));
//	if(err < B_OK)
//		return err;

//			printf("Run Server %s\n", STR_MUSCLE_DEAMON_NAME);
//			status_t err = be_roster->Launch(STR_MUSCLE_DEAMON_NAME);
			//printf("Has started\n");
//			if(err != B_OK)
//				printf("Error %s\n", err);
//			else
//				printf("OK %s\n", err);

//			if (be_roster->IsRunning(STR_MUSCLE_DEAMON_NAME)) {
//				printf("The server is running\n");
//			} else
//				printf("The server is NOT running\n");
//Start Server
/*			// Select the Web Directory, root directory to look in.
			fWebDirFilePanel->SetTarget(this);
			BMessage webDirSelectedMsg(MSG_FILE_PANEL_SELECT_WEB_DIR);
			fWebDirFilePanel->SetMessage(&webDirSelectedMsg);
			if (!fWebDirFilePanel->IsShowing())
				fWebDirFilePanel->Show();
*/			break;
		}
		default:
			BWindow::MessageReceived(message);
			break;
	}
}
