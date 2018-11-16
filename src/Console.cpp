//////////////////////////////////////////////////////////////////////////
// Console - quick curses-like library for windows by Ryan Ingram
// from http://github.com/ryani/hoppity (GPL licensed there)
// Licensed for non-GPL usage to Valve by permission of the author

#include "pch.h"
#include "Console.h"
#include <conio.h>

Console::Console()
	: mHandle( INVALID_HANDLE_VALUE )
	, mFocusWindow( nullptr )
{}

Console::~Console()
{
	Shutdown();
}

bool Console::Initialize(HANDLE consoleHandle)
{
	if ( mHandle != INVALID_HANDLE_VALUE )
		return false;

	mHandle = consoleHandle;

	CONSOLE_SCREEN_BUFFER_INFO info;
	if (!GetConsoleScreenBufferInfo(mHandle, &info))
		return false;

	// leave off 1 character horizontally to avoid scrolling when writing text to final line (TODO)
	mRect = ConRect( 0, 0, info.dwSize.X - 1, info.dwSize.Y );
	mDefaultAttributes = info.wAttributes;

	// Clear screen
	int consoleSize = ((int)info.dwSize.X) * info.dwSize.Y;
	COORD origin = { 0, 0 };
	DWORD ignored_;
	FillConsoleOutputCharacter(mHandle, TEXT(' '), consoleSize, origin, &ignored_);
	FillConsoleOutputAttribute(mHandle, mDefaultAttributes, consoleSize, origin, &ignored_);

	// Put the cursor on the input line.
	ResetCursor();

	// Set up output.  Resetting the cursor scrolled the window to the bottom possible
	// point, so now we can start at the top of the current screen.
	if (!GetConsoleScreenBufferInfo(mHandle, &info))
		return false;

	// set up screen rectangle
	mScreenRect.x = info.srWindow.Left;
	mScreenRect.y = info.srWindow.Top;
	mScreenRect.w = mRect.w - mScreenRect.x;
	mScreenRect.h = mRect.h - mScreenRect.y;

	return true;
}

void Console::Shutdown()
{
	while ( !mWindows.empty() )
		RemoveWindow( mWindows.back() );

	mHandle = INVALID_HANDLE_VALUE;
}

void Console::Update()
{
	while (_kbhit())
	{
		// HACK: Work around bug in win10 library.  See https://developercommunity.visualstudio.com/content/problem/247770/-getch-broken-in-vs-157.html
		//int key = _getch();
		int key = _getwch();

		// handle arrow keys
		if ( key == 0 || key == 0xe0 )
		{
			int code = _getwch();

			if ( mFocusWindow )
				mFocusWindow->HandleFunctionKey( code );
		}
		else
		{
			if ( mFocusWindow )
				mFocusWindow->HandleKey( key );
		}
	}
}

void Console::SetFocusWindow( ConWindowBase* pWindow )
{
	if ( pWindow && pWindow->mOwner != this )
		return;
	if ( mFocusWindow == pWindow )
		return;

	if ( mFocusWindow )
		mFocusWindow->mbFocus = false;

	if ( pWindow )
		pWindow->mbFocus = true;

	mFocusWindow = pWindow;
	ResetCursor();
}

void Console::AddWindow( ConWindowBase* pWindow )
{
	if ( pWindow->mOwner )
		return;

	mWindows.push_back( pWindow );
	pWindow->mOwner = this;
}

void Console::RemoveWindow( ConWindowBase* pWindow )
{
	if ( pWindow->mOwner != this )
		return;

	if ( mFocusWindow == pWindow )
		SetFocusWindow( nullptr );

	pWindow->mOwner = nullptr;
	mWindows.erase( std::find( mWindows.begin(), mWindows.end(), pWindow ) );
}

void ConWindowBase::ScrollWindow( ConRect moveArea, ConPoint target, wchar_t fillChar )
{
	if ( moveArea.w <= 0 || moveArea.h <= 0 )
		return;

	SMALL_RECT scrollRect = { SHORT( moveArea.x + mPos.x ), SHORT( moveArea.y + mPos.y ), SHORT( moveArea.x + mPos.x + moveArea.w - 1 ), SHORT( moveArea.y + mPos.y + moveArea.h - 1 ) };
	COORD destOrigin = { SHORT( mPos.x + target.x ), SHORT( mPos.y + target.y ) };
	CHAR_INFO chFill;
	chFill.Char.UnicodeChar = fillChar;
	chFill.Attributes = mOwner->mDefaultAttributes;
	ScrollConsoleScreenBuffer( mOwner->mHandle, &scrollRect, nullptr, destOrigin, &chFill );
}

void Console::ResetCursor()
{
	if ( mFocusWindow )
	{
		COORD c;
		c.Y = SHORT( mFocusWindow->mCursor.y + mFocusWindow->mPos.y );
		c.X = SHORT( mFocusWindow->mCursor.x + mFocusWindow->mPos.x );
		SetConsoleCursorPosition( mHandle, c );
	}
	else
	{
		// just place on bottom right of screen
		COORD c;
		c.Y = SHORT( mRect.h - 1 );
		c.X = SHORT( mRect.w );
		SetConsoleCursorPosition( mHandle, c );
	}
}


ConWindowBase::ConWindowBase( Console* owner, ConRect pos )
	: mOwner( nullptr )
	, mbFocus( false )
	, mPos( pos )
	, mCursor()
	, mData( new wchar_t[pos.w * pos.h] )
{
	// clear character data
	int size = pos.w * pos.h;
	for ( int i = 0; i < size; ++i )
	{
		mData[i] = L' ';
	}

	if ( owner )
		owner->AddWindow( this );
}

ConWindowBase::~ConWindowBase()
{
	if ( mOwner )
		mOwner->RemoveWindow( this );
}

void ConWindowBase::SetChar( int row, int col, wchar_t c )
{
	if ( row < 0 || row >= mPos.h || col < 0 || col >= mPos.w )
		return;

	mData[row * mPos.w + col] = c;

	// Write character
	CHAR_INFO ch[1];
	memset( ch, 0, sizeof( ch ) );
	ch[0].Char.UnicodeChar = c;
	ch[0].Attributes = mOwner->mDefaultAttributes;

	col += mPos.x;
	row += mPos.y;

	COORD chSize = { 1, 1 };
	COORD chPos = { 0, 0 };
	SMALL_RECT consoleRect = { SHORT( col ), SHORT( row ), SHORT( col ), SHORT( row ) };
	WriteConsoleOutput( mOwner->mHandle, ch, chSize, chPos, &consoleRect );
}

wchar_t ConWindowBase::GetChar( int row, int col )
{
	if ( row < 0 || row >= mPos.h || col < 0 || col >= mPos.w )
		return 0;

	return mData[row * mPos.w + col];
}

void ConWindowBase::SetCursor( int row, int col )
{
	// explicitly allow placing cursor 1 off the right side (> mPos.w vs >=)
	if ( row < 0 || row >= mPos.h || col < 0 || col > mPos.w )
		return;

	mCursor.y = row;
	mCursor.x = col;

	if ( mbFocus )
		mOwner->ResetCursor();
}

bool ConWindowBase::HasFocus()
{
	return mbFocus;
}

void ConWindowBase::GiveFocus()
{
	mOwner->SetFocusWindow( this );
	//assert(mbFocus);
}

void ConWindowBase::HandleKey( int keyCode )
{
}

void ConWindowBase::HandleFunctionKey( int specialKeyCode )
{
}


ConLineInputWindow::ConLineInputWindow( Console* owner, ConRect pos )
	: ConWindowBase( owner, pos )
	, mCurrentInput()
	, mPendingInput()
{
}

bool ConLineInputWindow::ReadLine( std::string* pStr )
{
	if ( mPendingInput.empty() )
		return false;

	*pStr = mPendingInput.front();
	mPendingInput.pop_front();
	return true;
}

void ConLineInputWindow::HandleKey( int keyCode )
{
	if ( keyCode == '\r' )
	{
		// TODO: convert to utf-8
		std::string encoded_str;
		encoded_str.append( mCurrentInput.begin(), mCurrentInput.end() );

		mPendingInput.emplace_back( std::move( encoded_str ) );
		mCurrentInput.clear();
	}
	else if ( keyCode == '\b' )
	{
		if ( !mCurrentInput.empty() )
			mCurrentInput.pop_back();
	}
	else if ( keyCode >= 0x80 )
	{
		mCurrentInput.push_back( 0x3f ); // TODO: handle unicode
	}
	else
	{
		mCurrentInput.push_back( keyCode );
	}

	UpdateInputLine();
	ResetCursor();
}

void ConLineInputWindow::HandleFunctionKey( int keyCode )
{
	// TODO: implement?
}

void ConLineInputWindow::UpdateInputLine()
{
	int start = 0, len = mCurrentInput.length();
	if ( len > mPos.w )
	{
		start += ( len - mPos.w );
		len = mPos.w;
	}

	int i = 0;
	for ( ; i < len; ++i )
	{
		SetChar( 0, i, mCurrentInput[start + i] );
	}

	for ( ; i < mPos.w; ++i )
	{
		SetChar( 0, i, L' ' );
	}
}

void ConLineInputWindow::ResetCursor()
{
	int len = mCurrentInput.length();
	if ( len > mPos.w )
		len = mPos.w;

	SetCursor( 0, len );
}

ConScrollingTextWindow::ConScrollingTextWindow( Console* owner, ConRect pos )
	: ConWindowBase( owner, pos )
{
}

void ConScrollingTextWindow::Write( const char* text )
{
	while ( *text )
		WriteCharInternal( *text++ );
}

void ConScrollingTextWindow::Clear()
{
	// could use win32 fill for this, but lets do simple way for now
	// to maintain our invariants
	for ( int y = 0; y < mPos.h; ++y )
		for ( int x = 0; x < mPos.w; ++x )
			SetChar( y, x, L' ' );

	mOutputLoc = ConPoint( 0, 0 );
	SetCursor( mOutputLoc.y, mOutputLoc.x );
}

const char* ConScrollingTextWindow::Write_WordWrap_Line( const char* text, int lineLen )
{
	if ( lineLen == 0 )
		lineLen = mPos.w;

	while ( *text )
	{
		// advance to end of spaces
		const char* spaceEnd = text;
		while ( isspace( *spaceEnd ) )
		{
			// always wrap if newline encountered
			if ( *spaceEnd == '\n' )
				return spaceEnd + 1;

			spaceEnd++;
		}

		// always wrap if spaces would wrap a line
		int spaceLen = spaceEnd - text;
		if ( mOutputLoc.x + spaceLen > lineLen )
			return spaceEnd;

		// advance to end of word
		const char* wordEnd = spaceEnd;
		while ( *wordEnd && !isspace( *wordEnd ) )
			++wordEnd;

		int wordLen = wordEnd - spaceEnd;
		int totalLen = wordLen + spaceLen;
		
		// word wrap if word is shorter than a full line and not wrapping would
		// overflow the current line
		if ( wordLen < lineLen && mOutputLoc.x + totalLen > lineLen )
			return spaceEnd;

		// print it
		while ( text < wordEnd )
			WriteCharInternal( *text++ );
	}

	return nullptr;
}

int ConScrollingTextWindow::Write_WordWrap( const char* text, int lineLen )
{
	int nNewLines = 0;

	// write first line
	text = Write_WordWrap_Line( text, lineLen );

	// write remainder
	while ( text )
	{
		WriteCharInternal( '\n' );
		nNewLines++;
		text = Write_WordWrap_Line( text, lineLen );
	}

	return nNewLines;
}

void ConScrollingTextWindow::WriteCharInternal( char c )
{
	// We only handle printable characters and tabs/newlines
	if ( c != '\t' && c != '\n' && !isprint( c ) )
		return;

	// Handle newline
	if ( c == '\n' || mOutputLoc.x == mPos.w )
	{
		mOutputLoc.x = 0;
		mOutputLoc.y++;

		// Scroll if needed
		if ( mOutputLoc.y == mPos.h )
		{
			mOutputLoc.y--;

			ConRect rect;
			rect.x = 0;
			rect.y = 1;
			rect.w = mPos.w;
			rect.h = mPos.h - 1;

			ConPoint target;
			target.x = 0;
			target.y = 0;

			ScrollWindow( rect, target, L' ' );
		}

		if ( c == '\n' )
			return;
	}

	// Handle tab
	if ( c == '\t' )
	{
		// TODO: Handle end-of-line tab better
		int n = 4 - ( mOutputLoc.x % 4 );
		for ( int i = 0; i < n; ++i )
			WriteCharInternal( ' ' );
		return;
	}

	SetChar( mOutputLoc.y, mOutputLoc.x, c );
	mOutputLoc.x++;
	SetCursor( mOutputLoc.y, mOutputLoc.x );
}

ConCharInputWindow::ConCharInputWindow( Console* owner, ConRect pos )
	: ConScrollingTextWindow( owner, pos )
{
}

bool ConCharInputWindow::ReadChar( int *pData )
{
	if ( mPendingKeys.empty() )
		return false;

	*pData = mPendingKeys.front();
	mPendingKeys.pop_front();
	return true;
}

void ConCharInputWindow::HandleKey( int key )
{
	mPendingKeys.push_back( key );
}
