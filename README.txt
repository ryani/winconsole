Library for creating Windows console-apps.

Basic usage:

- Create a Win32 console application in Visual Studio 2017.

- Initialize the console:
	Console console;
	console.Initialize( GetStdHandle( STD_OUTPUT_HANDLE ) );

- Create a window:
	ConCharInputWindow window( &console, console.GetScreenRect() );
        window.GiveFocus();

- Use it synchronously or in a game loop.  Synchronous example:

    // synchronous usage helper
    int ReadChar( Console& console, ConCharInputWindow& inputWindow ) {
        for(;;) {
            int rseult;
            if( inputWindow.ReadChar(&result) )
                return result;

            console.Update();
        }
    }

    // usage -- simple app that just echos typed characters
        for(;;) {
            int ch = ReadChar( console, window );
            char buf[2] = { (char)ch, 0 };
            window.Write( buf );
        }

- Clean up when finished (or let the destructors do it for you)
        console.Shutdown();
