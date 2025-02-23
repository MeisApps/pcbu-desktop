//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// This file contains some global variables that describe what our
// sample tile looks like.  For example, it defines what fields a tile has
// and which fields show in which states of LogonUI. This sample illustrates
// the use of each UI field type.

#pragma once
#include "helpers.h"

// The indexes of each of the fields in our credential provider's tiles. Note that we're
// using each of the nine available field types here.
enum SAMPLE_FIELD_ID {
  SFI_TILEIMAGE = 0,
  SFI_USERNAME = 1,
  SFI_MESSAGE = 2,
  SFI_PASSWORD = 3,
  SFI_SUBMIT_BUTTON = 4,
  SFI_NUM_FIELDS = 5, // Note: if new fields are added, keep NUM_FIELDS last.  This is used as a count of the number of fields
};

// The first value indicates when the tile is displayed (selected, not selected)
// the second indicates things like whether the field is enabled, whether it has key focus, etc.
struct FIELD_STATE_PAIR {
  CREDENTIAL_PROVIDER_FIELD_STATE cpfs;
  CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
};

// These two arrays are seperate because a credential provider might
// want to set up a credential with various combinations of field state pairs
// and field descriptors.

// The field state value indicates whether the field is displayed
// in the selected tile, the deselected tile, or both.
// The Field interactive state indicates when
static const FIELD_STATE_PAIR s_rgFieldStatePairs[] = {
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},             // SFI_TILEIMAGE
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},             // SFI_LABEL
    {CPFS_DISPLAY_IN_BOTH, CPFIS_NONE},             // SFI_LABEL
    {CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_FOCUSED}, // SFI_PASSWORD
    {CPFS_DISPLAY_IN_SELECTED_TILE, CPFIS_NONE},    // SFI_SUBMIT_BUTTON
};
