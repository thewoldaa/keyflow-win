// ---------------------------------------------------------------------------
// Resource identifiers.
//
// The interface HTML is compiled into the executable as an RCDATA resource.
// That is what makes the app a single file with no directory of assets to go
// missing, and it is why the page is loaded with NavigateToString rather than
// from a path.
// ---------------------------------------------------------------------------

#pragma once

/// The interface. Referenced by app.rc and by the code that loads it.
#define IDR_UI_HTML 101

/// The application icon.
#define IDI_APP 102
