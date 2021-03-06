#include <PalmOS.h>

#include "MathLib.h"

#include "gemgrab.h"

#define CREATOR_ID 'rngg'
#define DB_NAME "GemGrabberDB"
#define DB_TYPE 'DATA'

#define INTERVAL 5
#define STARTING_TIME 60 * sysTicksPerSecond
#define NUM_GEMS 20
#define NUM_HIGHSCORES 8

#define QUARTER_PI 0.78540
#define HALF_PI 1.57080

typedef struct GemType
{
	Coord x;
	Coord y;
	UInt8 size;
} GemType;

typedef struct HighScoreType
{
	Char name[21];
	UInt16 score;
} HighScoreType;

typedef struct DbRecordType
{
	HighScoreType scores[NUM_HIGHSCORES];
	UInt8 numScores;
} DbRecordType;
typedef DbRecordType *DbRecordTypePtr;

UInt32 swingTimer = 0;
double swingPosition;
double swingX;
double swingY;
double extendAmount = 0.0;
Boolean extending = false;
double baseExtendSpeed = 60.0;
double extendSpeed = 1.0;
UInt8 numGems = NUM_GEMS;
GemType gems[NUM_GEMS];
int heldGemIndex = -1;
UInt32 lastTicks;

UInt16 score = 0;
UInt16 time;

DmOpenRef db;
UInt16 dbCard;
LocalID dbId;
DbRecordType highScores;

Boolean gameOver = false;

BitmapPtr gemBitmaps[5];
BitmapPtr countdownBitmaps[3];
BitmapPtr clawOpenBitmaps[5];
BitmapPtr clawClosedBitmaps[5];

WinHandle gemBuffer;

// Double buffering code from Noiz

WinHandle screenBufferH;
WinHandle oldDrawWinH;

/**
 * create off screen buffer
 */
void createScreenBuffer()
{
	UInt16 error;
	RectangleType sourceBounds;
	sourceBounds.topLeft.x = 0;
	sourceBounds.topLeft.y = 0;
	sourceBounds.extent.x = 160;
	sourceBounds.extent.y = 144;

	screenBufferH = WinCreateOffscreenWindow(160, 144, screenFormat, &error);
	oldDrawWinH = WinGetDrawWindow();
	WinSetDrawWindow(screenBufferH);
	WinEraseRectangle(&sourceBounds, 0);
	WinSetDrawWindow(oldDrawWinH);
}

void startDrawOffscreen()
{
	oldDrawWinH = WinGetDrawWindow();
	WinSetDrawWindow(screenBufferH);
}

void endDrawOffscreen()
{
	WinSetDrawWindow(oldDrawWinH);
}

void flipDisplay()
{
	RectangleType sourceBounds;
	sourceBounds.topLeft.x = 0;
	sourceBounds.topLeft.y = 0;
	sourceBounds.extent.x = 160;
	sourceBounds.extent.y = 144;
	WinCopyRectangle(screenBufferH, 0, &sourceBounds, 0, 16, winPaint);
}

// End double buffering code from Noiz

void SelectRecord(UInt16 index)
{
	MemHandle recordHandle;
	DbRecordTypePtr recordPointer;
	int i;

	if ((recordHandle = (MemHandle)DmQueryRecord(db, index)))
	{
		if (DmRecordInfo(db, index, NULL, NULL, NULL))
		{
			FrmCustomAlert(ErrorAlert, "Unable to get record info", "", "");
			return;
		}
		recordPointer = (DbRecordTypePtr)MemHandleLock(recordHandle);

		for (i = 0; i < recordPointer->numScores; i++)
		{
			highScores.scores[i].score = recordPointer->scores[i].score;
			StrCopy(highScores.scores[i].name, recordPointer->scores[i].name);
		}
		highScores.numScores = recordPointer->numScores;

		MemHandleUnlock(recordHandle);
	}
	else
	{
		highScores.numScores = 0;
	}
}

void InsertRecord()
{
	UInt16 index;
	MemHandle recordHandle;
	MemPtr recordPointer;

	index = DmNumRecords(db);
	if (!(recordHandle = DmNewRecord(db, &index, sizeof(DbRecordType))))
	{
		FrmCustomAlert(ErrorAlert, "Unable to create record", "", "");
		return;
	}

	recordPointer = MemHandleLock(recordHandle);
	DmWrite(recordPointer, 0, &highScores, sizeof(DbRecordType));
	MemPtrUnlock(recordPointer);

	DmReleaseRecord(db, index, true);
}

void UpdateRecord()
{
	UInt16 index = 0;
	MemHandle recordHandle;
	MemPtr recordPointer;

	if (DmRecordInfo(db, index, NULL, NULL, NULL))
	{
		FrmCustomAlert(ErrorAlert, "Unable to get record info", "", "");
		return;
	}

	if ((recordHandle = (MemHandle)DmGetRecord(db, index)))
	{
		recordPointer = MemHandleLock(recordHandle);
		DmWrite(recordPointer, 0, &highScores, sizeof(DbRecordType));
		MemPtrUnlock(recordPointer);

		DmReleaseRecord(db, index, true);
	}
	else
	{
		FrmCustomAlert(ErrorAlert, "Unable to get record", "", "");
	}
}

void UpsertRecord()
{
	UInt16 index = DmNumRecords(db);
	if (index > 0)
	{
		UpdateRecord();
	}
	else
	{
		InsertRecord();
	}
}

void CreateGemBuffer()
{
	UInt16 error;
	gemBuffer = WinCreateOffscreenWindow(160, 144, screenFormat, &error);
}

void SetUpBitmaps()
{
	MemHandle countdown1Handle = DmGetResource(bitmapRsc, Countdown1);
	MemHandle countdown2Handle = DmGetResource(bitmapRsc, Countdown2);
	MemHandle countdown3Handle = DmGetResource(bitmapRsc, Countdown3);

	MemHandle gem4Handle = DmGetResource(bitmapRsc, Gem4);
	MemHandle gem5Handle = DmGetResource(bitmapRsc, Gem5);
	MemHandle gem6Handle = DmGetResource(bitmapRsc, Gem6);
	MemHandle gem7Handle = DmGetResource(bitmapRsc, Gem7);
	MemHandle gem8Handle = DmGetResource(bitmapRsc, Gem8);

	MemHandle clawClosed1Handle = DmGetResource(bitmapRsc, ClawClosed1);
	MemHandle clawClosed2Handle = DmGetResource(bitmapRsc, ClawClosed2);
	MemHandle clawClosed3Handle = DmGetResource(bitmapRsc, ClawClosed3);
	MemHandle clawClosed4Handle = DmGetResource(bitmapRsc, ClawClosed4);
	MemHandle clawClosed5Handle = DmGetResource(bitmapRsc, ClawClosed5);

	MemHandle clawOpen1Handle = DmGetResource(bitmapRsc, ClawOpen1);
	MemHandle clawOpen2Handle = DmGetResource(bitmapRsc, ClawOpen2);
	MemHandle clawOpen3Handle = DmGetResource(bitmapRsc, ClawOpen3);
	MemHandle clawOpen4Handle = DmGetResource(bitmapRsc, ClawOpen4);
	MemHandle clawOpen5Handle = DmGetResource(bitmapRsc, ClawOpen5);

	countdownBitmaps[0] = MemHandleLock(countdown1Handle);
	countdownBitmaps[1] = MemHandleLock(countdown2Handle);
	countdownBitmaps[2] = MemHandleLock(countdown3Handle);

	gemBitmaps[0] = MemHandleLock(gem4Handle);
	gemBitmaps[1] = MemHandleLock(gem5Handle);
	gemBitmaps[2] = MemHandleLock(gem6Handle);
	gemBitmaps[3] = MemHandleLock(gem7Handle);
	gemBitmaps[4] = MemHandleLock(gem8Handle);

	clawClosedBitmaps[0] = MemHandleLock(clawClosed1Handle);
	clawClosedBitmaps[1] = MemHandleLock(clawClosed2Handle);
	clawClosedBitmaps[2] = MemHandleLock(clawClosed3Handle);
	clawClosedBitmaps[3] = MemHandleLock(clawClosed4Handle);
	clawClosedBitmaps[4] = MemHandleLock(clawClosed5Handle);

	clawOpenBitmaps[0] = MemHandleLock(clawOpen1Handle);
	clawOpenBitmaps[1] = MemHandleLock(clawOpen2Handle);
	clawOpenBitmaps[2] = MemHandleLock(clawOpen3Handle);
	clawOpenBitmaps[3] = MemHandleLock(clawOpen4Handle);
	clawOpenBitmaps[4] = MemHandleLock(clawOpen5Handle);
}

void TearDownBitmaps()
{
	int i;
	MemHandle handle;

	for (i = 0; i < 3; i++)
	{
		handle = MemPtrRecoverHandle(countdownBitmaps[i]);
		MemHandleUnlock(handle);
		DmReleaseResource(handle);
	}

	for (i = 0; i < 5; i++)
	{
		handle = MemPtrRecoverHandle(gemBitmaps[i]);
		MemHandleUnlock(handle);
		DmReleaseResource(handle);

		handle = MemPtrRecoverHandle(clawClosedBitmaps[i]);
		MemHandleUnlock(handle);
		DmReleaseResource(handle);

		handle = MemPtrRecoverHandle(clawOpenBitmaps[i]);
		MemHandleUnlock(handle);
		DmReleaseResource(handle);
	}
}

void SetUpGems()
{
	int i = 0;
	for (i = 0; i < NUM_GEMS; i++)
	{
		gems[i].x = SysRandom(0) % 150 + 5;
		gems[i].y = SysRandom(0) % 100 + 39;
		gems[i].size = SysRandom(0) % 40 + 40;
	}
}

void RenderGems()
{
	int i = 0;
	WinHandle tempDrawWindow = WinGetDrawWindow();
	RectangleType eraseRect;

	eraseRect.topLeft.x = 0;
	eraseRect.topLeft.y = 0;
	eraseRect.extent.x = 160;
	eraseRect.extent.y = 144;

	WinSetDrawWindow(gemBuffer);

	WinEraseRectangle(&eraseRect, 0);

	for (i = 0; i < numGems; i++)
	{
		int gemSize = gems[i].size / 10;
		WinDrawBitmap(gemBitmaps[gemSize - 4], gems[i].x - gemSize / 2, gems[i].y - gemSize / 2);
	}

	WinSetDrawWindow(tempDrawWindow);
}

void EraseGem(index)
{
	WinHandle tempDrawWindow = WinGetDrawWindow();
	RectangleType gemRect;
	UInt8 gemSize = gems[index].size / 10;
	gemRect.topLeft.x = gems[index].x - gemSize / 2;
	gemRect.topLeft.y = gems[index].y - gemSize / 2;
	gemRect.extent.x = gemSize;
	gemRect.extent.y = gemSize;

	WinSetDrawWindow(gemBuffer);
	WinEraseRectangle(&gemRect, 0);
	WinSetDrawWindow(tempDrawWindow);
}

void DrawGemBuffer()
{
	RectangleType sourceBounds;
	sourceBounds.topLeft.x = 0;
	sourceBounds.topLeft.y = 0;
	sourceBounds.extent.x = 160;
	sourceBounds.extent.y = 144;
	WinCopyRectangle(gemBuffer, 0, &sourceBounds, 0, 0, winPaint);
}

void GrabGem(UInt8 index)
{
	int i;
	score += gems[index].size;
	for (i = index; i < numGems - 1; i++)
	{
		gems[i] = gems[i + 1];
	}
	numGems--;
	heldGemIndex = -1;
	RenderGems();
}

Err InitialSetup()
{
	UInt16 attributes;
	Err retcode = 0;

	if ((db = DmOpenDatabaseByTypeCreator(DB_TYPE, CREATOR_ID, dmModeReadWrite | dmModeShowSecret)))
	{
		DmOpenDatabaseInfo(db, &dbId, NULL, NULL, &dbCard, NULL);
	}
	else
	{
		if ((retcode = DmCreateDatabase(0, DB_NAME, CREATOR_ID, DB_TYPE, false)))
		{
			return retcode;
		}

		if (!(db = DmOpenDatabaseByTypeCreator(DB_TYPE, CREATOR_ID, dmModeReadWrite | dmModeShowSecret)))
		{
			return dmErrCantOpen;
		}

		DmOpenDatabaseInfo(db, &dbId, NULL, NULL, &dbCard, NULL);
		DmDatabaseInfo(dbCard, dbId, NULL, &attributes, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		DmSetDatabaseInfo(dbCard, dbId, NULL, &attributes, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	}

	SetUpBitmaps();
	createScreenBuffer();
	CreateGemBuffer();

	return retcode;
}

void ResetGame()
{
	lastTicks = TimGetTicks();
	time = STARTING_TIME + 3 * sysTicksPerSecond;
	numGems = NUM_GEMS;
	extending = false;
	extendAmount = 0.0;
	heldGemIndex = -1;
	score = 0;
}

UInt8 MenuFormHandleEvent(EventPtr e)
{
	if (e->eType == ctlSelectEvent)
	{
		switch (e->data.ctlSelect.controlID)
		{
		case ButtonPlay:
			FrmGotoForm(FormPlay);
			return true;
		case ButtonHowto:
			FrmGotoForm(FormHowto);
			return true;
		case ButtonScores:
			FrmGotoForm(FormScores);
			return true;
		}
	}
	return false;
}

UInt8 HowtoFormHandleEvent(EventPtr e)
{
	if (e->eType == ctlSelectEvent)
	{
		if (e->data.ctlSelect.controlID == ButtonBack)
		{
			FrmGotoForm(FormMenu);
			return true;
		}
	}
	return false;
}

UInt8 GameOverFormHandleEvent(EventPtr e)
{
	Int16 i;
	Char *name;
	FormType *pfrm;
	FieldType *pfld;
	UInt16 objectIndex;

	if (e->eType == ctlSelectEvent)
	{
		if (e->data.ctlSelect.controlID == ButtonSave)
		{
			pfrm = FrmGetActiveForm();
			objectIndex = FrmGetObjectIndex(pfrm, FieldHighScoreName);
			pfld = FrmGetObjectPtr(pfrm, objectIndex);
			name = FldGetTextPtr(pfld);
			SelectRecord(0);
			if (highScores.numScores == 0)
			{
				StrNCopy(highScores.scores[0].name, name, 20);
				highScores.scores[0].score = score;
			}
			for (i = highScores.numScores - 1; i >= 0; i--)
			{
				if (score < highScores.scores[i].score)
				{
					if (i < NUM_HIGHSCORES - 1)
					{
						StrNCopy(highScores.scores[i + 1].name, name, 20);
						highScores.scores[i + 1].score = score;
					}
					break;
				}
				if (i < NUM_HIGHSCORES - 1)
				{
					highScores.scores[i + 1] = highScores.scores[i];
					if (i == 0)
					{
						StrNCopy(highScores.scores[0].name, name, 20);
						highScores.scores[0].score = score;
					}
				}
			}
			if (highScores.numScores < NUM_HIGHSCORES)
			{
				highScores.numScores++;
			}
			UpsertRecord();
			FrmGotoForm(FormScores);
			return true;
		}
	}
	return false;
}

UInt8 ScoresFormHandleEvent(EventPtr e)
{
	if (e->eType == ctlSelectEvent)
	{
		if (e->data.ctlSelect.controlID == ButtonBack)
		{
			FrmGotoForm(FormMenu);
			return true;
		}
	}
	return false;
}

void RenderHighScores()
{
	UInt8 i;
	Char scoreLine[31];
	UInt8 scoreLineLength;
	for (i = 0; i < highScores.numScores; i++)
	{
		scoreLineLength = StrPrintF(scoreLine, "%d: %s - %d", i + 1, highScores.scores[i].name, highScores.scores[i].score);
		WinDrawChars(scoreLine, scoreLineLength, 20, 40 + i * 12);
	}
}

UInt8 PlayFormHandleEvent(EventPtr e)
{
	UInt8 i;
	UInt8 clawBitmapIndex;
	UInt8 timeInSeconds;
	UInt8 timeStringLen; // timeStringLength is defined as 9 in (PalmSDK)/include/Core/System/DateTime.h
	UInt8 scoreStringLength;
	UInt32 curTicks = TimGetTicks();
	double delta = (curTicks - lastTicks) / (sysTicksPerSecond * 1.0);
	Char timeString[9];
	Char scoreString[12];

	if (time > curTicks - lastTicks)
	{
		time -= curTicks - lastTicks;
	}
	else
	{
		time = 0;
	}
	lastTicks = curTicks;

	startDrawOffscreen();
	switch (e->eType)
	{
	case nilEvent:
		DrawGemBuffer();

		timeInSeconds = time / sysTicksPerSecond;
		timeStringLen = StrPrintF(timeString, "Time: %d", timeInSeconds);
		scoreStringLength = StrPrintF(scoreString, "Score: %d", score);

		WinDrawChars(timeString, timeStringLen, 0, 0);
		WinDrawChars(scoreString, scoreStringLength, 110, 0);

		if (extendAmount == 0)
		{
			// move grabber angle
			if (heldGemIndex > -1)
			{
				GrabGem(heldGemIndex);
			}

			if (time == 0)
			{
				FrmGotoForm(FormGameOver);
			}

			swingTimer++;
			swingPosition = sin(swingTimer / 10.0) * QUARTER_PI + HALF_PI;
		}
		else
		{
			if (extending)
			{
				extendAmount += baseExtendSpeed * extendSpeed * delta;
				if (swingX < 0 || swingX >= 160 || swingY >= 144)
				{
					extending = false;
				}
				for (i = 0; i < numGems; i++)
				{
					if (swingX - gems[i].x > -4 && swingX - gems[i].x < 4 && swingY - gems[i].y > -4 && swingY - gems[i].y < 4)
					{
						heldGemIndex = i;
						extending = false;
						extendSpeed = 4.0 / (gems[heldGemIndex].size / 10);
						EraseGem(heldGemIndex);
					}
				}
			}
			else
			{
				extendAmount -= baseExtendSpeed * extendSpeed * delta;
				if (extendAmount < 0)
				{
					extendSpeed = 1.0;
					extendAmount = 0;
				}
			}
		}
		swingX = floor((16.0 + extendAmount) * cos(swingPosition)) + 80;
		swingY = floor((16.0 + extendAmount) * sin(swingPosition));

		clawBitmapIndex = (swingPosition - QUARTER_PI) / HALF_PI * 5;

		if (extendAmount == 0)
		{
			WinDrawLine(80, 0, (swingX - 80) * 2 + 80, swingY * 2);
		}
		else
		{
			WinDrawLine(80, 0, swingX, swingY);
		}
		if (heldGemIndex > -1)
		{
			WinDrawBitmap(clawClosedBitmaps[clawBitmapIndex], swingX - 4, swingY - 4);
			WinDrawBitmap(gemBitmaps[gems[heldGemIndex].size / 10 - 4], swingX + (4 - gems[heldGemIndex].size / 10) / 2, swingY + 4);
		}
		else
		{
			WinDrawBitmap(clawOpenBitmaps[clawBitmapIndex], swingX - 4, swingY - 4);
		}

		if (time > STARTING_TIME)
		{
			WinDrawBitmap(countdownBitmaps[(time - STARTING_TIME) / sysTicksPerSecond], 64, 64);
		}

		break;
	case penDownEvent:
		if (extendAmount == 0 && heldGemIndex == -1 && time < STARTING_TIME)
		{
			extendAmount = 1;
			extending = true;
		}
		break;
	default:
		break;
	}
	endDrawOffscreen();
	flipDisplay();
	return false;
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
	short err;
	Err retcode;
	EventType e;
	FormType *pfrm;
	UInt16 useCount;
	long nextTickCount, tickCount;
	int intervalRemaining;
	UInt16 scoreLabelIndex;
	MemPtr scoreLabelPointer;
	Char scoreString[5];

	if (cmd == sysAppLaunchCmdNormalLaunch) // Make sure only react to NormalLaunch, not Reset, Beam, Find, GoTo...
	{
		err = SysLibFind(MathLibName, &MathLibRef);
		if (err)
			err = SysLibLoad(LibType, MathLibCreator, &MathLibRef);
		ErrFatalDisplayIf(err, "Can't find MathLib");
		err = MathLibOpen(MathLibRef, MathLibVersion);
		ErrFatalDisplayIf(err, "Can't open MathLib");

		FrmGotoForm(FormMenu);
		if ((retcode = InitialSetup()) != 0)
		{
			return retcode;
		}

		nextTickCount = TimGetTicks() + INTERVAL;

		while (1)
		{
			intervalRemaining = nextTickCount - TimGetTicks();
			if (intervalRemaining < 0)
			{
				intervalRemaining = 0;
			}
			if (intervalRemaining > INTERVAL)
			{
				intervalRemaining = INTERVAL;
			}
			EvtGetEvent(&e, intervalRemaining);
			if (SysHandleEvent(&e))
				continue;
			if (MenuHandleEvent((void *)0, &e, &err))
				continue;

			switch (e.eType)
			{
			case frmLoadEvent:
				pfrm = FrmInitForm(e.data.frmLoad.formID);
				FrmSetActiveForm(pfrm);
				switch (e.data.frmLoad.formID)
				{
				case FormMenu:
					FrmSetEventHandler(pfrm, MenuFormHandleEvent);
					break;
				case FormPlay:
					ResetGame();
					FrmSetEventHandler(pfrm, PlayFormHandleEvent);
					break;
				case FormHowto:
					FrmSetEventHandler(pfrm, HowtoFormHandleEvent);
					break;
				case FormGameOver:
					scoreLabelIndex = FrmGetObjectIndex(pfrm, LabelScore);
					scoreLabelPointer = FrmGetObjectPtr(pfrm, scoreLabelIndex);
					CtlSetLabel(scoreLabelPointer, StrIToA(scoreString, score));
					FrmSetEventHandler(pfrm, GameOverFormHandleEvent);
					break;
				case FormScores:
					SelectRecord(0);
					FrmSetEventHandler(pfrm, ScoresFormHandleEvent);
					break;
				}
				break;

			case frmOpenEvent:
				pfrm = FrmGetActiveForm();
				FrmDrawForm(pfrm);
				switch (e.data.frmOpen.formID)
				{
				case FormPlay:
					SetUpGems();
					RenderGems();
					break;
				case FormScores:
					RenderHighScores();
					break;
				}
				break;

			case menuEvent:
				break;

			case appStopEvent:
				goto _quit;

			default:
				if (FrmGetActiveForm())
					FrmDispatchEvent(&e);
			}

			tickCount = TimGetTicks();
			if (tickCount >= nextTickCount)
			{
				nextTickCount = ((tickCount + nextTickCount) / 2) + INTERVAL;
			}
		}

	_quit:
		if (MathLibRef != sysInvalidRefNum)
		{
			MathLibClose(MathLibRef, &useCount);
			if (!useCount)
			{
				SysLibRemove(MathLibRef);
			}
		}
		TearDownBitmaps();
		FrmCloseAllForms();

		DmCloseDatabase(db);
	}

	return 0;
}
