/*
 * Copyright 2006-2009, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Clemens Zeidler, haiku@Clemens-Zeidler.de
 */
#ifndef MUSCLEADMINVIEW_H
#define MUSCLEADMINVIEW_H


#include <View.h>
#include <TextControl.h>
#include <Button.h>
#include <CheckBox.h>


class BFile;


class MuscleAdminView : public BView {
public:
							MuscleAdminView(BRect frame, int32 resizingMode, bool inDeskbar = false);


	virtual					~MuscleAdminView();

	virtual	status_t		Archive(BMessage* archive, bool deep = true) const;

	virtual	void			AttachedToWindow();
	virtual	void			DetachedFromWindow();

	virtual	void			MessageReceived(BMessage* message);
	virtual	void			Draw(BRect updateRect);
	virtual	void			GetPreferredSize(float *width, float *height);


protected:
							MuscleAdminView(BMessage* archive);

	virtual void			Update(bool force = false);

			void			FromMessage(const BMessage* message);
			status_t		ToMessage(BMessage* message) const;

private:
			void			_Init();
			void			_SetLabel(char* buffer, size_t bufferLength);
protected:
			bool			fInDeskbar;

			BSize			fPreferredSize;

			BButton*		fCancelButton;
			BButton*		fDoneButton;
			BButton*		fSelectWebDir;

			BTextControl*	fWebDir;
};


class MuscleAdminReplicant : public MuscleAdminView {
public:
							MuscleAdminReplicant(BRect frame,
								int32 resizingMode, bool inDeskbar = false);
							MuscleAdminReplicant(BMessage* archive);
	virtual					~MuscleAdminReplicant();

	static	MuscleAdminReplicant* Instantiate(BMessage* archive);
	virtual	status_t		Archive(BMessage* archive, bool deep = true) const;

	virtual	void			MessageReceived(BMessage* message);
	virtual	void			MouseDown(BPoint where);

private:
			void			_AboutRequested();
			void			_Init();
			void			_Quit();

			status_t		_GetSettings(BFile& file, int mode);
			void			_LoadSettings();
			void			_SaveSettings();

private:
			bool			fMessengerExist;
			BMessenger*		fExtWindowMessenger;
};


#endif	// MUSCLEADMINVIEW_H
