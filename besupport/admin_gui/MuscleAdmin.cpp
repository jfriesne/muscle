/*
 * Copyright 2006, 2013, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Fredrik Modéen
 *
 * Taken From Powerstatus
 *
 */


#include "MuscleAdmin.h"
#include "MuscleAdminWindow.h"

#include <Alert.h>
#include <Application.h>
//#include <Catalog.h>
#include <Deskbar.h>
#include <Entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//#undef B_TRANSLATION_CONTEXT
//#define B_TRANSLATION_CONTEXT "MuscleAdmin"


class MuscleAdmin : public BApplication {
	public:
		MuscleAdmin();
		virtual	~MuscleAdmin();

		virtual	void	ReadyToRun();
		virtual void	AboutRequested();
};


const char* kSignature = "application/x-vnd.Haiku-MuscleAdmin";
const char* kDeskbarSignature = "application/x-vnd.Be-TSKB";
const char* kDeskbarItemName = "MuscleAdmin";


status_t
our_image(image_info& image)
{
	int32 cookie = 0;
	while (get_next_image_info(B_CURRENT_TEAM, &cookie, &image) == B_OK) {
		if ((char *)our_image >= (char *)image.text
			&& (char *)our_image <= (char *)image.text + image.text_size)
			return B_OK;
	}

	return B_ERROR;
}


//	#pragma mark -


MuscleAdmin::MuscleAdmin()
	: BApplication(kSignature)
{
}


MuscleAdmin::~MuscleAdmin()
{
}


void
MuscleAdmin::ReadyToRun()
{
	bool isInstalled = false;
	bool isDeskbarRunning = true;

	{
		// if the Deskbar is not alive at this point, it might be after having
		// acknowledged the requester below
		BDeskbar deskbar;
		isDeskbarRunning = deskbar.IsRunning();
		isInstalled = deskbar.HasItem(kDeskbarItemName);
	}

	if (isDeskbarRunning && !isInstalled) {
		BAlert* alert = new BAlert("",
			"You can run MuscleAdmin in a window or install it in the Deskbar."
			, "Run in window", "Install in Deskbar", NULL, B_WIDTH_AS_USUAL,
			B_WARNING_ALERT);

		if (alert->Go()) {
			image_info info;
			entry_ref ref;

			if (our_image(info) == B_OK
				&& get_ref_for_path(info.name, &ref) == B_OK) {
				BDeskbar deskbar;
				deskbar.AddItem(&ref);
			}

			Quit();
			return;
		}
	}

	BWindow* window = new MuscleAdminWindow();
	window->Show();
}


void
MuscleAdmin::AboutRequested()
{
	BWindow* window = WindowAt(0);
	if (window == NULL)
		return;

	BView* view = window->FindView(kDeskbarItemName);
	if (view == NULL)
		return;

	BMessenger target((BHandler*)view);
	BMessage about(B_ABOUT_REQUESTED);
	target.SendMessage(&about);
}


//	#pragma mark -


int
main(int, char**)
{
	MuscleAdmin app;
	app.Run();

	return 0;
}

