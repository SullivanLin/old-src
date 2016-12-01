//              This is a multi-threaded app with two primary threads.  One
//              sits in the message loop, waiting specifically for WM_PAINT
//              messages which are generated by the other thread, on which
//              the actual unit test runs.
//
//              When the window thread receives an update message, it takes
//              a snapshot of the unit test state (protected by a mutex),
//              and redraws the screen accordingly.
//
//              When the unit test thread wants a resource to be drawn in
//              the main window, it places the handle to that resource (for
//              example, an HMETAFILEPICT) in the ctest object associated
//              with the window thread, then fires a screen update.  In
//              doing so, ownership of the resource is transfered from the
//              unit test thread to the window thread.  By using this
//              mechanism, the window thread can draw the resource at its
//              leisure, while the unit test proceeds.  The onus is on
//              the window thread to clean up any resources which have
//              been deposited in its care when it exists.
//
//              If the window thread receives a WM_CLOSE message, it must
//              first check to see that the unit test thread has completed.
//              If not, it spins in a RETRY/CANCEL loop until the unit test
//              thread has completed, or until the user selects CANCEL, at
//              which point execution proceeds, ignoring the WM_CLOSE.
//
//                  "OVER-ENGINEERED, AND BUILT TO STAY THAT WAY" (tm)
//


#include "headers.hxx"
#pragma hdrstop

CCacheTestApp ctest;    // Application instance
TestInstance  inst;     // Test instance

//
// Prototype for the entry-point of the unit test thread
//

unsigned long __stdcall testmain(void *);

//+---------------------------------------------------------------------------
//
//  Function:   WinMain
//
//  Synopsis:   windows entry point
//
//  Arguments:  [hInst]       --
//              [hPrevInst]   --
//              [lpszCmdLine] --
//              [nCmdShow]    --
//
//  Returns:    int
//
//  History:    05-Sep-94  davepl   Created
//
//  Notes:
//
//----------------------------------------------------------------------------

int WINAPI WinMain (HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszCmdLine, int nCmdShow)
{
     MSG message;

     //
     // Initialize the application
     //

     if (SUCCEEDED(ctest.Initialize(hInst, hPrevInst, lpszCmdLine)))
     {
	  //
	  // Show and update the window
	  //

	  ShowWindow(ctest.Window(), nCmdShow);
	  UpdateWindow(ctest.Window());

	  //
	  // The main message loop
	  //

	  while (GetMessage(&message, NULL, NULL, NULL))
	  {
	       TranslateMessage(&message);
	       DispatchMessage(&message);
	  }
     }
     else
     {
	  return(0);
     }

     return(message.wParam);
}

//+---------------------------------------------------------------------------
//
//  Member:     CCacheTestApp::CCacheTestApp
//
//  Synopsis:   Constructor
//
//  Arguments:  (none)
//
//  Returns:    nothing
//
//  History:    05-Sep-94   Davepl   Created
//
//  Notes:
//
//----------------------------------------------------------------------------

CCacheTestApp::CCacheTestApp ()
{

}

//+---------------------------------------------------------------------------
//
//  Member:     CCacheTestApp::Initialize
//
//  Synopsis:   initializes the application
//
//  Arguments:  [hInst]       -- current instance
//              [hPrevInst]   -- previous instance
//              [lpszCmdLine] -- command line parameters
//
//  Returns:    HRESULT
//
//  History:    05-Sep-94   Davepl   Created
//
//  Notes:
//
//----------------------------------------------------------------------------

HRESULT CCacheTestApp::Initialize (HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszCmdLine)
{
     HRESULT      hr = S_OK;

     //
     // Register the window class
     //

     if (hPrevInst == NULL)
     {
	  WNDCLASS wndclass;

	  wndclass.style         = 0;
	  wndclass.lpfnWndProc   = CacheTestAppWndProc;

	  wndclass.cbClsExtra    = 0;
	  wndclass.cbWndExtra    = 0;

	  wndclass.hInstance     = hInst;
	  wndclass.hIcon         = LoadIcon(hInst, IDI_EXCLAMATION);
	  wndclass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	  wndclass.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
	  wndclass.lpszMenuName  = NULL;
	  wndclass.lpszClassName = CTESTAPPCLASS;

	  if (RegisterClass(&wndclass) == 0)
	  {
	       hr = HRESULT_FROM_WIN32(GetLastError());
	  }
     }

     //
     // Create the mutex
     //

     m_hMutex = CreateMutex(NULL, FALSE, NULL);
     if (NULL == m_hMutex)
     {
	hr = HRESULT_FROM_WIN32(GetLastError());
     }

     //
     // Create the window
     //

     if (SUCCEEDED(hr))
     {
	  if ((m_hWnd = CreateWindowEx(0L,
				       CTESTAPPCLASS,
				       CTESTAPPTITLE,
				       WS_OVERLAPPEDWINDOW,
				       CW_USEDEFAULT,
				       0,
				       CW_USEDEFAULT,
				       0,
				       NULL,
				       NULL,
				       hInst,
				       NULL)) == NULL)
	  {
	       hr = HRESULT_FROM_WIN32(GetLastError());
	  }
     }

     return(hr);
}

//+---------------------------------------------------------------------------
//
//  Member:     CCacheTestApp::~CCacheTestApp
//
//  Synopsis:   Destructor
//
//  Arguments:  (none)
//
//  Returns:    nothing
//
//  History:    05-Sep-94   Davepl   Created
//
//  Notes:
//
//----------------------------------------------------------------------------

CCacheTestApp::~CCacheTestApp ()
{

}

//+---------------------------------------------------------------------------
//
//  Function:   CacheTestAppWndProc
//
//  Synopsis:   window procedure
//
//  Arguments:  [hWnd]    -- window
//              [message] -- message id
//              [wParam]  -- parameter
//              [lParam]  -- parameter
//
//  Returns:    LRESULT
//
//  History:    05-Sep-94   Davepl   Created
//
//  Notes:
//
//----------------------------------------------------------------------------
LRESULT FAR PASCAL CacheTestAppWndProc (HWND hWnd,
					UINT message,
					WPARAM wParam,
					LPARAM lParam)
{
     //
     // Process the messages
     //

     switch (message)
     {
     case WM_CREATE:

	//
	// The unit test window is opening.  Create another thread
	// on which the unit test itself runs, while this thread
	// continues to spin, waiting for redraws, close, and so
	// on...
	//

	ctest.m_hTest = CreateThread(NULL,
				     0,
				     testmain,
				     NULL,
				     0,
				     &ctest.m_dwThreadID);

	if (NULL == ctest.m_hTest)
	{
	    mprintf("Unable to begin unit test\n");
	    PostQuitMessage(0);
	}

	break;


     case WM_PAINT:
	  {
	       PAINTSTRUCT ps;
	       HDC         hDC;

	       //
	       // Get the DC for painting
	       //

	       hDC = BeginPaint(hWnd, &ps);
	       if (hDC)
	       {
		    //
		    // Draw the metafile
		    //

		    inst.Draw(hDC);

		    EndPaint(hWnd, &ps);
	       }
	  }
	  break;

     case WM_SIZE:

	  //
	  // Invalidate the rectangle
	  //

	  InvalidateRect(hWnd, NULL, TRUE);
	  return DefWindowProc(hWnd, message, wParam, lParam);
	  break;

     case WM_CLOSE:

	{
	    //
	    // The user has tried to exit the main program.  Before we
	    // can shut down, we must wait until the child thread has
	    // completed.  We allow the user to keep retrying on the
	    // thread, or to "cancel" and wait until later.  We do not
	    // provide the option of terminating the child thread while
	    // it is still busy.
	    //

	    DWORD dwStatus = 0;

	    if (FALSE == GetExitCodeThread(ctest.m_hTest, &dwStatus))
	    {
		mprintf("Could not get thread information!");
		break;
	    }
	    else
	    {
		INT response = IDRETRY;

		while (STILL_ACTIVE == dwStatus)
		{
		    response = MessageBox(ctest.Window(),
			       "The child thread has not yet completed.",
			       "Cannot Shutdown",
			       MB_RETRYCANCEL);

		    if (IDCANCEL == response)
		    {
			break;
		    }
		
		}
	    }
	
	    //
	    // Destroy the window if the child has gone away
	    //

	    if (STILL_ACTIVE != dwStatus)
	    {
		DestroyWindow(hWnd);
	    }

	    break;
	}

     case WM_DESTROY:
	
	  PostQuitMessage(0);
	  break;


     default:
	  return DefWindowProc(hWnd, message, wParam, lParam);
     }

     return NULL;
}

//+----------------------------------------------------------------------------
//
//      Member:
//
//      Synopsis:
//
//      Arguments:
//
//      Returns:        HRESULT
//
//      Notes:
//
//      History:        23-Aug-94  Davepl       Created
//
//-----------------------------------------------------------------------------

unsigned long __stdcall testmain(void *)
{
    HRESULT hr;

    hr = inst.CreateAndInit( OLESTR("mystg") );

    if (S_OK != hr)
    {
	mprintf("Cache Unit Test Failed [CreateAndInit] hr = %x\n", hr);
	goto exit;
    }

    hr = inst.EnumeratorTest();
    if (S_OK != hr)
    {
	mprintf("Cache Unit Test Failed [EnumeratorTest] hr = %x\n", hr);
	goto exit;
    }

    hr = inst.MultiCache(50);
    if (S_OK != hr)
    {
	mprintf("Cache Unit Test Failed [MultiCache] hr = %x\n", hr);
	goto exit;
    }

    hr = inst.CacheDataTest("TIGER.BMP", "TIGERNPH.WMF");
    if (S_OK != hr)
    {
	mprintf("Cache Unit Test Failed [CacheDataTest] hr = %x\n", hr);
	goto exit;
    }

exit:

    PostMessage(ctest.Window(), WM_CLOSE, (WPARAM) hr, 0);
    return (ULONG) hr;

}


//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::TestInstance
//
//      Synopsis:       Constructor
//
//      Arguments:
//
//      Returns:
//
//      Notes:
//
//      History:        23-Aug-94  Davepl       Created
//
//-----------------------------------------------------------------------------

TestInstance::TestInstance()
{

    m_pStorage        = NULL;
    m_pPersistStorage = NULL;
    m_pOleCache       = NULL;
    m_pOleCache2      = NULL;
    m_pDataObject     = NULL;
    m_pViewObject     = NULL;
    m_State           = TEST_STARTING;
}

TestInstance::~TestInstance()
{
    //
    // Release our interface pointers
    //

    if (m_pDataObject)
    {
	m_pDataObject->Release();
    }

    if (m_pViewObject)
    {
	m_pViewObject->Release();
    }

    if (m_pPersistStorage)
    {
	m_pPersistStorage->Release();
    }

    if (m_pOleCache2)
    {
	m_pOleCache2->Release();
    }

    if (m_pOleCache)
    {
	m_pOleCache->Release();
    }

    if (m_pStorage)
    {
	m_pStorage->Release();
    }
}

//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::CreateAndInit
//
//      Synopsis:       Creates a cache and sets up internal interface ptrs
//
//      Arguments:      (none)
//
//      Returns:        HRESULT
//
//      Notes:
//
//      History:        23-Aug-94  Davepl       Created
//
//-----------------------------------------------------------------------------

HRESULT TestInstance::CreateAndInit(LPOLESTR lpwszStgName)
{
    HRESULT hr;

    TraceLog Log(this, "TestInstance::CreateAndInit", GS_CACHE, VB_MINIMAL);
    Log.OnEntry (" ( %p ) \n", lpwszStgName);
    Log.OnExit  (" ( %X ) \n", &hr);
	
    //
    // Create the storage on which we will instantiate our cache
    //

    // BUGBUG use correct strcpy fn

    wcscpy(m_wszStorage, lpwszStgName);

    hr = StgCreateDocfile(lpwszStgName,
			  STGM_DIRECT |
			  STGM_READWRITE |
			  STGM_SHARE_EXCLUSIVE |
			  STGM_CREATE,
			  0,
			  &m_pStorage);

    //
    // Create a Data Cache on our IStorage
    //

    if (S_OK == hr)
    {
	hr = CreateDataCache(NULL,
			 CLSID_NULL,
			 IID_IPersistStorage,
			 (void **)&m_pPersistStorage);
    }

    if (S_OK == hr)
    {
	hr = m_pPersistStorage->InitNew(m_pStorage);
    }

    //
    // Get an IOleCache interface pointer to the cache
    //

    if (S_OK == hr)
    {
	hr = m_pPersistStorage->QueryInterface(IID_IOleCache,
					       (void **) &m_pOleCache);
    }

    //
    // Get an IOleCache2 interface pointer to the cache
    //

    if (S_OK == hr)
    {
	hr = m_pPersistStorage->QueryInterface(IID_IOleCache2,
					       (void **) &m_pOleCache2);
    }

    //
    // Get an IViewObject interface pointer to the cache
    //

    if (S_OK == hr)
    {
	hr = m_pPersistStorage->QueryInterface(IID_IViewObject,
					       (void **) &m_pViewObject);
    }

    //
    // Get an IDataObject interface pointer to the cache
    //

    if (S_OK == hr)
    {
	hr = m_pPersistStorage->QueryInterface(IID_IDataObject,
					       (void **) &m_pDataObject);
    }

    return hr;
}

//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::SaveAndReload
//
//      Synopsis:       Saves the cache to its storage and reloads it
//                      right back.
//
//      Arguments:      (none)
//
//      Returns:        HRESULT
//
//      Notes:          Once saved, the behavior of DiscardCache will
//                      change, since each node present at the time of
//                      save will have a stream from which it can demand
//                      load its data.
//
//                      Since each interface pointer is released and
//                      reaquired, the pointer values will (likely) change
//                      during this call; hence, so _not_ cache the pointers
//                      locally around this call.
//
//      History:        23-Aug-94  Davepl       Created
//
//-----------------------------------------------------------------------------

HRESULT TestInstance::SaveAndReload()
{
    HRESULT hr;

    TraceLog Log(NULL, "TestInstance::SaveAndReload", GS_CACHE, VB_MINIMAL);
    Log.OnEntry ();
    Log.OnExit  (" ( %X )\n", &hr);

    SetCurrentState(SAVE_AND_RELOAD);

    hr = m_pPersistStorage->Save(m_pStorage, TRUE);

    if (S_OK == hr)
    {
       hr = m_pPersistStorage->SaveCompleted(NULL);
    }

    // Release our hold on the storage and the cache

    if (S_OK == hr)
    {
	m_pViewObject->Release();
	m_pViewObject = NULL;
	m_pDataObject->Release();
	m_pDataObject = NULL;

	m_pStorage->Release();
	m_pStorage = NULL;

	m_pPersistStorage->Release();
	m_pPersistStorage = NULL;

	m_pOleCache2->Release();
	m_pOleCache2 = NULL;

	m_pOleCache->Release();
	m_pOleCache = NULL;


	//
	// Reload the cache and QI to get our interfaces back
	//

	hr = StgOpenStorage(m_wszStorage,
			NULL,
			STGM_DIRECT |
			STGM_READWRITE |
			STGM_SHARE_EXCLUSIVE,
			NULL,
			0,
			&m_pStorage);

	//
	// Create a Data Cache on our IStorage
	//

	if (S_OK == hr)
	{
	    hr = CreateDataCache(NULL,
				 CLSID_NULL,
				 IID_IPersistStorage,
				(void **)&m_pPersistStorage);
	}
	
	if (S_OK == hr)
	{
	     hr = m_pPersistStorage->Load(m_pStorage);
	}

	//
	// Get an IOleCache interface pointer to the cache
	//

	if (S_OK == hr)
	{
	    hr = m_pPersistStorage->QueryInterface(IID_IOleCache,
					       (void **) &m_pOleCache);
	}
	
	//
	// Get an IOleCache2 interface pointer to the cache
	//

	if (S_OK == hr)
	{
	    hr = m_pPersistStorage->QueryInterface(IID_IOleCache2,
					       (void **) &m_pOleCache2);
	}

	//
	// Get an IViewObject interface pointer to the cache
	//

	if (S_OK == hr)
	{
	    hr = m_pPersistStorage->QueryInterface(IID_IViewObject,
					       (void **) &m_pViewObject);
	}

	//
	// Get an IDataObject interface pointer to the cache
	//

	if (S_OK == hr)
	{
	    hr = m_pPersistStorage->QueryInterface(IID_IDataObject,
					       (void **) &m_pDataObject);
	}
    }

    return hr;
}


//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::CacheDataTest
//
//      Synopsis:       Checks the cache for data integrity
//
//      Arguments:      lpszBMPFileName - Name of .BMP file to use for test
//                      lpszWMFFileName - Name of .WMF file to use for test
//
//      Returns:        HRESULT
//
//      Notes:
//
//      History:        04-Sep-94  Davepl       Created
//
//-----------------------------------------------------------------------------

HRESULT TestInstance::CacheDataTest(char * lpszBMPFileName, char * lpszWMFFileName)
{
    HRESULT hr = S_OK;

    TraceLog Log(NULL, "TestInstance::CacheDataTest", GS_CACHE, VB_MINIMAL);
    Log.OnEntry (" ( BMP=%s, WMF=%s  )\n", lpszBMPFileName, lpszWMFFileName);
    Log.OnExit  (" ( %X )\n", &hr);

    SetCurrentState(DATA_TEST);

    CBitmapFile bmpFile;
    HGLOBAL     hDIB = NULL;
	
    //
    // Allocate an hglobal to hold our metafilepict structure
    //

    HGLOBAL hMFPICT = GlobalAlloc(GMEM_FIXED, sizeof(METAFILEPICT));
    if (NULL == hMFPICT)
    {
	hr = HRESULT_FROM_WIN32(GetLastError());
    }
    METAFILEPICT * pMFPICT = (METAFILEPICT *) hMFPICT;

    //
    // Load the bitmap from disk
    //

    if (S_OK == hr)
    {
	hr = bmpFile.LoadBitmapFile(lpszBMPFileName);
    }

    //
    // Create a DIB on an HGLOBAL from the bitmap
    //

    if (S_OK == hr)
    {
	hr = bmpFile.CreateDIBInHGlobal(&hDIB);
    }

    //
    // Add DIB and MF nodes to the cache
    //

    DWORD dwDIBCon;
    DWORD dwMFCon;

    if (S_OK == hr)
    {
	hr = AddDIBCacheNode(&dwDIBCon);
    }

    if (S_OK == hr)
    {
	hr = AddMFCacheNode(&dwMFCon);
    }

    //
    // Load the metafile from disk, then set up the
    // METAFILEPICT structure
    //

    if (S_OK == hr)
    {
	pMFPICT->hMF = GetMetaFileA(lpszWMFFileName);
	if (NULL == pMFPICT->hMF)
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
	}
	else
	{
	    //
	    // We deem the metafile to have the same extents
	    // as the the DIB.  This is completely arbitrary,
	    // but might aid in tracking down extents problems.
	    // After all, we have to pick _some_ value, so it
	    // might as well be a useful one...
	    //

	    pMFPICT->xExt = ConvWidthInPelsToLHM(NULL, bmpFile.GetDIBHeight());
	    pMFPICT->yExt = ConvHeightInPelsToLHM(NULL, bmpFile.GetDIBWidth());
	    pMFPICT->mm   = MM_ANISOTROPIC;
	}
    }

    //
    // Place the nodes in the cache.  We keep ownership of the handles,
    // which will force the cache to duplicate it.  We can then compare
    // our original with whatever we later get back from the cache.
    //

    FORMATETC fetcDIB =
		     {
			CF_DIB,
			NULL,
			DVASPECT_CONTENT,
			DEF_LINDEX,
			TYMED_HGLOBAL
		     };

    STGMEDIUM stgmDIB;

    FORMATETC fetcMF =
		     {
			CF_METAFILEPICT,
			NULL,
			DVASPECT_CONTENT,
			DEF_LINDEX,
			TYMED_MFPICT
		     };

    STGMEDIUM stgmMF;



    if (S_OK == hr)
    {
	stgmDIB.tymed   = TYMED_HGLOBAL;
	stgmDIB.hGlobal = hDIB;

	hr = m_pOleCache->SetData(&fetcDIB, &stgmDIB, FALSE);
    }

    if (S_OK == hr)
    {
	stgmMF.tymed = TYMED_MFPICT,
	stgmMF.hMetaFilePict = hMFPICT;

	hr = m_pOleCache->SetData(&fetcMF, &stgmMF, FALSE);
    }

    //
    // If we were able to place the data in the cache, check
    // to make sure whatever is in the cache matches our
    // original.
    //

    if (S_OK == hr)
    {
	hr = CompareDIB(hDIB);
	
	if (S_OK == hr)
	{
	   hr = CompareMF(hMFPICT);
	}
    }

    //
    // Save and Reload the cache to test persistance
    //

    if (S_OK == hr)
    {
	hr = SaveAndReload();
    }

    if (S_OK == hr)
    {
	SetCurrentState(DATA_TEST);
    }

    //
    // Compare the data again
    //

    if (S_OK == hr)
    {
	hr = CompareDIB(hDIB);
	
	if (S_OK == hr)
	{
	   hr = CompareMF(hMFPICT);
	}
    }

    //
    // Discard the cache.
    //

    if (S_OK == hr)
    {
	hr = m_pOleCache2->DiscardCache(DISCARDCACHE_NOSAVE);
    }

    //
    // Now compare again against the current presentations,
    // which would have to be demand-loaded after the discard.
    //
	
    if (S_OK == hr)
    {
	hr = CompareDIB(hDIB);
	
	if (S_OK == hr)
	{
	   hr = CompareMF(hMFPICT);
	}
    }


    //
    // Try to draw the cache's best presentation (which should
    // be metafile at this point) into a metafile DC which we
    // will then hand off to the window thread for drawing.
    //

    if (S_OK == hr)
    {
	hr = DrawCacheToMetaFilePict(&ctest.m_hMFP, FALSE);
	
	if (S_OK == hr)
	{
	    SetCurrentState(DRAW_METAFILE_NOW);
	}
    }

    //
    // Now draw the metafile tiled 4 times into the display
    // metafile, and hand it off...
    //

    if (S_OK == hr)
    {
	hr = DrawCacheToMetaFilePict(&ctest.m_hMFPTILED, TRUE);
	
	if (S_OK == hr)
	{
	    SetCurrentState(DRAW_METAFILETILED_NOW);
	}
    }

    //
    // Uncache the metafile node, which will leave the DIB node
    // as the best (and only) node left for drawing
    //

    if (S_OK == hr)
    {
	hr = UncacheFormat(CF_METAFILEPICT);
    }

    //
    // Now draw the DIB into a metafile and hand that off
    // to the window thread for drawing
    //

    if (S_OK == hr)
    {
	hr = DrawCacheToMetaFilePict(&ctest.m_hMFPDIB, FALSE);
	
	if (S_OK == hr)
	{
	    SetCurrentState(DRAW_DIB_NOW);
	}
    }

    //
    // Now draw the DIB again, this time tiled into the mf
    //
																	
    if (S_OK == hr)
    {
	hr = DrawCacheToMetaFilePict(&ctest.m_hMFPDIBTILED, TRUE);
	
	if (S_OK == hr)
	{
	    SetCurrentState(DRAW_DIBTILED_NOW);
	}
    }

    //
    // Cleanup our local DIB
    //

    if (hDIB)
    {
	GlobalFree(hDIB);
    }

    //
    // Cleaup our local metafile
    //

    if (pMFPICT)
    {
	if (pMFPICT->hMF)
	{
	    if (FALSE == DeleteMetaFile(pMFPICT->hMF))
	    {
		hr = HRESULT_FROM_WIN32(GetLastError());
	    }
	}

	GlobalFree(hMFPICT);
    }

    return hr;
}

HRESULT TestInstance::CompareDIB(HGLOBAL hDIB)
{
    return S_OK;
}

HRESULT TestInstance::CompareMF(HMETAFILEPICT hMFPICT)
{
    return S_OK;
}


//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::DrawCacheToMetaFilePict
//
//      Synopsis:       Draws the cache's current best presentation to
//                      a metafile, contained in a metafilepict structure,
//                      which is allocated off of the hGlobal pointer passed
//                      in by the caller
//
//      Arguments:      [phGlobal] - The ptr to the hglobal to allocate on
//                      [fTile]    - If true, the pres is tiled into the mf
//
//      Returns:        HRESULT
//
//      Notes:
//
//      History:        06-Sep-94  Davepl       Created
//
//-----------------------------------------------------------------------------

HRESULT TestInstance::DrawCacheToMetaFilePict(HGLOBAL *phGlobal, BOOL fTile)
{
    HRESULT hr = S_OK;

    TraceLog Log(NULL, "TestInstance::DrawCacheToMetaFilePict", GS_CACHE, VB_MINIMAL);
    Log.OnEntry (" ( %p, %d  )\n", phGlobal, fTile);
    Log.OnExit  (" ( %X )\n", &hr);

    //
    // Create a metafile, and have the cache draw its metafile
    // into _our_ metafile.
    //

    //
    // First, set up the METAFILEPICT structure.
    // Since ANISOTROPIC mode allows arbitrary scaling extents, we
    // pick 1000 as a nice arbitrary size.
    //
    //
	
    METAFILEPICT *pmfp = NULL;
    if (S_OK == hr)
    {
	*phGlobal = GlobalAlloc(GMEM_FIXED, sizeof(METAFILEPICT));
	if (NULL == *phGlobal)
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
	}
	else
	{
	    pmfp = (METAFILEPICT *) GlobalLock(*phGlobal);
	    if (NULL == pmfp)
	    {
		GlobalFree(*phGlobal);
		*phGlobal = NULL;
		hr = HRESULT_FROM_WIN32(GetLastError());
	    }
	    else
	    {
		pmfp->xExt = 1000;
		pmfp->yExt = 1000;
		pmfp->mm   = MM_ANISOTROPIC;
	    }
	}
    }

    //
    // Now create the metafile within the METAFILEPICT structure,
    // and ask the cache to draw to it.
    //

    HDC mfdc;
    if (S_OK == hr)
    {
	mfdc = CreateMetaFile(NULL);
	if (NULL == mfdc)
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
	    GlobalUnlock(*phGlobal);
	    GlobalFree(*phGlobal);
	    *phGlobal = NULL;
	}
    }

    //
    // If we are not tiling the metafile, we draw it exactly once,
    // scaled to fit the entire output metafile
    //

    if (S_OK == hr  && FALSE == fTile)
    {
	RECTL rcBounds  = {0, 0, 1000, 1000};
	RECTL rcWBounds = {0, 0, 1000, 1000};

	SetMapMode(mfdc, MM_ANISOTROPIC);
	SetWindowExtEx(mfdc, 1000, 1000, NULL);
	SetWindowOrgEx(mfdc, 0, 0, NULL);

	hr = m_pViewObject->Draw(DVASPECT_CONTENT, // Aspect
				 DEF_LINDEX,       // LIndex
				 NULL,             // pvAspect
				 NULL,             // ptd
				 NULL,             // hicTargetDev
				 mfdc,             // hdc to draw to
				 &rcBounds,        // rectange to draw to
				 &rcWBounds,       // bounds of our mfdc
				 NULL,             // callback fn
				 0);               // continue param
	
    }

    //
    // If we are tiling the metafile (which tests the ability of
    // the cache to offset and scale the presentation to a rect within
    // a larger metafile rect), we draw it once in each of the four
    // corners
    //

    if (S_OK == hr && TRUE == fTile)
    {
	RECTL rcBounds;
	RECTL rcWBounds = {0, 0, 1000, 1000};

	SetMapMode(mfdc, MM_ANISOTROPIC);
	SetWindowExtEx(mfdc, 1000, 1000, NULL);
	SetWindowOrgEx(mfdc, 0, 0, NULL);

	for (int a=0; a < 4 && S_OK == hr; a++)
	{
	    switch(a)
	    {
		case 0:         // Upper left hand tile

		    rcBounds.left   = 0;
		    rcBounds.top    = 0;
		    rcBounds.right  = 500;
		    rcBounds.bottom = 500;
		    break;

		case 1:         // Upper right hand tile

		    rcBounds.left   = 500;
		    rcBounds.top    = 0;
		    rcBounds.right  = 1000;
		    rcBounds.bottom = 500;
		    break;

		case 2:         // Lower left hand tile

		    rcBounds.left   = 0;
		    rcBounds.top    = 500;
		    rcBounds.right  = 500;
		    rcBounds.bottom = 1000;
		    break;

		case 3:         // Lower right hand tile

		    rcBounds.left   = 500;
		    rcBounds.top    = 500;
		    rcBounds.right  = 1000;
		    rcBounds.bottom = 1000;
		    break;
	    }
	
	    hr = m_pViewObject->Draw(DVASPECT_CONTENT, // Aspect
				     DEF_LINDEX,       // LIndex
				     NULL,             // pvAspect
				     NULL,             // ptd
				     NULL,             // hicTargetDev
				     mfdc,             // hdc to draw to
				     &rcBounds,        // rectange to draw to
				     &rcWBounds,       // bounds of our mfdc
				     NULL,             // callback fn
				     0);               // continue param
	}
    }
			
    //
    // If the draw failed, clean up the metafile DC now
    //
			
    if (S_OK != hr)
    {
	GlobalUnlock(*phGlobal);
	GlobalFree(*phGlobal);

	HMETAFILE temp = CloseMetaFile(mfdc);
	if (temp)
	{
	    DeleteMetaFile(temp);
	}
    }

    //
    // Finish up the metafile and prepare to return it to the caller
    //

    if (S_OK == hr)
    {
	pmfp->hMF = CloseMetaFile(mfdc);

	if (pmfp->hMF)
	{
	    GlobalUnlock(*phGlobal);
	}
	else
	{
	    hr = HRESULT_FROM_WIN32(GetLastError());
	    GlobalUnlock(*phGlobal);
	    GlobalFree(*phGlobal);
	    *phGlobal = NULL;
	}
    }

    return hr;
}

//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::GetCurrentState
//
//      Synopsis:       Returns the state of the unit test (for drawing)
//
//      Arguments:      (none)
//
//      Returns:        HRESULT
//
//      Notes:
//
//      History:        04-Sep-94  Davepl       Created
//
//-----------------------------------------------------------------------------

TEST_STATE TestInstance::GetCurrentState()
{
    //
    // In order to avoid race conditions, we have a mutex around the
    // current state of the unit test (required because this member
    // function will be running on the window's thread, not the current
    // test instance thread.)
    //

    DWORD dwResult = WaitForSingleObject(ctest.Mutex(), INFINITE);
    if (WAIT_FAILED == dwResult)
    {
	return INVALID_STATE;
    }

    TEST_STATE tsSnapshot = m_State;

    ReleaseMutex(ctest.Mutex());

    return tsSnapshot;
}

//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::SetCurrentState
//
//      Synopsis:       Sets the current (drawing) state of the unit test
//
//      Arguments:      [state] - the state to set
//
//      Returns:        HRESULT
//
//      Notes:
//
//      History:        04-Sep-94  Davepl       Created
//
//-----------------------------------------------------------------------------

void TestInstance::SetCurrentState(TEST_STATE state)
{
    //
    // In order to avoid race conditions, we have a mutex around the
    // current state of the unit test (required because this member
    // function will be running on the window's thread, not the current
    // test instance thread.)
    //

    DWORD dwResult = WaitForSingleObject(ctest.Mutex(), INFINITE);
    if (WAIT_FAILED == dwResult)
    {
	return;
    }

    m_State = state;

    ReleaseMutex(ctest.Mutex());

    //
    // Invalid the main window so it will redraw itself with the new
    // state of the test.
    //

    InvalidateRgn(ctest.Window(), NULL, FALSE);
    UpdateWindow(ctest.Window());
	
}

//+----------------------------------------------------------------------------
//
//      Member:         TestInstance::Draw
//
//      Synopsis:       Draws the current state of the unit test
//
//      Arguments:      [hdc]   - The DC to draw to
//
//      Returns:        HRESULT
//
//      Notes:          The DC is supplied, but the main window is assumed
//
//      History:        04-Sep-94  Davepl       Created
//
//-----------------------------------------------------------------------------

static char szStarting[]      = "Test is starting...";
static char szInvalid[]       = "The state of the test has become invalid...";
static char szEnumerator[]    = "Testing the cache enumerator...";
static char szSaveReload[]    = "Saving and reloading the cache and its data...";
static char szDataTest[]      = "Testing data integrity within the cache...";
static char szMulti[]         = "Testing a large number of simultaneous cache nodes...";
static char szMetafile[]      = "MF -> MF";
static char szMetafileTiled[] = "MF -> MF (Tiled)";
static char szDib[]           = "";     // Dib contains its own title

void TestInstance::Draw(HDC hdc)
{
    //
    // Retrieve the current state of the unit test
    //

    TEST_STATE tsState = GetCurrentState();

    //
    // Clear the window
    //

    RECT rect;
    if (TRUE == GetClientRect(ctest.Window(), &rect))
    {
	FillRect(hdc, &rect, (HBRUSH) GetStockObject(LTGRAY_BRUSH));
    }

    //
    // Draw the current state
    //

    int iBackMode = SetBkMode(hdc, TRANSPARENT);

    switch(tsState)
    {
	case TEST_STARTING:

	    TextOut(hdc, 10, 10, szStarting, strlen(szStarting));
	    break;

	case TESTING_ENUMERATOR:
	
	    TextOut(hdc, 10, 10, szEnumerator, strlen(szEnumerator));
	    break;

	case SAVE_AND_RELOAD:

	    TextOut(hdc, 10, 10, szSaveReload, strlen(szSaveReload));
	    break;

	case DATA_TEST:

	    TextOut(hdc, 10, 10, szDataTest, strlen(szDataTest));
	    break;

	case MULTI_CACHE:

	    TextOut(hdc, 10, 10, szMulti, strlen(szMulti));
	    break;

	case DRAW_METAFILE_NOW:
	case DRAW_METAFILETILED_NOW:
	case DRAW_DIB_NOW:
	case DRAW_DIBTILED_NOW:
	{
	    // We know now that we have to draw a metafile, so
	    // determine which of the metafiles we should be drawing,
	    // and set a handle (so we can reuse the draw code) and
	    // the description text appropriately.

	    HGLOBAL hMFP;
	    char * szDesc;
	
	    if (DRAW_METAFILE_NOW == tsState)
	    {
		hMFP = ctest.m_hMFP;
		szDesc = szMetafile;
	    }
	    else if (DRAW_METAFILETILED_NOW == tsState)
	    {
		hMFP = ctest.m_hMFPTILED;
		szDesc = szMetafileTiled;
	    }
	    else if (DRAW_DIB_NOW == tsState)
	    {
		hMFP = ctest.m_hMFPDIB;
		szDesc = szDib;
	    }
	    else if (DRAW_DIBTILED_NOW == tsState)
	    {
		hMFP = ctest.m_hMFPDIBTILED;
		szDesc = szDib;
	    }

	    TextOut(hdc, 10, 10, szDesc, strlen(szDesc));
			
	    //
	    // Now actually draw the metafile to our main window
	    //
		
	    if (hMFP)
	    {
		METAFILEPICT *pMFP = (METAFILEPICT *) GlobalLock(hMFP);
		if (NULL == pMFP)
		{
		    mprintf("Unable to lock metafile handle");
		    break;
		}

		int save = SaveDC(hdc);

		SetMapMode(hdc, pMFP->mm);
		SetWindowExtEx(hdc, pMFP->xExt, pMFP->yExt, NULL);
		
		RECT client;
		GetClientRect(ctest.Window(), &client);

		SetViewportExtEx(hdc, client.right, client.bottom, NULL);
		SetWindowOrgEx(hdc, 0, 0, NULL);
		SetViewportOrgEx(hdc, client.left, client.top, NULL);
		
		PlayMetaFile(hdc, pMFP->hMF);
		
		RestoreDC(hdc, save);

	    }
	    break;
	}

	case INVALID_STATE:
	default:

	    TextOut(hdc, 10, 10, szInvalid, strlen(szInvalid));
	    break;

    }

    SetBkMode(hdc, iBackMode);

}
