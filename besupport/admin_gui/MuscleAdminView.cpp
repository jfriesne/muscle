/*
 * Copyright 2006-2012, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Clemens Zeidler, haiku@Clemens-Zeidler.de
 *		Alexander von Gluck, kallisti5@unixzen.com 
 */


#include "MuscleAdminView.h"

#include <Application.h>
#include <Box.h>
#include <LayoutBuilder.h>
#include <Dragger.h>
#include <Deskbar.h>
#include <Path.h>
#include <File.h>
#include <FindDirectory.h>

#include "MuscleDaemon.h"
#include "MuscleAdmin.h"

#include "constants.h"

extern "C" _EXPORT BView *instantiate_deskbar_item(void);
extern const char* kDeskbarItemName;

const uint32 kMinIconWidth = 16;
const uint32 kMinIconHeight = 16;

const char* STR_BBX_LOCATION = "Website location";
const char* STR_TXT_DIRECTORY = "Server App Name:";
const char* STR_BTN_DIRECTORY = "Run Server";



MuscleAdminView::MuscleAdminView(BRect frame, int32 resizingMode, bool inDeskbar)
	:
	BView(frame, kDeskbarItemName, resizingMode,
		B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fInDeskbar(inDeskbar)
{
	fPreferredSize.width = frame.Width();
	fPreferredSize.height = frame.Height();
	_Init();
}


MuscleAdminView::MuscleAdminView(BMessage* archive)
	: BView(archive)
{
	_Init();
	FromMessage(archive);
}


MuscleAdminView::~MuscleAdminView()
{
}


status_t
MuscleAdminView::Archive(BMessage* archive, bool deep) const
{
	status_t status = BView::Archive(archive, deep);
	if (status == B_OK)
		status = ToMessage(archive);

	return status;
}


void
MuscleAdminView::_Init()
{
	SetViewColor(B_TRANSPARENT_COLOR);
	fCancelButton = new BButton("Cancel Button", "Cancel",
		new BMessage(MSG_PREF_BTN_CANCEL));
	fDoneButton = new BButton("Done Button", "Done",
		new BMessage(MSG_PREF_BTN_DONE));

	SetLayout(new BGroupLayout(B_VERTICAL));

	// Web Site Location BBox
	BBox* webSiteLocation = new BBox("Run Server");
	webSiteLocation->SetLabel(STR_BBX_LOCATION);

	// Service Name
	//STR_MUSCLE_DEAMON_NAME
	fWebDir = new BTextControl(STR_TXT_DIRECTORY, STR_MUSCLE_DEAMON_NAME, NULL);
//	SetWebDir(win->WebDir());
	
	// Run Service
	fSelectWebDir = new BButton("Select Web Dir", STR_BTN_DIRECTORY,
		new BMessage(MSG_PREF_SITE_BTN_SELECT));	
	
	BGroupLayout* webSiteLocationLayout = new BGroupLayout(B_VERTICAL, 0);
	webSiteLocation->SetLayout(webSiteLocationLayout);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_ITEM_INSETS)
		.AddGroup(webSiteLocationLayout)
			.SetInsets(B_USE_ITEM_INSETS)
			.AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
				.SetInsets(0, B_USE_ITEM_INSETS, 0, 0)
				.AddTextControl(fWebDir, 0, 0, B_ALIGN_LEFT, 1, 2)
				.Add(fSelectWebDir, 2, 1)
				.SetColumnWeight(1, 10.f)
				.End()
			.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fDoneButton);
}


void
MuscleAdminView::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (Parent())
		SetLowColor(Parent()->ViewColor());
	else
		SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	Update();
}


void
MuscleAdminView::DetachedFromWindow()
{
}


void
MuscleAdminView::MessageReceived(BMessage *message)
{
	switch (message->what) {
//		case kMsgUpdate:
//			Update();
//			break;

		default:
			BView::MessageReceived(message);
	}
}


void
MuscleAdminView::GetPreferredSize(float *width, float *height)
{
	*width = fPreferredSize.width;
	*height = fPreferredSize.height;
}


void
MuscleAdminView::Draw(BRect updateRect)
{
/*	bool drawBackground = Parent() == NULL
		|| (Parent()->Flags() & B_DRAW_ON_CHILDREN) == 0;
	if (drawBackground)
		FillRect(updateRect, B_SOLID_LOW);

	float aspect = Bounds().Width() / Bounds().Height();
	bool below = aspect <= 1.0f;

	font_height fontHeight;
	GetFontHeight(&fontHeight);
	float baseLine = ceilf(fontHeight.ascent);

	char text[64];
	_SetLabel(text, sizeof(text));

	float textHeight = ceilf(fontHeight.descent + fontHeight.ascent);
	float textWidth = StringWidth(text);
	bool showLabel = fShowLabel && text[0];

	BRect iconRect;

	if (fShowStatusIcon) {
		iconRect = Bounds();
		if (showLabel) {
			if (below)
				iconRect.bottom -= textHeight + 4;
			else
				iconRect.right -= textWidth + 4;
		}

		// make a square
		iconRect.bottom = min_c(iconRect.bottom, iconRect.right);
		iconRect.right = iconRect.bottom;

		if (iconRect.Width() + 1 >= kMinIconWidth
			&& iconRect.Height() + 1 >= kMinIconHeight) {
			_DrawBattery(iconRect);
		} else {
			// there is not enough space for the icon
			iconRect.Set(0, 0, -1, -1);
		}
	}

	if (showLabel) {
		BPoint point(0, baseLine);

		if (iconRect.IsValid()) {
			if (below) {
				point.x = (iconRect.Width() - textWidth) / 2;
				point.y += iconRect.Height() + 2;
			} else {
				point.x = iconRect.Width() + 2;
				point.y += (iconRect.Height() - textHeight) / 2;
			}
		} else {
			point.x = (Bounds().Width() - textWidth) / 2;
			point.y += (Bounds().Height() - textHeight) / 2;
		}

		if (drawBackground)
			SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
		else {
			SetDrawingMode(B_OP_OVER);
			rgb_color c = Parent()->LowColor();
			if (c.red + c.green + c.blue > 128 * 3)
				SetHighColor(0, 0, 0);
			else
				SetHighColor(255, 255, 255);
		}

		DrawString(text, point);
	}*/
}


void
MuscleAdminView::_SetLabel(char* buffer, size_t bufferLength)
{
/*	if (bufferLength < 1)
		return;

	buffer[0] = '\0';

	if (!fShowLabel)
		return;

	const char* open = "";
	const char* close = "";
	if (fOnline) {
		open = "(";
		close = ")";
	}

	if (!fShowTime && fPercent >= 0) {
		snprintf(buffer, bufferLength, "%s%" B_PRId32 "%%%s", open, fPercent,
			close);
	} else if (fShowTime && fTimeLeft >= 0) {
		snprintf(buffer, bufferLength, "%s%" B_PRId32 ":%02" B_PRId32 "%s",
			open, fTimeLeft / 3600, (fTimeLeft / 60) % 60, close);
	}*/
}



void
MuscleAdminView::Update(bool force)
{
/*	int32 previousPercent = fPercent;
	bool previousTimeLeft = fTimeLeft;
	bool wasOnline = fOnline;

	_GetBatteryInfo(&fBatteryInfo, fBatteryID);

	fHasBattery = (fBatteryInfo.state & BATTERY_CRITICAL_STATE) == 0;

	if (fBatteryInfo.full_capacity > 0 && fHasBattery) {
		fPercent = (100 * fBatteryInfo.capacity) / fBatteryInfo.full_capacity;
		fOnline = (fBatteryInfo.state & BATTERY_CHARGING) != 0;
		fTimeLeft = fBatteryInfo.time_left;
	} else {
		fPercent = 0;
		fOnline = false;
		fTimeLeft = false;
	}


	if (fInDeskbar) {
		// make sure the tray icon is large enough
		float width = fShowStatusIcon ? kMinIconWidth + 2 : 0;

		if (fShowLabel) {
			char text[64];
			_SetLabel(text, sizeof(text));

			if (text[0])
				width += ceilf(StringWidth(text)) + 4;
		} else {
			char text[256];
			const char* open = "";
			const char* close = "";
			if (fOnline) {
				open = "(";
				close = ")";
			}
			if (fHasBattery) {
				size_t length = snprintf(text, sizeof(text), "%s%" B_PRId32
					"%%%s", open, fPercent, close);
				if (fTimeLeft) {
					length += snprintf(text + length, sizeof(text) - length,
						"\n%" B_PRId32 ":%02" B_PRId32, fTimeLeft / 3600,
						(fTimeLeft / 60) % 60);
				}

				const char* state = NULL;
				if ((fBatteryInfo.state & BATTERY_CHARGING) != 0)
					state = "charging");
				else if ((fBatteryInfo.state & BATTERY_DISCHARGING) != 0)
					state = "discharging");

				if (state != NULL) {
					snprintf(text + length, sizeof(text) - length, "\n%s",
						state);
				}
			} else
				muscleStrcpy(text, "no battery"));
			SetToolTip(text);
		}
		if (width == 0) {
			// make sure we're not going away completely
			width = 8;
		}

		if (width != Bounds().Width())
			ResizeTo(width, Bounds().Height());
	}

	if (force || wasOnline != fOnline
		|| (fShowTime && fTimeLeft != previousTimeLeft)
		|| (!fShowTime && fPercent != previousPercent))
		Invalidate();*/
}


void
MuscleAdminView::FromMessage(const BMessage* archive)
{
/*	bool value;
	if (archive->FindBool("show label", &value) == B_OK)
		fShowLabel = value;
	if (archive->FindBool("show icon", &value) == B_OK)
		fShowStatusIcon = value;
	if (archive->FindBool("show time", &value) == B_OK)
		fShowTime = value;
	
	//Incase we have a bad saving and none are showed..
	if (!fShowLabel && !fShowStatusIcon)
		fShowLabel = true;

	int32 intValue;
	if (archive->FindInt32("battery id", &intValue) == B_OK)
		fBatteryID = intValue;*/
}


status_t
MuscleAdminView::ToMessage(BMessage* archive) const
{
/*	status_t status = archive->AddBool("show label", fShowLabel);
	if (status == B_OK)
		status = archive->AddBool("show icon", fShowStatusIcon);
	if (status == B_OK)
		status = archive->AddBool("show time", fShowTime);
	if (status == B_OK)
		status = archive->AddInt32("battery id", fBatteryID);
*/
	return B_OK;
}


//	#pragma mark -


MuscleAdminReplicant::MuscleAdminReplicant(BRect frame, int32 resizingMode,
		bool inDeskbar)
	:
	MuscleAdminView(frame, resizingMode, inDeskbar)
{
	_Init();
	_LoadSettings();

	if (!inDeskbar) {
		// we were obviously added to a standard window - let's add a dragger
		frame.OffsetTo(B_ORIGIN);
		frame.top = frame.bottom - 7;
		frame.left = frame.right - 7;
		BDragger* dragger = new BDragger(frame, this,
			B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
		AddChild(dragger);
	} else
		Update();
}


MuscleAdminReplicant::MuscleAdminReplicant(BMessage* archive)
	:
	MuscleAdminView(archive)
{
	_Init();
	_LoadSettings();
}


MuscleAdminReplicant::~MuscleAdminReplicant()
{
	if (fMessengerExist)
		delete fExtWindowMessenger;

//	fDriverInterface->StopWatching(this);
//	fDriverInterface->Disconnect();
//	fDriverInterface->ReleaseReference();

	_SaveSettings();
}


MuscleAdminReplicant*
MuscleAdminReplicant::Instantiate(BMessage* archive)
{
	if (!validate_instantiation(archive, "MuscleAdminReplicant"))
		return NULL;

	return new MuscleAdminReplicant(archive);
}


status_t
MuscleAdminReplicant::Archive(BMessage* archive, bool deep) const
{
	status_t status = MuscleAdminView::Archive(archive, deep);
	if (status == B_OK)
		status = archive->AddString("add_on", kSignature);
	if (status == B_OK)
		status = archive->AddString("class", "MuscleAdminReplicant");

	return status;
}


void
MuscleAdminReplicant::MessageReceived(BMessage *message)
{
/*	switch (message->what) {
		case kMsgToggleLabel:
			if (fShowStatusIcon)
				fShowLabel = !fShowLabel;
			else
				fShowLabel = true;
				
			Update(true);
			break;

		case kMsgToggleTime:
			fShowTime = !fShowTime;
			Update(true);
			break;

		case kMsgToggleStatusIcon:
			if (fShowLabel)
				fShowStatusIcon = !fShowStatusIcon;
			else
				fShowStatusIcon = true;

			Update(true);
			break;

		case kMsgToggleExtInfo:
			_OpenExtendedWindow();
			break;

		case B_ABOUT_REQUESTED:
			_AboutRequested();
			break;

		case B_QUIT_REQUESTED:
			_Quit();
			break;

		default:
			MuscleAdminView::MessageReceived(message);
	}*/
}


void
MuscleAdminReplicant::MouseDown(BPoint point)
{
/*	BPopUpMenu *menu = new BPopUpMenu(B_EMPTY_STRING, false, false);
	menu->SetFont(be_plain_font);

	BMenuItem* item;
	menu->AddItem(item = new BMenuItem("Show text label"),
		new BMessage(kMsgToggleLabel)));
	if (fShowLabel)
		item->SetMarked(true);
	menu->AddItem(item = new BMenuItem("Show status icon"),
		new BMessage(kMsgToggleStatusIcon)));
	if (fShowStatusIcon)
		item->SetMarked(true);
	menu->AddItem(new BMenuItem(!fShowTime ? "Show time") :
	"Show percent"),
		new BMessage(kMsgToggleTime)));

	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Battery info" B_UTF8_ELLIPSIS),
		new BMessage(kMsgToggleExtInfo)));

	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("About" B_UTF8_ELLIPSIS),
		new BMessage(B_ABOUT_REQUESTED)));
	menu->AddItem(new BMenuItem("Quit"), 
		new BMessage(B_QUIT_REQUESTED)));
	menu->SetTargetForItems(this);

	ConvertToScreen(&point);
	menu->Go(point, true, false, true);*/
}


void
MuscleAdminReplicant::_AboutRequested()
{
/*	BAboutWindow* window = new BAboutWindow(
		B_TRANSLATE_SYSTEM_NAME("MuscleAdmin"), kSignature);

	const char* authors[] = {
		"Axel Dörfler",
		"Alexander von Gluck",
		"Clemens Zeidler",
		NULL
	};

	window->AddCopyright(2006, "Haiku, Inc.");
	window->AddAuthors(authors);

	window->Show();*/
}


void
MuscleAdminReplicant::_Init()
{
/*	fDriverInterface = new ACPIDriverInterface;
	if (fDriverInterface->Connect() != B_OK) {
		delete fDriverInterface;
		fDriverInterface = new APMDriverInterface;
		if (fDriverInterface->Connect() != B_OK) {
			fprintf(stderr, "No power interface found.\n");
			_Quit();
		}
	}

	fExtendedWindow = NULL;
	fMessengerExist = false;
	fExtWindowMessenger = NULL;

	fDriverInterface->StartWatching(this);*/
}


void
MuscleAdminReplicant::_Quit()
{
	if (fInDeskbar) {
		BDeskbar deskbar;
		deskbar.RemoveItem(kDeskbarItemName);
	} else
		be_app->PostMessage(B_QUIT_REQUESTED);
}


status_t
MuscleAdminReplicant::_GetSettings(BFile& file, int mode)
{
	BPath path;
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path,
		(mode & O_ACCMODE) != O_RDONLY);
	if (status != B_OK)
		return status;

	path.Append("MuscleAdmin settings");

	return file.SetTo(path.Path(), mode);
}


void
MuscleAdminReplicant::_LoadSettings()
{
//	fShowLabel = false;

	BFile file;
	if (_GetSettings(file, B_READ_ONLY) != B_OK)
		return;

	BMessage settings;
	if (settings.Unflatten(&file) < B_OK)
		return;

	FromMessage(&settings);
}


void
MuscleAdminReplicant::_SaveSettings()
{
	BFile file;
	if (_GetSettings(file, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE) != B_OK)
		return;

	BMessage settings('pwst');
	ToMessage(&settings);

	ssize_t size = 0;
	settings.Flatten(&file, &size);
}

/*
void
MuscleAdminReplicant::_OpenExtendedWindow()
{
	if (!fExtendedWindow) {
		fExtendedWindow = new ExtendedInfoWindow(fDriverInterface);
		fExtWindowMessenger = new BMessenger(NULL, fExtendedWindow);
		fExtendedWindow->Show();
		return;
	}

	BMessage msg(B_SET_PROPERTY);
	msg.AddSpecifier("Hidden", int32(0));
	if (fExtWindowMessenger->SendMessage(&msg) == B_BAD_PORT_ID) {
		fExtendedWindow = new ExtendedInfoWindow(fDriverInterface);
		if (fMessengerExist)
			delete fExtWindowMessenger;
		fExtWindowMessenger = new BMessenger(NULL, fExtendedWindow);
		fMessengerExist = true;
		fExtendedWindow->Show();
	} else
		fExtendedWindow->Activate();

}*/


//	#pragma mark -


extern "C" _EXPORT BView*
instantiate_deskbar_item(void)
{
	return new MuscleAdminReplicant(BRect(0, 0, 15, 15), B_FOLLOW_NONE, true);
}

