#include "Mouse.h"

#define MAX_Y_VALUE 65535
#define CONSEC_CLICK_INTERVAL 10000000

static BOOLEAN IsInversed = FALSE;

void HandleMouseInput(PMOUSE_INPUT_DATA Input)
{
	BOOLEAN backButtonPressed = Input->ButtonFlags & MOUSE_BUTTON_4_DOWN;
	if (backButtonPressed)
	{
		IsInversed = !IsInversed;
	}

	if (IsInversed) 
	{
		if (Input->Flags & MOUSE_MOVE_ABSOLUTE)
		{
			Input->LastY = MAX_Y_VALUE - Input->LastY;
		}
		else
		{
			Input->LastY = -Input->LastY;
		}
	}
}