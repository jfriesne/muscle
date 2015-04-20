/*
 * Copyright 2006, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 */
#ifndef MUSCLEADMINWINDOW_H
#define MUSCLEADMINWINDOW_H


#include <Window.h>


class MuscleAdminWindow : public BWindow {
	public:
		MuscleAdminWindow();
		virtual ~MuscleAdminWindow();

		virtual bool QuitRequested();
		void MessageReceived(BMessage* message);
};

#endif	// MUSCLEADMINWINDOW_H
