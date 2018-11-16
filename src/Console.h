//////////////////////////////////////////////////////////////////////////
// Console - quick curses-like library for windows by Ryan Ingram
// from http://github.com/ryani/hoppity (GPL licensed there)
// Licensed for non-GPL usage to Valve by permission of the author

#ifndef CONSOLE_H
#define CONSOLE_H

#include <windows.h>
#include <string>
#include <deque>
#include <memory>
#include <vector>

struct ConRect
{
	ConRect() : x( 0 ), y( 0 ), w( 0 ), h( 0 ) {}
	ConRect( int x_, int y_, int w_, int h_ ) : x( x_ ), y( y_ ), w( w_ ), h( h_ ) {}

	int x, y, w, h;
};

struct ConPoint
{
	ConPoint() : x( 0 ), y( 0 ) {}
	ConPoint( int x_, int y_ ) : x( x_ ), y( y_ ) {}

	int x, y;
};

class ConWindowBase;

// TODO: We don't currently handle overlapping windows.  Windows must be distinct coordinates
class Console
{
public:
	Console();
	~Console();

	bool Initialize(HANDLE screenHandle);
	void Update();
	void Shutdown();

	void SetFocusWindow( ConWindowBase* pWindow );

	ConRect GetRect() { return mRect; }
	ConRect GetScreenRect() { return mScreenRect; }

private:
	Console(const Console&) = delete;
	Console& operator=(const Console&) = delete;

	void ResetCursor();

	ConRect mRect;
	ConRect mScreenRect;

	std::vector< ConWindowBase* > mWindows;
	ConWindowBase* mFocusWindow;

	// ConWindowBase interface
	friend class ConWindowBase;
	void AddWindow( ConWindowBase* pWindow );
	void RemoveWindow( ConWindowBase* pWindow );
	HANDLE mHandle;
	WORD mDefaultAttributes;
};

class ConWindowBase
{
public:
	ConWindowBase( Console* owner, ConRect pos );
	~ConWindowBase();

	ConWindowBase( const ConWindowBase& ) = delete;
	ConWindowBase& operator=( const ConWindowBase& ) = delete;

	bool HasFocus(); // cursor only appears if in focus
	void GiveFocus();

	ConRect GetRect() { return mPos; }
	ConPoint GetCursor() { return mCursor; }

protected:
	// basic interface
	void SetChar( int row, int col, wchar_t c );
	wchar_t GetChar( int row, int col );
	void SetCursor( int row, int col );
	void ScrollWindow( ConRect moveFrom, ConPoint targetPos, wchar_t fillChar );

	// when in focus
	virtual void HandleKey( int keyCode );
	virtual void HandleFunctionKey( int specialKeyCode );

protected:
	Console* mOwner;
	bool mbFocus;
	ConRect mPos;
	ConPoint mCursor;

	// size = mPos.w * mPos.h
	wchar_t* mData = nullptr;

	friend class Console;
};

class ConScrollingTextWindow : public ConWindowBase
{
public:
	ConScrollingTextWindow( Console* owner, ConRect pos );

	void Clear(); // places cursor in top-left
	void Write( const char* text );
	int Write_WordWrap( const char* text, int lineLen = 0 ); // returns # of newlines written
	const char* Write_WordWrap_Line( const char* text, int lineLen = 0 ); // returns first char to write on next line, or null if done

private:
	void WriteCharInternal( char c );

	ConPoint mOutputLoc;
};

class ConLineInputWindow : public ConWindowBase
{
public:
	ConLineInputWindow( Console* owner, ConRect pos );

	bool ReadLine( std::string* pInput );

protected:
	virtual void HandleKey( int keyCode ) override;
	virtual void HandleFunctionKey( int keyCode ) override;

private:
	void AddInputChar( int ch );
	void UpdateInputLine();
	void ResetCursor();

	// data user is currently inputting
	std::wstring mCurrentInput;

	// data ready to be returned by ReadLine
	std::deque<std::string> mPendingInput;
};

class ConCharInputWindow : public ConScrollingTextWindow
{
public:
	ConCharInputWindow( Console* owner, ConRect pos );

	bool ReadChar( int* pData );

protected:
	virtual void HandleKey( int keyCode ) override;

private:
	std::deque<int> mPendingKeys;
};

#endif // CONSOLE_H