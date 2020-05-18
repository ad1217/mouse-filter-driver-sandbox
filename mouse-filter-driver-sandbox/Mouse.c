#include "Mouse.h"

#define MAX_Y_VALUE 65535
#define CONSEC_CLICK_INTERVAL 10000000

static BOOLEAN IsInversed = FALSE;
static BOOLEAN WasLeftBtnDownLastTime = FALSE;
static BOOLEAN WasRightBtnDownLastTime = FALSE;
static LARGE_INTEGER PrevTimerValue = { 0 };
static enum { FIRST_L, SECOND_R, THIRD_R, FOURTH_L } CurrentPressStage;

void HandleMouseInput(PMOUSE_INPUT_DATA Input)
{
	BOOLEAN leftBtnPressed = Input->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN;
	BOOLEAN rightBtnPressed = Input->ButtonFlags & MOUSE_RIGHT_BUTTON_DOWN;
	if (leftBtnPressed || rightBtnPressed)
	{
		LARGE_INTEGER currentTimerValue = { 0 };
		KeQuerySystemTime(&currentTimerValue);

		if (CurrentPressStage != FIRST_L && 
			(currentTimerValue.QuadPart - PrevTimerValue.QuadPart) > CONSEC_CLICK_INTERVAL)
		{
			CurrentPressStage = leftBtnPressed ? SECOND_R : FIRST_L;
		}
		else
		{
			if ((CurrentPressStage == FIRST_L && leftBtnPressed) ||
				(CurrentPressStage == SECOND_R && rightBtnPressed) ||
				(CurrentPressStage == THIRD_R && rightBtnPressed))
			{
				CurrentPressStage++;
			}
			else if (CurrentPressStage == FOURTH_L && leftBtnPressed)
			{
				IsInversed = !IsInversed;
				CurrentPressStage = FIRST_L;
			}
			else
			{
				CurrentPressStage = leftBtnPressed ? SECOND_R : FIRST_L;
			}
		}
		PrevTimerValue = currentTimerValue;
		KdPrint(("Current press stage %d \r\n", CurrentPressStage));
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