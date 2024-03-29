/*
 * refit/menu.c
 * Menu functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications copyright (c) 2012 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 *
 */

#include "global.h"
#include "screen.h"
#include "lib.h"
#include "menu.h"
#include "config.h"
#include "libeg.h"
#include "libegint.h"
#include "../include/refit_call_wrapper.h"

#include "../include/egemb_back_selected_small.h"
#include "../include/egemb_back_selected_big.h"
#include "../include/egemb_arrow_left.h"
#include "../include/egemb_arrow_right.h"

// other menu definitions

#define MENU_FUNCTION_INIT            (0)
#define MENU_FUNCTION_CLEANUP         (1)
#define MENU_FUNCTION_PAINT_ALL       (2)
#define MENU_FUNCTION_PAINT_SELECTION (3)
#define MENU_FUNCTION_PAINT_TIMEOUT   (4)
#define MENU_FUNCTION_PAINT_HINTS     (5)

typedef VOID (*MENU_STYLE_FUNC)(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText);

static CHAR16 ArrowUp[2] = { ARROW_UP, 0 };
static CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };
static UINTN TileSizes[2] = { 144, 64 };

// Text and icon spacing constants....
#define TEXT_YMARGIN (2)
#define TITLEICON_SPACING (16)

#define TILE_XSPACING (8)
#define TILE_YSPACING (16)

// Alignment values for PaintIcon()
#define ALIGN_RIGHT 1
#define ALIGN_LEFT 0

static EG_IMAGE *SelectionImages[2] = { NULL, NULL };
static EG_PIXEL SelectionBackgroundPixel = { 0xff, 0xff, 0xff, 0 };

//
// Graphics helper functions
//

static VOID InitSelection(VOID)
{
    EG_IMAGE    *TempSmallImage = NULL, *TempBigImage = NULL;
    BOOLEAN     LoadedSmallImage = FALSE;

    if (!AllowGraphicsMode)
        return;
    if (SelectionImages[0] != NULL)
        return;

    // load small selection image
    if (GlobalConfig.SelectionSmallFileName != NULL) {
        TempSmallImage = egLoadImage(SelfDir, GlobalConfig.SelectionSmallFileName, TRUE);
    }
    if (TempSmallImage == NULL)
        TempSmallImage = egPrepareEmbeddedImage(&egemb_back_selected_small, TRUE);
    else
       LoadedSmallImage = TRUE;
    SelectionImages[1] = egScaleImage(TempSmallImage, TileSizes[1], TileSizes[1]);

    // load big selection image
    if (GlobalConfig.SelectionBigFileName != NULL) {
        TempBigImage = egLoadImage(SelfDir, GlobalConfig.SelectionBigFileName, TRUE);
    }
    if (TempBigImage == NULL) {
        if (LoadedSmallImage) {
           // calculate big selection image from small one
           TempBigImage = egCopyImage(TempSmallImage);
        } else {
           TempBigImage = egPrepareEmbeddedImage(&egemb_back_selected_big, TRUE);
        }
    }
    SelectionImages[0] = egScaleImage(TempBigImage, TileSizes[0], TileSizes[0]);

    if (TempSmallImage)
       egFreeImage(TempSmallImage);
    if (TempBigImage)
       egFreeImage(TempBigImage);
} // VOID InitSelection()

//
// Scrolling functions
//

static VOID InitScroll(OUT SCROLL_STATE *State, IN UINTN ItemCount, IN UINTN VisibleSpace)
{
    State->PreviousSelection = State->CurrentSelection = 0;
    State->MaxIndex = (INTN)ItemCount - 1;
    State->FirstVisible = 0;
    if (AllowGraphicsMode) {
       State->MaxVisible = UGAWidth / (TileSizes[0] + TILE_XSPACING) - 1;
    } else
       State->MaxVisible = ConHeight - 4;
    if ((VisibleSpace > 0) && (VisibleSpace < State->MaxVisible))
        State->MaxVisible = (INTN)VisibleSpace;
    State->PaintAll = TRUE;
    State->PaintSelection = FALSE;

    State->LastVisible = State->FirstVisible + State->MaxVisible - 1;
}

// Adjust variables relating to the scrolling of tags, for when a selected icon isn't
// visible given the current scrolling condition....
static VOID AdjustScrollState(IN SCROLL_STATE *State) {
   if (State->CurrentSelection > State->LastVisible) {
      State->LastVisible = State->CurrentSelection;
      State->FirstVisible = 1 + State->CurrentSelection - State->MaxVisible;
      if (State->FirstVisible < 0) // shouldn't happen, but just in case....
         State->FirstVisible = 0;
      State->PaintAll = TRUE;
   } // Scroll forward
   if (State->CurrentSelection < State->FirstVisible) {
      State->FirstVisible = State->CurrentSelection;
      State->LastVisible = State->CurrentSelection + State->MaxVisible - 1;
      State->PaintAll = TRUE;
   } // Scroll backward
} // static VOID AdjustScrollState

static VOID UpdateScroll(IN OUT SCROLL_STATE *State, IN UINTN Movement)
{
    State->PreviousSelection = State->CurrentSelection;

    switch (Movement) {
        case SCROLL_LINE_LEFT:
            if (State->CurrentSelection > 0) {
                State->CurrentSelection --;
            }
            break;

        case SCROLL_LINE_RIGHT:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection ++;
            }
            break;

        case SCROLL_LINE_UP:
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
               if (State->CurrentSelection >= State->InitialRow1) {
                  if (State->MaxIndex > State->InitialRow1) { // avoid division by 0!
                     State->CurrentSelection = State->FirstVisible + (State->LastVisible - State->FirstVisible) *
                                               (State->CurrentSelection - State->InitialRow1) /
                                               (State->MaxIndex - State->InitialRow1);
                  } else {
                     State->CurrentSelection = State->FirstVisible;
                  } // if/else
               } // if in second row
            } else {
               if (State->CurrentSelection > 0)
                  State->CurrentSelection--;
            } // if/else
            break;

        case SCROLL_LINE_DOWN:
           if (State->ScrollMode == SCROLL_MODE_ICONS) {
               if (State->CurrentSelection <= State->FinalRow0) {
                  if (State->LastVisible > State->FirstVisible) { // avoid division by 0!
                     State->CurrentSelection = State->InitialRow1 + (State->MaxIndex - State->InitialRow1) *
                                               (State->CurrentSelection - State->FirstVisible) /
                                               (State->LastVisible - State->FirstVisible);
                  } else {
                     State->CurrentSelection = State->InitialRow1;
                  } // if/else
               } // if in first row
            } else {
               if (State->CurrentSelection < State->MaxIndex)
                  State->CurrentSelection++;
            } // if/else
            break;

        case SCROLL_PAGE_UP:
           if (State->CurrentSelection <= State->FinalRow0)
              State->CurrentSelection -= State->MaxVisible;
           else if (State->CurrentSelection == State->InitialRow1)
              State->CurrentSelection = State->FinalRow0;
           else
              State->CurrentSelection = State->InitialRow1;
           if (State->CurrentSelection < 0)
              State->CurrentSelection = 0;
           break;

        case SCROLL_FIRST:
           if (State->CurrentSelection > 0) {
              State->PaintAll = TRUE;
              State->CurrentSelection = 0;
           }
           break;

        case SCROLL_PAGE_DOWN:
           if (State->CurrentSelection < State->FinalRow0) {
              State->CurrentSelection += State->MaxVisible;
              if (State->CurrentSelection > State->FinalRow0)
                 State->CurrentSelection = State->FinalRow0;
           } else if (State->CurrentSelection == State->FinalRow0) {
              State->CurrentSelection++;
           } else {
              State->CurrentSelection = State->MaxIndex;
           }
           if (State->CurrentSelection > State->MaxIndex)
              State->CurrentSelection = State->MaxIndex;
           break;

        case SCROLL_LAST:
           if (State->CurrentSelection < State->MaxIndex) {
              State->PaintAll = TRUE;
              State->CurrentSelection = State->MaxIndex;
           }
           break;

        case SCROLL_NONE:
            break;

    }
    if (State->ScrollMode == SCROLL_MODE_TEXT)
       AdjustScrollState(State);

    if (!State->PaintAll && State->CurrentSelection != State->PreviousSelection)
        State->PaintSelection = TRUE;
    State->LastVisible = State->FirstVisible + State->MaxVisible - 1;
} // static VOID UpdateScroll()

//
// menu helper functions
//

VOID AddMenuInfoLine(IN REFIT_MENU_SCREEN *Screen, IN CHAR16 *InfoLine)
{
    AddListElement((VOID ***) &(Screen->InfoLines), &(Screen->InfoLineCount), InfoLine);
}

VOID AddMenuEntry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry)
{
    AddListElement((VOID ***) &(Screen->Entries), &(Screen->EntryCount), Entry);
}


static INTN FindMenuShortcutEntry(IN REFIT_MENU_SCREEN *Screen, IN CHAR16 *Defaults)
{
    UINTN i, j = 0;
    CHAR16 *Shortcut;

    while ((Shortcut = FindCommaDelimited(Defaults, j)) != NULL) {
       if (StrLen(Shortcut) == 1) {
         if (Shortcut[0] >= 'a' && Shortcut[0] <= 'z')
            Shortcut[0] -= ('a' - 'A');
         if (Shortcut[0]) {
            for (i = 0; i < Screen->EntryCount; i++) {
                  if (Screen->Entries[i]->ShortcutDigit == Shortcut[0] || Screen->Entries[i]->ShortcutLetter == Shortcut[0]) {
                     MyFreePool(Shortcut);
                     return i;
                  } // if
            } // for
         } // if
       } else if (StrLen(Shortcut) > 1) {
          for (i = 0; i < Screen->EntryCount; i++) {
             if (StriSubCmp(Shortcut, Screen->Entries[i]->Title)) {
                MyFreePool(Shortcut);
                return i;
             } // if
          } // for
       }
       MyFreePool(Shortcut);
       j++;
    } // while()
    return -1;
}

// Identify the end of row 0 and the beginning of row 1; store the results in the
// appropriate fields in State. Also reduce MaxVisible if that value is greater
// than the total number of row-0 tags and if we're in an icon-based screen
static VOID IdentifyRows(IN SCROLL_STATE *State, IN REFIT_MENU_SCREEN *Screen) {
   UINTN i;

   State->FinalRow0 = 0;
   State->InitialRow1 = State->MaxIndex;
   for (i = 0; i <= State->MaxIndex; i++) {
      if (Screen->Entries[i]->Row == 0) {
         State->FinalRow0 = i;
      } else if ((Screen->Entries[i]->Row == 1) && (State->InitialRow1 > i)) {
         State->InitialRow1 = i;
      } // if/else
   } // for
   if ((State->ScrollMode == SCROLL_MODE_ICONS) && (State->MaxVisible > (State->FinalRow0 + 1)))
      State->MaxVisible = State->FinalRow0 + 1;
} // static VOID IdentifyRows()

// Blank the screen, wait for a keypress, and restore banner/background.
// Screen may still require redrawing of text and icons on return.
// TODO: Support more sophisticated screen savers, such as power-saving
// mode and dynamic images.
static VOID SaveScreen(VOID) {
   UINTN index;
   EG_PIXEL Black = { 0x0, 0x0, 0x0, 0 };

   egClearScreen(&Black);
   refit_call3_wrapper(BS->WaitForEvent, 1, &ST->ConIn->WaitForKey, &index);
   if (AllowGraphicsMode)
      SwitchToGraphicsAndClear();
   ReadAllKeyStrokes();
} // VOID SaveScreen()

//
// generic menu function
//
static UINTN RunGenericMenu(IN REFIT_MENU_SCREEN *Screen, IN MENU_STYLE_FUNC StyleFunc, IN OUT INTN *DefaultEntryIndex,
                            OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    SCROLL_STATE State;
    EFI_STATUS Status;
    EFI_INPUT_KEY key;
    UINTN index;
    INTN ShortcutEntry;
    BOOLEAN HaveTimeout = FALSE;
    UINTN TimeoutCountdown = 0;
    INTN PreviousTime = -1, CurrentTime, TimeSinceKeystroke = 0;
    CHAR16 TimeoutMessage[256];
    CHAR16 KeyAsString[2];
    UINTN MenuExit;
//     EG_PIXEL Black = { 0x0, 0x0, 0x0, 0 };

    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    }
    MenuExit = 0;

    StyleFunc(Screen, &State, MENU_FUNCTION_INIT, NULL);
    IdentifyRows(&State, Screen);
    // override the starting selection with the default index, if any
    if (*DefaultEntryIndex >= 0 && *DefaultEntryIndex <= State.MaxIndex) {
        State.CurrentSelection = *DefaultEntryIndex;
        if (GlobalConfig.ScreensaverTime != -1)
           UpdateScroll(&State, SCROLL_NONE);
    }
    if (GlobalConfig.ScreensaverTime != -1)
        State.PaintAll = TRUE;

    while (!MenuExit) {
        // update the screen
        if (State.PaintAll && (GlobalConfig.ScreensaverTime != -1)) {
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_ALL, NULL);
            State.PaintAll = FALSE;
        } else if (State.PaintSelection) {
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_SELECTION, NULL);
            State.PaintSelection = FALSE;
        }

        if (HaveTimeout) {
            CurrentTime = (TimeoutCountdown + 5) / 10;
            if (CurrentTime != PreviousTime) {
               SPrint(TimeoutMessage, 255, L"%s in %d seconds", Screen->TimeoutText, CurrentTime);
               if (GlobalConfig.ScreensaverTime != -1)
                  StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, TimeoutMessage);
               PreviousTime = CurrentTime;
            }
        }

        // read key press (and wait for it if applicable)
        Status = refit_call2_wrapper(ST->ConIn->ReadKeyStroke, ST->ConIn, &key);
        if (Status != EFI_SUCCESS) {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // timeout expired
                MenuExit = MENU_EXIT_TIMEOUT;
                break;
            } else if (HaveTimeout) {
                refit_call1_wrapper(BS->Stall, 100000); // Pause for 100 ms
                TimeoutCountdown--;
                TimeSinceKeystroke++;
            } else if (GlobalConfig.ScreensaverTime > 0) {
                refit_call1_wrapper(BS->Stall, 100000); // Pause for 100 ms
                TimeSinceKeystroke++;
                if (TimeSinceKeystroke > (GlobalConfig.ScreensaverTime * 10)) {
                   SaveScreen();
                   State.PaintAll = TRUE;
                   TimeSinceKeystroke = 0;
                } // if
            } else {
                refit_call3_wrapper(BS->WaitForEvent, 1, &ST->ConIn->WaitForKey, &index);
            }
            continue;
        } else {
           TimeSinceKeystroke = 0;
        } // if/else !read keystroke

        if (HaveTimeout) {
            // the user pressed a key, cancel the timeout
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
            HaveTimeout = FALSE;
            if (GlobalConfig.ScreensaverTime == -1) { // cancel start-with-blank-screen coding
               GlobalConfig.ScreensaverTime = 0;
               if (!GlobalConfig.TextOnly)
                 BltClearScreen(TRUE);
            }
        }

        // react to key press
        switch (key.ScanCode) {
            case SCAN_UP:
                UpdateScroll(&State, SCROLL_LINE_UP);
                break;
            case SCAN_LEFT:
                UpdateScroll(&State, SCROLL_LINE_LEFT);
                break;
            case SCAN_DOWN:
                UpdateScroll(&State, SCROLL_LINE_DOWN);
                break;
            case SCAN_RIGHT:
                UpdateScroll(&State, SCROLL_LINE_RIGHT);
                break;
            case SCAN_HOME:
                UpdateScroll(&State, SCROLL_FIRST);
                break;
            case SCAN_END:
                UpdateScroll(&State, SCROLL_LAST);
                break;
            case SCAN_PAGE_UP:
                UpdateScroll(&State, SCROLL_PAGE_UP);
                break;
            case SCAN_PAGE_DOWN:
                UpdateScroll(&State, SCROLL_PAGE_DOWN);
                break;
            case SCAN_ESC:
                MenuExit = MENU_EXIT_ESCAPE;
                break;
            case SCAN_INSERT:
            case SCAN_F2:
                MenuExit = MENU_EXIT_DETAILS;
                break;
            case SCAN_F10:
                egScreenShot();
                break;
            case 0x0016: // F12
               if (EjectMedia())
                  MenuExit = MENU_EXIT_ESCAPE;
               break;
        }
        switch (key.UnicodeChar) {
            case CHAR_LINEFEED:
            case CHAR_CARRIAGE_RETURN:
            case ' ':
                MenuExit = MENU_EXIT_ENTER;
                break;
            case '+':
                MenuExit = MENU_EXIT_DETAILS;
                break;
            default:
                KeyAsString[0] = key.UnicodeChar;
                KeyAsString[1] = 0;
                ShortcutEntry = FindMenuShortcutEntry(Screen, KeyAsString);
                if (ShortcutEntry >= 0) {
                    State.CurrentSelection = ShortcutEntry;
                    MenuExit = MENU_EXIT_ENTER;
                }
                break;
        }
    }

    StyleFunc(Screen, &State, MENU_FUNCTION_CLEANUP, NULL);

    if (ChosenEntry)
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    *DefaultEntryIndex = State.CurrentSelection;
    return MenuExit;
} /* static UINTN RunGenericMenu() */

//
// text-mode generic style
//

// Show information lines in text mode.
static VOID ShowTextInfoLines(IN REFIT_MENU_SCREEN *Screen) {
   INTN i;

   BeginTextScreen(Screen->Title);
   if (Screen->InfoLineCount > 0) {
      refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
      for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
         refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 3, 4 + i);
         refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, Screen->InfoLines[i]);
      }
   }
} // VOID ShowTextInfoLines()

// Do most of the work for text-based menus....
static VOID TextMenuStyle(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText)
{
    INTN i;
    UINTN MenuWidth, ItemWidth, MenuHeight;
    static UINTN MenuPosY;
    static CHAR16 **DisplayStrings;
    CHAR16 TimeoutMessage[256];

    State->ScrollMode = SCROLL_MODE_TEXT;
    switch (Function) {

        case MENU_FUNCTION_INIT:
            // vertical layout
            MenuPosY = 4;
            if (Screen->InfoLineCount > 0)
                MenuPosY += Screen->InfoLineCount + 1;
            MenuHeight = ConHeight - MenuPosY - 3;
            if (Screen->TimeoutSeconds > 0)
                MenuHeight -= 2;
            InitScroll(State, Screen->EntryCount, MenuHeight);

            // determine width of the menu
            MenuWidth = 20;  // minimum
            for (i = 0; i <= State->MaxIndex; i++) {
                ItemWidth = StrLen(Screen->Entries[i]->Title);
                if (MenuWidth < ItemWidth)
                    MenuWidth = ItemWidth;
            }
            MenuWidth += 2;
            if (MenuWidth > ConWidth - 3)
                MenuWidth = ConWidth - 3;

            // prepare strings for display
            DisplayStrings = AllocatePool(sizeof(CHAR16 *) * Screen->EntryCount);
            for (i = 0; i <= State->MaxIndex; i++) {
                // Note: Theoretically, SPrint() is a cleaner way to do this; but the
                // description of the StrSize parameter to SPrint implies it's measured
                // in characters, but in practice both TianoCore and GNU-EFI seem to
                // use bytes instead, resulting in truncated displays. I could just
                // double the size of the StrSize parameter, but that seems unsafe in
                // case a future library change starts treating this as characters, so
                // I'm doing it the hard way in this instance.
                // TODO: Review the above and possibly change other uses of SPrint()
                DisplayStrings[i] = AllocateZeroPool(2 * sizeof(CHAR16));
                DisplayStrings[i][0] = L' ';
                MergeStrings(&DisplayStrings[i], Screen->Entries[i]->Title, 0);
                if (StrLen(DisplayStrings[i]) > MenuWidth)
                   DisplayStrings[i][MenuWidth - 1] = 0;
                // TODO: use more elaborate techniques for shortening too long strings (ellipses in the middle)
                // TODO: account for double-width characters
            } // for

            break;

        case MENU_FUNCTION_CLEANUP:
            // release temporary memory
            for (i = 0; i <= State->MaxIndex; i++)
                MyFreePool(DisplayStrings[i]);
            MyFreePool(DisplayStrings);
            break;

        case MENU_FUNCTION_PAINT_ALL:
            // paint the whole screen (initially and after scrolling)

            ShowTextInfoLines(Screen);
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 2, MenuPosY + (i - State->FirstVisible));
                    if (i == State->CurrentSelection)
                       refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_CHOICE_CURRENT);
                    else
                       refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_CHOICE_BASIC);
                    refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, DisplayStrings[i]);
                }
            }
            // scrolling indicators
            refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_SCROLLARROW);
            refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 0, MenuPosY);
            if (State->FirstVisible > 0)
                refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, ArrowUp);
            else
               refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, L" ");
            refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 0, MenuPosY + State->MaxVisible);
            if (State->LastVisible < State->MaxIndex)
               refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, ArrowDown);
            else
               refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, L" ");
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
               if (Screen->Hint1 != NULL) {
                  refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 0, ConHeight - 2);
                  refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, Screen->Hint1);
               }
               if (Screen->Hint2 != NULL) {
                  refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 0, ConHeight - 1);
                  refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, Screen->Hint2);
               }
            }
            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 2,
                                MenuPosY + (State->PreviousSelection - State->FirstVisible));
            refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_CHOICE_BASIC);
            refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, DisplayStrings[State->PreviousSelection]);
            refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 2,
                                MenuPosY + (State->CurrentSelection - State->FirstVisible));
            refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_CHOICE_CURRENT);
            refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, DisplayStrings[State->CurrentSelection]);
            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // clear message
                refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_BASIC);
                refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 0, ConHeight - 3);
                refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, BlankLine + 1);
            } else {
                // paint or update message
                refit_call2_wrapper(ST->ConOut->SetAttribute, ST->ConOut, ATTR_ERROR);
                refit_call3_wrapper(ST->ConOut->SetCursorPosition, ST->ConOut, 3, ConHeight - 3);
                SPrint(TimeoutMessage, 255, L"%s  ", ParamText);
                refit_call2_wrapper(ST->ConOut->OutputString, ST->ConOut, TimeoutMessage);
            }
            break;

    }
}

//
// graphical generic style
//

inline static UINTN TextLineHeight(VOID) {
   return egGetFontHeight() + TEXT_YMARGIN * 2;
} // UINTN TextLineHeight()

//
// Display a submenu
//

// Display text with a solid background (MenuBackgroundPixel or SelectionBackgroundPixel).
// Indents text by one character and placed TEXT_YMARGIN pixels down from the
// specified XPos and YPos locations.
static VOID DrawText(IN CHAR16 *Text, IN BOOLEAN Selected, IN UINTN FieldWidth, IN UINTN XPos, IN UINTN YPos)
{
   EG_IMAGE *TextBuffer;
   EG_PIXEL Bg;

   TextBuffer = egCreateImage(FieldWidth, TextLineHeight(), FALSE);

   egFillImage(TextBuffer, &MenuBackgroundPixel);
   Bg = MenuBackgroundPixel;
   if (Selected) {
       // draw selection bar background
       egFillImageArea(TextBuffer, 0, 0, FieldWidth, TextBuffer->Height, &SelectionBackgroundPixel);
       Bg = SelectionBackgroundPixel;
   }

   // render the text
   egRenderText(Text, TextBuffer, egGetFontCellWidth(), TEXT_YMARGIN, (Bg.r + Bg.g + Bg.b) / 3);
   egDrawImageWithTransparency(TextBuffer, NULL, XPos, YPos, TextBuffer->Width, TextBuffer->Height);
//    BltImage(TextBuffer, XPos, YPos);
}

// Finds the average brightness of the input Image.
// NOTE: Passing an Image that covers the whole screen can strain the
// capacity of a UINTN on a 32-bit system with a very large display.
// Using UINT64 instead is unworkable, since the code won't compile
// on a 32-bit system. As the intended use for this function is to handle
// a single text string's background, this shouldn't be a problem, but it
// may need addressing if it's applied more broadly....
static UINT8 AverageBrightness(EG_IMAGE *Image) {
   UINTN i;
   UINTN Sum = 0;

   if (Image != NULL) {
      for (i = 0; i < (Image->Width * Image->Height); i++) {
         Sum += (Image->PixelData[i].r + Image->PixelData[i].g + Image->PixelData[i].b);
      }
   } // if
   return (UINT8) (Sum / (Image->Width * Image->Height * 3));
} // UINT8 AverageBrightness()

// Display text against the screen's background image. Special case: If Text is NULL
// or 0-length, clear the line. Does NOT indent the text or reposition it relative
// to the specified XPos and YPos values.
static VOID DrawTextWithTransparency(IN CHAR16 *Text, IN UINTN XPos, IN UINTN YPos)
{
    UINTN TextWidth;
    EG_IMAGE *TextBuffer = NULL;

    if (Text == NULL)
       Text = L"";

    egMeasureText(Text, &TextWidth, NULL);
    if (TextWidth == 0) {
       TextWidth = UGAWidth;
       XPos = 0;
    }

    TextBuffer = egCropImage(GlobalConfig.ScreenBackground, XPos, YPos, TextWidth, TextLineHeight());
    if (TextBuffer == NULL)
       return;

    // render the text
    egRenderText(Text, TextBuffer, 0, 0, AverageBrightness(TextBuffer));
    egDrawImageWithTransparency(TextBuffer, NULL, XPos, YPos, TextBuffer->Width, TextBuffer->Height);
    egFreeImage(TextBuffer);
}

// Compute the size & position of the window that will hold a subscreen's information.
static VOID ComputeSubScreenWindowSize(REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, UINTN *XPos, UINTN *YPos,
                                       UINTN *Width, UINTN *Height, UINTN *LineWidth) {
   UINTN i, ItemWidth, HintTop, BannerBottomEdge, TitleWidth;
   UINTN FontCellWidth = egGetFontCellWidth();
   UINTN FontCellHeight = egGetFontHeight();

   *Width = 20;
   *Height = 5;
   TitleWidth = egComputeTextWidth(Screen->Title);

   for (i = 0; i < Screen->InfoLineCount; i++) {
       ItemWidth = StrLen(Screen->InfoLines[i]);
       if (*Width < ItemWidth) {
           *Width = ItemWidth;
       }
       (*Height)++;
   }
   for (i = 0; i <= State->MaxIndex; i++) {
       ItemWidth = StrLen(Screen->Entries[i]->Title);
       if (*Width < ItemWidth) {
           *Width = ItemWidth;
       }
       (*Height)++;
   }
   *Width = (*Width + 2) * FontCellWidth;
   *LineWidth = *Width;
   if (Screen->TitleImage)
      *Width += (Screen->TitleImage->Width + TITLEICON_SPACING * 2 + FontCellWidth);
   else
      *Width += FontCellWidth;

   if (*Width < TitleWidth)
      *Width = TitleWidth + 2 * FontCellWidth;

   // Keep it within the bounds of the screen, or 2/3 of the screen's width
   // for screens over 800 pixels wide
   if (*Width > UGAWidth)
      *Width = UGAWidth;

   *XPos = (UGAWidth - *Width) / 2;

   HintTop = UGAHeight - (FontCellHeight * 3); // top of hint text
   *Height *= TextLineHeight();
   if (Screen->TitleImage && (*Height < (Screen->TitleImage->Height + TextLineHeight() * 4)))
      *Height = Screen->TitleImage->Height + TextLineHeight() * 4;

   if (GlobalConfig.BannerBottomEdge >= HintTop) { // probably a full-screen image; treat it as an empty banner
      BannerBottomEdge = 0;
   } else {
      BannerBottomEdge = GlobalConfig.BannerBottomEdge;
   }
   if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
      BannerBottomEdge = 0;
   }
   if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
      // TODO: Implement scrolling in text screen.
      *Height = (HintTop - BannerBottomEdge - FontCellHeight * 2);
   }

   *YPos = ((UGAHeight - *Height) / 2);
   if (*YPos < BannerBottomEdge)
      *YPos = BannerBottomEdge + FontCellHeight + (HintTop - BannerBottomEdge - *Height) / 2;
} // VOID ComputeSubScreenWindowSize()

// Displays sub-menus
static VOID GraphicsMenuStyle(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText)
{
    INTN i;
    UINTN ItemWidth;
    static UINTN LineWidth, MenuWidth, MenuHeight, EntriesPosX, TitlePosX, EntriesPosY, TimeoutPosY, CharWidth;
    EG_IMAGE *Window;
    EG_PIXEL *BackgroundPixel = &(GlobalConfig.ScreenBackground->PixelData[0]);

    CharWidth = egGetFontCellWidth();
    State->ScrollMode = SCROLL_MODE_TEXT;
    switch (Function) {

        case MENU_FUNCTION_INIT:
            InitScroll(State, Screen->EntryCount, 0);
            ComputeSubScreenWindowSize(Screen, State, &EntriesPosX, &EntriesPosY, &MenuWidth, &MenuHeight, &LineWidth);
            TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TextLineHeight();

            // initial painting
            SwitchToGraphicsAndClear();
            Window = egCreateFilledImage(MenuWidth, MenuHeight, FALSE, BackgroundPixel);
            egDrawImage(Window, EntriesPosX, EntriesPosY);
            ItemWidth = egComputeTextWidth(Screen->Title);
            if (MenuWidth > ItemWidth) {
               TitlePosX = EntriesPosX + (MenuWidth - ItemWidth) / 2 - CharWidth;
            } else {
               TitlePosX = EntriesPosX;
               if (CharWidth > 0) {
                  i = MenuWidth / CharWidth - 2;
                  if (i > 0)
                     Screen->Title[i] = 0;
               } // if
            } // if/else
            break;

        case MENU_FUNCTION_CLEANUP:
            // nothing to do
            break;

        case MENU_FUNCTION_PAINT_ALL:
           ComputeSubScreenWindowSize(Screen, State, &EntriesPosX, &EntriesPosY, &MenuWidth, &MenuHeight, &LineWidth);
           DrawText(Screen->Title, FALSE, (StrLen(Screen->Title) + 2) * CharWidth, TitlePosX, EntriesPosY += TextLineHeight());
           if (Screen->TitleImage) {
              BltImageAlpha(Screen->TitleImage, EntriesPosX + TITLEICON_SPACING, EntriesPosY + TextLineHeight() * 2,
                            BackgroundPixel);
              EntriesPosX += (Screen->TitleImage->Width + TITLEICON_SPACING * 2);
           }
           EntriesPosY += (TextLineHeight() * 2);
           if (Screen->InfoLineCount > 0) {
               for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
                   DrawText(Screen->InfoLines[i], FALSE, LineWidth, EntriesPosX, EntriesPosY);
                   EntriesPosY += TextLineHeight();
               }
               EntriesPosY += TextLineHeight();  // also add a blank line
           }

           for (i = 0; i <= State->MaxIndex; i++) {
              DrawText(Screen->Entries[i]->Title, (i == State->CurrentSelection), LineWidth, EntriesPosX,
                       EntriesPosY + i * TextLineHeight());
           }
           if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
              if ((Screen->Hint1 != NULL) && (StrLen(Screen->Hint1) > 0))
                 DrawTextWithTransparency(Screen->Hint1, (UGAWidth - egComputeTextWidth(Screen->Hint1)) / 2,
                                          UGAHeight - (egGetFontHeight() * 3));
              if ((Screen->Hint2 != NULL) && (StrLen(Screen->Hint2) > 0))
                 DrawTextWithTransparency(Screen->Hint2, (UGAWidth - egComputeTextWidth(Screen->Hint2)) / 2,
                                           UGAHeight - (egGetFontHeight() * 2));
           } // if
           break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            DrawText(Screen->Entries[State->PreviousSelection]->Title, FALSE, LineWidth,
                     EntriesPosX, EntriesPosY + State->PreviousSelection * TextLineHeight());
            DrawText(Screen->Entries[State->CurrentSelection]->Title, TRUE, LineWidth,
                     EntriesPosX, EntriesPosY + State->CurrentSelection * TextLineHeight());
            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawText(ParamText, FALSE, LineWidth, EntriesPosX, TimeoutPosY);
            break;

    }
} // static VOID GraphicsMenuStyle()

//
// graphical main menu style
//

static VOID DrawMainMenuEntry(REFIT_MENU_ENTRY *Entry, BOOLEAN selected, UINTN XPos, UINTN YPos)
{
   EG_IMAGE *Background;

   if (SelectionImages != NULL) {
      if (selected) {
         Background = egCropImage(GlobalConfig.ScreenBackground, XPos, YPos,
                                  SelectionImages[Entry->Row]->Width, SelectionImages[Entry->Row]->Height);
         egComposeImage(Background, SelectionImages[Entry->Row], 0, 0);
         BltImageCompositeBadge(Background, Entry->Image, Entry->BadgeImage, XPos, YPos);
      } else { // Image not selected; copy background
         egDrawImageWithTransparency(Entry->Image, Entry->BadgeImage, XPos, YPos,
                                     SelectionImages[Entry->Row]->Width, SelectionImages[Entry->Row]->Height);
      } // if/else
   } // if
} // VOID DrawMainMenuEntry()

static VOID PaintAll(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, UINTN *itemPosX,
                     UINTN row0PosY, UINTN row1PosY, UINTN textPosY) {
   INTN i;

   if (Screen->Entries[State->CurrentSelection]->Row == 0)
      AdjustScrollState(State);
   for (i = State->FirstVisible; i <= State->MaxIndex; i++) {
      if (Screen->Entries[i]->Row == 0) {
         if (i <= State->LastVisible) {
            DrawMainMenuEntry(Screen->Entries[i], (i == State->CurrentSelection) ? TRUE : FALSE,
                              itemPosX[i - State->FirstVisible], row0PosY);
         } // if
      } else {
         DrawMainMenuEntry(Screen->Entries[i], (i == State->CurrentSelection) ? TRUE : FALSE, itemPosX[i], row1PosY);
      }
   }
   if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
      DrawTextWithTransparency(L"", 0, textPosY);
      DrawTextWithTransparency(Screen->Entries[State->CurrentSelection]->Title,
                               (UGAWidth - egComputeTextWidth(Screen->Entries[State->CurrentSelection]->Title)) >> 1,
                               textPosY);
   }

   if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
      DrawTextWithTransparency(Screen->Hint1, (UGAWidth - egComputeTextWidth(Screen->Hint1)) / 2,
                               UGAHeight - (egGetFontHeight() * 3));
      DrawTextWithTransparency(Screen->Hint2, (UGAWidth - egComputeTextWidth(Screen->Hint2)) / 2,
                               UGAHeight - (egGetFontHeight() * 2));
   } // if
} // static VOID PaintAll()

// Move the selection to State->CurrentSelection, adjusting icon row if necessary...
static VOID PaintSelection(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, UINTN *itemPosX,
                           UINTN row0PosY, UINTN row1PosY, UINTN textPosY) {
   UINTN XSelectPrev, XSelectCur, YPosPrev, YPosCur;

   if (((State->CurrentSelection <= State->LastVisible) && (State->CurrentSelection >= State->FirstVisible)) ||
       (State->CurrentSelection >= State->InitialRow1) ) {
      if (Screen->Entries[State->PreviousSelection]->Row == 0) {
         XSelectPrev = State->PreviousSelection - State->FirstVisible;
         YPosPrev = row0PosY;
      } else {
         XSelectPrev = State->PreviousSelection;
         YPosPrev = row1PosY;
      } // if/else
      if (Screen->Entries[State->CurrentSelection]->Row == 0) {
         XSelectCur = State->CurrentSelection - State->FirstVisible;
         YPosCur = row0PosY;
      } else {
         XSelectCur = State->CurrentSelection;
         YPosCur = row1PosY;
      } // if/else
      DrawMainMenuEntry(Screen->Entries[State->PreviousSelection], FALSE, itemPosX[XSelectPrev], YPosPrev);
      DrawMainMenuEntry(Screen->Entries[State->CurrentSelection], TRUE, itemPosX[XSelectCur], YPosCur);
      if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
         DrawTextWithTransparency(L"", 0, textPosY);
         DrawTextWithTransparency(Screen->Entries[State->CurrentSelection]->Title,
                                  (UGAWidth - egComputeTextWidth(Screen->Entries[State->CurrentSelection]->Title)) >> 1,
                                  textPosY);
      }
   } else { // Current selection not visible; must redraw the menu....
      MainMenuStyle(Screen, State, MENU_FUNCTION_PAINT_ALL, NULL);
   }
} // static VOID MoveSelection(VOID)

// Display a 48x48 icon at the specified location. Uses the image specified by
// ExternalFilename if it's available, or BuiltInImage if it's not. The 
// Y position is specified as the center value, and so is adjusted by half
// the icon's height. The X position is set along the icon's left
// edge if Alignment == ALIGN_LEFT, and along the right edge if
// Alignment == ALIGN_RIGHT
static VOID PaintIcon(IN EG_EMBEDDED_IMAGE *BuiltInIcon, IN CHAR16 *ExternalFilename, UINTN PosX, UINTN PosY, UINTN Alignment) {
   EG_IMAGE *Icon = NULL;

   Icon = egFindIcon(ExternalFilename, GlobalConfig.IconSizes[ICON_SIZE_SMALL]);
   if (Icon == NULL)
      Icon = egPrepareEmbeddedImage(BuiltInIcon, TRUE);
   if (Icon != NULL) {
      if (Alignment == ALIGN_RIGHT)
         PosX -= Icon->Width;
      egDrawImageWithTransparency(Icon, NULL, PosX, PosY - (Icon->Height / 2), Icon->Width, Icon->Height);
   }
} // static VOID ()

inline UINTN ComputeRow0PosY(VOID) {
   return ((UGAHeight / 2) - TileSizes[0] / 2);
} // UINTN ComputeRow0PosY()

// Display (or erase) the arrow icons to the left and right of an icon's row,
// as appropriate.
static VOID PaintArrows(SCROLL_STATE *State, UINTN PosX, UINTN PosY, UINTN row0Loaders) {
   EG_IMAGE *TempImage;
   UINTN Width, Height, RightX, AdjPosY;

   // NOTE: Assume that left and right arrows are of the same size....
   Width = egemb_arrow_left.Width;
   Height = egemb_arrow_left.Height;
   RightX = (UGAWidth + (TileSizes[0] + TILE_XSPACING) * State->MaxVisible) / 2 + TILE_XSPACING;
   AdjPosY = PosY - (Height / 2);

   // For PaintIcon() calls, the starting Y position is moved to the midpoint
   // of the surrounding row; PaintIcon() adjusts this back up by half the
   // icon's height to properly center it.
   if ((State->FirstVisible > 0) && (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS))) {
      PaintIcon(&egemb_arrow_left, L"arrow_left", PosX, PosY, ALIGN_RIGHT);
   } else {
      TempImage = egCropImage(GlobalConfig.ScreenBackground, PosX - Width, AdjPosY, Width, Height);
      BltImage(TempImage, PosX - Width, AdjPosY);
      egFreeImage(TempImage);
   } // if/else

   if ((State->LastVisible < (row0Loaders - 1)) && (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS))) {
      PaintIcon(&egemb_arrow_right, L"arrow_right", RightX, PosY, ALIGN_LEFT);
   } else {
      TempImage = egCropImage(GlobalConfig.ScreenBackground, RightX, AdjPosY, Width, Height);
      BltImage(TempImage, RightX, AdjPosY);
      egFreeImage(TempImage);
   } // if/else
} // VOID PaintArrows()

// Display main menu in graphics mode
VOID MainMenuStyle(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText)
{
    INTN i;
    static UINTN row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    UINTN row0Count, row1Count, row1PosX, row1PosXRunning;
    static UINTN *itemPosX;
    static UINTN row0PosY, textPosY;

    State->ScrollMode = SCROLL_MODE_ICONS;
    switch (Function) {

        case MENU_FUNCTION_INIT:
            InitScroll(State, Screen->EntryCount, GlobalConfig.MaxTags);

            // layout
            row0Count = 0;
            row1Count = 0;
            row0Loaders = 0;
            for (i = 0; i <= State->MaxIndex; i++) {
               if (Screen->Entries[i]->Row == 1) {
                  row1Count++;
               } else {
                  row0Loaders++;
                  if (row0Count < State->MaxVisible)
                     row0Count++;
               }
            }
            row0PosX = (UGAWidth + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
            row0PosY = ComputeRow0PosY();
            row1PosX = (UGAWidth + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
            row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;
            if (row1Count > 0)
                textPosY = row1PosY + TileSizes[1] + TILE_YSPACING;
            else
                textPosY = row1PosY;

            itemPosX = AllocatePool(sizeof(UINTN) * Screen->EntryCount);
            row0PosXRunning = row0PosX;
            row1PosXRunning = row1PosX;
            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 0) {
                    itemPosX[i] = row0PosXRunning;
                    row0PosXRunning += TileSizes[0] + TILE_XSPACING;
                } else {
                    itemPosX[i] = row1PosXRunning;
                    row1PosXRunning += TileSizes[1] + TILE_XSPACING;
                }
            }
            // initial painting
            InitSelection();
            SwitchToGraphicsAndClear();
            break;

        case MENU_FUNCTION_CLEANUP:
            MyFreePool(itemPosX);
            break;

        case MENU_FUNCTION_PAINT_ALL:
            PaintAll(Screen, State, itemPosX, row0PosY, row1PosY, textPosY);
            // For PaintArrows(), the starting Y position is moved to the midpoint
            // of the surrounding row; PaintIcon() adjusts this back up by half the
            // icon's height to properly center it.
            PaintArrows(State, row0PosX - TILE_XSPACING, row0PosY + (TileSizes[0] / 2), row0Loaders);
            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            PaintSelection(Screen, State, itemPosX, row0PosY, row1PosY, textPosY);
            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
               DrawTextWithTransparency(L"", 0, textPosY + TextLineHeight());
               DrawTextWithTransparency(ParamText, (UGAWidth - egComputeTextWidth(ParamText)) >> 1, textPosY + TextLineHeight());
            }
            break;

    }
} // VOID MainMenuStyle()

// Enable the user to edit boot loader options.
// Returns TRUE if the user exited with edited options; FALSE if the user
// pressed Esc to terminate the edit.
static BOOLEAN EditOptions(LOADER_ENTRY *MenuEntry) {
   UINTN x_max, y_max;
   CHAR16 *EditedOptions;
   BOOLEAN retval = FALSE;

   if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
      return FALSE;
   }

   refit_call4_wrapper(ST->ConOut->QueryMode, ST->ConOut, ST->ConOut->Mode->Mode, &x_max, &y_max);

   if (!GlobalConfig.TextOnly)
      SwitchToText(TRUE);

   if (line_edit(MenuEntry->LoadOptions, &EditedOptions, x_max)) {
      MyFreePool(MenuEntry->LoadOptions);
      MenuEntry->LoadOptions = EditedOptions;
      retval = TRUE;
   } // if
   if (!GlobalConfig.TextOnly)
      SwitchToGraphics();
   return retval;
} // VOID EditOptions()

//
// user-callable dispatcher functions
//

UINTN RunMenu(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    INTN            DefaultEntry = -1;
    MENU_STYLE_FUNC Style = TextMenuStyle;

    if (AllowGraphicsMode)
        Style = GraphicsMenuStyle;

    return RunGenericMenu(Screen, Style, &DefaultEntry, ChosenEntry);
}

UINTN RunMainMenu(REFIT_MENU_SCREEN *Screen, CHAR16** DefaultSelection, REFIT_MENU_ENTRY **ChosenEntry)
{
    MENU_STYLE_FUNC Style = TextMenuStyle;
    MENU_STYLE_FUNC MainStyle = TextMenuStyle;
    REFIT_MENU_ENTRY *TempChosenEntry;
    CHAR16 *MenuTitle;
    UINTN MenuExit = 0;
    INTN DefaultEntryIndex = -1;
    INTN DefaultSubmenuIndex = -1;

    TileSizes[0] = (GlobalConfig.IconSizes[ICON_SIZE_BIG] * 9) / 8;
    TileSizes[1] = (GlobalConfig.IconSizes[ICON_SIZE_SMALL] * 4) / 3;

    if ((DefaultSelection != NULL) && (*DefaultSelection != NULL)) {
        // Find a menu entry that includes *DefaultSelection as a substring
        DefaultEntryIndex = FindMenuShortcutEntry(Screen, *DefaultSelection);
    }

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
        MainStyle = MainMenuStyle;
    }

    while (!MenuExit) {
        MenuExit = RunGenericMenu(Screen, MainStyle, &DefaultEntryIndex, &TempChosenEntry);
        Screen->TimeoutSeconds = 0;

        MenuTitle = StrDuplicate(TempChosenEntry->Title);
        if (MenuExit == MENU_EXIT_DETAILS) {
            if (TempChosenEntry->SubScreen != NULL) {
               MenuExit = RunGenericMenu(TempChosenEntry->SubScreen, Style, &DefaultSubmenuIndex, &TempChosenEntry);
               if (MenuExit == MENU_EXIT_ESCAPE || TempChosenEntry->Tag == TAG_RETURN)
                   MenuExit = 0;
               if (MenuExit == MENU_EXIT_DETAILS) {
                  if (!EditOptions((LOADER_ENTRY *) TempChosenEntry))
                     MenuExit = 0;
               } // if
            } else { // no sub-screen; ignore keypress
               MenuExit = 0;
            }
        } // Enter sub-screen
    }

    if (ChosenEntry)
        *ChosenEntry = TempChosenEntry;
    if (DefaultSelection) {
       MyFreePool(*DefaultSelection);
       *DefaultSelection = MenuTitle;
    } // if
    return MenuExit;
} /* UINTN RunMainMenu() */
